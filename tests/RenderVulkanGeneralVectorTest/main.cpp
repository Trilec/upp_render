#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <RenderSoftware/RenderSoftware.h>
#include <cmath>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
using namespace Upp;
static bool Check(bool b,const char *s) { if(!b) Cout()<<"FAIL: "<<s<<EOL; return b; }
static FARPROC WINAPI Resolver(HMODULE,LPCSTR) { return reinterpret_cast<FARPROC>(1); }
static UiPath Star() {
	UiPath p; for(int i=0;i<10;++i) {
		double a=i*M_PI/5-M_PI/2,r=i&1?14:34; Pointf q(44+r*cos(a),44+r*sin(a));
		if(i) p.LineTo(q); else p.MoveTo(q);
	} return p.Close();
}
static UiPath Ring(bool reverse) {
	UiPath p; p.MoveTo(Pointf(8.25,9.5)).LineTo(Pointf(78.25,9.5)).LineTo(Pointf(78.25,77.5)).LineTo(Pointf(8.25,77.5)).Close();
	p.MoveTo(Pointf(28.25,29.5));
	if(reverse) p.LineTo(Pointf(28.25,57.5)).LineTo(Pointf(58.25,57.5)).LineTo(Pointf(58.25,29.5));
	else p.LineTo(Pointf(58.25,29.5)).LineTo(Pointf(58.25,57.5)).LineTo(Pointf(28.25,57.5));
	return p.Close();
}
static double Linear(byte c) { double x=c/255.0; return x<=.04045?x/12.92:pow((x+.055)/1.055,2.4); }
static int Encoded(double x) { return (int)round((x<=.0031308?12.92*x:1.055*pow(x,1/2.4)-.055)*255); }
static bool Run(VulkanGpuDevice& device) {
	bool ok=true; const Size size(96,96);
	for(GpuFormat format:{GpuFormat::RGBA8,GpuFormat::BGRA8,GpuFormat::RGBA8Srgb,GpuFormat::BGRA8Srgb}) {
		GpuTextureDesc d; d.size=size; d.format=format; d.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_TransferSrc;
		GpuTextureId target; ok &= Check(device.CreateTexture(d,target)==GpuResult::Ok,"target creates");
		{
			UiRenderer2D renderer(device); UiRenderer2DTarget output; output.color_target=target; output.size=size; output.color_format=format; output.clear_color.alpha=0;
			const bool srgb=format==GpuFormat::RGBA8Srgb||format==GpuFormat::BGRA8Srgb;
			const bool bgra=format==GpuFormat::BGRA8||format==GpuFormat::BGRA8Srgb;
			int cases=0; double worst_mean=0, worst_max=0;
			auto pixels=[&](const UiDisplayList& list,bool cold) {
				if(!renderer.Render(list,output)) { Cout()<<renderer.GetError()<<EOL; return Check(false,"GPU general vector renders"); }
				const auto& stats=renderer.GetStats();
				bool pass=Check(stats.vector_raster_count==0 && stats.texture_upload_count==0 && stats.gpu_vector_draw_count>0,"coverage uses no CPU raster/pixel upload");
				if(cold) pass &= Check(stats.gpu_vector_raster_count>0,"cold GPU coverage counted");
				Vector<byte> bytes; pass &= Check(device.ReadTexturePixels(target,bytes) && bytes.GetCount()==96*96*4,"pixels read back");
				return pass;
			};
			for(int kind=0;kind<5;++kind) {
				UiDisplayListBuilder b; UiPath path=kind==0?Star():Ring(kind==1);
				if(kind==4) {
					path.Clear(); path.MoveTo(Pointf(8,12)).CubicTo(Pointf(92,2),Pointf(-12,93),Pointf(82,70))
					.QuadraticTo(Pointf(45,98),Pointf(8,12)).Close();
				}
				b.FillPath(path,UiPaint::Solid(Rgba8(70,160,230,180)),kind==2?UiFillRule::EvenOdd:UiFillRule::NonZero);
				UiDisplayList list; b.Finish(list); ok &= pixels(list,true);
				Vector<byte> bytes; device.ReadTexturePixels(target,bytes);
				ImagePainter reference(size); reference.Clear(RGBAZero()); SoftwareUiRenderer software;
				ok &= Check(software.Replay(list,reference),"software reference replays"); Image image=reference.GetResult();
				double mean=0,maximum=0;
				for(int y=0;y<96;++y) for(int x=0;x<96;++x) {
					const byte *v=bytes.Begin()+4*(y*96+x); double coverage=image[y][x].a/180.0;
					for(int c=0;c<3;++c) {
						int colour=c==0?70:c==1?160:230;
						int wanted=srgb?Encoded(Linear(colour)*(180/255.0)*coverage):(int)round(colour*(180/255.0)*coverage);
						int actual=v[c==0&&bgra?2:c==2&&bgra?0:c]; double delta=abs(actual-wanted);
						mean+=delta/(96*96*3); maximum=max(maximum,delta);
					}
				}
				worst_mean=max(worst_mean,mean); worst_max=max(worst_max,maximum);
				ok &= Check(mean<1.0 && maximum<80,"compound/concave pixels agree with independent raster reference");
				ok &= pixels(list,false);
				ok &= Check(renderer.GetStats().gpu_vector_raster_count==0,"warm GPU coverage cache");
				++cases;
			}
			for(int cap=0;cap<3;++cap) for(int join=0;join<3;++join) for(int dash=0;dash<2;++dash) {
				UiPath path; path.MoveTo(Pointf(11,64)).LineTo(Pointf(25,18)).CubicTo(Pointf(80,3),Pointf(6,85),Pointf(78,68));
				UiStrokeStyle stroke; stroke.width=3.5; stroke.cap=(UiLineCap)cap; stroke.join=(UiLineJoin)join;
				if(dash) { stroke.dash<<5.0<<2.0<<3.0; stroke.dash_offset=1.25; }
				UiDisplayListBuilder b; Transform2D t; t.x=Pointf(.9,.1); t.y=Pointf(-.06,1.03); t.t=Pointf(2.375,.125);
				b.ConcatTransform(t); b.StrokePath(path,UiPaint::Solid(Rgba8(255,255,255)),stroke);
				UiDisplayList list; b.Finish(list); ok &= pixels(list,true);
				Vector<byte> bytes; device.ReadTexturePixels(target,bytes);
				ImagePainter reference(size); reference.Clear(RGBAZero()); SoftwareUiRenderer software; software.Replay(list,reference); Image image=reference.GetResult();
				double mean=0, maximum=0;
				for(int y=0;y<96;++y) for(int x=0;x<96;++x) {
					int wanted=srgb?Encoded(image[y][x].a/255.0):image[y][x].a;
					double delta=abs((int)bytes[4*(y*96+x)]-wanted); mean+=delta/(96*96); maximum=max(maximum,delta);
				}
				worst_mean=max(worst_mean,mean); worst_max=max(worst_max,maximum);
				if(maximum>=100) Cout()<<"stroke cap="<<cap<<" join="<<join<<" dash="<<dash<<" mean="<<mean<<" max="<<maximum<<EOL;
				// Overlapping square caps expose the legacy software area accumulator;
				// exact union coverage is independently checked below.
				ok &= Check(mean<2.0 && maximum<=128,"curved stroke coverage stays within declared legacy AA difference");
				++cases;
			}
			{
				// All square-capped dashes overlap: their exact union is one rectangle.
				UiPath line; line.MoveTo(Pointf(16.125,32.375)).LineTo(Pointf(46.125,32.375)).LineTo(Pointf(76.125,32.375));
				UiStrokeStyle stroke; stroke.width=3.5; stroke.cap=UiLineCap::Square; stroke.dash<<5.0<<2.0; stroke.dash_offset=1.25;
				UiDisplayListBuilder b; b.StrokePath(line,UiPaint::Solid(Rgba8(255,255,255)),stroke); UiDisplayList list; b.Finish(list);
				ok &= pixels(list,true); Vector<byte> bytes; device.ReadTexturePixels(target,bytes);
				int maximum=0;
				for(int y=0;y<96;++y) for(int x=0;x<96;++x) {
					double dx=max(0.0,min(x+1.0,77.625)-max((double)x,14.375));
					double dy=max(0.0,min(y+1.0,34.125)-max((double)y,30.625));
					int expected=srgb?Encoded(dx*dy):(int)round(dx*dy*255);
					maximum=max(maximum,abs((int)bytes[4*(y*96+x)]-expected));
				}
				Cout()<<"square_dash_union_max_error="<<maximum<<EOL;
				ok &= Check(maximum<=3,"overlapping dash caps have exact analytic union coverage");
				++cases;
			}
			for(int kind=0;kind<2;++kind) for(int spread=0;spread<3;++spread) {
				UiPaint paint=kind?UiPaint::Radial(Pointf(42,44),Pointf(44,44),22,Rgba8(30,170,240,230),Rgba8(220,70,40,180),(UiGradientSpread)spread):
				                  UiPaint::Linear(Pointf(15,15),Pointf(60,60),Rgba8(30,170,240,230),Rgba8(220,70,40,180),(UiGradientSpread)spread);
				paint.AddStop(.4,Rgba8(80,220,120,200)); UiDisplayListBuilder b; b.FillPath(Star(),paint); UiDisplayList list; b.Finish(list);
				ok &= pixels(list,true); ok &= pixels(list,false);
				ok &= Check(renderer.GetStats().gpu_vector_raster_count==0,"gradient cache reused");
				// Fully covered pixels isolate gradient numerics after a sheared,
				// nonuniform transform, including spread, alpha and sRGB.
				Transform2D t; t.x=Pointf(.9,.15); t.y=Pointf(-.1,1); t.t=Pointf(3.375,2.125);
				UiDisplayListBuilder transformed; transformed.ClipRect(Rectf(19,21,70,71));
				transformed.ConcatTransform(t); transformed.FillPath(Ring(false),paint);
				UiDisplayList probe; transformed.Finish(probe); ok &= pixels(probe,true);
				Vector<byte> values; device.ReadTexturePixels(target,values); int maximum=0;
				for(int y=33;y<55;y+=3) for(int x=32;x<59;x+=3) {
					double dx=x+.5-t.t.x,dy=y+.5-t.t.y,det=t.x.x*t.y.y-t.x.y*t.y.x;
					Pointf q((t.y.y*dx-t.x.y*dy)/det,(-t.y.x*dx+t.x.x*dy)/det);
					double ratio;
					if(!kind) ratio=((q.x-15)+(q.y-15))/90;
					else {
						Pointf ray=q-Pointf(42,44),offset=Pointf(-2,0);
						double a=ray.x*ray.x+ray.y*ray.y,b=2*(offset.x*ray.x+offset.y*ray.y),c=4-22*22;
						double root=(-b+sqrt(b*b-4*a*c))/(2*a); ratio=1/root;
					}
					if(spread==1) ratio-=floor(ratio);
					else if(spread==2) { ratio=fmod(ratio,2); if(ratio<0) ratio+=2; ratio=1-fabs(ratio-1); }
					else ratio=minmax(ratio,0.0,1.0);
					Rgba8 a=ratio<=.4?Rgba8(30,170,240,230):Rgba8(80,220,120,200);
					Rgba8 z=ratio<=.4?Rgba8(80,220,120,200):Rgba8(220,70,40,180);
					double u=ratio<=.4?ratio/.4:(ratio-.4)/.6;
					double alpha=(a.a*(1-u)+z.a*u)/255.0;
					for(int channel=0;channel<4;++channel) {
						double aa=channel==0?a.r:channel==1?a.g:channel==2?a.b:a.a;
						double zz=channel==0?z.r:channel==1?z.g:channel==2?z.b:z.a;
						double colour=(aa*(1-u)+zz*u)/255.0;
						int expected;
						if(channel==3) expected=(int)round(alpha*255);
						else if(srgb) {
							double linear=colour<=.04045?colour/12.92:pow((colour+.055)/1.055,2.4);
							expected=Encoded(linear*alpha);
						} else expected=(int)round(colour*alpha*255);
						int actual=values[4*(y*96+x)+(channel==0&&bgra?2:channel==2&&bgra?0:channel)];
						maximum=max(maximum,abs(actual-expected));
					}
				}
				ok &= Check(maximum<=2,"transformed gradient colour/alpha match analytic reference");
				int outside=0;
				for(int y=0;y<96;++y) for(int x=0;x<96;++x)
					if(x<19||x>=70||y<21||y>=71) outside+=values[4*(y*96+x)+3]!=0;
				ok &= Check(!outside,"GPU gradient respects clipping");
				UiDisplayListBuilder moved; moved.ClipRect(Rectf(23,18,74,68));
				t.t+=Pointf(4,-3); moved.ConcatTransform(t); moved.FillPath(Ring(false),paint);
				UiDisplayList shifted; moved.Finish(shifted); ok &= pixels(shifted,false);
				ok &= Check(renderer.GetStats().gpu_vector_raster_count==0,"integer translation reuses GPU coverage");
				Vector<byte> shifted_values; device.ReadTexturePixels(target,shifted_values);
				bool equal=true;
				for(int y=21;y<71;++y) for(int x=19;x<70;++x) for(int c=0;c<4;++c)
					equal &= values[4*(y*96+x)+c]==shifted_values[4*((y-3)*96+x+4)+c];
				ok &= Check(equal,"cached translated GPU coverage preserves exact pixel phase");
				++cases;
			}
			Cout()<<"format="<<(int)format<<" cases="<<cases<<" reference_mean_max="<<worst_mean<<" reference_pixel_max="<<worst_max<<EOL;
			{
				UiPath huge; huge.MoveTo(Pointf(0,0)).CubicTo(Pointf(6000,0),Pointf(6000,40),Pointf(0,40)).Close();
				UiDisplayListBuilder guard; guard.FillPath(huge,UiPaint::Linear(Pointf(0,0),Pointf(80,80),Rgba8(0,0,0),Rgba8(255,255,255)));
				UiDisplayList invalid; guard.Finish(invalid);
				ok &= Check(!renderer.Render(invalid,output) && !renderer.GetError().IsEmpty(),"oversized coverage fails before allocation");
				UiStrokeStyle tiny; tiny.width=2; tiny.dash<<.0001<<.0001;
				UiDisplayListBuilder dashed; dashed.StrokePath(Star(),UiPaint::Solid(Rgba8(255,255,255)),tiny); dashed.Finish(invalid);
				ok &= Check(!renderer.Render(invalid,output) && !renderer.GetError().IsEmpty(),"excessive dash expansion fails explicitly");
				UiPaint too_many=UiPaint::Linear(Pointf(0,0),Pointf(80,80),Rgba8(0,0,0),Rgba8(255,255,255));
				for(int i=1;i<32;++i) too_many.AddStop(i/32.0,Rgba8(i,i,i));
				UiDisplayListBuilder stops; stops.FillPath(Star(),too_many); stops.Finish(invalid);
				ok &= Check(!renderer.Render(invalid,output) && !renderer.GetError().IsEmpty(),"gradient storage budget fails explicitly");
			}
			UiDisplayListBuilder b; b.FillPath(Ring(true),UiPaint::Solid(Rgba8(40,50,60))); UiDisplayList list; b.Finish(list);
			UiRenderer2DCacheLimits limits=renderer.GetCacheLimits(); limits.vector_bytes=1; renderer.SetCacheLimits(limits);
			ok &= Check(!renderer.Render(list,output) && !renderer.GetError().IsEmpty(),"coverage budget fails explicitly");
		}
		ok &= Check(device.DestroyTexture(target)==GpuResult::Ok,"target retires");
	}
	return ok;
}
CONSOLE_APP_MAIN {
	bool ok=true; VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	HWND hwnd=CreateWindowExW(0,L"STATIC",L"GPU vector qualification",WS_POPUP,0,0,96,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
	VulkanSurfaceSession session; GpuNativeWindowDesc w; w.kind=GpuNativeWindowKind::Win32; w.handle=(uintptr_t)hwnd;
	ok &= Check(hwnd && session.Open(true,w,&Resolver),"validated Vulkan session opens");
	if(session.IsReady()) {
		{ VulkanGpuDevice device(session); ok &= Run(device);
		  ok &= Check(!device.GetLiveTextureCount()&&!device.GetLiveBufferCount()&&!device.GetLiveShaderCount()&&!device.GetLivePipelineCount()&&!device.GetLiveCommandCount(),"all resources retire"); }
		session.Close(); ok &= Check(!session.GetReport().validation_warning_count&&!session.GetReport().validation_error_count,"zero validation diagnostics");
	}
	session.Close(); if(hwnd) DestroyWindow(hwnd);
	auto diag=VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(!diag.runtime_live_count&&!diag.device_live_count&&!diag.surface_live_count&&!diag.swapchain_live_count,"zero native ownership");
	if(ok) Cout()<<"RenderVulkanGeneralVectorTest passed"<<EOL; else SetExitCode(1);
}
