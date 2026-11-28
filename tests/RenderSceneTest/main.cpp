#include <RenderScene/Scene3D.h>
#include <RenderScene/Scene3DGaussianRecord.h>
#include <cmath>
#include <limits>

using namespace Upp;

namespace {
int checks = 0, failures = 0;
void Check(bool condition, const char *message)
{
	++checks;
	if(!condition) { ++failures; Cout() << "FAIL: " << message << EOL; }
}
bool Near(double a, double b, double epsilon = 1e-9) { return std::abs(a-b) <= epsilon; }
bool Near(const Scene3DVec3& a, const Scene3DVec3& b, double epsilon = 1e-9)
{
	return Near(a.x,b.x,epsilon) && Near(a.y,b.y,epsilon) && Near(a.z,b.z,epsilon);
}
bool SameCamera(const Scene3DCamera& a, const Scene3DCamera& b)
{
	return a.revision==b.revision && Near(a.target,b.target,0) && a.yaw==b.yaw && a.pitch==b.pitch &&
	       a.distance==b.distance && a.orthographic==b.orthographic && a.fov_y==b.fov_y &&
	       a.ortho_height==b.ortho_height && a.near_plane==b.near_plane && a.far_plane==b.far_plane &&
	       a.roll==b.roll && a.gate_aspect==b.gate_aspect;
}
Scene3DVertex Vertex(Scene3DVec3 p, Scene3DVec3 normal = {0,0,1})
{
	Scene3DVertex out; out.position=p; out.normal=normal; return out;
}
Scene3DCamera Camera()
{
	Scene3DCamera camera; camera.target={0,0,0}; camera.yaw=0; camera.pitch=0; camera.distance=10;
	camera.fov_y=3.14159265358979323846/2; camera.near_plane=0.1; camera.far_plane=100;
	return camera;
}
Scene3DObjectDesc Object(const char *id, std::shared_ptr<const Scene3DMesh> mesh, Scene3DVec3 translation = {})
{
	Scene3DObjectDesc object; object.id=id; object.mesh=mesh; object.local=Scene3DMatrix::Translation(translation); return object;
}
void MatrixTests()
{
	Scene3DMatrix identity;
	Check(Near(identity.TransformPoint({1,2,3}),{1,2,3}),"matrix defaults to identity");
	Scene3DMatrix matrix=Scene3DMatrix::Translation({4,5,6})*Scene3DMatrix::Scale({2,3,4});
	Check(Near(matrix.TransformPoint({1,2,3}),{6,11,18}),"column-vector order: scale then translation");
	Check(Near(matrix.TransformVector({1,2,3}),{2,6,12}),"vector transform omits translation");
	Scene3DMatrix inverse;
	Check(matrix.InverseAffine(inverse) && Near(inverse.TransformPoint({6,11,18}),{1,2,3}),"affine inverse expected values");
	Check(Near(matrix.TransformNormal({1,1,0}),Scene3DVec3(3,2,0).Normalized()),"inverse-transpose normal under nonuniform scale");
	Check(Near(Scene3DMatrix::RotationY(3.14159265358979323846/2).TransformPoint({1,0,0}),{0,0,-1}),"right-handed ninety-degree Y rotation");
	Check(!Scene3DMatrix::Scale({0,1,1}).InverseAffine(inverse),"singular transform rejected");
	Check(!Scene3DMatrix::Scale({-1,1,1}).InverseAffine(inverse),"negative determinant rejected for winding contract");
	Check(Scene3DMatrix::Scale({0.001,0.002,0.003}).InverseAffine(inverse),"small positive scales use relative determinant tolerance");
	matrix.m[3]=0.5; Check(!matrix.InverseAffine(inverse),"projective object transform rejected");
	matrix=Scene3DMatrix(); matrix.m[0]=std::numeric_limits<double>::infinity();
	Check(!matrix.InverseAffine(inverse),"infinite transform rejected");
	Scene3DBounds bounds; Check(bounds.empty,"bounds default empty"); bounds.Add({-2,3,1}); bounds.Add({4,-1,5});
	Check(Near(bounds.GetCenter(),{1,1,3}) && Near(bounds.GetSize(),{6,4,4}),"bounds accumulate points");
}
void MeshTests()
{
	String error;
	std::vector<Scene3DVertex> vertices={Vertex({0,0,0}),Vertex({1,0,0}),Vertex({0,1,0})};
	std::vector<uint32> indices={0,1,2};
	auto mesh=Scene3DMesh::Create(vertices,indices,error);
	Check(mesh && error.IsEmpty(),"valid triangle accepted");
	vertices[0].position={99,99,99}; indices[0]=999;
	Check(mesh && Near(mesh->GetVertices()[0].position,{0,0,0}) && mesh->GetIndices()[0]==0,"mesh factory owns copies, no caller aliases");
	Check(!Scene3DMesh::Create(vertices,indices,error) && !error.IsEmpty(),"invalid vertex index rejected");
	indices={0,1}; Check(!Scene3DMesh::Create(vertices,indices,error),"incomplete triangle rejected");
	indices={0,1,2}; vertices[0].position={0,0,0}; vertices[2].position={2,0,0};
	Check(!Scene3DMesh::Create(vertices,indices,error),"degenerate triangle rejected");
	vertices[2].position={0,1,0}; vertices[1].normal={0,0,0};
	Check(!Scene3DMesh::Create(vertices,indices,error),"zero normal rejected");
	vertices[1].normal={0,0,1}; vertices[1].a=0.5f;
	Check(bool(Scene3DMesh::Create(vertices,indices,error)),"finite straight vertex alpha is retained");
	vertices[1].a=1; vertices[1].u=std::numeric_limits<float>::quiet_NaN();
	Check(!Scene3DMesh::Create(vertices,indices,error),"NaN UV rejected");
	auto box=Scene3DMesh::MakeBox({2,4,6},error);
	Check(box && box->GetVertices().size()==24 && box->GetIndices().size()==36,"box has distinct face normals and twelve triangles");
	Check(box && Near(box->GetBounds().min,{-1,-2,-3}) && Near(box->GetBounds().max,{1,2,3}),"box full-size convention");
	bool winding=true;
	if(box) for(size_t i=0;i<box->GetIndices().size();i+=3) {
		const auto& a=box->GetVertices()[box->GetIndices()[i]];
		const auto& b=box->GetVertices()[box->GetIndices()[i+1]];
		const auto& c=box->GetVertices()[box->GetIndices()[i+2]];
		winding &= (b.position-a.position).Cross(c.position-a.position).Dot(a.normal)>0;
	}
	Check(box && winding,"all box faces have outward counterclockwise winding");
	Check(!Scene3DMesh::MakeBox({0,1,1},error),"zero-size box rejected");
}
void SceneTests()
{
	String error; auto box=Scene3DMesh::MakeBox({2,2,2},error);
	Scene3DSceneDesc desc; desc.revision=17;
	desc.objects.push_back(Object("actor:child",box,{1,0,0})); desc.objects[0].parent_id="actor:parent";
	desc.objects.push_back(Object("actor:parent",{}, {10,0,0})); desc.objects[1].local=Scene3DMatrix::Translation({10,0,0})*Scene3DMatrix::Scale({2,3,4});
	auto scene=Scene3DSnapshot::Create(desc,error);
	Check(scene && scene->GetRevision()==17 && error.IsEmpty(),"child before parent accepted with stable revision");
	const auto *child=scene?scene->FindObject("actor:child"):nullptr;
	Check(child && Near(child->world.TransformPoint({0,0,0}),{12,0,0}),"hierarchy composes parent and local transforms");
	Check(child && Near(child->bounds.min,{10,-3,-4}) && Near(child->bounds.max,{14,3,4}),"world bounds include nonuniform scale");
	Check(scene && !scene->FindObject("actor:absent"),"unknown object lookup returns null");
	desc.objects[0].id="actor:changed"; desc.objects[1].visible=false;
	Check(scene && scene->FindObject("actor:child") && scene->FindObject("actor:child")->visible,"snapshot detached from later descriptor edits");
	auto hidden=Scene3DSnapshot::Create(desc,error);
	Check(hidden && !hidden->GetObjects()[0].visible && hidden->GetBounds().empty,"parent visibility inherited; hidden geometry excluded from fit bounds");
	desc.objects[1].id=desc.objects[0].id; Check(!Scene3DSnapshot::Create(desc,error),"duplicate IDs rejected");
	desc.objects[1].id="actor:parent"; desc.objects[0].parent_id="actor:missing";
	Check(!Scene3DSnapshot::Create(desc,error),"missing parent rejected");
	desc.objects[0].parent_id="actor:parent"; desc.objects[1].parent_id="actor:changed";
	Check(!Scene3DSnapshot::Create(desc,error),"hierarchy cycle rejected without recursion");
	desc.objects[1].parent_id.Clear(); desc.objects[0].id.Clear();
	Check(!Scene3DSnapshot::Create(desc,error),"empty stable ID rejected");
	desc.objects[0].id="actor:child"; desc.objects[0].local=Scene3DMatrix::Scale({0,1,1});
	Check(!Scene3DSnapshot::Create(desc,error),"singular local transform rejected before publication");
	desc.objects[0].local=Scene3DMatrix(); desc.objects[0].a=0.9f;
	Check(bool(Scene3DSnapshot::Create(desc,error)),"finite material alpha is independent of opaque coverage");
	desc.objects[0].a=1; desc.revision=0;
	Check(!Scene3DSnapshot::Create(desc,error),"zero scene revision rejected");
	desc=Scene3DSceneDesc();
	for(int i=0;i<12000;++i) {
		Scene3DObjectDesc object; object.id="node:"+AsString(i); if(i) object.parent_id="node:"+AsString(i-1);
		desc.objects.push_back(object);
	}
	auto deep=Scene3DSnapshot::Create(desc,error);
	Check(deep && deep->GetObjects().size()==12000,"deep hierarchy uses iterative bounded traversal");
}
void CameraTests()
{
	String error; auto camera=Camera(); Pointf screen; double depth=0;
	Check(camera.Validate(error),"canonical camera validates");
	Check(Near(camera.GetEye(),{0,0,10}),"yaw zero eye is on positive Z");
	Check(camera.Project({0,0,0},Size(800,600),screen,depth) && Near(screen.x,400) && Near(screen.y,300),"target projects to screen centre");
	Check(Near(depth,99.0/99.9),"perspective depth independently matches Vulkan zero-to-one formula");
	Check(camera.Project({0,1,0},Size(800,600),screen,depth) && Near(screen.y,270),"world up projects toward top of screen");
	Check(camera.Project({1,0,0},Size(800,600),screen,depth) && Near(screen.x,430),"perspective aspect and horizontal projection");
	Scene3DRay ray;
	Check(camera.WorldRay(Pointf(400,300),Size(800,600),ray) && Near(ray.origin,{0,0,10}) && Near(ray.direction,{0,0,-1}),"centre world ray has independent expected direction");
	Check(Near(ray.min_distance,0.1) && Near(ray.max_distance,100),"centre ray retains camera clipping interval");
	Check(camera.WorldRay(Pointf(800,300),Size(800,600),ray) && Near(ray.direction,Scene3DVec3(4.0/3,0,-1).Normalized()),"edge ray expected aspect and field of view");
	Check(Near(ray.min_distance*ray.direction.Dot({0,0,-1}),0.1),"off-axis near clipping uses view-plane distance");
	Scene3DVec3 point(1,2,-3);
	Check(camera.Project(point,Size(800,600),screen,depth) && camera.WorldRay(screen,Size(800,600),ray) &&
	      (point-ray.origin).Cross(ray.direction).Length()<1e-9,"perspective world-screen-ray round trip");
	Check(!camera.Project({0,0,10},Size(800,600),screen,depth),"camera-plane point cannot project");
	Check(!camera.Project({0,0,11},Size(800,600),screen,depth),"behind-camera point cannot project");
	Check(!camera.WorldRay(Pointf(0,0),Size(0,600),ray),"zero-width viewport rejected");
	Check(camera.WorldRay(Pointf(0.5,0.5),Size(1,1),ray),"one-pixel viewport remains valid");
	Scene3DMatrix vp=camera.GetViewProjection(Size(800,600));
	double clipx=vp.m[0]*point.x+vp.m[4]*point.y+vp.m[8]*point.z+vp.m[12];
	double clipy=vp.m[1]*point.x+vp.m[5]*point.y+vp.m[9]*point.z+vp.m[13];
	double clipz=vp.m[2]*point.x+vp.m[6]*point.y+vp.m[10]*point.z+vp.m[14];
	double clipw=vp.m[3]*point.x+vp.m[7]*point.y+vp.m[11]*point.z+vp.m[15];
	camera.Project(point,Size(800,600),screen,depth);
	Check(Near((clipx/clipw+1)*400,screen.x) && Near((clipy/clipw+1)*300,screen.y) && Near(clipz/clipw,depth),"GPU matrix agrees with independent projection formula");
	camera.orthographic=true; camera.ortho_height=6;
	Check(camera.Project({1,1,0},Size(800,600),screen,depth) && Near(screen.x,500) && Near(screen.y,200),"orthographic expected screen coordinates");
	Check(camera.WorldRay(Pointf(500,200),Size(800,600),ray) && Near(ray.origin,{1,1,10}) && Near(ray.direction,{0,0,-1}),"orthographic ray origins vary, direction stays fixed");
	Check(Near(depth,9.9/99.9),"orthographic depth independently matches expected value");
	uint64 revision=camera.revision; Check(camera.Orbit(0,0) && camera.revision==revision,"unchanged orbit preserves revision");
	Check(camera.Orbit(0.1,100) && camera.revision==revision+1 && camera.Validate(error),"orbit clamps to valid pitch range and increments revision");
	camera=Camera(); revision=camera.revision;
	Check(camera.Pan(30,60,Size(800,600)) && Near(camera.target,{-1,2,0}) && camera.revision==revision+1,"pan pixels convert to target-plane world units");
	Check(camera.Dolly(std::log(2.0)) && Near(camera.distance,5),"dolly logarithmic delta halves perspective distance");
	auto before=camera;
	Check(!camera.Orbit(std::numeric_limits<double>::quiet_NaN(),0) && camera.revision==before.revision && camera.yaw==before.yaw,"invalid navigation leaves camera unchanged");
	camera.near_plane=camera.far_plane; Check(!camera.Validate(error),"invalid near/far range rejected");
	camera=Camera(); camera.revision=std::numeric_limits<uint64>::max();
	Check(!camera.Orbit(0.1,0) && camera.yaw==0,"revision exhaustion rejects mutation without wraparound");
	camera=Camera(); Scene3DBounds bounds; bounds.Add({-2,-1,-3}); bounds.Add({2,1,3});
	Check(camera.Fit(bounds,Size(300,800)),"fit succeeds in tall narrow viewport");
	bool fits=true;
	for(int i=0;i<8;++i) {
		Scene3DVec3 corner(i&1?2:-2,i&2?1:-1,i&4?3:-3);
		fits &= camera.Project(corner,Size(300,800),screen,depth) && screen.x>=0 && screen.x<=300 && screen.y>=0 && screen.y<=800;
	}
	Check(fits,"fit includes every bound corner and clipping planes");
	// A fit must not replace useful default clips with a tight interval that makes
	// the next few wheel ticks clip an ordinary box. Zoom-in can crop screen edges;
	// these checks distinguish screen framing from unexpected near/far rejection.
	Scene3DBounds unit_box; unit_box.Add({-1,-1,-1}); unit_box.Add({1,1,1});
	Scene3DCamera fitted; fitted.yaw=0; fitted.pitch=0;
	const double default_near=fitted.near_plane, default_far=fitted.far_plane;
	Check(fitted.Fit(unit_box,Size(800,600)) && fitted.near_plane==default_near && fitted.far_plane==default_far,
	      "fit preserves already adequate default near/far planes");
	Scene3DCamera zoom_in=fitted, zoom_out=fitted;
	bool navigation_ok=true;
	for(int i=0;i<3;++i) navigation_ok &= zoom_in.Dolly(0.16);
	for(int i=0;i<6;++i) navigation_ok &= zoom_out.Dolly(-0.16);
	bool in_depth=true, out_framed=true;
	for(int i=0;i<8;++i) {
		Scene3DVec3 corner(i&1?1:-1,i&2?1:-1,i&4?1:-1);
		in_depth &= zoom_in.Project(corner,Size(800,600),screen,depth);
		out_framed &= zoom_out.Project(corner,Size(800,600),screen,depth) &&
		              screen.x>=0 && screen.x<=800 && screen.y>=0 && screen.y<=600;
	}
	Check(navigation_ok && in_depth,"three wheel-sized dolly-in steps after fit keep box corners depth-valid");
	Check(navigation_ok && out_framed,"six wheel-sized dolly-out steps after fit keep the whole box visible");
	Check(zoom_in.Project({0,0,0},Size(800,600),screen,depth) &&
	      zoom_out.Project({0,0,0},Size(800,600),screen,depth),"fitted pivot survives ordinary navigation in both directions");
	Scene3DCamera tight; tight.yaw=0; tight.pitch=0; tight.near_plane=5; tight.far_plane=6;
	Check(tight.Fit(unit_box,Size(800,600)) && tight.near_plane<5 && tight.far_plane>6,
	      "fit expands insufficient authored clip planes outward");
	uint64 fit_revision=tight.revision;
	Check(tight.Fit(unit_box,Size(800,600)) && tight.revision==fit_revision,"repeating the same fit preserves camera revision");
	tight=Camera(); tight.near_plane=2; tight.far_plane=12;
	Check(tight.Dolly(-0.64) && tight.near_plane==2 && tight.far_plane==12 &&
	      !tight.Project(tight.target,Size(800,600),screen,depth),
	      "dolly preserves intentional tight clipping rather than widening clips or clamping distance");
}
void CameraAxisTests()
{
	struct AxisCase {
		Scene3DViewDirection direction;
		Scene3DVec3 back, right, up;
	};
	// Independent expected world bases from the Y-up native handoff, not computed
	// through the production camera's angles or cross-product implementation.
	const AxisCase axes[]={
		{Scene3DViewDirection::Front, {0,0,1}, {1,0,0}, {0,1,0}},
		{Scene3DViewDirection::Back, {0,0,-1}, {-1,0,0}, {0,1,0}},
		{Scene3DViewDirection::Right, {1,0,0}, {0,0,-1}, {0,1,0}},
		{Scene3DViewDirection::Left, {-1,0,0}, {0,0,1}, {0,1,0}},
		{Scene3DViewDirection::Top, {0,1,0}, {1,0,0}, {0,0,-1}},
		{Scene3DViewDirection::Bottom, {0,-1,0}, {1,0,0}, {0,0,1}}
	};
	const Size size(800,600);
	String error;
	for(const AxisCase& axis:axes) for(bool ortho:{false,true}) {
		auto camera=Camera(); camera.target={3,4,5}; camera.yaw=0.3; camera.pitch=0.2;
		Check(camera.SetOrthographic(ortho),"axis test projection prepared");
		auto before=camera;
		Check(camera.AlignTo(axis.direction) && camera.Validate(error) && camera.revision==before.revision+1,
		      "all six exact axis alignments are valid and increment revision");
		Check(camera.orthographic==before.orthographic && Near(camera.target,before.target,0) &&
		      camera.distance==before.distance && camera.ortho_height==before.ortho_height &&
		      camera.fov_y==before.fov_y && camera.near_plane==before.near_plane && camera.far_plane==before.far_plane,
		      "axis alignment changes only pose and revision");
		Scene3DVec3 right,up,forward; camera.GetBasis(right,up,forward);
		Check(Near(right,axis.right,0) && Near(up,axis.up,0) && Near(forward,-axis.back,0),
		      "axis bases are exact, with correct screen-up including poles");
		Check(Near(camera.GetEye(),camera.target+axis.back*10,0) && camera.GetViewDirection(0)==axis.direction,
		      "exact axis eye and zero-tolerance direction recognition agree");
		before=camera;
		Check(camera.AlignTo(axis.direction) && SameCamera(camera,before),"reapplying exact direction is a successful no-op");
		Pointf projected; double depth;
		Check(camera.Project(camera.target+axis.up,size,projected,depth) && Near(projected.x,400) && Near(projected.y,270),
		      "each canonical screen-up vector projects exactly thirty pixels upward");
		Check(camera.Project(camera.target+axis.right,size,projected,depth) && Near(projected.x,430) && Near(projected.y,300),
		      "each canonical screen-right vector projects exactly thirty pixels rightward");
		Scene3DRay ray;
		bool ray_ok=camera.WorldRay(Pointf(475,225),size,ray);
		Scene3DVec3 expected_origin=ortho ? camera.GetEye()+axis.right*2.5+axis.up*2.5 : camera.GetEye();
		Scene3DVec3 expected_direction=ortho ? -axis.back : (-axis.back+axis.right*0.25+axis.up*0.25).Normalized();
		Check(ray_ok && Near(ray.origin,expected_origin) && Near(ray.direction,expected_direction),
		      "all axis rays agree with independent perspective and orthographic expected values");
		Scene3DVec3 point=camera.target+axis.right*2.5+axis.up*2.5;
		Check(camera.Project(point,size,projected,depth) && Near(projected.x,475) && Near(projected.y,225) &&
		      (point-ray.origin).Cross(ray.direction).Length()<1e-9,"all axis project/ray paths intersect the same target-plane point");
		Scene3DMatrix matrix=camera.GetViewProjection(size);
		double x=matrix.m[0]*point.x+matrix.m[4]*point.y+matrix.m[8]*point.z+matrix.m[12];
		double y=matrix.m[1]*point.x+matrix.m[5]*point.y+matrix.m[9]*point.z+matrix.m[13];
		double z=matrix.m[2]*point.x+matrix.m[6]*point.y+matrix.m[10]*point.z+matrix.m[14];
		double w=matrix.m[3]*point.x+matrix.m[7]*point.y+matrix.m[11]*point.z+matrix.m[15];
		Check(Near((x/w+1)*400,475) && Near((y/w+1)*300,225) && Near(z/w,depth),
		      "GPU projection matrices match independent expected coordinates for all axes and both projections");
	}
	auto camera=Camera(); camera.orthographic=true; camera.AlignTo(Scene3DViewDirection::Top);
	Scene3DVec3 right,up,forward;
	Check(camera.Pan(30,60,size) && Near(camera.target,{-0.4,0,-0.8}),"top pan uses -Z as screen-up");
	camera=Camera(); camera.orthographic=true; camera.AlignTo(Scene3DViewDirection::Bottom);
	Check(camera.Pan(30,60,size) && Near(camera.target,{-0.4,0,0.8}),"bottom pan uses +Z as screen-up");
	camera=Camera(); camera.AlignTo(Scene3DViewDirection::Top);
	Check(camera.Orbit(0.2,0) && camera.GetViewDirection()==Scene3DViewDirection::User &&
	      camera.pitch==3.14159265358979323846/2,"yaw rotation at a pole retains its exact axis but is a user orientation");
	camera.GetBasis(right,up,forward);
	Check(Near(forward,{0,-1,0}) && Near(right.Length(),1) && Near(up.Length(),1) && Near(right.Dot(up),0),
	      "rolled top view remains a finite orthonormal basis");
	Check(camera.AlignTo(Scene3DViewDirection::Top) && camera.GetViewDirection(0)==Scene3DViewDirection::Top,
	      "aligning a rolled pole restores canonical screen-up");
	Check(camera.Orbit(0,-0.1) && camera.GetViewDirection()==Scene3DViewDirection::User,
	      "orbit can leave an exact top view without singularity");
	camera=Camera(); camera.Orbit(0.5e-6,0);
	Check(camera.GetViewDirection(1e-6)==Scene3DViewDirection::Front && camera.GetViewDirection(1e-8)==Scene3DViewDirection::User,
	      "direction recognition applies angular tolerance without mutating pose");
	auto before=camera;
	Check(camera.GetViewDirection(-1)==Scene3DViewDirection::User &&
	      camera.GetViewDirection(std::numeric_limits<double>::quiet_NaN())==Scene3DViewDirection::User &&
	      camera.GetViewDirection(1)==Scene3DViewDirection::User && SameCamera(camera,before),
	      "invalid direction tolerance returns User without mutation");
	Check(!camera.AlignTo(Scene3DViewDirection::User) && SameCamera(camera,before) &&
	      !camera.AlignTo(static_cast<Scene3DViewDirection>(999)) && SameCamera(camera,before),
	      "User and unknown direction values are rejected atomically");
	camera=Camera(); camera.pitch=3.14159265358979323846/2+0.001; before=camera;
	Check(!camera.Validate(error) && !camera.AlignTo(Scene3DViewDirection::Front) && SameCamera(camera,before) &&
	      camera.GetViewDirection()==Scene3DViewDirection::User,"out-of-range pitch remains invalid and alignment does not repair invalid state silently");
	camera=Camera(); camera.revision=std::numeric_limits<uint64>::max(); before=camera;
	Check(camera.AlignTo(Scene3DViewDirection::Front) && SameCamera(camera,before) &&
	      !camera.AlignTo(Scene3DViewDirection::Right) && SameCamera(camera,before),
	      "exhausted camera revision permits alignment no-op but rejects a changed pose");
	camera=Camera(); camera.target={1e9,-1e9,1e9}; camera.distance=1e-6;
	camera.AlignTo(Scene3DViewDirection::Top); camera.GetBasis(right,up,forward);
	Check(Near(right,{1,0,0},0) && Near(up,{0,0,-1},0) && Near(forward,{0,-1,0},0),
	      "exact basis does not lose direction precision at a large translated target");
}
void CameraProjectionTests()
{
	const Size size(800,600);
	auto camera=Camera(); camera.target={3,4,5}; camera.yaw=0.3; camera.pitch=0.2;
	Scene3DVec3 right,up,forward; camera.GetBasis(right,up,forward);
	Scene3DVec3 point=camera.target+up*2+right*1.5;
	Pointf before_screen,after_screen; double depth;
	camera.Project(point,size,before_screen,depth);
	auto before=camera;
	Check(camera.SetOrthographic(true) && camera.orthographic && Near(camera.ortho_height,20) &&
	      camera.revision==before.revision+1,"perspective-to-orthographic conversion uses independently expected twenty-metre span");
	Check(Near(camera.target,before.target,0) && camera.yaw==before.yaw && camera.pitch==before.pitch &&
	      camera.distance==before.distance && camera.fov_y==before.fov_y && camera.near_plane==before.near_plane &&
	      camera.far_plane==before.far_plane,"projection switch preserves target, pose, perspective lens and authored clips");
	Check(camera.Project(point,size,after_screen,depth) && Near(before_screen.x,after_screen.x) && Near(before_screen.y,after_screen.y),
	      "projection switch keeps target-plane horizontal and vertical framing");
	before=camera;
	Check(camera.SetOrthographic(true) && SameCamera(camera,before),"repeated projection selection is a successful no-op");
	Check(camera.SetOrthographic(false) && !camera.orthographic && Near(camera.distance,10) &&
	      camera.revision==before.revision+1,"orthographic-to-perspective round trip restores orbit distance");
	Check(camera.Project(point,size,after_screen,depth) && Near(before_screen.x,after_screen.x) && Near(before_screen.y,after_screen.y),
	      "projection round trip preserves target-plane framing");
	camera.SetOrthographic(true); camera.ortho_height=8; before=camera;
	Check(camera.SetOrthographic(false) && Near(camera.distance,4) && camera.ortho_height==8 &&
	      camera.yaw==before.yaw && camera.pitch==before.pitch && Near(camera.target,before.target,0),
	      "authored orthographic span controls perspective distance without a different camera orientation");
	Scene3DCamera repeated; repeated.fov_y=0.7; repeated.distance=7.3;
	bool ok=true;
	for(int i=0;i<100;++i) ok &= repeated.SetOrthographic(true) && repeated.SetOrthographic(false);
	Check(ok && Near(repeated.distance,7.3,1e-11),"one hundred projection round trips do not accumulate meaningful framing drift");
	camera=Camera(); camera.distance=1e9; camera.fov_y=3; before=camera;
	Check(!camera.SetOrthographic(true) && SameCamera(camera,before),"projection rejects oversized derived orthographic span without clamping");
	camera=Camera(); camera.distance=1e-6; camera.fov_y=0.01; before=camera;
	Check(!camera.SetOrthographic(true) && SameCamera(camera,before),"projection rejects undersized derived orthographic span without mutation");
	camera=Camera(); camera.orthographic=true; camera.ortho_height=1e9; camera.fov_y=0.01; before=camera;
	Check(!camera.SetOrthographic(false) && SameCamera(camera,before),"projection rejects oversized derived perspective distance without mutation");
	camera=Camera(); camera.orthographic=true; camera.ortho_height=1e-6; camera.fov_y=3; before=camera;
	Check(!camera.SetOrthographic(false) && SameCamera(camera,before),"projection rejects undersized derived perspective distance without mutation");
	camera=Camera(); camera.fov_y=std::numeric_limits<double>::quiet_NaN(); before=camera;
	Check(!camera.SetOrthographic(true) && !camera.orthographic && std::isnan(camera.fov_y) &&
	      camera.revision==before.revision && camera.distance==before.distance && camera.ortho_height==before.ortho_height,
	      "projection rejects invalid current lens without changing camera fields");
	camera=Camera(); camera.revision=std::numeric_limits<uint64>::max(); before=camera;
	Check(camera.SetOrthographic(false) && SameCamera(camera,before) && !camera.SetOrthographic(true) && SameCamera(camera,before),
	      "exhausted revision permits projection no-op but rejects a changed projection");
}
void PickTests()
{
	String error; auto box=Scene3DMesh::MakeBox({2,2,2},error);
	Scene3DSceneDesc desc; desc.revision=55;
	desc.objects={Object("actor:back",box,{0,0,-3}),Object("actor:front",box)};
	auto scene=Scene3DSnapshot::Create(desc,error); auto camera=Camera(); Scene3DRay ray;
	camera.WorldRay(Pointf(400,300),Size(800,600),ray); auto hit=scene->Pick(ray,77);
	Check(hit.hit && hit.id=="actor:front" && Near(hit.distance,9),"nearest triangle wins regardless of object order");
	Check(Near(hit.world_point,{0,0,1}) && Near(hit.world_normal,{0,0,1}),"hit includes exact world point and outward normal");
	Check(hit.scene_revision==55 && hit.camera_revision==77 && hit.triangle>=0,"hit includes source revisions and triangle identity");
	std::swap(desc.objects[0],desc.objects[1]); scene=Scene3DSnapshot::Create(desc,error); hit=scene->Pick(ray,77);
	Check(hit.hit && hit.id=="actor:front" && Near(hit.distance,9),"reverse object submission preserves nearest pick");
	desc.objects[0].selectable=false; scene=Scene3DSnapshot::Create(desc,error); hit=scene->Pick(ray,77);
	Check(hit.hit && hit.id=="actor:back","unselectable object does not become selection authority");
	desc.objects[0].selectable=true; desc.objects[0].visible=false; scene=Scene3DSnapshot::Create(desc,error); hit=scene->Pick(ray,77);
	Check(hit.hit && hit.id=="actor:back","invisible object excluded from picking");
	desc.objects.resize(1); desc.objects[0].visible=true; scene=Scene3DSnapshot::Create(desc,error);
	ray.min_distance=9.5; hit=scene->Pick(ray,77);
	Check(hit.hit && Near(hit.distance,11),"near clipping skips front surface and allows valid back surface");
	ray.max_distance=10.5; hit=scene->Pick(ray,77);
	Check(!hit.hit && hit.id.IsEmpty() && hit.scene_revision==55 && hit.camera_revision==77,"near and far exclude all surfaces with deterministic miss metadata");
	ray.direction={0,0,0}; Check(!scene->Pick(ray,77).hit,"invalid ray does not hit");
	// A sloped plane under nonuniform scale: x/2+y/3=0, normal proportional(3,2,0).
	std::vector<Scene3DVertex> vertices={Vertex({-2,2,-2},{1,1,0}),Vertex({2,-2,-2},{1,1,0}),Vertex({0,0,2},{1,1,0})};
	auto plane=Scene3DMesh::Create(vertices,{0,1,2},error); desc.objects={Object("actor:scaled",plane)};
	desc.objects[0].local=Scene3DMatrix::Scale({2,3,4}); scene=Scene3DSnapshot::Create(desc,error);
	ray.origin={0,10,0}; ray.direction={0,-1,0}; ray.min_distance=0; ray.max_distance=100;
	hit=scene->Pick(ray,2);
	Check(hit.hit && Near(hit.distance,10) && Near(hit.world_normal,Scene3DVec3(3,2,0).Normalized()),"pick uses inverse-transpose normal and world-unit distance under nonuniform scale");
}
void MaterialCameraTests()
{
    String error;
    auto camera=Camera();
    const auto original=camera;
    Check(camera.SetLookAt({0,0,10},{0,0,0},{0,1,0},error) && SameCamera(camera,original),
          "exact unchanged eye/aim/up is a revision-preserving no-op");
    Check(camera.SetLookAt({2,3,4},{1,.5,-1},{.4,1,.2},error),"authored eye aim and up accepted");
    Scene3DVec3 right,up,forward; camera.GetBasis(right,up,forward);
    const auto wanted_forward=(Scene3DVec3(1,.5,-1)-Scene3DVec3(2,3,4)).Normalized();
    const auto wanted_up=(Scene3DVec3(.4,1,.2)-wanted_forward*Scene3DVec3(.4,1,.2).Dot(wanted_forward)).Normalized();
    Check(Near(camera.GetEye(),{2,3,4}) && Near(camera.target,{1,.5,-1}) &&
          Near(forward,wanted_forward) && Near(up,wanted_up),"eye/aim/up reproduces the actual authored camera basis");
    const auto accepted=camera;
    Check(!camera.SetLookAt({0,0,1},{0,0,0},{0,0,1},error) && SameCamera(camera,accepted),
          "parallel up rejects atomically");
    Check(!camera.SetLookAt({0,0,1},{0,0,0},{0,0,0},error) && SameCamera(camera,accepted),
          "zero up rejects atomically");
    for(int sign:{-1,1}) {
        Check(camera.SetLookAt({0,5.0*sign,0},{0,0,0},{1,0,0},error),"exact pole camera accepts a non-world-up roll");
        camera.GetBasis(right,up,forward);
        Check(Near(up,{1,0,0}) && Near(camera.GetEye(),{0,5.0*sign,0}),"pole up remains authored rather than guessed");
    }
    camera=Camera(); camera.roll=3.14159265358979323846/2;
    Pointf point; double depth;
    Check(camera.Project({1,0,0},Size(800,600),point,depth) && Near(point.x,400) && point.y<300,
          "positive roll maps world +X to screen up");
    camera.gate_aspect=1;
    Check(camera.GetGateRect(Size(1280,720))==Rect(280,0,1000,720),"square film gate is pillarboxed in a wide view");
    camera.gate_aspect=16.0/9;
    Check(camera.GetGateRect(Size(720,1280))==Rect(0,437,720,842),"widescreen film gate is letterboxed in a tall view");
    bool projected=true;
    for(bool orthographic:{false,true}) for(Size size:{Size(1280,720),Size(720,1280),Size(853,617)}) {
        camera=Camera(); camera.roll=.37; camera.gate_aspect=16.0/9;
        camera.orthographic=orthographic; camera.ortho_height=5;
        for(Scene3DVec3 world:std::vector<Scene3DVec3>{{-.5,.3,0},{1,-.7,-2},{0,0,1}}) {
            Scene3DRay ray;
            projected &= camera.Project(world,size,point,depth) && camera.WorldRay(point,size,ray);
            const auto delta=world-ray.origin;
            projected &= delta.Cross(ray.direction).Length()<1e-8 && delta.Dot(ray.direction)>0;
        }
    }
    Check(projected,"rolled perspective and orthographic film gates preserve project/ray agreement at wide, tall and odd sizes");
    camera=Camera(); camera.gate_aspect=std::numeric_limits<double>::quiet_NaN();
    Check(!camera.Validate(error) && camera.GetGateRect(Size(800,600)).IsEmpty(),"invalid film gate cannot produce a usable projection");

    std::vector<byte> rgba={255,0,0,0, 0,255,0,255, 0,0,255,128, 255,255,255,64};
    auto texture=Scene3DTexture::Create(Size(2,2),rgba,Scene3DTextureColorSpace::Srgb,error);
    Check(bool(texture),"exact top-down straight RGBA texture accepted");
    if(!texture) return;
    rgba[3]=255;
    Check(texture->GetPixels()[3]==0 && texture->GetSize()==Size(2,2) &&
          texture->GetColorSpace()==Scene3DTextureColorSpace::Srgb,"immutable texture owns its pixel copy and color-space identity");
    Check(Near(texture->SampleAlpha(.25,.75,false),0) && Near(texture->SampleAlpha(.75,.75,false),1) &&
          Near(texture->SampleAlpha(.25,.25,false),128.0/255) &&
          Near(texture->SampleAlpha(.75,.25,false),64.0/255),"V-up UVs address all four top-down texel centers correctly");
    Check(Near(texture->SampleAlpha(.5,.5,false),(0+255+128+64)/(4.0*255)),
          "alpha interpolation is linear independently of RGB color space");
    Check(Near(texture->SampleAlpha(1.75,.75,true),1) && Near(texture->SampleAlpha(-100,100,false),0),
          "repeat and clamp-to-edge use their declared addressing policies");
    Check(texture->SampleAlpha(std::numeric_limits<double>::infinity(),0,false)==0,
          "non-finite sample coordinates cannot address texture memory");
    rgba.pop_back();
    Check(!Scene3DTexture::Create(Size(2,2),rgba,Scene3DTextureColorSpace::Srgb,error),"incomplete RGBA pixels reject");
    Check(!Scene3DTexture::Create(Size(0,2),rgba,Scene3DTextureColorSpace::Srgb,error),"empty texture dimensions reject");
    Check(!Scene3DTexture::Create(Size(16385,2),rgba,Scene3DTextureColorSpace::Srgb,error),"oversized texture dimensions reject");
    rgba={255,255,255,255};
    Check(!Scene3DTexture::Create(Size(1,1),rgba,(Scene3DTextureColorSpace)999,error),"unknown texture encoding rejects");
}
void MaterialPickTests()
{
    String error;
    auto a=Vertex({-2,-2,0}),b=Vertex({2,-2,0}),c=Vertex({0,2,0});
    a.u=b.u=c.u=.5; a.v=b.v=c.v=.5;
    auto mesh=Scene3DMesh::Create({a,b,c},{0,1,2},error);
    Scene3DSceneDesc desc;
    desc.objects={Object("front",mesh),Object("back",mesh,{0,0,-1})};
    const auto front=desc.objects[0];
    Scene3DRay ray; ray.origin={0,0,10}; ray.direction={0,0,-1}; ray.max_distance=100;
    auto pick=[&] {
        auto scene=Scene3DSnapshot::Create(desc,error);
        Check(bool(scene),"material pick scene validates");
        return scene?scene->Pick(ray,1):Scene3DPick();
    };
    auto texture=[&](byte alpha) {
        return Scene3DTexture::Create(Size(1,1),std::vector<byte>{255,255,255,alpha},
                                     Scene3DTextureColorSpace::Srgb,error);
    };
    desc.objects[0].a=0; desc.objects[0].material.color_texture=texture(0);
    Check(pick().id=="front","Opaque ignores material and texture alpha for picking");
    desc.objects[0].opacity=.1f;
    Check(pick().id=="front","independent fade leaves a nonzero opaque surface selectable");
    desc.objects[0].opacity=0;
    Check(pick().id=="back","zero independent opacity removes a surface from picking");
    desc.objects[0]=front; desc.objects[0].material.alpha_mode=Scene3DAlphaMode::Mask;
    desc.objects[0].material.color_texture=texture(0);
    Check(pick().id=="back","masked hole permits picking the actual surface behind it");
    desc.objects[0].material.color_texture=texture(255); desc.objects[0].a=.5f;
    desc.objects[0].opacity=.1f;
    Check(pick().id=="front","mask equality survives cutoff before applying independent opacity");
    desc.objects[0].a=.49f;
    Check(pick().id=="back","mask tests material coverage before the object fade");
    desc.objects[0].a=0; desc.objects[0].material.alpha_cutoff=0;
    Check(pick().id=="front","zero alpha at a zero mask cutoff remains covered");
    desc.objects[0].material.alpha_mode=Scene3DAlphaMode::Blend;
    Check(pick().id=="back","zero blend coverage is unpickable");
    desc.objects[0].a=.01f;
    Check(pick().id=="front","positive blend coverage remains pickable");
    desc.objects={front}; desc.objects[0].material.double_sided=false;
    ray.origin={0,0,-10}; ray.direction={0,0,1};
    Check(!pick().hit,"single-sided picking rejects a back face");
    desc.objects[0].material.double_sided=true;
    Check(pick().hit,"double-sided picking accepts the same back face");
    desc.objects[0].opacity=std::numeric_limits<float>::quiet_NaN();
    Check(!Scene3DSnapshot::Create(desc,error),"non-finite independent opacity rejects atomically");
    desc.objects[0].opacity=1; desc.objects[0].material.alpha_cutoff=2;
    Check(!Scene3DSnapshot::Create(desc,error),"invalid alpha cutoff cannot enter a scene snapshot");
}

Scene3DSplat GaussianPoint()
{
    Scene3DSplat point;
    point.covariance[0]=.04; point.covariance[1]=.006; point.covariance[2]=.003;
    point.covariance[3]=.01; point.covariance[4]=.002; point.covariance[5]=.09;
    point.r=.8f; point.g=.3f; point.b=.1f; point.a=.7f;
    return point;
}
void GaussianResourceTests()
{
    String error;
    std::vector<Scene3DSplat> input={GaussianPoint()};
    input[0].position={1,2,3};
    const auto original=input[0];
    auto cloud=Scene3DSplats::Create(input,error);
    Check(cloud && error.IsEmpty(),"Gaussian finite positive-semidefinite source is accepted");
    input[0].position={99,98,97}; input[0].covariance[1]=99; input[0].a=0;
    Check(cloud && Near(cloud->GetSplats()[0].position,original.position) &&
          Near(cloud->GetSplats()[0].covariance[1],.006) && cloud->GetSplats()[0].a==.7f,
          "Gaussian factory detaches position covariance and colour from every caller alias");
    // A Gaussian's coordinate extrema depend on its marginal variances, even
    // when its principal axes are oblique.
    const double contour=std::sqrt(2*std::log(255.0));
    Check(cloud && Near(cloud->GetBounds().min,{1-.2*contour,2-.1*contour,3-.3*contour}) &&
          Near(cloud->GetBounds().max,{1+.2*contour,2+.1*contour,3+.3*contour}),
          "Gaussian bounds enclose the analytical one-over-255 marginal contour");
    Check(!Scene3DSplats::Create({},error) && !error.IsEmpty(),"empty Gaussian resource is rejected");
    auto rejected=[&](const Scene3DSplat& point,const char *message) {
        Check(!Scene3DSplats::Create({point},error) && !error.IsEmpty(),message);
    };
    auto point=GaussianPoint(); point.position.x=std::numeric_limits<double>::quiet_NaN();
    rejected(point,"Gaussian NaN centre rejects");
    point=GaussianPoint(); point.position.z=1e9+1; rejected(point,"Gaussian centre budget rejects");
    point=GaussianPoint(); point.r=-.1f; rejected(point,"Gaussian negative linear colour rejects");
    point=GaussianPoint(); point.a=1.01f; rejected(point,"Gaussian opacity above one rejects");
    point=GaussianPoint(); point.g=std::numeric_limits<float>::infinity(); rejected(point,"Gaussian infinite colour rejects");
    point=GaussianPoint(); point.covariance[0]=-.01; rejected(point,"Gaussian negative marginal variance rejects");
    point=GaussianPoint(); point.covariance[2]=std::numeric_limits<double>::quiet_NaN();
    rejected(point,"Gaussian non-finite covariance rejects");
    point=GaussianPoint(); point.covariance[0]=1e18+1e10; rejected(point,"Gaussian covariance budget rejects");
    point=Scene3DSplat(); point.covariance[1]=2;
    rejected(point,"Gaussian invalid two-axis covariance rejects");
    point=Scene3DSplat(); point.covariance[1]=point.covariance[2]=point.covariance[4]=-.75;
    rejected(point,"Gaussian negative determinant rejects even with positive two-axis minors");
    point=Scene3DSplat();
    const double rank_one[6]={4,6,8,9,12,16};
    for(int i=0;i<6;++i) point.covariance[i]=rank_one[i];
    Check(bool(Scene3DSplats::Create({point},error)),"Gaussian rank-one covariance remains valid");
    for(double& covariance:point.covariance) covariance=0;
    Check(bool(Scene3DSplats::Create({point},error)),"Gaussian zero covariance remains valid for pixel filtering");
    point=Scene3DSplat(); point.covariance[0]=point.covariance[3]=point.covariance[5]=1e18;
    Check(bool(Scene3DSplats::Create({point},error)),"Gaussian bounded large covariance remains valid");
    std::vector<Scene3DSplat> maximum(Scene3DMaxSplats,GaussianPoint());
    Check(bool(Scene3DSplats::Create(maximum,error)),"exact Gaussian resource point budget is admitted");
    maximum.push_back(GaussianPoint());
    Check(!Scene3DSplats::Create(maximum,error) && error.Find(AsString(Scene3DMaxSplats))>=0,
          "Gaussian resource point budget plus one rejects with the actual limit");
}
void GaussianProjectionTests()
{
    String error; auto camera=Camera();
    camera.orthographic=true; camera.ortho_height=2;
    Check(camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error),"Gaussian analytic camera is valid");
    const Size viewport(200,100);
    auto point=GaussianPoint(); point.position={.2,.3,0};
    Scene3DSplatProjection projection;
    Check(Scene3DProjectSplat(point,camera.GetViewProjection(viewport),viewport,projection),
          "Gaussian orthographic projection succeeds");
    Check(Near(projection.center.x,110) && Near(projection.center.y,35) &&
          Near(projection.covariance_xx,100.25) && Near(projection.covariance_xy,-15) &&
          Near(projection.covariance_yy,25.25),
          "Gaussian orthographic covariance matches independent fifty-pixels-per-metre values");
    Check(Near(projection.Coverage(projection.center),1),"Gaussian centre has unit coverage");
    const double expected=std::exp(-.5*(25.25*100-900+100.25*9)/(100.25*25.25-225));
    Check(Near(projection.Coverage(projection.center+Pointf(10,-3)),expected),
          "Gaussian oblique density matches analytical inverse-covariance evaluation");
    Check(projection.Coverage(projection.center+Pointf(100,100))==0,
          "Gaussian finite coverage contour excludes distant points");
    point.position={0,0,0};
    auto parent=Scene3DMatrix::Translation({.5,0,0})*Scene3DMatrix::Scale({2,3,4});
    Check(Scene3DProjectSplat(point,camera.GetViewProjection(viewport)*parent,viewport,projection) &&
          Near(projection.center.x,125) && Near(projection.center.y,50) &&
          Near(projection.covariance_xx,400.25) && Near(projection.covariance_xy,-90) &&
          Near(projection.covariance_yy,225.25),
          "Gaussian full parent transform changes position and covariance exactly once");
    camera.roll=1.57079632679489661923;
    Check(Scene3DProjectSplat(point,camera.GetViewProjection(viewport),viewport,projection) &&
          Near(projection.covariance_xx,25.25) && Near(projection.covariance_xy,15) &&
          Near(projection.covariance_yy,100.25),"Gaussian roll rotates the full covariance");
    camera.roll=0; camera.gate_aspect=4;
    Check(camera.GetGateRect(viewport)==Rect(0,25,200,75) &&
          Scene3DProjectSplat(point,camera.GetViewProjection(viewport),viewport,projection) &&
          Near(projection.covariance_xx,25.25) && Near(projection.covariance_xy,-3.75) &&
          Near(projection.covariance_yy,6.5),"Gaussian physical gate changes the actual pixel scale");
    camera.gate_aspect=0; camera.orthographic=false;
    point.position={.5,.2,0};
    Check(Scene3DProjectSplat(point,camera.GetViewProjection(viewport),viewport,projection) &&
          Near(projection.center.x,105) && Near(projection.center.y,48) &&
          Near(projection.covariance_xx,4.4) && Near(projection.covariance_xy,-.668) &&
          Near(projection.covariance_yy,1.2804),
          "Gaussian perspective includes off-axis XZ YZ and depth covariance");
    const auto valid=projection;
    for(double z:{4.95,6.0,-200.0}) {
        point.position.z=z;
        Check(!Scene3DProjectSplat(point,camera.GetViewProjection(viewport),viewport,projection) &&
              projection.center==valid.center && projection.covariance_xx==valid.covariance_xx,
              "Gaussian invalid near far or behind-camera projection preserves output");
    }
    point=GaussianPoint();
    Check(!Scene3DProjectSplat(point,camera.GetViewProjection(viewport),Size(0,100),projection),
          "Gaussian zero-sized projection rejects");
    auto invalid=camera.GetViewProjection(viewport); invalid.m[0]=std::numeric_limits<double>::infinity();
    Check(!Scene3DProjectSplat(point,invalid,viewport,projection),"Gaussian non-finite projection rejects");
    for(double& covariance:point.covariance) covariance=0;
    Check(Scene3DProjectSplat(point,camera.GetViewProjection(viewport),viewport,projection) &&
          Near(projection.covariance_xx,.25) && Near(projection.covariance_yy,.25) &&
          Near(projection.covariance_xy,0),"Gaussian zero density retains the declared quarter-pixel variance");
}
void GaussianScenePickTests()
{
    String error; auto camera=Camera(); camera.orthographic=true; camera.ortho_height=2;
    camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error); camera.revision=87;
    Scene3DSplat point; point.covariance[0]=point.covariance[3]=point.covariance[5]=.01;
    auto cloud=Scene3DSplats::Create({point},error);
    Scene3DObjectDesc object; object.id="gaussian:original"; object.splats=cloud;
    Scene3DSceneDesc desc; desc.revision=43; desc.objects={object};
    auto scene=Scene3DSnapshot::Create(desc,error);
    Check(scene && !scene->GetBounds().empty,"Gaussian scene contributes actual covariance bounds");
    if(!scene) return;
    auto hit=scene->PickScreen(Pointf(100,50),Size(200,100),camera);
    Check(hit.hit && hit.id==object.id && hit.splat==0 && hit.triangle==-1 &&
          hit.scene_revision==43 && hit.camera_revision==87 &&
          Near(hit.world_point,{0,0,0}) && Near(hit.distance,5),
          "Gaussian screen pick retains exact point identity revisions and centre-plane depth");
    Check(!scene->PickScreen(Pointf(115,65),Size(200,100),camera).hit,
          "Gaussian pick rejects a transparent ellipse corner inside its quad bounds");
    Check(scene->PickScreen(Pointf(114,50),Size(200,100),camera).hit,
          "Gaussian analytical contour remains selectable before independent fade");
    desc.objects[0].a=.5f; desc.objects[0].opacity=.25f;
    auto faded=Scene3DSnapshot::Create(desc,error);
    Check(faded && !faded->PickScreen(Pointf(114,50),Size(200,100),camera).hit &&
          faded->PickScreen(Pointf(110,50),Size(200,100),camera).hit,
          "Gaussian pick applies the same multiplied-alpha threshold as fragment coverage");
    desc.objects[0]=object; desc.objects[0].selectable=false;
    auto unselectable=Scene3DSnapshot::Create(desc,error);
    Check(unselectable && !unselectable->PickScreen(Pointf(100,50),Size(200,100),camera).hit,
          "Gaussian selectability remains independent from visible geometry");
    auto front=point; front.position.z=.5;
    desc.objects[0]=object; desc.objects[0].splats=Scene3DSplats::Create({point,front},error);
    auto layered=Scene3DSnapshot::Create(desc,error);
    Check(layered && layered->PickScreen(Pointf(100,50),Size(200,100),camera).splat==1,
          "Gaussian nearest pick preserves original unsorted resource point index");
    const auto mesh=Scene3DMesh::Create({Vertex({-2,-2,0}),Vertex({2,-2,0}),Vertex({0,2,0})},{0,1,2},error);
    desc.objects={object,Object("mesh:front",mesh,{0,0,1})};
    auto occluded=Scene3DSnapshot::Create(desc,error);
    Check(occluded && occluded->PickScreen(Pointf(100,50),Size(200,100),camera).id=="mesh:front",
          "Gaussian screen pick respects actual opaque mesh occlusion");
    desc.objects[0].local=Scene3DMatrix::Translation({0,0,2});
    auto foreground=Scene3DSnapshot::Create(desc,error);
    Check(foreground && foreground->PickScreen(Pointf(100,50),Size(200,100),camera).id==object.id,
          "Gaussian in front of the mesh wins at its rendered depth plane");
    desc.objects={object,object}; desc.objects[1].id="gaussian:second";
    auto tied=Scene3DSnapshot::Create(desc,error);
    Check(tied && tied->PickScreen(Pointf(100,50),Size(200,100),camera).id==object.id,
          "Gaussian equal-depth object ties retain descriptor order");
    auto miss=scene->PickScreen(Pointf(-1,50),Size(200,100),camera);
    Check(!miss.hit && miss.scene_revision==43 && miss.camera_revision==87,
          "Gaussian outside-view miss retains source and camera revisions");
    desc.objects={object}; desc.objects[0].mesh=mesh;
    Check(!Scene3DSnapshot::Create(desc,error),"scene rejects simultaneous Gaussian and mesh geometry");
    desc.objects[0]=object;
    desc.objects[0].material.color_texture=Scene3DTexture::Create(Size(1,1),{255,255,255,255},Scene3DTextureColorSpace::Srgb,error);
    Check(!Scene3DSnapshot::Create(desc,error),"Gaussian scene rejects an inapplicable mesh texture");
    desc.objects[0]=object; desc.objects[0].parent_id="group";
    auto group=Object("group",{}); group.local=Scene3DMatrix::Translation({.5,0,0})*Scene3DMatrix::Scale({2,3,1});
    desc.objects.push_back(group);
    auto parented=Scene3DSnapshot::Create(desc,error);
    Check(parented && parented->GetObjects()[1].bounds.empty &&
          parented->PickScreen(Pointf(125,50),Size(200,100),camera).id==object.id,
          "geometry-free grouping preserves actual transformed Gaussian picks");
    desc.objects[1].visible=false;
    auto hidden=Scene3DSnapshot::Create(desc,error);
    Check(hidden && hidden->GetBounds().empty && !hidden->PickScreen(Pointf(125,50),Size(200,100),camera).hit,
          "hidden Gaussian parent removes fitting and picking without source edits");
    std::vector<Scene3DSplat> half(Scene3DMaxSplats/2,point);
    object.splats=Scene3DSplats::Create(half,error); object.parent_id.Clear();
    desc.objects={object,object}; desc.objects[1].id="instance:second";
    Check(bool(Scene3DSnapshot::Create(desc,error)),"exact instanced Gaussian scene budget is admitted");
    Scene3DObjectDesc extra; extra.id="one-too-many"; extra.splats=cloud; desc.objects.push_back(extra);
    Check(!Scene3DSnapshot::Create(desc,error) && error.Find(AsString(Scene3DMaxSplats))>=0,
          "shared-resource Gaussian instances count toward the reported aggregate draw budget");
}

void GaussianSurfaceSampleTests()
{
    String error;
    auto camera=Camera(); camera.orthographic=true; camera.ortho_height=2;
    camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error); camera.revision=87;
    const Size viewport(200,100);
    const Pointf centre(100,50);
    std::vector<Scene3DSplat> plane;
    for(int y=-5;y<=5;++y) for(int x=-5;x<=5;++x) {
        Scene3DSplat point; point.position={x*.1,y*.1,0};
        point.covariance[0]=point.covariance[3]=point.covariance[5]=.0004;
        plane.push_back(point);
    }
    Scene3DObjectDesc object; object.id="surface:target";
    object.splats=Scene3DSplats::Create(plane,error);
    Scene3DSceneDesc desc; desc.revision=43; desc.objects={object};
    auto scene=Scene3DSnapshot::Create(desc,error);
    Check(bool(scene),"Gaussian surface-sample fixture is valid");
    if(!scene) return;
    const auto direct=scene->PickScreen(centre,viewport,camera);
    auto sample=scene->SampleSplatSurface(centre,0,viewport,camera,object.id);
    Check(sample.pick.hit && sample.pick.id==direct.id && sample.pick.splat==direct.splat &&
          Near(sample.pick.world_point,direct.world_point) && Near(sample.pick.distance,direct.distance) &&
          sample.pick.scene_revision==43 && sample.pick.camera_revision==87,
          "zero-radius target sampling preserves exact single-hit depth and revisions");
    Check(sample.support.sample_count==1 && sample.support.hit_count==1 &&
          sample.support.support_count==1 && sample.support.splat_count==1 &&
          Near(sample.support.confidence,.25) && !sample.support.normal_valid &&
          Near(sample.support.world_normal,{0,0,0}),
          "single-hit evidence is sparse and never reports a fabricated surface normal");

    sample=scene->SampleSplatSurface(centre,20,viewport,camera,object.id);
    Check(sample.pick.hit && Near(sample.pick.world_point,{0,0,0}) &&
          sample.support.sample_count==25 && sample.support.hit_count==25 &&
          sample.support.support_count==25 && sample.support.splat_count>=9,
          "uniform front surface returns a central observed hit with bounded distributed support");
    Check(sample.support.screen_spread_pixels>15 && sample.support.world_spread>.3 &&
          Near(sample.support.depth_spread,0) && Near(sample.support.confidence,1),
          "surface evidence reports independent screen/world spread and coherent depth");

    const auto mesh=Scene3DMesh::MakeBox({2,2,.1},error);
    auto foreground=Object("surface:occluder",mesh,{0,0,2});
    desc.objects={object,foreground};
    auto occluded=Scene3DSnapshot::Create(desc,error);
    Check(occluded && occluded->PickScreen(centre,viewport,camera).id==foreground.id &&
          occluded->SampleSplatSurface(centre,0,viewport,camera,object.id).pick.id==object.id,
          "target-only sampling cannot take the endpoint from an unrelated foreground mesh");
    Check(occluded && !occluded->SampleSplatSurface(centre,20,viewport,camera,foreground.id).pick.hit &&
          !occluded->SampleSplatSurface(centre,20,viewport,camera,"missing").pick.hit,
          "missing and non-Gaussian targets never fall back to another asset");

    auto with_floater=plane;
    Scene3DSplat floater; floater.position={0,0,2};
    floater.covariance[0]=floater.covariance[3]=floater.covariance[5]=0;
    with_floater.push_back(floater);
    desc.objects={object}; desc.objects[0].splats=Scene3DSplats::Create(with_floater,error);
    auto contaminated=Scene3DSnapshot::Create(desc,error);
    Check(bool(contaminated),"Gaussian separated-depth fixture is valid");
    if(!contaminated) return;
    const auto click=contaminated->SampleSplatSurface(centre,0,viewport,camera,object.id);
    sample=contaminated->SampleSplatSurface(centre,20,viewport,camera,object.id);
    Check(click.pick.hit && Near(click.pick.world_point.z,2) &&
          sample.pick.hit && Near(sample.pick.world_point.z,0) &&
          sample.support.hit_count==25 && sample.support.support_count==24 &&
          sample.support.splat_count>1 && Near(sample.support.depth_spread,0),
          "patch support rejects an isolated nearer floater while single click keeps frontmost semantics");

    auto split_plane=plane;
    for(auto& point:split_plane) point.position.z=point.position.x<=0 ? 1 : -1;
    desc.objects[0].splats=Scene3DSplats::Create(split_plane,error);
    auto discontinuous=Scene3DSnapshot::Create(desc,error);
    Check(bool(discontinuous),"Gaussian two-wall fixture is valid");
    if(!discontinuous) return;
    sample=discontinuous->SampleSplatSurface(centre,20,viewport,camera,object.id);
    Check(sample.pick.hit && (Near(sample.pick.world_point.z,1) || Near(sample.pick.world_point.z,-1)) &&
          sample.support.support_count<sample.support.hit_count && Near(sample.support.depth_spread,0),
          "a patch crossing a depth discontinuity never averages its endpoint between the walls");

    Scene3DSplat origin; origin.covariance[0]=origin.covariance[3]=origin.covariance[5]=.001;
    desc.objects={object}; desc.objects[0].splats=Scene3DSplats::Create({origin},error);
    desc.objects[0].parent_id="surface:group";
    desc.objects[0].local=Scene3DMatrix::Translation({.5,.2,1})*Scene3DMatrix::Scale({2,3,.5});
    auto parent=Object("surface:group",{}); parent.local=Scene3DMatrix::RotationY(.3);
    const auto world=parent.local*desc.objects[0].local;
    desc.objects.push_back(parent);
    auto transformed=Scene3DSnapshot::Create(desc,error);
    Check(bool(transformed),"Gaussian transformed sampling fixture is valid");
    if(!transformed) return;
    const auto expected=world.TransformPoint({0,0,0});
    camera.roll=.2; camera.gate_aspect=1.4;
    for(bool orthographic:{true,false}) {
        camera.orthographic=orthographic;
        Pointf screen; double depth=0;
        Check(camera.Project(expected,viewport,screen,depth),"transformed fixture projects into both cameras");
        sample=transformed->SampleSplatSurface(screen,0,viewport,camera,object.id);
        Check(sample.pick.hit && Near(sample.pick.world_point,expected),
              "target sampling respects parent rotation nonuniform scale film gate and camera roll");
    }
    camera.orthographic=true;
    desc.objects={object}; desc.objects[0].selectable=false;
    auto disabled=Scene3DSnapshot::Create(desc,error);
    Check(disabled && !disabled->SampleSplatSurface(centre,20,viewport,camera,object.id).pick.hit,
          "surface sampling respects target selectability");
    desc.objects[0]=object; desc.objects[0].visible=false;
    disabled=Scene3DSnapshot::Create(desc,error);
    Check(disabled && !disabled->SampleSplatSurface(centre,20,viewport,camera,object.id).pick.hit,
          "surface sampling respects target visibility");
    desc.objects[0]=object; desc.objects[0].opacity=0;
    disabled=Scene3DSnapshot::Create(desc,error);
    Check(disabled && !disabled->SampleSplatSurface(centre,20,viewport,camera,object.id).pick.hit,
          "surface sampling respects transparent targets");
    for(double radius:{-1.0,std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        sample=scene->SampleSplatSurface(centre,radius,viewport,camera,object.id);
        Check(!sample.pick.hit && sample.pick.scene_revision==43 && sample.pick.camera_revision==87 &&
              sample.support.support_count==0,"invalid patch radii return revision-bearing misses");
    }
    sample=scene->SampleSplatSurface(centre,1e9,viewport,camera,object.id);
    Check(Near(sample.support.radius_pixels,128) && sample.support.sample_count<=25,
          "large requested patch radii and probe work are bounded");
    sample=scene->SampleSplatSurface(Pointf(1,1),20,viewport,camera,object.id);
    Check(sample.support.sample_count>0 && sample.support.sample_count<25,
          "patch probes outside the viewport are excluded from evidence counts");
    Check(!scene->SampleSplatSurface(centre,20,Size(0,0),camera,object.id).pick.hit &&
          !scene->SampleSplatSurface(Pointf(-1,50),20,viewport,camera,object.id).pick.hit,
          "invalid viewport and out-of-view centres do not sample hidden rays");
}

void CompactGaussianRecordTests()
{
    Scene3DGaussianParameters input;
    input.center = {-.17365512, .25, .1345678431};
    input.log_scale = {-100, -2.1234, 20};
    input.rotation_wxyz = {.378228194, -.567342291, .189114097, -.709177864};
    input.dc = {.25, -.5, .000001};
    input.opacity = .76;
    Scene3DGaussianRecord record;
    Check(EncodeScene3DGaussian(input, record), "valid compact Gaussian encodes");
    Scene3DGaussianParameters output;
    Check(DecodeScene3DGaussian(record, output), "valid compact Gaussian decodes");
    for(int i = 0; i < 3; ++i) {
        Check(Near(input.center[i], output.center[i], 1.5e-8), "centre preserves float32 precision");
        Check(Near(input.log_scale[i], output.log_scale[i], .000917), "log-scale error stays under one half-step");
        Check(Near(input.dc[i], output.dc[i], 3e-8), "raw DC and binary16 subnormals survive");
    }
    double dot = 0, norm2 = 0;
    for(int i = 0; i < 4; ++i) {
        dot += input.rotation_wxyz[i] * output.rotation_wxyz[i];
        norm2 += input.rotation_wxyz[i] * input.rotation_wxyz[i];
        input.rotation_wxyz[i] = -input.rotation_wxyz[i];
    }
    Check(std::abs(dot) / std::sqrt(norm2) > std::cos(0.01 * 3.141592653589793 / 360.),
          "compact quaternion angular error stays below 0.01 degree");
    Check(Near(input.opacity, output.opacity, .00000763), "opacity error stays under one half-step");
    Scene3DGaussianRecord antipodal;
    Check(EncodeScene3DGaussian(input, antipodal) && antipodal.words == record.words,
          "antipodal input yields identical device words");
    const auto old = output;
    auto damaged = record;
    damaged.words[4] |= 0x8000;
    Check(!DecodeScene3DGaussian(damaged, output) && output.center == old.center && output.dc == old.dc,
          "unknown reserved bits do not publish corrupt data");
    input.dc[1] = 70000;
    Check(!EncodeScene3DGaussian(input, antipodal) && antipodal.words == record.words,
          "wide DC values are rejected without clamping or modifying previous output");

    Scene3DGaussianParameters axes;
    axes.center = {1, 0, -1};
    axes.log_scale = {-100, 20, -100};
    axes.dc = {.25, -.5, 0};
    axes.opacity = 1.;
    Check(EncodeScene3DGaussian(axes, record) &&
          record.words == std::array<uint32, 8>{0x3f800000, 0, 0xbf800000, 0xdfffbfff,
                                                0x00000fff, 0x0000ffff, 0x3400ffff, 0x0000b800},
          "device ABI has the fixed IEEE centre, half, quaternion and scale words");
}

}

CONSOLE_APP_MAIN
{
	MatrixTests(); MeshTests(); SceneTests(); CameraTests(); CameraAxisTests(); CameraProjectionTests(); PickTests(); MaterialCameraTests(); MaterialPickTests();
	CompactGaussianRecordTests();
	GaussianResourceTests(); GaussianProjectionTests(); GaussianScenePickTests(); GaussianSurfaceSampleTests();
	Cout() << "RenderSceneTest: " << checks << " checks, " << failures << " failures" << EOL;
	if(!checks || failures) SetExitCode(1);
}
