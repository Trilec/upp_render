#ifndef _RenderScene_Scene3D_h_
#define _RenderScene_Scene3D_h_

#include <Core/Core.h>
#include <memory>
#include <vector>

namespace Upp {

// Apache-2.0, upp_render. Neutral immutable scene values: no GUI or GPU ownership.
// Metres; right-handed XYZ, Y up, actor forward +Z. Matrices are column-major,
// multiply column vectors; world = parent * local. Camera yaw0 looks towards -Z.
// Angles are radians. Projection has Vulkan depth 0..1 and upper-left screen origin.
struct Scene3DVec3 {
	double x = 0, y = 0, z = 0;
	Scene3DVec3() = default;
	Scene3DVec3(double x, double y, double z) : x(x), y(y), z(z) {}
	Scene3DVec3 operator+(const Scene3DVec3& b) const;
	Scene3DVec3 operator-(const Scene3DVec3& b) const;
	Scene3DVec3 operator-() const;
	Scene3DVec3 operator*(double s) const;
	Scene3DVec3 operator/(double s) const;
	double Dot(const Scene3DVec3& b) const;
	Scene3DVec3 Cross(const Scene3DVec3& b) const;
	double Length() const;
	Scene3DVec3 Normalized() const;
	bool IsFinite() const;
};

inline Scene3DVec3 operator*(double s, const Scene3DVec3& v) { return v * s; }

struct Scene3DMatrix {
	double m[16];
	Scene3DMatrix();
	static Scene3DMatrix Identity();
	static Scene3DMatrix Translation(const Scene3DVec3& v);
	static Scene3DMatrix Scale(const Scene3DVec3& v);
	static Scene3DMatrix RotationY(double radians);
	Scene3DMatrix operator*(const Scene3DMatrix& b) const;
	Scene3DVec3 TransformPoint(const Scene3DVec3& p) const;
	Scene3DVec3 TransformVector(const Scene3DVec3& p) const;
	Scene3DVec3 TransformNormal(const Scene3DVec3& n) const;
	bool InverseAffine(Scene3DMatrix& out) const;
	bool IsValidAffine() const;
};

struct Scene3DBounds {
	Scene3DVec3 min, max;
	bool empty = true;
	void Add(const Scene3DVec3& p);
	void Add(const Scene3DBounds& b);
	Scene3DVec3 GetCenter() const;
	Scene3DVec3 GetSize() const;
};

struct Scene3DVertex {
	Scene3DVec3 position;
	Scene3DVec3 normal = Scene3DVec3(0, 1, 0);
	float u = 0, v = 0;
	float r = 1, g = 1, b = 1, a = 1;
};

// Factory copies and validates input. The resulting arrays have no mutable alias.
// std::shared_ptr<const> deliberately defines the cross-thread resource lifetime;
// publication to a UI control itself remains the UI thread's responsibility.
class Scene3DMesh {
public:
	static std::shared_ptr<const Scene3DMesh> Create(const std::vector<Scene3DVertex>& vertices,
	                                              const std::vector<uint32>& indices, String& error,
	                                              const std::vector<uint32>& viewport_outline = {});
	static std::shared_ptr<const Scene3DMesh> MakeBox(const Scene3DVec3& full_size, String& error);
	const std::vector<Scene3DVertex>& GetVertices() const { return vertices; }
	const std::vector<uint32>& GetIndices() const { return indices; }
	// Optional editor-only line pairs. The triangles still define the pick surface.
	const std::vector<uint32>& GetViewportOutline() const { return viewport_outline; }
	const Scene3DBounds& GetBounds() const { return bounds; }
private:
	Scene3DMesh() = default;
	std::vector<Scene3DVertex> vertices;
	std::vector<uint32> indices, viewport_outline;
	Scene3DBounds bounds;
};


// Object-local Gaussian density. Covariance is symmetric xx,xy,xz,yy,yz,zz
// in squared scene units; linear RGB and straight opacity are finite in 0..1.
// Zero/planar positive-semidefinite densities remain valid; rasterization adds
// a documented quarter-pixel variance without changing these authored values.
struct Scene3DSplat {
    Scene3DVec3 position;
    double covariance[6] = {1, 0, 0, 1, 0, 1};
    float r = 1, g = 1, b = 1, a = 1;
};
// Aggregate submitted working-preview points, including repeated instances.
// Large original files belong to the host's streaming source/preview boundary.
constexpr int Scene3DMaxSplats = 1000000;
constexpr double Scene3DSplatAlphaCutoff = 1.0 / 255.0;
constexpr double Scene3DSplatPixelVariance = 0.25;

// Immutable source values; input is copied so no mutable array alias survives.
// Bounds conservatively enclose the density's 1/255 coverage contour.
class Scene3DSplats {
public:
    static std::shared_ptr<const Scene3DSplats> Create(
        const std::vector<Scene3DSplat>& splats, String& error);
    const std::vector<Scene3DSplat>& GetSplats() const { return splats; }
    const Scene3DBounds& GetBounds() const { return bounds; }
private:
    Scene3DSplats() = default;
    std::vector<Scene3DSplat> splats;
    Scene3DBounds bounds;
};

// Same screen covariance and Gaussian coverage used by the native renderer.
// Projection includes the supplied complete local-to-clip matrix, so parent
// transforms, camera roll and physical gate share one coordinate contract.
struct Scene3DSplatProjection {
    Pointf center;
    double covariance_xx = 0, covariance_xy = 0, covariance_yy = 0;
    double depth = 0; // Vulkan normalized depth, 0..1.
    double opacity_scale = 1; // Perspective fade for oversized near-view footprints.
    double Coverage(Pointf point) const; // Truncated Gaussian coverage including the viewport fade.
};
bool Scene3DProjectSplat(const Scene3DSplat&, const Scene3DMatrix& clip_from_local,
                         Size viewport, Scene3DSplatProjection& output);

enum class Scene3DTextureColorSpace { Srgb, Linear };

// Immutable, top-down, straight RGBA8 pixels. UV coordinates are V-up.
// The factory copies its input; rendering never decodes files or retains an
// application-owned mutable image buffer.
class Scene3DTexture {
public:
	static std::shared_ptr<const Scene3DTexture> Create(Size size,
	    const std::vector<byte>& straight_rgba, Scene3DTextureColorSpace color_space, String& error);
	Size GetSize() const { return size; }
	const std::vector<byte>& GetPixels() const { return pixels; }
	Scene3DTextureColorSpace GetColorSpace() const { return color_space; }
	// Matches a linear GPU sampler with clamp-to-edge or repeat addressing.
	double SampleAlpha(double u, double v, bool repeat_uv) const;
private:
	Scene3DTexture() = default;
	Size size = Size(0, 0);
	std::vector<byte> pixels;
	Scene3DTextureColorSpace color_space = Scene3DTextureColorSpace::Srgb;
};

enum class Scene3DAlphaMode { Opaque, Mask, Blend };

struct Scene3DMaterial {
	std::shared_ptr<const Scene3DTexture> color_texture;
	Scene3DAlphaMode alpha_mode = Scene3DAlphaMode::Opaque;
	float alpha_cutoff = 0.5f;
	bool unlit = false, repeat_uv = false;
	bool double_sided = true; // Preserve the original viewport's unculled surfaces.
	bool Validate(String& error) const;
};

struct Scene3DObjectDesc {
	String id, parent_id;
	Scene3DMatrix local;
	std::shared_ptr<const Scene3DMesh> mesh;
	bool visible = true, selectable = true;
	float r = 1, g = 1, b = 1, a = 1; // Linear material/base-color factor.
	// Hosts can supply joint/element highlighting while retaining object selection and framing.
	bool selection_outline = true;
	// Appended fields preserve earlier positional aggregate initializers.
	float opacity = 1; // Independent object fade, after resolving material coverage.
	Scene3DMaterial material; // Mesh-only material; Gaussian coverage is intrinsic.
	std::shared_ptr<const Scene3DSplats> splats; // May not coexist with mesh.
};

struct Scene3DSceneDesc {
	uint64 revision = 1;
	std::vector<Scene3DObjectDesc> objects;
};

struct Scene3DObject {
	String id, parent_id;
	Scene3DMatrix world, inverse_world;
	std::shared_ptr<const Scene3DMesh> mesh;
	Scene3DBounds bounds;
	bool visible = true, selectable = true;
	float r = 1, g = 1, b = 1, a = 1; // Linear material/base-color factor.
	// Hosts can supply joint/element highlighting while retaining object selection and framing.
	bool selection_outline = true;
	// Appended fields preserve earlier positional aggregate initializers.
	float opacity = 1; // Independent object fade, after resolving material coverage.
	Scene3DMaterial material; // Mesh-only material; Gaussian coverage is intrinsic.
	std::shared_ptr<const Scene3DSplats> splats; // May not coexist with mesh.
};

struct Scene3DRay {
	Scene3DVec3 origin, direction = Scene3DVec3(0, 0, -1);
	double min_distance = 0, max_distance = 1e12;
};

struct Scene3DPick {
	bool hit = false;
	String id;
	Scene3DVec3 world_point, world_normal;
	double distance = 0;
	uint64 scene_revision = 0, camera_revision = 0;
	int triangle = -1;
	int splat = -1; // Resource point index for a screen-space Gaussian hit.
};

// Bounded screen-patch evidence, not a reconstruction accuracy estimate.
// Counts describe probe rays; splat_count counts distinct source Gaussians.
// Spreads are RMS about the retained mean (pixels/world units); depth_spread
// is the retained camera-depth range. No geometric normal is inferred.
struct Scene3DSplatSurfaceSupport {
	int sample_count = 0, hit_count = 0, support_count = 0, splat_count = 0;
	double radius_pixels = 0, screen_spread_pixels = 0, world_spread = 0, depth_spread = 0;
	// Retained probe fraction times min(1, splat_count / 4); heuristic 0..1.
	double confidence = 0;
	bool normal_valid = false;
	Scene3DVec3 world_normal;
};

struct Scene3DSplatSurfaceSample {
	Scene3DPick pick;
	Scene3DSplatSurfaceSupport support;
};

struct Scene3DCamera;

// Validates the complete candidate before publication, preserving descriptor order.
// Stable IDs are opaque byte strings, unique within this snapshot; use app prefixes
// (e.g. actor:123). Visibility inherits through parents, selection does not.
// Bounds/positive-determinant affine transforms are prepared once. No mutable scene
// data is exposed. Linear RGBA factors; opaque, masked or blended surfaces.
// At most 100000 nodes, 2M instanced vertices, 6M instanced indices and 512 MiB
// of distinct immutable texture pixels and Scene3DMaxSplats instanced Gaussian points. No importer, animation clock, document
// or undo authority.
class Scene3DSnapshot {
public:
	static std::shared_ptr<const Scene3DSnapshot> Create(const Scene3DSceneDesc& desc, String& error);
	uint64 GetRevision() const { return revision; }
	const std::vector<Scene3DObject>& GetObjects() const { return objects; }
	const Scene3DBounds& GetBounds() const { return bounds; }
	const Scene3DObject *FindObject(const String& id) const;
	// Bounded CPU bounds/triangle pick, nearest visible/selectable hit. Ray direction
	// must be unit length; camera-generated rays carry the near/far clipping interval.
	// Miss always returns hit=false and current revisions. Equal-distance ties keep
	// descriptor order. No authoritative application selection is changed.
	Scene3DPick Pick(const Scene3DRay& ray, uint64 camera_revision) const;
	// Exact projected Gaussian coverage plus the existing mesh ray test. The
	// camera provides near/far clips, roll and gate; misses retain revisions.
	Scene3DPick PickScreen(Pointf screen, Size viewport, const Scene3DCamera&) const;
	// Samples only this visible/selectable Gaussian object; never falls back to
	// another object or mesh. Radius 0 is the nearest covered target hit. A
	// finite positive radius is capped at 128 pixels and uses at most 25 probes
	// in its circle, projecting each target Gaussian once. Separated depth
	// groups are rejected; the dominant central group returns an actual hit
	// (weighted medoid), not an average between surfaces. Its point may be
	// offset from screen within the circle. pick.splat identifies that probe's
	// Gaussian; pick.world_normal is camera-facing, not a surface normal.
	// Invalid inputs/target or no coverage return a miss with current revisions.
	// This ignores other-object occlusion and UI film mattes by design. Use on
	// demand/gesture commit; the target scan is linear in its Gaussian count.
	Scene3DSplatSurfaceSample SampleSplatSurface(Pointf screen, double radius_pixels,
	                                            Size viewport, const Scene3DCamera&,
	                                            const String& object_id) const;
private:
	Scene3DSnapshot() = default;
	uint64 revision = 0;
	std::vector<Scene3DObject> objects;
	Index<String> ids;
	Scene3DBounds bounds;
};

// Direction is independent of projection. User means a pose that does not match
// one of the canonical axis bases; it is not an assignable orientation preset.
enum class Scene3DViewDirection { User, Front, Back, Right, Left, Top, Bottom };

struct Scene3DCamera {
	uint64 revision = 1;
	Scene3DVec3 target;
	double yaw = 0.65, pitch = 0.40, distance = 8.0;
	bool orthographic = false;
	double fov_y = 0.7853981633974483, ortho_height = 8.0;
	double near_plane = 0.01, far_plane = 10000.0;
	// Roll is right-handed about the viewing direction. A zero gate aspect uses
	// the whole viewport; a positive aspect fits a centered physical film gate.
	// fov_y/ortho_height describe that gate, while the viewport may show overscan.
	double roll = 0, gate_aspect = 0;
	Rect GetGateRect(Size size) const;
	// Sets eye/aim/up atomically, including exact pole poses. Parallel/zero up
	// and exhausted revisions reject without changing the last valid camera.
	bool SetLookAt(const Scene3DVec3& eye, const Scene3DVec3& aim,
	               const Scene3DVec3& up, String& error);
	bool Validate(String& error) const;
	Scene3DVec3 GetEye() const;
	// Defined at exact poles. At Top screen-up is -Z; at Bottom it is +Z when
	// aligned by AlignTo. At other pole yaw values the basis retains that roll.
	void GetBasis(Scene3DVec3& right, Scene3DVec3& up, Scene3DVec3& forward) const;
	Scene3DMatrix GetViewProjection(Size size) const;
	// False for invalid dimensions/state or outside the near/far clip volume;
	// an otherwise valid point may project beyond the screen edges.
	bool Project(const Scene3DVec3& world, Size size, Pointf& screen, double& depth) const;
	bool WorldRay(Pointf screen, Size size, Scene3DRay& ray) const;
	// Mutators return success (including an unchanged valid state), reject non-finite
	// input, and increment revision only for a changed valid state. Pan uses screen pixels;
	// positive Dolly logarithmic_delta moves closer (one unit multiplies by exp(-1)).
	bool Orbit(double yaw_delta, double pitch_delta);
	bool Pan(double dx, double dy, Size size);
	// Align pose only; projection, target, framing values and clips stay unchanged.
	// User/invalid direction, invalid current camera or exhausted revision rejects
	// without mutation. Exact Front/Back/Right/Left/Top/Bottom is a successful no-op.
	bool AlignTo(Scene3DViewDirection direction);
	// Changes projection while preserving vertical framing at the target plane:
	// ortho_height = 2 * distance * tan(fov_y / 2). Switching back solves for distance.
	// Target, orientation, FOV and clips stay unchanged. Unrepresentable framing
	// rejects without mutation rather than clamping or silently changing the lens.
	bool SetOrthographic(bool on);
	// Recognizes the complete canonical orientation, including screen-up at poles.
	// Finite angular tolerance is in radians, 0 <= tolerance < pi/4. An invalid
	// camera/tolerance or a rotated pole basis returns User; this never mutates.
	Scene3DViewDirection GetViewDirection(double tolerance_radians = 1e-6) const;
	// Dolly preserves authored clip planes; deliberately tight clipping remains
	// application-controlled. It does not silently widen clips or constrain the pivot.
	bool Dolly(double logarithmic_delta);
	// Fit expands the existing near/far interval only when needed for these bounds;
	// it never tightens a usable interval. Default clips remain useful for navigation.
	bool Fit(const Scene3DBounds& bounds, Size size);
};

}

#endif
