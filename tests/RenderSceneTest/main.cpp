#include <RenderScene/Scene3D.h>
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
	Check(!Scene3DMesh::Create(vertices,indices,error),"unsupported transparent vertex rejected");
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
	Check(!Scene3DSnapshot::Create(desc,error),"unsupported transparent material rejected");
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
	Check(camera.Orbit(0.1,100) && camera.revision==revision+1 && camera.Validate(error),"orbit clamps away from poles and increments revision");
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
}

CONSOLE_APP_MAIN
{
	MatrixTests(); MeshTests(); SceneTests(); CameraTests(); PickTests();
	Cout() << "RenderSceneTest: " << checks << " checks, " << failures << " failures" << EOL;
	if(!checks || failures) SetExitCode(1);
}
