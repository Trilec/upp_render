#pragma once

#include <RenderCanvas/RenderCanvas.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

struct UiRenderer2DTarget : Moveable<UiRenderer2DTarget> {
	GpuTextureId color_target;
	Size size = Size(0, 0);
	GpuFormat color_format = GpuFormat::Unknown;
	GpuLoadOp load_op = GpuLoadOp::Clear;
	GpuStoreOp store_op = GpuStoreOp::Store;
	GpuClearColor clear_color;
};

struct UiRenderer2DStats : Moveable<UiRenderer2DStats> {
	int display_op_count = 0;
	int primitive_count = 0;
	int emitted_primitive_count = 0;
	int clipped_primitive_count = 0;
	int triangle_count = 0;
	int vertex_count = 0;
	int translucent_vertex_count = 0;
	int draw_count = 0;
	int batch_count = 0;
	int64 uploaded_bytes = 0;
	int64 vertex_buffer_capacity = 0;
	bool vertex_buffer_grew = false;
};

// Backend-neutral solid-colour 2D renderer. The GpuDevice must outlive this object.
class UiRenderer2D {
public:
	explicit UiRenderer2D(GpuDevice& device);
	~UiRenderer2D();

	UiRenderer2D(const UiRenderer2D&) = delete;
	UiRenderer2D& operator=(const UiRenderer2D&) = delete;

	bool Render(const UiDisplayList& list, const UiRenderer2DTarget& target);
	bool RenderFrame(const UiDisplayList& list, const GpuFrameInfo& frame,
	                 const GpuClearColor& clear_color = GpuClearColor());
	void Close();

	bool IsReady() const { return ready; }
	const String& GetError() const { return error; }
	const UiRenderer2DStats& GetStats() const { return stats; }

private:
	struct Vertex : Moveable<Vertex> {
		float x = 0;
		float y = 0;
		float r = 0;
		float g = 0;
		float b = 0;
		float a = 1;
	};

	struct PipelineEntry : Moveable<PipelineEntry> {
		GpuFormat format = GpuFormat::Unknown;
		GpuPipelineId pipeline;
	};

	struct ReplayState : Moveable<ReplayState> {
		Transform2D transform;
		bool has_clip = false;
		Rectf clip = Rectf(0, 0, 0, 0);
	};

	GpuDevice *device = nullptr;
	bool ready = false;
	String error;
	GpuShaderId vertex_shader;
	GpuShaderId fragment_shader;
	GpuBufferId vertex_buffer;
	int64 vertex_buffer_capacity = 0;
	Vector<PipelineEntry> pipelines;
	Vector<Vertex> vertices;
	UiRenderer2DStats stats;

	bool EnsureShaders();
	bool EnsurePipeline(GpuFormat format, GpuPipelineId& out);
	bool EnsureVertexBuffer(int64 required_bytes);
	bool BuildGeometry(const UiDisplayList& list, Size target_size);
	bool Submit(const UiRenderer2DTarget& target, GpuPipelineId pipeline);
	bool Fail(const String& message);
};

}
