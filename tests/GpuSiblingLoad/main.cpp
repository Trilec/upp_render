#include <CtrlLib/CtrlLib.h>
#include <GpuRender/GpuRender.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <chrono>
#include <memory>
using namespace Upp;
namespace {
double Now() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double P(Vector<double>& v, double p) { if(v.IsEmpty()) return -1; Sort(v); return v[min(v.GetCount()-1, (int)ceil(v.GetCount()*p)-1)]; }
class LoadWindow : public TopWindow {
public:
 GpuCtrl light;
 std::unique_ptr<GpuCtrl> heavy;
 Vector<double> baseline, loaded, light_gaps;
 double started=0, due=0, last_light=0, next_recreate=20000;
 int light_frames=0, heavy_frames=0, recreations=0;
 bool ok=true;
 LoadWindow() {
  Title("GpuSiblingLoad"); SetRect(80,80,1000,600);
  light.SetValidation(); Add(light); light.SetRect(0,0,300,300);
  light.SetGpuPaint([=](GpuPainter& p) {
   const double now=Now(); if(last_light && now-started>=12000) light_gaps.Add(now-last_light); last_light=now; ++light_frames;
   p.Clear(Color(18,24,36)); p.FillRect(Rectf(20+(light_frames%100),20,80+(light_frames%100),80),Color(70,150,240));
  });
 }
 void Recreate() {
  if(heavy) { RemoveChild(heavy.get()); heavy.reset(); ++recreations; }
  heavy.reset(new GpuCtrl); heavy->SetValidation(); Add(*heavy); heavy->SetRect(320,0,600,560);
  heavy->SetGpuPaint([=](GpuPainter& p) {
   ++heavy_frames; p.Clear(Color(18,24,36));
   for(int i=0;i<4096;++i) {
    const int x=(i*17+heavy_frames)%570, y=(i*31)%530;
    p.FillRect(Rectf(x,y,x+24,y+24),Color((i*3)%255,(i*7)%255,(i*11)%255));
   }
  });
 }
 void Start() { SaveFile(GetExeDirFile("GpuSiblingLoad.txt"),"test_status=RUNNING\n"); started=Now(); due=started+16; SetTimeCallback(-16,[=]{Tick();}); }
 void Tick() {
  const double now=Now(), duration=now-started;
  if(duration>=4000) { if(duration<12000) baseline.Add(max(0.0,now-due)); else loaded.Add(max(0.0,now-due)); }
  due=now+16;
  if(duration>=12000 && !heavy) Recreate();
  if(duration>=next_recreate) { Recreate(); next_recreate+=6000; }
  light.RequestGpuRefresh(); if(heavy) heavy->RequestGpuRefresh();
  if(duration<60000) return;
  KillTimeCallback();
  const double b99=P(baseline,.99), h99=P(loaded,.99), worst=P(loaded,1);
  ok=light.IsGpuReady() && heavy && heavy->IsGpuReady() && light.GetGpuError().IsEmpty() && heavy->GetGpuError().IsEmpty()
     && light_frames>=50 && heavy_frames>=50 && recreations>=6 && b99>=0 && h99<=50 && worst<=100;
  String report;
  report << "light_primitives=1 heavy_primitives=4096 duration_ms=" << duration << "\n"
         << "baseline_ui_p99_ms=" << b99 << " loaded_ui_p99_ms=" << h99 << " loaded_ui_max_ms=" << worst << "\n"
         << "light_frame_gap_p99_ms=" << P(light_gaps,.99) << " max_ms=" << P(light_gaps,1) << "\n"
         << "light_frames=" << light_frames << " heavy_frames=" << heavy_frames << " heavy_recreations=" << recreations << "\n"
         << "light_error=" << light.GetGpuError() << " heavy_error=" << heavy->GetGpuError() << "\n"
         << "sibling_responsiveness=" << (ok?"PASS":"FAIL") << "\n";
  SaveFile(GetExeDirFile("GpuSiblingLoad.txt"),report); Cout()<<report;
  if(!ok) SetExitCode(1); Close();
 }
};
}
GUI_APP_MAIN {
 { LoadWindow w; w.Start(); w.Run(); }
 const auto d=VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
 const bool zero=!d.runtime_live_count && !d.instance_live_count && !d.device_live_count && !d.surface_live_count && !d.swapchain_live_count && !VulkanGpuDevice::GetSharedImmutableAllocationBytes();
 String path=GetExeDirFile("GpuSiblingLoad.txt"), report=LoadFile(path);
 report<<"final_native_ownership="<<(zero?"ZERO":"NONZERO")<<"\n"; SaveFile(path,report);
 Cout()<<"final_native_ownership="<<(zero?"ZERO":"NONZERO")<<EOL;
 if(!zero || report.Find("sibling_responsiveness=PASS") < 0) SetExitCode(1);
}
