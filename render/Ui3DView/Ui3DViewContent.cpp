#include "Ui3DViewContent.h"
#include "../../shaders/Ui3DMeshVertSpirv.h"
#include "../../shaders/Ui3DMeshFragSpirv.h"
#include "../../shaders/Ui3DEdgeFragSpirv.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Upp {

Scene3DVec3 Ui3DViewGridLayout::ToWorld(double u, double v) const
{
	switch(plane) {
	case Ui3DGridPlane::XY: return Scene3DVec3(u, v, 0);
	case Ui3DGridPlane::YZ: return Scene3DVec3(0, u, v);
	case Ui3DGridPlane::XZ: return Scene3DVec3(u, 0, v);
	}
	return Scene3DVec3();
}

bool Ui3DViewGridLayout::operator==(const Ui3DViewGridLayout& other) const
{
	return plane == other.plane && spacing == other.spacing && center_u == other.center_u &&
	       center_v == other.center_v && extent == other.extent;
}

bool ComputeUi3DViewGridLayout(const Scene3DCamera& camera, const Ui3DViewOptions& options,
                              Size viewport, Ui3DViewGridLayout& out)
{
	out = Ui3DViewGridLayout();
	String error;
	if(viewport.cx <= 0 || viewport.cy <= 0 || !camera.Validate(error) ||
	   options.grid_extent < 1 || options.grid_extent > 100 || !std::isfinite(options.grid_spacing) ||
	   options.grid_spacing < 0.001 || options.grid_spacing > 10000 ||
	   (options.grid_plane != Ui3DGridPlane::XY && options.grid_plane != Ui3DGridPlane::YZ &&
	    options.grid_plane != Ui3DGridPlane::XZ)) return false;
	Ui3DViewGridLayout grid;
	grid.plane = options.grid_plane; grid.extent = options.grid_extent; grid.spacing = options.grid_spacing;
	if(options.adaptive_grid) {
		const double height = camera.orthographic ? camera.ortho_height :
		                      2 * camera.distance * std::tan(camera.fov_y * 0.5);
		const double width = height * ((double)viewport.cx / viewport.cy);
		// Maintain readable cells and enough bounded coverage after a half-cell snap.
		const double requested = std::max(height * 48.0 / viewport.cy,
		                                  std::max(width, height) / (2 * grid.extent - 1));
		const double decade = std::pow(10.0, std::floor(std::log10(requested / options.grid_spacing)));
		const double fraction = requested / (options.grid_spacing * decade);
		const double nice = fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10;
		grid.spacing = std::max(1e-9, std::min(1e10, options.grid_spacing * decade * nice));
		double u = camera.target.x, v = camera.target.z;
		if(grid.plane == Ui3DGridPlane::XY) v = camera.target.y;
		if(grid.plane == Ui3DGridPlane::YZ) u = camera.target.y;
		grid.center_u = std::round(u / grid.spacing) * grid.spacing;
		grid.center_v = std::round(v / grid.spacing) * grid.spacing;
	}
	out = grid;
	return true;
}

namespace {

struct MeshVertex {
	float x, y, z;
	float nx, ny, nz;
	float r, g, b, a;
};
static_assert(sizeof(MeshVertex) == 40, "untextured RHI 3D vertex stride");

struct MeshParameters {
	float mvp[16];
	float normal[12]; // Padded normal columns; w: lighting/source, sRGB encoding, edge bias.
	float tint[4];
};
static_assert(sizeof(MeshParameters) == 128, "portable minimum graphics push payload");

float Linear(byte value)
{
	const float x = value / 255.0f;
	return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}
bool IsSrgb(GpuFormat format)
{
	return format == GpuFormat::RGBA8Srgb || format == GpuFormat::BGRA8Srgb;
}
GpuClearColor ClearColor(Rgba8 color, GpuFormat format)
{
	GpuClearColor result;
	result.red = IsSrgb(format) ? Linear(color.r) : color.r / 255.0f;
	result.green = IsSrgb(format) ? Linear(color.g) : color.g / 255.0f;
	result.blue = IsSrgb(format) ? Linear(color.b) : color.b / 255.0f;
	result.alpha = 1;
	return result;
}

struct MeshGpu {
	std::shared_ptr<const Scene3DMesh> source;
	GpuBufferId vertices, indices, edges;
	int index_count = 0, edge_count = 0;
};

class ViewContent final : public GpuSurfaceContent {
public:
	explicit ViewContent(std::shared_ptr<Ui3DViewState> value) : state(std::move(value)) {}
	bool Render(GpuDevice& device, const GpuFrameInfo& frame, String& error) override;
	void Close(GpuDevice& device) override;

private:
	std::shared_ptr<Ui3DViewState> state;
	std::shared_ptr<const Scene3DSnapshot> cached_scene;
	std::map<const Scene3DMesh *, MeshGpu> meshes;
	MeshGpu grid, axes;
	uint64 decoration_revision = 0, selection_revision = 0;
	Index<String> selected;
	GpuShaderId vertex_shader, fragment_shader, edge_shader;
	GpuPipelineId triangles, wire_lines, overlay_lines;
	Ui3DViewGridLayout grid_layout;
	bool have_grid_layout = false;
	GpuTextureId depth;
	Size depth_size = Size(0, 0);
	GpuFormat target_format = GpuFormat::Unknown;

	bool Result(GpuDevice& device, GpuResult result, const char *operation, String& error);
	bool Buffer(GpuDevice& device, int usage, const void *data, int64 bytes,
	            GpuBufferId& id, String& error);
	void ReleaseMesh(GpuDevice& device, MeshGpu& mesh);
	bool EnsureMesh(GpuDevice& device, std::shared_ptr<const Scene3DMesh> source, String& error);
	bool EnsureDecorations(GpuDevice& device, Size viewport, String& error);
	bool EnsurePipelines(GpuDevice& device, GpuFormat format, String& error);
	bool EnsureDepth(GpuDevice& device, Size size, String& error);
	MeshParameters Parameters(const Scene3DMatrix& view_projection,
	                          const Scene3DMatrix& world, const Scene3DMatrix& inverse,
	                          float r, float g, float b, float a, bool lighting) const;
};

bool ViewContent::Result(GpuDevice& device, GpuResult result, const char *operation, String& error)
{
	if(result == GpuResult::Ok) return true;
	error = String(operation) + ": " + DumpGpuResult(result);
	String detail = device.GetLastError();
	if(!detail.IsEmpty()) error << " (" << detail << ")";
	state->error = error;
	return false;
}

bool ViewContent::Buffer(GpuDevice& device, int usage, const void *data, int64 bytes,
                         GpuBufferId& id, String& error)
{
	GpuBufferDesc desc;
	desc.size = bytes;
	desc.usage = usage | GpuBufferUsage_TransferDst;
	desc.label = "Ui3DView retained geometry";
	if(!Result(device, device.CreateBuffer(desc, id), "create mesh buffer", error)) return false;
	if(!Result(device, device.WriteBuffer(id, 0, data, bytes), "upload mesh buffer", error)) {
		device.DestroyBuffer(id); id = GpuBufferId(); return false;
	}
	state->stats.uploaded_bytes += bytes;
	return true;
}

void ViewContent::ReleaseMesh(GpuDevice& device, MeshGpu& mesh)
{
	if(mesh.edges.IsValid()) device.DestroyBuffer(mesh.edges);
	if(mesh.indices.IsValid()) device.DestroyBuffer(mesh.indices);
	if(mesh.vertices.IsValid()) device.DestroyBuffer(mesh.vertices);
	mesh = MeshGpu();
}

bool ViewContent::EnsureMesh(GpuDevice& device, std::shared_ptr<const Scene3DMesh> source, String& error)
{
	if(meshes.find(source.get()) != meshes.end()) return true;
	MeshGpu mesh;
	mesh.source = source;
	std::vector<MeshVertex> vertices;
	vertices.reserve(source->GetVertices().size());
	for(const auto& v : source->GetVertices()) {
		MeshVertex vertex = {
			(float)v.position.x, (float)v.position.y, (float)v.position.z,
			(float)v.normal.x, (float)v.normal.y, (float)v.normal.z,
			v.r, v.g, v.b, v.a
		};
		vertices.push_back(vertex);
	}
	const auto& indices = source->GetIndices();
	struct Edge { uint32 a, b; };
	static_assert(sizeof(Edge) == 2 * sizeof(uint32), "packed edge index pair");
	std::vector<Edge> edges;
	edges.reserve(indices.size());
	auto edge = [&](uint32 a, uint32 b) { edges.push_back({ std::min(a, b), std::max(a, b) }); };
	for(size_t i = 0; i < indices.size(); i += 3) {
		edge(indices[i], indices[i + 1]); edge(indices[i + 1], indices[i + 2]); edge(indices[i + 2], indices[i]);
	}
	std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		return a.a < b.a || (a.a == b.a && a.b < b.b);
	});
	edges.erase(std::unique(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
		return a.a == b.a && a.b == b.b;
	}), edges.end());
	bool ok = Buffer(device, GpuBufferUsage_Vertex, vertices.data(),
	                 (int64)vertices.size() * sizeof(MeshVertex), mesh.vertices, error) &&
	          Buffer(device, GpuBufferUsage_Index, indices.data(),
	                 (int64)indices.size() * sizeof(uint32), mesh.indices, error) &&
	          Buffer(device, GpuBufferUsage_Index, edges.data(),
	                 (int64)edges.size() * sizeof(Edge), mesh.edges, error);
	if(!ok) { ReleaseMesh(device, mesh); return false; }
	mesh.index_count = (int)indices.size();
	mesh.edge_count = (int)edges.size() * 2;
	meshes.emplace(source.get(), std::move(mesh));
	++state->stats.mesh_uploads;
	state->stats.resident_meshes = (int)meshes.size();
	return true;
}

bool ViewContent::EnsureDecorations(GpuDevice& device, Size viewport, String& error)
{
	if(!state->options.show_grid && !state->options.show_axes) return true;
	Ui3DViewGridLayout next;
	if(!ComputeUi3DViewGridLayout(state->camera, state->options, viewport, next)) {
		error = state->error = "invalid camera/grid state"; return false;
	}
	if(have_grid_layout && next == grid_layout && decoration_revision == state->decoration_revision) return true;
	std::vector<MeshVertex> vertices;
	std::vector<uint32> indices;
	auto line = [&](Scene3DVec3 a, Scene3DVec3 b, Rgba8 color) {
		const uint32 index = (uint32)vertices.size();
		for(const Scene3DVec3& p : { a, b })
			vertices.push_back({(float)p.x, (float)p.y, (float)p.z, 0, 1, 0,
			                    Linear(color.r), Linear(color.g), Linear(color.b), 1});
		indices.push_back(index); indices.push_back(index + 1);
	};
	const double step = next.spacing, span = next.extent * step;
	for(int i = -next.extent; i <= next.extent; ++i) {
		const double u = next.center_u + i * step, v = next.center_v + i * step;
		line(next.ToWorld(next.center_u - span, v), next.ToWorld(next.center_u + span, v), state->options.grid_color);
		line(next.ToWorld(u, next.center_v - span), next.ToWorld(u, next.center_v + span), state->options.grid_color);
	}
	MeshGpu new_grid, new_axes;
	bool ok = Buffer(device, GpuBufferUsage_Vertex, vertices.data(),
	                 (int64)vertices.size() * sizeof(MeshVertex), new_grid.vertices, error) &&
	          Buffer(device, GpuBufferUsage_Index, indices.data(),
	                 (int64)indices.size() * sizeof(uint32), new_grid.indices, error);
	new_grid.index_count = (int)indices.size();
	vertices.clear(); indices.clear();
	const double length = std::min(span, 3.0 * step);
	line(Scene3DVec3(), Scene3DVec3(length, 0, 0), Rgba8(213, 91, 94));
	line(Scene3DVec3(), Scene3DVec3(0, length, 0), Rgba8(110, 186, 120));
	line(Scene3DVec3(), Scene3DVec3(0, 0, length), Rgba8(99, 153, 226));
	ok = ok &&
	     Buffer(device, GpuBufferUsage_Vertex, vertices.data(),
	            (int64)vertices.size() * sizeof(MeshVertex), new_axes.vertices, error) &&
	     Buffer(device, GpuBufferUsage_Index, indices.data(),
	            (int64)indices.size() * sizeof(uint32), new_axes.indices, error);
	new_axes.index_count = (int)indices.size();
	if(!ok) { ReleaseMesh(device, new_grid); ReleaseMesh(device, new_axes); return false; }
	ReleaseMesh(device, grid); ReleaseMesh(device, axes);
	grid = std::move(new_grid); axes = std::move(new_axes);
	grid_layout = next; have_grid_layout = true;
	decoration_revision = state->decoration_revision;
	return true;
}

bool ViewContent::EnsurePipelines(GpuDevice& device, GpuFormat format, String& error)
{
	if(!vertex_shader.IsValid()) {
		GpuShaderDesc desc;
		desc.stage = GpuShaderStage::Vertex; desc.format = GpuShaderFormat::SpirV;
		desc.code = String(reinterpret_cast<const char *>(kUi3DMeshVertex), sizeof(kUi3DMeshVertex));
		desc.label = "Ui3DView mesh vertex";
		if(!Result(device, device.CreateShader(desc, vertex_shader), "create vertex shader", error)) return false;
	}
	if(!fragment_shader.IsValid()) {
		GpuShaderDesc desc;
		desc.stage = GpuShaderStage::Fragment; desc.format = GpuShaderFormat::SpirV;
		desc.code = String(reinterpret_cast<const char *>(kUi3DMeshFragment), sizeof(kUi3DMeshFragment));
		desc.label = "Ui3DView mesh fragment";
		if(!Result(device, device.CreateShader(desc, fragment_shader), "create fragment shader", error)) return false;
	}
	if(!edge_shader.IsValid()) {
		GpuShaderDesc desc;
		desc.stage = GpuShaderStage::Fragment; desc.format = GpuShaderFormat::SpirV;
		desc.code = String(reinterpret_cast<const char *>(kUi3DEdgeFragment), sizeof(kUi3DEdgeFragment));
		desc.label = "Ui3DView visible edge fragment";
		if(!Result(device, device.CreateShader(desc, edge_shader), "create edge fragment shader", error)) return false;
	}
	if(target_format != format) {
		if(triangles.IsValid()) device.DestroyPipeline(triangles);
		if(wire_lines.IsValid()) device.DestroyPipeline(wire_lines);
		if(overlay_lines.IsValid()) device.DestroyPipeline(overlay_lines);
		triangles = GpuPipelineId(); wire_lines = GpuPipelineId(); overlay_lines = GpuPipelineId();
		target_format = format;
	}
	GpuPipelineDesc desc;
	desc.color_format = format;
	desc.vertex_shader = vertex_shader; desc.fragment_shader = fragment_shader;
	desc.vertex_layout = GpuVertexLayout::Position3Normal3Color4F;
	desc.parameter_bytes = sizeof(MeshParameters);
	desc.depth_format = GpuFormat::D32F;
	desc.depth_test = true; desc.depth_write = true;
	desc.depth_compare = GpuCompareOp::LessEqual;
	desc.cull_mode = GpuCullMode::None;
	desc.front_face = GpuFrontFace::CounterClockwise;
	desc.label = "Ui3DView opaque depth mesh";
	if(!triangles.IsValid() &&
	   !Result(device, device.CreatePipeline(desc, triangles), "create depth triangle pipeline", error)) return false;
	desc.topology = GpuPrimitiveTopology::LineList;
	desc.fragment_shader = edge_shader;
	desc.label = "Ui3DView primary wire lines";
	if(!wire_lines.IsValid() &&
	   !Result(device, device.CreatePipeline(desc, wire_lines), "create wire line pipeline", error)) return false;
	desc.depth_write = false;
	desc.label = "Ui3DView visible edge overlays";
	if(!overlay_lines.IsValid() &&
	   !Result(device, device.CreatePipeline(desc, overlay_lines), "create edge overlay pipeline", error)) return false;
	return true;
}

bool ViewContent::EnsureDepth(GpuDevice& device, Size size, String& error)
{
	if(depth.IsValid() && size == depth_size) return true;
	if(size.cx <= 0 || size.cy <= 0 || (int64)size.cx * size.cy > 16 * 1024 * 1024) {
		error = state->error = "Ui3DView target must be nonempty and at most 16 million pixels";
		return false;
	}
	GpuTextureCapabilities caps;
	if(!Result(device, device.GetTextureCapabilities(GpuFormat::D32F,
	                  GpuTextureUsage_DepthAttachment, caps), "query depth support", error)) return false;
	if(!caps.depth_attachment) {
		error = state->error = "backend does not support the required D32F depth attachment"; return false;
	}
	GpuTextureDesc desc;
	desc.size = size; desc.format = GpuFormat::D32F;
	desc.usage = GpuTextureUsage_DepthAttachment;
	desc.label = "Ui3DView retained depth";
	GpuTextureId replacement;
	if(!Result(device, device.CreateTexture(desc, replacement), "create depth target", error)) return false;
	if(depth.IsValid()) device.DestroyTexture(depth);
	depth = replacement; depth_size = size;
	++state->stats.depth_rebuilds;
	return true;
}

MeshParameters ViewContent::Parameters(const Scene3DMatrix& view_projection,
                                      const Scene3DMatrix& world, const Scene3DMatrix& inverse,
                                      float r, float g, float b, float a, bool lighting) const
{
	MeshParameters parameters {};
	Scene3DMatrix mvp = view_projection * world;
	for(int i = 0; i < 16; ++i) parameters.mvp[i] = (float)mvp.m[i];
	for(int column = 0; column < 3; ++column)
		for(int row = 0; row < 3; ++row)
			parameters.normal[column * 4 + row] = (float)inverse.m[row * 4 + column];
	parameters.normal[3] = lighting ? 1.0f : 0.0f;
	parameters.normal[7] = IsSrgb(target_format) ? 0.0f : 1.0f;
	parameters.tint[0] = r; parameters.tint[1] = g;
	parameters.tint[2] = b; parameters.tint[3] = a;
	return parameters;
}

bool ViewContent::Render(GpuDevice& device, const GpuFrameInfo& frame, String& error)
{
	if(!EnsurePipelines(device, frame.color_format, error) ||
	   !EnsureDepth(device, frame.size, error) || !EnsureDecorations(device, frame.size, error)) return false;

	const auto scene = state->GetRenderScene();
	if(cached_scene != scene) {
		std::set<const Scene3DMesh *> used;
		if(scene) for(const auto& object : scene->GetObjects())
			if(object.visible && object.mesh) used.insert(object.mesh.get());
		for(auto it = meshes.begin(); it != meshes.end();) {
			if(used.find(it->first) == used.end()) {
				ReleaseMesh(device, it->second); it = meshes.erase(it);
			}
			else ++it;
		}
		cached_scene = scene;
		state->stats.resident_meshes = (int)meshes.size();
	}
	if(scene) for(const auto& object : scene->GetObjects())
		if(object.visible && object.mesh && !EnsureMesh(device, object.mesh, error)) return false;
	if(selection_revision != state->selection_revision) {
		selected.Clear();
		for(const String& id : state->selection) selected.FindAdd(id);
		selection_revision = state->selection_revision;
	}
	const Scene3DMatrix view_projection = state->camera.GetViewProjection(frame.size);
	GpuCommandListId command;
	if(!Result(device, device.BeginCommands(command), "begin mesh commands", error)) return false;
	auto fail = [&] {
		// No partial erroneous frame is submitted. This also releases references
		// retained while command recording was interrupted.
		device.DiscardCommands(command);
		return false;
	};
	GpuRenderPassDesc pass;
	pass.flip_viewport_y = false; // Projection already applies Vulkan's Y orientation.
	pass.color_target = frame.color_target; pass.color_format = frame.color_format;
	pass.color_load = GpuLoadOp::Clear; pass.color_store = GpuStoreOp::Store;
	pass.clear_color = ClearColor(state->options.background, frame.color_format);
	pass.depth_target = depth; pass.depth_format = GpuFormat::D32F;
	pass.depth_load = GpuLoadOp::Clear; pass.depth_store = GpuStoreOp::DontCare;
	pass.clear_depth = 1;
	pass.label = "Ui3DView opaque mesh and depth";
	if(!Result(device, device.BeginRenderPass(command, pass), "begin mesh pass", error)) return fail();
	int draws = 0;
	GpuPipelineId current;
	auto draw = [&](const MeshGpu& mesh, GpuPipelineId pipeline, bool edges, const MeshParameters& parameters) {
		if(pipeline != current) {
			if(!Result(device, device.SetPipeline(command, pipeline), "bind mesh pipeline", error)) return false;
			current = pipeline;
		}
		const GpuBufferId index = edges && mesh.edges.IsValid() ? mesh.edges : mesh.indices;
		const int count = edges && mesh.edges.IsValid() ? mesh.edge_count : mesh.index_count;
		if(!Result(device, device.SetVertexBuffer(command, mesh.vertices), "bind mesh vertices", error) ||
		   !Result(device, device.SetIndexBuffer(command, index, GpuIndexType::UInt32), "bind mesh indices", error) ||
		   !Result(device, device.SetGraphicsParameters(command, &parameters, sizeof(parameters)), "bind mesh parameters", error) ||
		   !Result(device, device.DrawIndexed(command, count), "draw indexed mesh", error)) return false;
		++draws;
		return true;
	};
	// All filled objects establish the complete scene depth before any overlay.
	// Wireframe is deliberately edge-only; it does not silently introduce a fill.
	if(scene) for(const auto& object : scene->GetObjects()) {
		if(!object.visible || !object.mesh) continue;
		auto parameters = Parameters(view_projection, object.world, object.inverse_world,
		                             object.r, object.g, object.b, 1,
		                             state->options.lighting && !state->options.wireframe);
		if(!draw(meshes.find(object.mesh.get())->second,
		         state->options.wireframe ? wire_lines : triangles, state->options.wireframe, parameters)) return fail();
	}
	const Scene3DMatrix identity = Scene3DMatrix::Identity();
	auto flat = Parameters(view_projection, identity, identity, 1, 1, 1, 1, false);
	flat.normal[3] = -1; // Edge shader preserves grid/axis vertex colors only on this path.
	flat.normal[11] = 2e-7f;
	if(state->options.show_grid && !draw(grid, overlay_lines, false, flat)) return fail();
	if(state->options.show_axes && !draw(axes, overlay_lines, false, flat)) return fail();
	auto outline = [&](const Scene3DObject& object, Rgba8 color) {
		auto parameters = Parameters(view_projection, object.world, object.inverse_world,
		                             Linear(color.r), Linear(color.g), Linear(color.b), 1, false);
		// Bias is applied after clipping by the edge fragment shader. It does not
		// move near/far clip planes and does not modify the stored solid depth.
		parameters.normal[11] = 2e-7f;
		return draw(meshes.find(object.mesh.get())->second, overlay_lines, true, parameters);
	};
	if(!state->options.wireframe && state->options.solid_edges && scene)
		for(const auto& object : scene->GetObjects())
			if(object.visible && object.mesh && !outline(object, state->options.edge_color)) return fail();
	// Selection is an independent uniform-color edge overlay. Authored surface
	// and vertex colors remain unchanged in every display mode.
	if(state->options.show_selection && scene)
		for(const auto& object : scene->GetObjects())
			if(object.visible && object.selection_outline && object.mesh && selected.Find(object.id) >= 0 &&
			   !outline(object, state->options.selection_color)) return fail();
	if(!Result(device, device.EndRenderPass(command), "end mesh pass", error) ||
	   !Result(device, device.EndCommands(command), "end mesh commands", error)) return fail();
	if(!Result(device, device.Submit(command), "submit mesh commands", error)) {
		device.DiscardCommands(command); return false;
	}
	++state->stats.rendered_frames;
	state->stats.target_size = frame.size;
	state->stats.draws_last_frame = draws;
	state->stats.depth_active = true;
	state->error.Clear(); error.Clear();
	return true;
}

void ViewContent::Close(GpuDevice& device)
{
	for(auto& entry : meshes) ReleaseMesh(device, entry.second);
	meshes.clear(); cached_scene.reset();
	ReleaseMesh(device, grid); ReleaseMesh(device, axes);
	if(depth.IsValid()) device.DestroyTexture(depth);
	if(wire_lines.IsValid()) device.DestroyPipeline(wire_lines);
	if(overlay_lines.IsValid()) device.DestroyPipeline(overlay_lines);
	if(triangles.IsValid()) device.DestroyPipeline(triangles);
	if(fragment_shader.IsValid()) device.DestroyShader(fragment_shader);
	if(edge_shader.IsValid()) device.DestroyShader(edge_shader);
	if(vertex_shader.IsValid()) device.DestroyShader(vertex_shader);
	depth = GpuTextureId(); wire_lines = GpuPipelineId(); overlay_lines = GpuPipelineId(); triangles = GpuPipelineId();
	vertex_shader = GpuShaderId(); fragment_shader = GpuShaderId(); edge_shader = GpuShaderId();
	depth_size = Size(0, 0); target_format = GpuFormat::Unknown;
	decoration_revision = 0; selection_revision = 0; selected.Clear(); have_grid_layout = false;
	state->stats.resident_meshes = 0;
	state->stats.depth_active = false;
}

}

std::unique_ptr<GpuSurfaceContent> CreateUi3DViewContent(
	const std::shared_ptr<Ui3DViewState>& state)
{
	return std::unique_ptr<GpuSurfaceContent>(new ViewContent(state));
}
}
