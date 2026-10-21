#include "GpuVectorRasterizer.h"
#include "../../shaders/VectorCoverageSpirv.h"
#include <Painter/Painter.h>
#include <cmath>
#include <cstring>

namespace Upp {
namespace {
constexpr int MAX_EDGES = 8192, MAX_POINTS = 4096;
constexpr double EPS = 1e-10;
bool Finite(Pointf p) { return std::isfinite(p.x) && std::isfinite(p.y) && fabs(p.x)<=1e7 && fabs(p.y)<=1e7; }
double Distance(Pointf p, Pointf a, Pointf b) {
	Pointf d=b-a; double len=d.x*d.x+d.y*d.y;
	Pointf delta=p-a;
	double t=len>EPS ? minmax((delta.x*d.x+delta.y*d.y)/len,0.0,1.0) : 0;
	Pointf q=p-a-d*t; return sqrt(q.x*q.x+q.y*q.y);
}
struct Contour : Moveable<Contour> { Vector<Pointf> points; bool closed=false; };
bool Add(Vector<Pointf>& points, Pointf p, int& count) {
	if(!Finite(p) || count>=MAX_POINTS) return false;
	if(points.IsEmpty() || Distance(p,points.Top(),points.Top())>EPS) { points.Add(p); ++count; }
	return true;
}
bool Cubic(Vector<Pointf>& points, Pointf a, Pointf b, Pointf c, Pointf d,
           const Transform2D& t, int& count, int depth=0) {
	Pointf ta=t.TransformPoint(a), td=t.TransformPoint(d);
	if(max(Distance(t.TransformPoint(b),ta,td),Distance(t.TransformPoint(c),ta,td))<=0.175)
		return Add(points,d,count);
	if(depth>=16) return false;
	Pointf ab=(a+b)*.5, bc=(b+c)*.5, cd=(c+d)*.5, abc=(ab+bc)*.5, bcd=(bc+cd)*.5, m=(abc+bcd)*.5;
	return Cubic(points,a,ab,abc,m,t,count,depth+1) && Cubic(points,m,bcd,cd,d,t,count,depth+1);
}
struct PointCollector : LinearPathConsumer {
	Vector<Pointf>& points; int& count; bool failed=false;
	PointCollector(Vector<Pointf>& points,int& count):points(points),count(count) {}
	void Move(const Pointf& p) override { Line(p); }
	void Line(const Pointf& p) override { if(!failed && !Add(points,p,count)) failed=true; }
};
bool Flatten(const UiPath& path, const Transform2D& t, Vector<Contour>& contours, double stroke_width) {
	if(path.GetCount()>8192) return false;
	int count=0;
	double scale=sqrt((t.x.x*t.x.x+t.x.y*t.x.y+t.y.x*t.y.x+t.y.y*t.y.y)*.5);
	for(int i=0;i<path.GetCount();++i) {
		const auto& c=path[i];
		if(c.verb==UiPathVerb::MoveTo) { if(!Add(contours.Add().points,c.p1,count)) return false; continue; }
		if(contours.IsEmpty() || contours.Top().points.IsEmpty()) return false;
		if(contours.Top().closed && c.verb!=UiPathVerb::Close) {
			Pointf start=contours.Top().points[0];
			if(!Add(contours.Add().points,start,count)) return false;
		}
		auto& line=contours.Top().points; Pointf at=line.Top();
		switch(c.verb) {
		case UiPathVerb::LineTo: if(!Add(line,c.p1,count)) return false; break;
		case UiPathVerb::QuadraticTo:
			if(stroke_width>0) {
				if(!Finite(c.p1)||!Finite(c.p2)) return false;
				PointCollector sink(line,count);
				double tol=0.3/max(scale,EPS)/max(.5*stroke_width,1.0)*min(1.0,2*Distance(at,c.p2,c.p2));
				ApproximateQuadratic(sink,at,c.p1,c.p2,tol);
				if(sink.failed) return false;
				break;
			}
			if(!Finite(c.p1)||!Finite(c.p2)||!Cubic(line,at,at+(c.p1-at)*(2.0/3),c.p2+(c.p1-c.p2)*(2.0/3),c.p2,t,count)) return false;
			break;
		case UiPathVerb::CubicTo:
			if(stroke_width>0) {
				if(!Finite(c.p1)||!Finite(c.p2)||!Finite(c.p3)) return false;
				PointCollector sink(line,count);
				double tol=0.3/max(scale,EPS)/max(.5*stroke_width,1.0)*min(1.0,2*Distance(at,c.p3,c.p3));
				ApproximateCubic(sink,at,c.p1,c.p2,c.p3,tol);
				if(sink.failed) return false;
				break;
			}
			if(!Finite(c.p1)||!Finite(c.p2)||!Finite(c.p3)||!Cubic(line,at,c.p1,c.p2,c.p3,t,count)) return false;
			break;
		case UiPathVerb::Close: contours.Top().closed=true; break;
		default: return false;
		}
	}
	return true;
}
struct Edge : Moveable<Edge> { Pointf a,b; };
struct Collector : LinearPathConsumer {
	Vector<Edge> edges;
	Transform2D transform;
	Pointf at=Pointf(0,0);
	bool failed=false, started=false;
	void Move(const Pointf& p) override { at=transform.TransformPoint(p); started=true; if(!Finite(at)) failed=true; }
	void Line(const Pointf& p) override {
		Pointf q=transform.TransformPoint(p);
		if(!started || !Finite(q) || edges.GetCount()>=MAX_EDGES) { failed=true; return; }
		if(q!=at) { Edge& e=edges.Add(); e.a=at; e.b=q; }
		at=q;
	}
};
int Cap(UiLineCap c) { return c==UiLineCap::Round?LINECAP_ROUND:c==UiLineCap::Square?LINECAP_SQUARE:LINECAP_BUTT; }
int Join(UiLineJoin c) { return c==UiLineJoin::Round?LINEJOIN_ROUND:c==UiLineJoin::Bevel?LINEJOIN_BEVEL:LINEJOIN_MITER; }
}
bool PrepareGpuVectorGeometry(const UiDisplayOp& op, const Transform2D& t, GpuVectorGeometry& out, String& error)
{
	out.edges.Clear(); out.rows.Clear(); out.stops.Clear(); out.bounds=Rect(0,0,0,0); out.parameters={};
	auto fail=[&](const char *s) { error=s; return false; };
	if((op.type!=UiDisplayOpType::FillPath && op.type!=UiDisplayOpType::StrokePath) ||
	   !op.paint.IsValid() || !Finite(t.x) || !Finite(t.y) || !Finite(t.t))
		return fail("GPU vector operation/paint/transform is invalid");
	double determinant=t.x.x*t.y.y-t.x.y*t.y.x;
	if(fabs(determinant)<1e-12) { error.Clear(); return true; }
	Vector<Contour> contours;
	if(!Flatten(op.path,t,contours,op.type==UiDisplayOpType::StrokePath?op.stroke.width:0)) return fail("GPU vector curve/point budget exceeded or malformed path");
	Collector collector; collector.transform=t;
	if(op.type==UiDisplayOpType::FillPath) {
		for(const auto& c:contours) if(c.points.GetCount()>1) {
			collector.Move(c.points[0]); for(int i=1;i<c.points.GetCount();++i) collector.Line(c.points[i]);
			collector.Line(c.points[0]);
		}
	} else {
		if(!op.stroke.IsValid()) return fail("GPU vector stroke is invalid");
		double length=0;
		for(const auto& c:contours) {
			for(int i=1;i<c.points.GetCount();++i) length+=Distance(c.points[i],c.points[i-1],c.points[i-1]);
			if(c.closed && !c.points.IsEmpty()) length+=Distance(c.points.Top(),c.points[0],c.points[0]);
		}
		double minimum=1e300;
		for(double d:op.stroke.dash) if(d>0) minimum=min(minimum,d);
		if(!op.stroke.dash.IsEmpty() && length/minimum>4096) return fail("GPU vector dash work budget exceeded");
		double scale_bound=sqrt(t.x.x*t.x.x+t.x.y*t.x.y+t.y.x*t.y.x+t.y.y*t.y.y);
		if(op.stroke.width*scale_bound>4096) return fail("GPU vector stroke extent budget exceeded");
		double tol=min(0.3/max(scale_bound/M_SQRT2,EPS),op.stroke.width*.1);
		double round_steps=ceil(2*M_PI/acos(1-tol/(op.stroke.width*.5)));
		int point_count=0; for(const auto& c:contours) point_count+=c.points.GetCount();
		double dash_segments=op.stroke.dash.IsEmpty()?0:length/minimum;
		double per_segment=(op.stroke.cap==UiLineCap::Round || op.stroke.join==UiLineJoin::Round)?max(10.0,round_steps):10;
		if((point_count+dash_segments)*per_segment>100000) return fail("GPU vector stroke expansion work budget exceeded");
		Stroker stroker; stroker.target=&collector;
		stroker.Init(op.stroke.width,op.stroke.miter_limit,tol,
		             Cap(op.stroke.cap),Join(op.stroke.join),Null);
		Dasher dasher; dasher.target=&stroker;
		LinearPathConsumer *consumer=&stroker;
		if(!op.stroke.dash.IsEmpty()) { dasher.Init(op.stroke.dash,op.stroke.dash_offset); consumer=&dasher; }
		for(const auto& c:contours) if(!c.points.IsEmpty()) {
			consumer->Move(c.points[0]); for(int i=1;i<c.points.GetCount();++i) consumer->Line(c.points[i]);
			if(c.closed) consumer->Line(c.points[0]);
			consumer->End();
		}
	}
	if(collector.failed) return fail("GPU vector edge/coordinate budget exceeded");
	if(collector.edges.IsEmpty()) { error.Clear(); return true; }
	Rectf bounds(collector.edges[0].a,collector.edges[0].a);
	for(const auto& e:collector.edges) for(Pointf p:{e.a,e.b}) {
		bounds.left=min(bounds.left,p.x); bounds.top=min(bounds.top,p.y);
		bounds.right=max(bounds.right,p.x); bounds.bottom=max(bounds.bottom,p.y);
	}
	out.bounds=Rect((int)floor(bounds.left)-1,(int)floor(bounds.top)-1,(int)ceil(bounds.right)+1,(int)ceil(bounds.bottom)+1);
	Size size=out.bounds.GetSize();
	if(size.cx>4096||size.cy>4096||(int64)size.cx*size.cy*4>32*1024*1024)
		return fail("GPU vector pixel payload/dimension budget exceeded");
	for(const auto& e:collector.edges) {
		Pointf a=e.a-Pointf(out.bounds.TopLeft()), b=e.b-Pointf(out.bounds.TopLeft());
		auto& f=out.edges.Add(); f.x0=(float)a.x; f.y0=(float)a.y; f.x1=(float)b.x; f.y1=(float)b.y;
	}
	int tile_rows=(size.cy+7)/8; out.rows.SetCount(tile_rows*2);
	uint64 work=0;
	for(int row=0;row<tile_rows;++row) {
		int offset=out.rows.GetCount(), count=0;
		for(int i=0;i<out.edges.GetCount();++i) {
			const auto& e=out.edges[i];
			if(max(e.y0,e.y1)>=row*8 && min(e.y0,e.y1)<=min(size.cy,row*8+8)) {
				if(out.rows.GetCount()>=1024*1024) return fail("GPU vector row-index budget exceeded");
				out.rows.Add(i); ++count;
			}
		}
		out.rows[row*2]=offset; out.rows[row*2+1]=count;
		work+=(uint64)count*size.cx*min(8,size.cy-row*8)*64;
		if(work>1024ull*1024*1024) return fail("GPU vector intersection work budget exceeded");
	}
	const auto& paint=op.paint;
	auto stop=[&](double position,Rgba8 c) {
		auto& s=out.stops.Add(); s.rgba[0]=c.r/255.0f; s.rgba[1]=c.g/255.0f;
		s.rgba[2]=c.b/255.0f; s.rgba[3]=c.a/255.0f; s.position=(float)position;
	};
	if(paint.kind==UiPaintKind::Solid) stop(0,paint.color);
	else {
		if(paint.stops.GetCount()>32) return fail("GPU vector gradient stop budget exceeded");
		for(const auto& s:paint.stops) stop(s.position,s.color);
	}
	auto& p=out.parameters; p.width=size.cx; p.height=size.cy;
	p.evenodd=op.type==UiDisplayOpType::FillPath && op.fill_rule==UiFillRule::EvenOdd;
	p.kind=(uint32)paint.kind; p.stop_count=out.stops.GetCount(); p.spread=(uint32)paint.spread;
	const Transform2D gradient = paint.transform * t;
	const double gd = gradient.x.x * gradient.y.y - gradient.x.y * gradient.y.x;
	double xx=gradient.y.y/gd, xy=-gradient.x.y/gd, yx=-gradient.y.x/gd, yy=gradient.x.x/gd;
	Pointf origin=Pointf(out.bounds.TopLeft())-gradient.t;
	p.inverse_x[0]=(float)xx; p.inverse_x[1]=(float)xy; p.inverse_x[2]=(float)(xx*origin.x+xy*origin.y-paint.p0.x); p.inverse_x[3]=(float)paint.radius;
	p.inverse_y[0]=(float)yx; p.inverse_y[1]=(float)yy; p.inverse_y[2]=(float)(yx*origin.x+yy*origin.y-paint.p0.y);
	p.points[2]=(float)(paint.p1.x-paint.p0.x); p.points[3]=(float)(paint.p1.y-paint.p0.y);
	if(paint.kind==UiPaintKind::AngularGradient) { p.points[2]=(float)paint.p1.x; p.points[3]=(float)paint.p1.y; }
	error.Clear(); return true;
}
GpuVectorRasterizer::~GpuVectorRasterizer()
{
	DiscardPending();
	if(pipeline.IsValid()) device.DestroyPipeline(pipeline);
	if(shader.IsValid()) device.DestroyShader(shader);
	for(auto b:buffers) if(b.IsValid()) device.DestroyBuffer(b);
}
bool GpuVectorRasterizer::IsSupported()
{
	if(supported<0) supported=device.GetComputeCapabilities(capabilities)==GpuResult::Ok;
	return supported!=0;
}
bool GpuVectorRasterizer::EnsureProgram(String& error)
{
	if(pipeline.IsValid()) return true;
	if(!IsSupported()) { error="GPU vector compute is unsupported"; return false; }
	GpuShaderDesc desc; desc.stage=GpuShaderStage::Compute; desc.format=GpuShaderFormat::SpirV;
	desc.code=String(reinterpret_cast<const char *>(VECTOR_COVERAGE_SPIRV),sizeof(VECTOR_COVERAGE_SPIRV));
	if(!shader.IsValid() && device.CreateShader(desc,shader)!=GpuResult::Ok) { error=device.GetLastError(); return false; }
	GpuComputePipelineDesc program; program.shader=shader; program.storage_buffer_count=4;
	program.parameter_bytes=sizeof(GpuVectorGeometry::Parameters);
	if(device.CreateComputePipeline(program,pipeline)!=GpuResult::Ok) { error=device.GetLastError(); return false; }
	return true;
}
bool GpuVectorRasterizer::EnsureBuffer(int slot,int64 bytes,String& error)
{
	if(capacities[slot]>=bytes) return true;
	if(buffers[slot].IsValid() && device.DestroyBuffer(buffers[slot])!=GpuResult::Ok) { error=device.GetLastError(); return false; }
	buffers[slot]=GpuBufferId(); capacities[slot]=0;
	int64 capacity=256; while(capacity<bytes) capacity*=2;
	GpuBufferDesc desc; desc.size=capacity; desc.usage=GpuBufferUsage_Storage;
	if(slot==3) { desc.memory=GpuBufferMemory::DeviceLocal; desc.usage|=GpuBufferUsage_TransferSrc; }
	if(device.CreateBuffer(desc,buffers[slot])!=GpuResult::Ok) { error=device.GetLastError(); return false; }
	capacities[slot]=capacity; return true;
}
int64 GpuVectorRasterizer::GetScratchBytes() const { int64 n=0; for(auto b:capacities) n+=b; return n; }
bool GpuVectorRasterizer::Render(const GpuVectorGeometry& g,GpuTextureId texture,Point origin,bool srgb,String& error)
{
	if(!Flush(error)) return false;
	if(g.bounds.IsEmpty()) return true;
	// Font coverage integrates horizontal spans at 32 vertical samples, with
	// a 32 x 32 crossing-overflow fallback. Bound the worst case before dispatch.
	uint64 work=0; uint64 samples=g.parameters.reserved==32?1024:64;
	for(int row=0;row<(g.bounds.Height()+7)/8;++row) {
		work+=(uint64)g.rows[row*2+1]*g.bounds.Width()*min(8,g.bounds.Height()-row*8)*samples;
		if(work>1024ull*1024*1024) { error="GPU vector dispatch work budget exceeded"; return false; }
	}
	if(!EnsureProgram(error)) return false;
	const int64 sizes[]={g.edges.GetCount()*(int64)sizeof(GpuVectorEdge),g.rows.GetCount()*(int64)sizeof(uint32),
	                     g.stops.GetCount()*(int64)sizeof(GpuVectorStop),(int64)g.bounds.Width()*g.bounds.Height()*4};
	const void *data[]={g.edges.Begin(),g.rows.Begin(),g.stops.Begin()};
	for(int i=0;i<4;++i) if(!EnsureBuffer(i,sizes[i],error)) return false;
	for(int i=0;i<3;++i) if(device.WriteBuffer(buffers[i],0,data[i],sizes[i])!=GpuResult::Ok) { error=device.GetLastError(); return false; }
	auto parameters=g.parameters; parameters.srgb=srgb;
	GpuCommandListId cmd; if(device.BeginCommands(cmd)!=GpuResult::Ok) { error=device.GetLastError(); return false; }
	bool ok=device.SetComputePipeline(cmd,pipeline)==GpuResult::Ok;
	for(int i=0;ok&&i<4;++i) ok=device.SetStorageBuffer(cmd,i,buffers[i],0,sizes[i])==GpuResult::Ok;
	GpuTextureWriteDesc write; write.origin=origin; write.size=g.bounds.GetSize(); write.row_pitch=write.size.cx*4;
	if(ok) ok=device.SetComputeParameters(cmd,&parameters,sizeof(parameters))==GpuResult::Ok &&
	          device.Dispatch(cmd,(write.size.cx+7)/8,(write.size.cy+7)/8)==GpuResult::Ok &&
	          device.CopyBufferToTexture(cmd,buffers[3],0,texture,write)==GpuResult::Ok;
	String detail=ok?String():device.GetLastError();
	if(device.EndCommands(cmd)!=GpuResult::Ok || device.Submit(cmd)!=GpuResult::Ok) { error=device.GetLastError(); return false; }
	if(!ok) { error=detail; return false; }
	++submissions;
	error.Clear(); return true;
}
void GpuVectorRasterizer::DiscardPending()
{
	pending.Clear(); for(auto& n:pending_sizes) n=0;
}
bool GpuVectorRasterizer::QueueRender(GpuVectorGeometry&& g,GpuTextureId texture,Point origin,bool srgb,String& error)
{
	if(g.bounds.IsEmpty()) return true;
	if(!IsSupported()) { error="GPU vector compute is unsupported"; return false; }
	uint64 work=0, samples=g.parameters.reserved==32?1024:64;
	for(int row=0;row<(g.bounds.Height()+7)/8;++row) {
		work+=(uint64)g.rows[row*2+1]*g.bounds.Width()*min(8,g.bounds.Height()-row*8)*samples;
		if(work>1024ull*1024*1024) { error="GPU vector dispatch work budget exceeded"; return false; }
	}
	int64 alignment=max<uint64>(1,capabilities.storage_offset_alignment);
	if(alignment>65536) return Render(g,texture,origin,srgb,error);
	if(alignment%4) alignment*=4;
	const int64 sizes[]={g.edges.GetCount()*(int64)sizeof(GpuVectorEdge),g.rows.GetCount()*(int64)sizeof(uint32),
	                     g.stops.GetCount()*(int64)sizeof(GpuVectorStop),(int64)g.bounds.Width()*g.bounds.Height()*4};
	auto aligned=[&](int64 n) { return (n+alignment-1)/alignment*alignment; };
	const int limit=min<int>(128,capabilities.max_dispatches_per_command);
	bool full=pending.GetCount()>=limit;
	int64 cpu=0;
	for(int i=0;i<4;++i) {
		int64 n=aligned(pending_sizes[i])+sizes[i];
		if(i<3) cpu+=n;
		else full|=n>32*1024*1024;
	}
	full|=cpu>2*1024*1024;
	if(full && !Flush(error)) return false;
	if(sizes[0]+sizes[1]+sizes[2]+alignment*3>2*1024*1024)
		return Render(g,texture,origin,srgb,error);
	if(limit<=0) { error="GPU vector dispatch capacity is zero"; return false; }
	Pending& item=pending.Add(); item.geometry=pick(g); item.texture=texture; item.origin=origin; item.srgb=srgb;
	for(int i=0;i<4;++i) {
		item.offsets[i]=aligned(pending_sizes[i]); item.sizes[i]=sizes[i];
		pending_sizes[i]=item.offsets[i]+sizes[i];
	}
	error.Clear(); return true;
}
bool GpuVectorRasterizer::Flush(String& error)
{
	if(pending.IsEmpty()) return true;
	auto fail=[&]() { error=device.GetLastError(); DiscardPending(); return false; };
	if(!EnsureProgram(error)) { DiscardPending(); return false; }
	Vector<byte> staging[3];
	for(int i=0;i<4;++i) {
		if(!EnsureBuffer(i,pending_sizes[i],error)) { DiscardPending(); return false; }
		if(i<3) { staging[i].SetCount((int)pending_sizes[i],0); }
	}
	for(const auto& item:pending) {
		const auto& g=item.geometry; const void *data[]={g.edges.Begin(),g.rows.Begin(),g.stops.Begin()};
		for(int i=0;i<3;++i) std::memcpy(staging[i].Begin()+item.offsets[i],data[i],(size_t)item.sizes[i]);
	}
	for(int i=0;i<3;++i)
		if(device.WriteBuffer(buffers[i],0,staging[i].Begin(),pending_sizes[i])!=GpuResult::Ok) return fail();
	GpuCommandListId cmd;
	if(device.BeginCommands(cmd)!=GpuResult::Ok) return fail();
	bool ok=device.SetComputePipeline(cmd,pipeline)==GpuResult::Ok;
	for(const auto& item:pending) {
		for(int i=0;ok&&i<4;++i) ok=device.SetStorageBuffer(cmd,i,buffers[i],item.offsets[i],item.sizes[i])==GpuResult::Ok;
		auto parameters=item.geometry.parameters; parameters.srgb=item.srgb;
		GpuTextureWriteDesc write; write.origin=item.origin; write.size=item.geometry.bounds.GetSize(); write.row_pitch=write.size.cx*4;
		if(ok) ok=device.SetComputeParameters(cmd,&parameters,sizeof(parameters))==GpuResult::Ok &&
		          device.Dispatch(cmd,(write.size.cx+7)/8,(write.size.cy+7)/8)==GpuResult::Ok &&
		          device.CopyBufferToTexture(cmd,buffers[3],item.offsets[3],item.texture,write)==GpuResult::Ok;
		if(!ok) break;
	}
	String detail=ok?String():device.GetLastError();
	if(device.EndCommands(cmd)!=GpuResult::Ok || device.Submit(cmd)!=GpuResult::Ok) return fail();
	DiscardPending(); ++submissions;
	if(!ok) { error=detail; return false; }
	error.Clear(); return true;
}
}
