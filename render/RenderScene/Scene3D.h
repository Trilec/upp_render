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
	                                              const std::vector<uint32>& indices, String& error);
	static std::shared_ptr<const Scene3DMesh> MakeBox(const Scene3DVec3& full_size, String& error);
	const std::vector<Scene3DVertex>& GetVertices() const { return vertices; }
	const std::vector<uint32>& GetIndices() const { return indices; }
	const Scene3DBounds& GetBounds() const { return bounds; }
private:
	Scene3DMesh() = default;
	std::vector<Scene3DVertex> vertices;
	std::vector<uint32> indices;
	Scene3DBounds bounds;
};

struct Scene3DObjectDesc {
	String id, parent_id;
	Scene3DMatrix local;
	std::shared_ptr<const Scene3DMesh> mesh;
	bool visible = true, selectable = true;
	float r = 1, g = 1, b = 1, a = 1;
	// Hosts can supply joint/element highlighting while retaining object selection and framing.
	bool selection_outline = true;
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
	float r = 1, g = 1, b = 1, a = 1;
	// Hosts can supply joint/element highlighting while retaining object selection and framing.
	bool selection_outline = true;
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
};

// Validates the complete candidate before publication, preserving descriptor order.
// Stable IDs are opaque byte strings, unique within this snapshot; use app prefixes
// (e.g. actor:123). Visibility inherits through parents, selection does not.
// Bounds/positive-determinant affine transforms are prepared once. No mutable scene
// data is exposed. Baseline: opaque RGB, 100000 nodes, at most 2M instanced vertices
// and 6M instanced indices. No importer, animation clock, document or undo authority.
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
