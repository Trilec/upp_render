#include "Scene3D.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Upp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kPitchLimit = kPi * 0.5;
bool Finite(double x) { return std::isfinite(x); }
double Clamp(double x, double low, double high) { return std::max(low, std::min(high, x)); }
bool ColorValid(float r, float g, float b, float a)
{
	return Finite(r) && Finite(g) && Finite(b) && Finite(a) &&
	       r >= 0 && r <= 1 && g >= 0 && g <= 1 && b >= 0 && b <= 1 && a == 1;
}
bool Bounded(const Scene3DVec3& v, double limit = 1e9)
{
	return v.IsFinite() && std::abs(v.x) <= limit && std::abs(v.y) <= limit && std::abs(v.z) <= limit;
}
bool ValidSize(Size size) { return size.cx > 0 && size.cy > 0; }
bool AdvanceRevision(Scene3DCamera& camera)
{
	if(camera.revision == std::numeric_limits<uint64>::max()) return false;
	++camera.revision;
	return true;
}
void CameraSinCos(double angle, double& sine, double& cosine)
{
	// Preserve truly cardinal poses exactly rather than leaving sin(pi) residuals.
	// General user angles are not rounded to a nearby preset.
	double a=std::remainder(angle,2*kPi);
	if(a==0) { sine=0; cosine=1; }
	else if(a==kPi*0.5) { sine=1; cosine=0; }
	else if(a==-kPi*0.5) { sine=-1; cosine=0; }
	else if(a==kPi || a==-kPi) { sine=0; cosine=-1; }
	else { sine=std::sin(a); cosine=std::cos(a); }
}
bool ViewAngles(Scene3DViewDirection direction, double& yaw, double& pitch)
{
	yaw=0; pitch=0;
	switch(direction) {
	case Scene3DViewDirection::Front: return true;
	case Scene3DViewDirection::Back: yaw=kPi; return true;
	case Scene3DViewDirection::Right: yaw=kPi*0.5; return true;
	case Scene3DViewDirection::Left: yaw=-kPi*0.5; return true;
	case Scene3DViewDirection::Top: pitch=kPi*0.5; return true;
	case Scene3DViewDirection::Bottom: pitch=-kPi*0.5; return true;
	default: return false;
	}
}
bool ValidId(const String& id)
{
	if(id.IsEmpty() || id.GetCount() > 256) return false;
	for(int i = 0; i < id.GetCount(); ++i) if((byte)id[i] < 32 || (byte)id[i] == 127) return false;
	return true;
}
bool IntersectBounds(const Scene3DBounds& bounds, const Scene3DRay& ray, double maximum)
{
	if(bounds.empty) return false;
	double low = ray.min_distance, high = std::min(ray.max_distance, maximum);
	const double origins[] = {ray.origin.x, ray.origin.y, ray.origin.z};
	const double directions[] = {ray.direction.x, ray.direction.y, ray.direction.z};
	const double minima[] = {bounds.min.x, bounds.min.y, bounds.min.z};
	const double maxima[] = {bounds.max.x, bounds.max.y, bounds.max.z};
	for(int axis = 0; axis < 3; ++axis) {
		if(std::abs(directions[axis]) < 1e-15) {
			if(origins[axis] < minima[axis] || origins[axis] > maxima[axis]) return false;
			continue;
		}
		double a = (minima[axis] - origins[axis]) / directions[axis];
		double b = (maxima[axis] - origins[axis]) / directions[axis];
		if(a > b) std::swap(a, b);
		low = std::max(low, a); high = std::min(high, b);
		if(low > high) return false;
	}
	return true;
}
}

Scene3DVec3 Scene3DVec3::operator+(const Scene3DVec3& b) const { return {x + b.x, y + b.y, z + b.z}; }
Scene3DVec3 Scene3DVec3::operator-(const Scene3DVec3& b) const { return {x - b.x, y - b.y, z - b.z}; }
Scene3DVec3 Scene3DVec3::operator-() const { return {-x, -y, -z}; }
Scene3DVec3 Scene3DVec3::operator*(double s) const { return {x * s, y * s, z * s}; }
Scene3DVec3 Scene3DVec3::operator/(double s) const { return {x / s, y / s, z / s}; }
double Scene3DVec3::Dot(const Scene3DVec3& b) const { return x * b.x + y * b.y + z * b.z; }
Scene3DVec3 Scene3DVec3::Cross(const Scene3DVec3& b) const { return {y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x}; }
double Scene3DVec3::Length() const { return std::hypot(x, std::hypot(y, z)); }
Scene3DVec3 Scene3DVec3::Normalized() const
{
	double length = Length();
	return length > 0 && Finite(length) ? *this / length : Scene3DVec3();
}
bool Scene3DVec3::IsFinite() const { return Finite(x) && Finite(y) && Finite(z); }

Scene3DMatrix::Scene3DMatrix()
{
	for(int i = 0; i < 16; ++i) m[i] = 0;
	m[0] = m[5] = m[10] = m[15] = 1;
}
Scene3DMatrix Scene3DMatrix::Identity() { return Scene3DMatrix(); }
Scene3DMatrix Scene3DMatrix::Translation(const Scene3DVec3& v)
{
	Scene3DMatrix out; out.m[12] = v.x; out.m[13] = v.y; out.m[14] = v.z; return out;
}
Scene3DMatrix Scene3DMatrix::Scale(const Scene3DVec3& v)
{
	Scene3DMatrix out; out.m[0] = v.x; out.m[5] = v.y; out.m[10] = v.z; return out;
}
Scene3DMatrix Scene3DMatrix::RotationY(double angle)
{
	Scene3DMatrix out; const double c = std::cos(angle), s = std::sin(angle);
	out.m[0] = c; out.m[8] = s; out.m[2] = -s; out.m[10] = c; return out;
}
Scene3DMatrix Scene3DMatrix::operator*(const Scene3DMatrix& b) const
{
	Scene3DMatrix out;
	for(int col = 0; col < 4; ++col) for(int row = 0; row < 4; ++row) {
		double value = 0;
		for(int k = 0; k < 4; ++k) value += m[k * 4 + row] * b.m[col * 4 + k];
		out.m[col * 4 + row] = value;
	}
	return out;
}
Scene3DVec3 Scene3DMatrix::TransformPoint(const Scene3DVec3& p) const
{
	return {m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12],
	        m[1]*p.x + m[5]*p.y + m[9]*p.z + m[13],
	        m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]};
}
Scene3DVec3 Scene3DMatrix::TransformVector(const Scene3DVec3& p) const
{
	return {m[0]*p.x + m[4]*p.y + m[8]*p.z,
	        m[1]*p.x + m[5]*p.y + m[9]*p.z,
	        m[2]*p.x + m[6]*p.y + m[10]*p.z};
}
bool Scene3DMatrix::IsValidAffine() const
{
	for(double value : m) if(!Finite(value) || std::abs(value) > 1e12) return false;
	return std::abs(m[3]) < 1e-12 && std::abs(m[7]) < 1e-12 &&
	       std::abs(m[11]) < 1e-12 && std::abs(m[15] - 1) < 1e-12;
}
bool Scene3DMatrix::InverseAffine(Scene3DMatrix& out) const
{
	if(!IsValidAffine()) return false;
	double a=m[0], b=m[4], c=m[8], d=m[1], e=m[5], f=m[9], g=m[2], h=m[6], i=m[10];
	double det = a*(e*i-f*h) - b*(d*i-f*g) + c*(d*h-e*g);
	double scale = Scene3DVec3(a,d,g).Length() * Scene3DVec3(b,e,h).Length() * Scene3DVec3(c,f,i).Length();
	// Reject reflections and nearly linearly dependent axes; positive nonuniform
	// scales are valid. A relative test avoids rejecting a uniformly small mesh.
	if(!Finite(det) || !Finite(scale) || scale <= 0 || det <= scale * 1e-12) return false;
	Scene3DMatrix candidate;
	candidate.m[0]=(e*i-f*h)/det; candidate.m[4]=(c*h-b*i)/det; candidate.m[8]=(b*f-c*e)/det;
	candidate.m[1]=(f*g-d*i)/det; candidate.m[5]=(a*i-c*g)/det; candidate.m[9]=(c*d-a*f)/det;
	candidate.m[2]=(d*h-e*g)/det; candidate.m[6]=(b*g-a*h)/det; candidate.m[10]=(a*e-b*d)/det;
	Scene3DVec3 translation = -candidate.TransformVector({m[12],m[13],m[14]});
	candidate.m[12]=translation.x; candidate.m[13]=translation.y; candidate.m[14]=translation.z;
	if(!candidate.IsValidAffine()) return false;
	out = candidate; return true;
}
Scene3DVec3 Scene3DMatrix::TransformNormal(const Scene3DVec3& n) const
{
	Scene3DMatrix inverse;
	if(!InverseAffine(inverse)) return {};
	return Scene3DVec3(inverse.m[0]*n.x + inverse.m[1]*n.y + inverse.m[2]*n.z,
	                   inverse.m[4]*n.x + inverse.m[5]*n.y + inverse.m[6]*n.z,
	                   inverse.m[8]*n.x + inverse.m[9]*n.y + inverse.m[10]*n.z).Normalized();
}

void Scene3DBounds::Add(const Scene3DVec3& p)
{
	if(!p.IsFinite()) return;
	if(empty) { min = max = p; empty = false; return; }
	min.x = std::min(min.x,p.x); min.y = std::min(min.y,p.y); min.z = std::min(min.z,p.z);
	max.x = std::max(max.x,p.x); max.y = std::max(max.y,p.y); max.z = std::max(max.z,p.z);
}
void Scene3DBounds::Add(const Scene3DBounds& b) { if(!b.empty) { Add(b.min); Add(b.max); } }
Scene3DVec3 Scene3DBounds::GetCenter() const { return empty ? Scene3DVec3() : (min + max) * 0.5; }
Scene3DVec3 Scene3DBounds::GetSize() const { return empty ? Scene3DVec3() : max - min; }

std::shared_ptr<const Scene3DMesh> Scene3DMesh::Create(const std::vector<Scene3DVertex>& input_vertices,
                                                   const std::vector<uint32>& input_indices, String& error)
{
	error.Clear();
	if(input_vertices.empty() || input_vertices.size() > 1000000 || input_indices.empty() ||
	   input_indices.size() > 3000000 || input_indices.size() % 3) {
		error = "Mesh needs 1..1000000 vertices and complete triangles (at most 3000000 indices)"; return {};
	}
	for(const auto& vertex : input_vertices) {
		if(!Bounded(vertex.position) || !Bounded(vertex.normal) || vertex.normal.Length() < 1e-12 ||
		   !Finite(vertex.u) || !Finite(vertex.v) || !ColorValid(vertex.r,vertex.g,vertex.b,vertex.a)) {
			error = "Mesh contains invalid position, normal, UV or opaque colour"; return {};
		}
	}
	for(uint32 index : input_indices) if(index >= input_vertices.size()) { error = "Mesh index outside vertex array"; return {}; }
	for(size_t i = 0; i < input_indices.size(); i += 3) {
		Scene3DVec3 a = input_vertices[input_indices[i+1]].position - input_vertices[input_indices[i]].position;
		Scene3DVec3 b = input_vertices[input_indices[i+2]].position - input_vertices[input_indices[i]].position;
		if(a.Cross(b).Length() <= a.Length() * b.Length() * 1e-12) { error = "Mesh has a degenerate triangle"; return {}; }
	}
	std::shared_ptr<Scene3DMesh> mesh(new Scene3DMesh);
	mesh->vertices = input_vertices; mesh->indices = input_indices;
	for(auto& vertex : mesh->vertices) { vertex.normal = vertex.normal.Normalized(); mesh->bounds.Add(vertex.position); }
	return mesh;
}

std::shared_ptr<const Scene3DMesh> Scene3DMesh::MakeBox(const Scene3DVec3& full_size, String& error)
{
	if(!Bounded(full_size) || full_size.x <= 0 || full_size.y <= 0 || full_size.z <= 0) {
		error = "Box size must be positive and finite"; return {};
	}
	const Scene3DVec3 h = full_size * 0.5;
	std::vector<Scene3DVertex> vertices;
	std::vector<uint32> indices;
	// Each face uses a tangent basis whose cross product is its outward normal.
	const Scene3DVec3 normals[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
	const Scene3DVec3 tangents[] = {{0,0,-1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{-1,0,0}};
	for(int face=0; face<6; ++face) {
		Scene3DVec3 n=normals[face], u=tangents[face], v=n.Cross(u);
		Scene3DVec3 center(n.x*h.x,n.y*h.y,n.z*h.z);
		u = {u.x*h.x,u.y*h.y,u.z*h.z}; v = {v.x*h.x,v.y*h.y,v.z*h.z};
		uint32 base = (uint32)vertices.size();
		const double signs[4][2] = {{-1,-1},{1,-1},{1,1},{-1,1}};
		for(int corner=0; corner<4; ++corner) {
			Scene3DVertex vertex; vertex.position=center+u*signs[corner][0]+v*signs[corner][1]; vertex.normal=n;
			vertex.u=(float)((signs[corner][0]+1)*0.5); vertex.v=(float)((signs[corner][1]+1)*0.5); vertices.push_back(vertex);
		}
		for(uint32 index : {0u,1u,2u,0u,2u,3u}) indices.push_back(base+index);
	}
	return Create(vertices, indices, error);
}

std::shared_ptr<const Scene3DSnapshot> Scene3DSnapshot::Create(const Scene3DSceneDesc& desc, String& error)
{
	error.Clear();
	if(!desc.revision || desc.objects.size() > 100000) { error = "Scene needs a nonzero revision and at most 100000 objects"; return {}; }
	std::shared_ptr<Scene3DSnapshot> scene(new Scene3DSnapshot);
	scene->revision = desc.revision; scene->objects.resize(desc.objects.size());
	const int count = (int)desc.objects.size();
	std::vector<int> parents(count, -1), queue;
	std::vector<std::vector<int>> children(count);
	size_t vertices = 0, indices = 0;
	for(int i=0; i<count; ++i) {
		const auto& source = desc.objects[i];
		Scene3DMatrix inverse;
		if(!ValidId(source.id) || scene->ids.Find(source.id) >= 0) { error = "Scene object IDs must be nonempty, unique and at most 256 bytes"; return {}; }
		if(!source.local.InverseAffine(inverse)) { error = "Scene transform must be finite, nonsingular affine with positive determinant"; return {}; }
		if(!ColorValid(source.r,source.g,source.b,source.a)) { error = "Scene material must be an opaque RGB colour in 0..1"; return {}; }
		if(source.mesh) {
			vertices += source.mesh->GetVertices().size(); indices += source.mesh->GetIndices().size();
			if(vertices > 2000000 || indices > 6000000) { error = "Scene exceeds the bounded instanced geometry budget"; return {}; }
		}
		scene->ids.Add(source.id);
		auto& object = scene->objects[i];
		object.id=source.id; object.parent_id=source.parent_id; object.mesh=source.mesh;
		object.selectable=source.selectable; object.r=source.r; object.g=source.g; object.b=source.b; object.a=source.a;
	}
	for(int i=0; i<count; ++i) {
		const String& parent = desc.objects[i].parent_id;
		if(parent.IsEmpty()) queue.push_back(i);
		else {
			int p = scene->ids.Find(parent);
			if(p < 0) { error = "Scene parent ID does not exist"; return {}; }
			parents[i]=p; children[p].push_back(i);
		}
	}
	// Topological traversal is iterative and O(nodes), including a very deep chain.
	for(size_t q=0; q<queue.size(); ++q) {
		int i=queue[q], parent=parents[i]; auto& object=scene->objects[i]; const auto& source=desc.objects[i];
		object.world=parent<0 ? source.local : scene->objects[parent].world*source.local;
		object.visible=source.visible && (parent<0 || scene->objects[parent].visible);
		if(!object.world.InverseAffine(object.inverse_world)) { error = "Composed scene transform exceeds the valid affine range"; return {}; }
		if(object.mesh) {
			const auto& b=object.mesh->GetBounds();
			for(int corner=0; corner<8; ++corner) {
				Scene3DVec3 point=object.world.TransformPoint({corner&1?b.max.x:b.min.x,corner&2?b.max.y:b.min.y,corner&4?b.max.z:b.min.z});
				if(!Bounded(point,1e12)) { error = "Scene world bounds exceed the finite coordinate range"; return {}; }
				object.bounds.Add(point);
			}
			if(object.visible) scene->bounds.Add(object.bounds);
		}
		for(int child : children[i]) queue.push_back(child);
	}
	if((int)queue.size()!=count) { error = "Scene hierarchy contains a cycle"; return {}; }
	return scene;
}

const Scene3DObject *Scene3DSnapshot::FindObject(const String& id) const
{
	int i=ids.Find(id); return i<0 ? nullptr : &objects[(size_t)i];
}

Scene3DPick Scene3DSnapshot::Pick(const Scene3DRay& ray, uint64 camera_revision) const
{
	Scene3DPick result; result.scene_revision=revision; result.camera_revision=camera_revision;
	if(!ray.origin.IsFinite() || !ray.direction.IsFinite() || std::abs(ray.direction.Length()-1)>1e-8 ||
	   !Finite(ray.min_distance) || !Finite(ray.max_distance) || ray.min_distance<0 || ray.max_distance<ray.min_distance) return result;
	double nearest=ray.max_distance;
	for(const auto& object : objects) {
		if(!object.visible || !object.selectable || !object.mesh || !IntersectBounds(object.bounds,ray,nearest)) continue;
		const auto& vertices=object.mesh->GetVertices(); const auto& indices=object.mesh->GetIndices();
		Scene3DVec3 origin=object.inverse_world.TransformPoint(ray.origin);
		Scene3DVec3 direction=object.inverse_world.TransformVector(ray.direction);
		for(size_t i=0; i<indices.size(); i+=3) {
			const auto& a=vertices[indices[i]]; const auto& b=vertices[indices[i+1]]; const auto& c=vertices[indices[i+2]];
			Scene3DVec3 e1=b.position-a.position, e2=c.position-a.position;
			Scene3DVec3 p=direction.Cross(e2); double determinant=e1.Dot(p);
			if(std::abs(determinant)<=e1.Length()*e2.Length()*direction.Length()*1e-12) continue;
			Scene3DVec3 delta=origin-a.position; double u=delta.Dot(p)/determinant;
			if(u<0 || u>1) continue;
			Scene3DVec3 q=delta.Cross(e1); double v=direction.Dot(q)/determinant;
			if(v<0 || u+v>1) continue;
			double distance=e2.Dot(q)/determinant;
			if(distance<ray.min_distance || distance>nearest || (result.hit && distance==nearest)) continue;
			Scene3DVec3 normal=a.normal*(1-u-v)+b.normal*u+c.normal*v;
			if(normal.Length()<1e-12) normal=e1.Cross(e2);
			// Inverse-transpose from the already prepared inverse avoids reinversion.
			const auto& inverse=object.inverse_world;
			result.world_normal=Scene3DVec3(inverse.m[0]*normal.x+inverse.m[1]*normal.y+inverse.m[2]*normal.z,
			                               inverse.m[4]*normal.x+inverse.m[5]*normal.y+inverse.m[6]*normal.z,
			                               inverse.m[8]*normal.x+inverse.m[9]*normal.y+inverse.m[10]*normal.z).Normalized();
			nearest=distance; result.hit=true; result.id=object.id; result.distance=distance;
			result.world_point=ray.origin+ray.direction*distance; result.triangle=(int)(i/3);
		}
	}
	return result;
}

bool Scene3DCamera::Validate(String& error) const
{
	error.Clear();
	if(!revision || !Bounded(target) || !Finite(yaw) || std::abs(yaw)>1e6 || !Finite(pitch) ||
	   std::abs(pitch)>kPitchLimit || !Finite(distance) || distance<1e-6 || distance>1e9 ||
	   !Finite(fov_y) || fov_y<0.01 || fov_y>kPi-0.01 || !Finite(ortho_height) || ortho_height<1e-6 || ortho_height>1e9 ||
	   !Finite(near_plane) || !Finite(far_plane) || near_plane<1e-6 || far_plane<=near_plane || far_plane>1e12) {
		error="Invalid camera: require finite position, pitch in [-pi/2,pi/2], positive scale and 0<near<far"; return false;
	}
	return true;
}
Scene3DVec3 Scene3DCamera::GetEye() const
{
	Scene3DVec3 right,up,forward; GetBasis(right,up,forward);
	return target-forward*distance;
}
void Scene3DCamera::GetBasis(Scene3DVec3& right, Scene3DVec3& up, Scene3DVec3& forward) const
{
	// The analytic yaw/pitch basis avoids a singular cross product with world-up
	// at +/-Y, and avoids loss of direction precision at a large translated target.
	double sy,cy,sp,cp; CameraSinCos(yaw,sy,cy); CameraSinCos(pitch,sp,cp);
	right={cy,0,-sy}; up={-sy*sp,cp,-cy*sp}; forward={-sy*cp,-sp,-cy*cp};
}
Scene3DMatrix Scene3DCamera::GetViewProjection(Size size) const
{
	String error; if(!ValidSize(size) || !Validate(error)) return {};
	Scene3DVec3 right,up,forward; GetBasis(right,up,forward); Scene3DVec3 eye=GetEye();
	Scene3DMatrix view;
	view.m[0]=right.x; view.m[4]=right.y; view.m[8]=right.z; view.m[12]=-right.Dot(eye);
	view.m[1]=up.x; view.m[5]=up.y; view.m[9]=up.z; view.m[13]=-up.Dot(eye);
	view.m[2]=-forward.x; view.m[6]=-forward.y; view.m[10]=-forward.z; view.m[14]=forward.Dot(eye);
	Scene3DMatrix projection; for(double& value:projection.m) value=0;
	double aspect=(double)size.cx/size.cy;
	if(orthographic) {
		projection.m[0]=2/(ortho_height*aspect); projection.m[5]=-2/ortho_height;
		projection.m[10]=1/(near_plane-far_plane); projection.m[14]=near_plane/(near_plane-far_plane); projection.m[15]=1;
	} else {
		double cotangent=1/std::tan(fov_y*0.5);
		projection.m[0]=cotangent/aspect; projection.m[5]=-cotangent;
		projection.m[10]=far_plane/(near_plane-far_plane); projection.m[14]=far_plane*near_plane/(near_plane-far_plane); projection.m[11]=-1;
	}
	return projection*view;
}
bool Scene3DCamera::Project(const Scene3DVec3& world, Size size, Pointf& screen, double& depth) const
{
	screen=Pointf(0,0); depth=0; String error;
	if(!world.IsFinite() || !ValidSize(size) || !Validate(error)) return false;
	Scene3DVec3 right,up,forward; GetBasis(right,up,forward); Scene3DVec3 delta=world-GetEye();
	double z=delta.Dot(forward); if(z<near_plane || z>far_plane) return false;
	double aspect=(double)size.cx/size.cy;
	double half_height=orthographic?ortho_height*0.5:z*std::tan(fov_y*0.5);
	screen.x=(1+delta.Dot(right)/(half_height*aspect))*size.cx*0.5;
	screen.y=(1-delta.Dot(up)/half_height)*size.cy*0.5;
	depth=orthographic?(z-near_plane)/(far_plane-near_plane):far_plane*(z-near_plane)/(z*(far_plane-near_plane));
	return Finite(screen.x) && Finite(screen.y) && Finite(depth);
}
bool Scene3DCamera::WorldRay(Pointf screen, Size size, Scene3DRay& ray) const
{
	ray=Scene3DRay(); String error;
	if(!Finite(screen.x) || !Finite(screen.y) || !ValidSize(size) || !Validate(error)) return false;
	Scene3DVec3 right,up,forward; GetBasis(right,up,forward);
	double x=2*screen.x/size.cx-1, y=1-2*screen.y/size.cy, aspect=(double)size.cx/size.cy;
	ray.origin=GetEye();
	if(orthographic) {
		ray.origin=ray.origin+right*(x*ortho_height*0.5*aspect)+up*(y*ortho_height*0.5); ray.direction=forward;
		ray.min_distance=near_plane; ray.max_distance=far_plane;
	} else {
		double tangent=std::tan(fov_y*0.5); ray.direction=(forward+right*(x*tangent*aspect)+up*(y*tangent)).Normalized();
		double cosine=ray.direction.Dot(forward); if(cosine<=0 || !Finite(cosine)) return false;
		ray.min_distance=near_plane/cosine; ray.max_distance=far_plane/cosine;
	}
	return ray.origin.IsFinite() && ray.direction.IsFinite() && Finite(ray.max_distance);
}
bool Scene3DCamera::Orbit(double yaw_delta, double pitch_delta)
{
	String error; if(!Validate(error) || !Finite(yaw_delta) || !Finite(pitch_delta)) return false;
	if(yaw_delta==0 && pitch_delta==0) return true;
	Scene3DCamera next=*this;
	next.yaw=std::remainder(yaw+yaw_delta,2*kPi); next.pitch=Clamp(pitch+pitch_delta,-kPitchLimit,kPitchLimit);
	if(next.yaw==yaw && next.pitch==pitch) return true;
	if(!AdvanceRevision(next) || !next.Validate(error)) return false;
	*this=next;
	return true;
}
bool Scene3DCamera::Pan(double dx, double dy, Size size)
{
	String error; if(!Validate(error) || !ValidSize(size) || !Finite(dx) || !Finite(dy)) return false;
	if(dx==0 && dy==0) return true;
	Scene3DVec3 right,up,forward; GetBasis(right,up,forward);
	double scale=(orthographic?ortho_height:2*distance*std::tan(fov_y*0.5))/size.cy;
	Scene3DCamera next=*this; next.target=target-right*(dx*scale)+up*(dy*scale);
	if(next.target.x==target.x && next.target.y==target.y && next.target.z==target.z) return true;
	if(!AdvanceRevision(next) || !next.Validate(error)) return false;
	*this=next;
	return true;
}
bool Scene3DCamera::AlignTo(Scene3DViewDirection direction)
{
	String error; if(!Validate(error)) return false;
	Scene3DCamera next=*this;
	if(!ViewAngles(direction,next.yaw,next.pitch)) return false;
	if(next.yaw==yaw && next.pitch==pitch) return true;
	if(!AdvanceRevision(next)) return false;
	*this=next;
	return true;
}
bool Scene3DCamera::SetOrthographic(bool on)
{
	String error; if(!Validate(error)) return false;
	if(orthographic==on) return true;
	Scene3DCamera next=*this;
	double span_per_distance=2*std::tan(fov_y*0.5);
	if(on) next.ortho_height=distance*span_per_distance;
	else next.distance=ortho_height/span_per_distance;
	next.orthographic=on;
	if(!next.Validate(error) || !AdvanceRevision(next)) return false;
	*this=next;
	return true;
}
Scene3DViewDirection Scene3DCamera::GetViewDirection(double tolerance_radians) const
{
	String error;
	if(!Validate(error) || !Finite(tolerance_radians) || tolerance_radians<0 || tolerance_radians>=kPi*0.25)
		return Scene3DViewDirection::User;
	Scene3DVec3 right,up,forward; GetBasis(right,up,forward);
	// Unit-vector chord length gives angular tolerance without acos cancellation
	// near exact alignment. Full-basis comparison distinguishes rolled pole views.
	double chord=2*std::sin(tolerance_radians*0.5);
	const Scene3DViewDirection directions[]={Scene3DViewDirection::Front,Scene3DViewDirection::Back,
		Scene3DViewDirection::Right,Scene3DViewDirection::Left,Scene3DViewDirection::Top,Scene3DViewDirection::Bottom};
	for(Scene3DViewDirection direction:directions) {
		Scene3DCamera aligned; ViewAngles(direction,aligned.yaw,aligned.pitch);
		Scene3DVec3 axis_right,axis_up,axis_forward; aligned.GetBasis(axis_right,axis_up,axis_forward);
		if((right-axis_right).Length()<=chord && (up-axis_up).Length()<=chord && (forward-axis_forward).Length()<=chord)
			return direction;
	}
	return Scene3DViewDirection::User;
}
bool Scene3DCamera::Dolly(double logarithmic_delta)
{
	String error; if(!Validate(error) || !Finite(logarithmic_delta)) return false;
	double factor=std::exp(-Clamp(logarithmic_delta,-50,50)); Scene3DCamera next=*this;
	if(orthographic) next.ortho_height=Clamp(ortho_height*factor,1e-6,1e9);
	else next.distance=Clamp(distance*factor,1e-6,1e9);
	if(next.distance==distance && next.ortho_height==ortho_height) return true;
	if(!AdvanceRevision(next)) return false;
	*this=next;
	return true;
}
bool Scene3DCamera::Fit(const Scene3DBounds& bounds, Size size)
{
	String error;
	if(!Validate(error) || !ValidSize(size) || bounds.empty || !Bounded(bounds.min) || !Bounded(bounds.max) ||
	   bounds.max.x<bounds.min.x || bounds.max.y<bounds.min.y || bounds.max.z<bounds.min.z) return false;
	double radius=std::max(bounds.GetSize().Length()*0.5,1e-5), aspect=(double)size.cx/size.cy;
	Scene3DCamera next=*this; next.target=bounds.GetCenter();
	double half_angle=std::atan(std::tan(fov_y*0.5)*std::min(1.0,aspect));
	next.distance=radius/std::sin(half_angle)*1.1;
	next.ortho_height=2.2*radius/std::min(1.0,aspect);
	// Expand only: a snug fit-specific clip interval would make ordinary wheel
	// navigation immediately cut through or disappear beyond the fitted bounds.
	next.near_plane=std::min(near_plane,std::max(1e-6,next.distance-radius*1.5));
	next.far_plane=std::max(far_plane,next.distance+radius*1.5);
	if(!next.Validate(error)) return false;
	if(next.target.x==target.x && next.target.y==target.y && next.target.z==target.z && next.distance==distance &&
	   next.ortho_height==ortho_height && next.near_plane==near_plane && next.far_plane==far_plane) return true;
	if(!AdvanceRevision(next)) return false;
	*this=next;
	return true;
}

}
