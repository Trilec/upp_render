#include <Ui/UiTextLayout.h>
#include <RenderCtrlBridge/RenderCtrlBridge.h>
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <cmath>
using namespace Upp;
namespace {
FARPROC WINAPI Resolver(HMODULE,LPCSTR) { return reinterpret_cast<FARPROC>(1); }
struct StyledProbe : Ctrl {
 UiTextLayout layout; bool uniform; Color ink; bool lines=false;
 StyledProbe(const UiTextLayout& value,bool uniform,Color ink):layout(value),uniform(uniform),ink(ink) { SetRect(0,0,400,100); }
 void Paint(Draw& draw) override {
  if(lines) for(int i=0;i<layout.GetLineCount();++i) layout.DrawLine(draw,i,Pointf(8,8+layout.GetLine(i).bounds.top),ink,uniform);
  else layout.Draw(draw,Pointf(8,8),ink,uniform);
 }
};
}
bool TestStyledText()
{
 bool ok=true; int checks=0;
 auto check=[&](bool pass,const char *why) { ++checks; if(!pass) { ok=false; Cout()<<"FAIL styled "<<why<<EOL; } };
 Vector<UiTextStyleRange> default_style; auto& inherited=default_style.Add();
 inherited.from=0; inherited.length=3; inherited.font=StdFont();
 check(UiTextLayout::CreateStyled(WString("std"),StdFont(),default_style).IsValid(),"realize system default span font before bounds validation");
 Font base=SansSerif(24);
 WString text("Styled ");
 int rtl=text.GetCount(); for(int cp:{0x633,0x644,0x627,0x645}) text.Cat(cp);
 text.Cat(' '); int object_position=text.GetCount(); text.Cat(0xfffc); text.Cat(" tail");
 Vector<UiTextStyleRange> styles;
 auto& a=styles.Add(); a.from=0; a.length=7; a.font=Font(base).Bold().Underline(); a.ink=Red();
 auto& b=styles.Add(); b.from=rtl; b.length=4; b.ink=Blue();
 Vector<UiTextInlineObject> objects;
 auto& object=objects.Add(); object.position=object_position; object.size=Sizef(32,38); object.baseline=30;
 auto layout=UiTextLayout::CreateStyled(text,base,styles,objects);
 check(layout.IsValid(),"mixed styled paragraph builds");
 if(!layout.IsValid()) { Cout()<<layout.GetError()<<EOL; return false; }
 check(layout.GetLineCount()==1 && layout.GetLine(0).from==0 && layout.GetLine(0).to==text.GetCount(),"line scalar coverage");
 check(layout.GetLine(0).bounds.Height()>=38 && layout.GetLine(0).baseline>=30,"inline metrics affect line height and baseline");
 check(layout.GetInlineCount()==1 && layout.GetInline(0).position==object_position &&
       fabs(layout.GetInline(0).bounds.Width()-32)<.01 && fabs(layout.GetInline(0).bounds.Height()-38)<.01,"native inline placement");
 const auto& rect=layout.GetInline(0).bounds;
 auto hit=layout.HitTest(Pointf(rect.left+1,(rect.top+rect.bottom)/2));
 check(hit.position==object_position,"inline hit uses same native layout");
 bool red=false,blue=false;
 for(const auto& run:layout.GetGpuText()->runs) { red|=run.ink==Red(); blue|=run.ink==Blue(); }
 check(red && blue && !layout.GetGpuText()->decorations.IsEmpty(),"resolved run and decoration inks");
 auto cached=UiTextLayout::CreateStyled(text,base,styles,objects);
 check(cached.GetGpuText()==layout.GetGpuText(),"styled cache identity");
 styles[0].ink=Green();
 check(UiTextLayout::CreateStyled(text,base,styles,objects).GetGpuText()!=layout.GetGpuText(),"cache separates inks");
 styles[0].ink=Red();
 UiTextLayoutOptions wrap; wrap.wrap=true; wrap.width=90;
 auto wrapped=UiTextLayout::CreateStyled(text,base,styles,objects,wrap);
 check(wrapped.IsValid() && wrapped.GetLineCount()>1,"styled native wrapping");
 int end=0; double bottom=0;
 for(int i=0;i<wrapped.GetLineCount();++i) {
  auto line=wrapped.GetLine(i);
  check(line.from==end && line.to>=line.from && line.bounds.top>=bottom-.01,"wrapped line continuity");
  end=line.to; bottom=line.bounds.bottom;
 }
 check(end==text.GetCount(),"wrapped lines cover all scalars");
 int line_glyphs=0;
 for(int i=0;i<wrapped.GetLineCount();++i) {
  auto snapshot=wrapped.GetLineGpuText(i);
  check(snapshot && snapshot->IsValid(),"valid retained native line snapshot");
  if(snapshot) line_glyphs+=snapshot->GetGlyphCount();
 }
 check(line_glyphs==wrapped.GetGlyphCount(),"line slices preserve complete glyph inventory");
 Vector<UiTextStyleRange> tracked;
 auto& tracking=tracked.Add(); tracking.from=0; tracking.length=4; tracking.tracking=2;
 auto plain=UiTextLayout::Create(WString("abcd"),base);
 auto spaced=UiTextLayout::CreateStyled(WString("abcd"),base,tracked);
 check(spaced.IsValid() && spaced.GetSize().cx>plain.GetSize().cx+7,"native tracking changes measurement");
 Vector<UiTextStyleRange> invalid; auto& bad=invalid.Add(); bad.from=-1; bad.length=1;
 check(!UiTextLayout::CreateStyled(text,base,invalid).IsValid(),"negative style range rejected");
 bad.from=0; bad.length=5; auto& overlap=invalid.Add(); overlap.from=4; overlap.length=2;
 check(!UiTextLayout::CreateStyled(text,base,invalid).IsValid(),"overlapping style ranges rejected");
 objects[0].position=0;
 check(!UiTextLayout::CreateStyled(text,base,styles,objects).IsValid(),"inline requires placeholder scalar");
 objects[0].position=object_position;
 UiDisplayList normal,uniform; String error; CtrlDisplayListRecordReport report;
 StyledProbe normal_control(layout,false,White()),uniform_control(layout,true,Green());
 check(RecordCtrlDisplayList(normal_control,normal,error,&report),"styled display recording");
 check(RecordCtrlDisplayList(uniform_control,uniform,error,&report),"uniform selection display recording");
 check(normal.Dump()!=uniform.Dump() && uniform.GetCount()>0,"ink override participates in display identity");
 HWND hwnd=CreateWindowExW(0,L"STATIC",L"Styled text offscreen",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 VulkanSurfaceSession session; GpuNativeWindowDesc native; native.kind=GpuNativeWindowKind::Win32; native.handle=(uintptr_t)hwnd;
 bool opened=hwnd && session.Open(true,native,&Resolver); check(opened,"Vulkan opens");
 if(opened) {
  VulkanGpuDevice device(session);
  GpuTextureDesc desc; desc.size=Size(400,100); desc.format=GpuFormat::RGBA8; desc.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_TransferSrc;
  GpuTextureId target; bool created=device.CreateTexture(desc,target)==GpuResult::Ok; check(created,"GPU target");
  if(created) {
   {
    UiRenderer2D renderer(device); UiRenderer2DTarget output; output.color_target=target; output.size=desc.size; output.color_format=desc.format; output.clear_color.alpha=1;
    Vector<byte> pixels; bool read=renderer.Render(normal,output) && device.ReadTexturePixels(target,pixels);
    check(read,"styled actual Vulkan pixels");
    if(read) {
     int red_pixels=0,blue_pixels=0;
     for(int i=0;i<pixels.GetCount();i+=4) { red_pixels+=pixels[i]>50 && pixels[i+1]<5 && pixels[i+2]<5; blue_pixels+=pixels[i+2]>50 && pixels[i]<5 && pixels[i+1]<5; }
     check(red_pixels>50 && blue_pixels>20,"Vulkan preserves per-run authored colour");
    }
    read=renderer.Render(uniform,output) && device.ReadTexturePixels(target,pixels); check(read,"uniform actual Vulkan pixels");
    if(read) {
     int green_pixels=0,other_pixels=0;
     for(int i=0;i<pixels.GetCount();i+=4) { green_pixels+=pixels[i+1]>30; other_pixels+=pixels[i]>5 || pixels[i+2]>5; }
     check(green_pixels>100 && other_pixels==0,"selection override recolours all glyphs and decorations");
    }
    UiDisplayList wrapped_whole,wrapped_lines;
    StyledProbe whole_control(wrapped,false,White()),line_control(wrapped,false,White()); line_control.lines=true;
    check(RecordCtrlDisplayList(whole_control,wrapped_whole,error,&report) && RecordCtrlDisplayList(line_control,wrapped_lines,error,&report),"whole and line recording");
    Vector<byte> expected,actual;
    check(renderer.Render(wrapped_whole,output) && device.ReadTexturePixels(target,expected) &&
          renderer.Render(wrapped_lines,output) && device.ReadTexturePixels(target,actual) &&
          actual==expected,"cached line slices have identical actual Vulkan pixels");
    check(renderer.GetStats().cpu_glyph_raster_count==0,"styled coverage stays on GPU");
   }
   device.DestroyTexture(target);
  }
  check(device.GetLiveBufferCount()==0 && device.GetLiveTextureCount()==0 && device.GetLiveShaderCount()==0 &&
        device.GetLivePipelineCount()==0 && device.GetLiveCommandCount()==0,"styled GPU resources released");
 }
 session.Close(); if(hwnd) DestroyWindow(hwnd);
 check(session.GetReport().validation_error_count==0 && session.GetReport().validation_warning_count==0,"zero styled validation messages");
 Cout()<<"styled_text_checks="<<checks<<" passed="<<(ok?1:0)<<EOL; return ok;
}
