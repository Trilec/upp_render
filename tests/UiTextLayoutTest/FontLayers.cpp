#include <Ui/UiTextLayout.h>
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderSoftware/RenderSoftware.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <plugin/png/png.h>
#include <cmath>
using namespace Upp;
namespace {
struct RectangleFace : GpuGlyphFaceSource {
 String identity="font-layer-rectangle-reference";
 const String& GetIdentity() const override { return identity; }
 bool GetOutline(uint32,double,FontGlyphConsumer& out,String& error) const override {
  out.Move(Pointf(0,-8)); out.Line(Pointf(12,-8)); out.Line(Pointf(12,4)); out.Line(Pointf(0,4)); out.Close();
  error.Clear(); return true;
 }
};
void AddRectangle(GpuShapedText& text,Color ink,byte alpha) {
 auto& run=text.runs.Add(); run.face=std::make_shared<RectangleFace>(); run.ink=ink; run.opacity=alpha;
 auto& glyph=run.glyphs.Add(); glyph.glyph=1; glyph.origin=Pointf(30,240);
}
}
bool TestFontLayers()
{
 bool ok=true; int checks=0;
 auto check=[&](bool pass,const char *why) { ++checks; if(!pass) { ok=false; Cout()<<"FAIL font_layers "<<why<<EOL; } return pass; };
 WString emoji; emoji.Cat(0x1f600); Font font=SansSerif(64).FaceName("Segoe UI Emoji");
 UiTextLayoutOptions outlines; outlines.color_fonts=false;
 auto mono=UiTextLayout::Create(emoji,font,outlines),color=UiTextLayout::Create(emoji,font);
 if(!check(mono.IsValid() && color.IsValid(),"native monochrome and color layout")) {
  Cout()<<"color_error="<<color.GetError()<<EOL; return false;
 }
 check(mono.GetGpuText()!=color.GetGpuText(),"cache separates color policy");
 check(mono.GetSize()==color.GetSize() && mono.GetCaret(1)==color.GetCaret(1),"color representation preserves native editing geometry");
 int palette=0; for(const auto& run:color.GetGpuText()->runs) palette+=!IsNull(run.ink);
 check(palette>=3 && color.GetGlyphCount()>mono.GetGlyphCount(),"installed Segoe emoji expands ordered native palette layers");
 WString multiline=emoji+WString("\n")+emoji;
 auto lines=UiTextLayout::Create(multiline,font);
 check(lines.IsValid() && lines.GetLineCount()==2,"color multiline layout");
 auto alpha=std::make_shared<GpuShapedText>(); AddRectangle(*alpha,Color(255,0,0),255); AddRectangle(*alpha,Color(0,0,255),128);
 check(alpha->IsValid(),"ordered translucent outline layer snapshot");
 auto bitmap=std::make_shared<GpuShapedText>(); auto& run=bitmap->runs.Add();
 run.face=mono.GetGpuText()->runs[0].face; run.em_size=font.GetHeight(); run.opacity=128;
 auto& glyph=run.glyphs.Add(); glyph.glyph=mono.GetGpuText()->runs[0].glyphs[0].glyph; glyph.origin=Pointf(80,170);
 ImageBuffer source(2,2); int n=0;
 for(RGBA& p:source) { p.r=n==0?255:0; p.g=n==1?255:0; p.b=n==2?255:0; p.a=n==3?0:255; ++n; }
 run.bitmap=Image(source); run.bitmap_bounds=Rectf(80,160,104,184);
 check(bitmap->IsValid(),"bounded premultiplied bitmap glyph");
 auto invalid=std::make_shared<GpuShapedText>(); auto& huge=invalid->runs.Add();
 huge.face=run.face; huge.em_size=run.em_size; huge.glyphs=clone(run.glyphs); ImageBuffer oversized(2048,1); Fill(~oversized,RGBAZero(),oversized.GetLength()); huge.bitmap=Image(oversized); huge.bitmap_bounds=run.bitmap_bounds;
 check(!invalid->IsValid(),"oversized glyph image rejected before GPU upload");
 huge.bitmap=run.bitmap; huge.bitmap_bounds.right=NAN;
 check(!invalid->IsValid(),"nonfinite bitmap placement rejected");
 UiTextLayoutOptions custom; custom.font_file="tests/UiTextLayoutTest/fixtures/samples-sbix.ttf";
 auto native_bitmap=UiTextLayout::Create(WString("simple_linear"),SansSerif(64),custom);
 check(native_bitmap.IsValid(),"application-owned sbix font loads without system installation");
 if(!native_bitmap.IsValid()) { Cout()<<"bitmap_font_error="<<native_bitmap.GetError()<<EOL; return false; }
 int bitmap_runs=0; int64 bitmap_serial=0;
 for(const auto& layer:native_bitmap.GetGpuText()->runs) if(!layer.bitmap.IsEmpty()) { ++bitmap_runs; bitmap_serial=layer.bitmap.GetSerialId(); }
 check(bitmap_runs==1 && native_bitmap.GetGlyphCount()==1,"native PNG ligature becomes one positioned bitmap glyph");
 WString repeated_text;
 for(int i=0;i<64;++i) { if(i) repeated_text.Cat(' '); repeated_text.Cat("simple_linear"); }
 auto repeated=UiTextLayout::Create(repeated_text,SansSerif(64),custom);
 check(repeated.IsValid(),"repeated bitmap text fits shared-source payload budget");
 if(!repeated.IsValid()) Cout()<<"repeated_bitmap_error="<<repeated.GetError()<<EOL;
 auto assets_before=GetUiTextLayoutStats();
 auto different=UiTextLayout::Create(WString("simple_linear "),SansSerif(64),custom);
 bool reused=false; if(different.IsValid()) for(const auto& layer:different.GetGpuText()->runs) reused|=!layer.bitmap.IsEmpty() && layer.bitmap.GetSerialId()==bitmap_serial;
 check(reused && GetUiTextLayoutStats().bitmap_asset_hits>assets_before.bitmap_asset_hits,"changed text shares decoded native glyph asset");
 auto bitmap_lines=UiTextLayout::Create(WString("simple_linear\nsimple_linear"),SansSerif(64),custom);
 check(bitmap_lines.IsValid() && bitmap_lines.GetLineCount()==2 &&
       bitmap_lines.GetLineGpuText(1)->IsValid() && !bitmap_lines.GetLineGpuText(1)->runs[0].bitmap.IsEmpty(),
       "retained native bitmap line snapshot");
 auto bounded=GetUiTextLayoutStats();
 check(bounded.bitmap_asset_entries<=bounded.bitmap_asset_limit && bounded.bitmap_asset_bytes<=bounded.bitmap_asset_byte_limit,
       "decoded font asset count and byte budgets");
 {
  GpuShapedText distinct;
  for(int i=0;i<3;++i) {
   auto& layer=distinct.runs.Add(); layer.face=run.face; layer.em_size=run.em_size; layer.glyphs=clone(run.glyphs);
   ImageBuffer pixels(600,600); Fill(~pixels,(RGBA)Color(40+i*20,70,90),pixels.GetLength());
   layer.bitmap=Image(pixels); layer.bitmap_bounds=Rectf(0,0,600,600);
  }
  check(!distinct.IsValid(),"distinct bitmap buffers still obey unchanged four MiB payload limit");
 }
 UiTextLayoutOptions missing; missing.font_file="tests/UiTextLayoutTest/fixtures/missing-font.ttf";
 check(!UiTextLayout::Create(WString("font"),font,missing).IsValid(),"missing application font fails explicitly");
 UiDisplayListBuilder build; build.DrawShapedText(Pointf(10,10),color.GetGpuText(),Rgba8(0,0,0));
 build.DrawShapedText(Pointf(200,10),color.GetGpuText(),Rgba8(0,0,255),true);
 build.DrawShapedText(Pointf(0,0),bitmap,Rgba8(255,255,255));
 build.DrawImage(run.bitmap_bounds+Pointf(60,0),run.bitmap,Rect(run.bitmap.GetSize()),Rgba8(255,255,255,128));
 build.DrawShapedText(Pointf(140,0),bitmap,Rgba8(0,255,0),true);
 build.DrawImage(run.bitmap_bounds+Pointf(200,0),run.bitmap,Rect(run.bitmap.GetSize()),Rgba8(0,255,0,128),true);
 build.DrawShapedText(Pointf(320,150),native_bitmap.GetGpuText(),Rgba8(0,0,0));
 build.DrawShapedText(Pointf(0,0),alpha,Rgba8(0,0,0));
 build.DrawShapedText(Pointf(60,0),alpha,Rgba8(0,0,0,128));
 UiDisplayList scene; check(build.Finish(scene),"ordered color and bitmap scene");
 ImagePainter reference(Size(400,300)); reference.Clear((RGBA)White()); SoftwareUiRenderer software;
 check(software.Replay(scene,reference),"software font palette/bitmap reference");
 Image reference_pixels=reference.GetResult();
 const auto& blend=reference_pixels[237][35]; const auto& faded=reference_pixels[237][95];
 check(abs((int)blend.r-127)<=2 && blend.g<=1 && abs((int)blend.b-128)<=2,"software palette alpha equation");
 check(abs((int)faded.r-191)<=2 && abs((int)faded.g-95)<=2 && abs((int)faded.b-159)<=2,"software drawing alpha equation");
 int software_bitmap_changed=0,software_bitmap_max=0;
 for(int y=160;y<184;++y) for(int x=80;x<104;++x) {
  const auto& a=reference_pixels[y][x]; const auto& b=reference_pixels[y][x+60];
  int delta=max(abs((int)a.r-b.r),max(abs((int)a.g-b.g),abs((int)a.b-b.b)));
  software_bitmap_changed+=delta>1; software_bitmap_max=max(software_bitmap_max,delta);
  if(delta>1 && software_bitmap_changed==1) Cout()<<"software_bitmap_first="<<x<<","<<y<<" actual="<<(int)a.r<<","<<(int)a.g<<","<<(int)a.b<<" reference="<<(int)b.r<<","<<(int)b.g<<","<<(int)b.b<<EOL;
 }
 Cout()<<"software_bitmap_changed="<<software_bitmap_changed<<" max_error="<<software_bitmap_max<<EOL;
 check(!software_bitmap_changed,"software embedded bitmap opacity applied once");
 ImagePainter native_reference(Size(400,300)); native_reference.Clear((RGBA)White());
 check(color.Draw(native_reference,Pointf(10,10),Black()) && native_bitmap.Draw(native_reference,Pointf(320,150),Black()),"native Ui Painter font layers");
 Image native_pixels=native_reference.GetResult(); int painter_changed=0;
 for(int y=10;y<100;++y) for(int x=10;x<130;++x) {
  const auto& a=reference_pixels[y][x]; const auto& b=native_pixels[y][x];
  painter_changed+=a.r!=b.r || a.g!=b.g || a.b!=b.b;
 }
 for(int y=150;y<230;++y) for(int x=320;x<400;++x) {
  const auto& a=reference_pixels[y][x]; const auto& b=native_pixels[y][x];
  painter_changed+=a.r!=b.r || a.g!=b.g || a.b!=b.b;
 }
 check(!painter_changed,"Ui Painter and semantic software replay agree for native palette/bitmap");
 HWND hwnd=CreateWindowExW(0,L"STATIC",L"Font layer offscreen qualification",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 auto resolver=[](HMODULE,LPCSTR)->FARPROC { return reinterpret_cast<FARPROC>(1); };
 VulkanSurfaceSession session; GpuNativeWindowDesc native; native.kind=GpuNativeWindowKind::Win32; native.handle=(uintptr_t)hwnd;
 if(check(hwnd && session.Open(true,native,resolver),"font layer Vulkan session")) {
  VulkanGpuDevice device(session);
  for(GpuFormat format:{GpuFormat::RGBA8,GpuFormat::BGRA8,GpuFormat::RGBA8Srgb,GpuFormat::BGRA8Srgb}) {
   GpuTextureDesc desc; desc.size=Size(400,300); desc.format=format; desc.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_TransferSrc;
   GpuTextureId texture; if(!check(device.CreateTexture(desc,texture)==GpuResult::Ok,"font layer color target")) continue;
   {
    UiRenderer2D renderer(device); UiRenderer2DTarget target; target.color_target=texture; target.size=desc.size; target.color_format=format;
    target.clear_color.red=target.clear_color.green=target.clear_color.blue=target.clear_color.alpha=1;
    Vector<byte> cold,warm;
    bool rendered=renderer.Render(scene,target);
    check(rendered,"color palette and bitmap GPU replay");
    if(!rendered) Cout()<<"font_gpu_error="<<renderer.GetError()<<EOL;
    check(rendered && device.ReadTexturePixels(texture,cold),"font layer readback");
    check(renderer.GetStats().cpu_glyph_raster_count==0 && renderer.GetStats().vector_raster_count==0,"font layers avoid CPU coverage rasterization");
    auto pixel=[&](int x,int y) {
     const byte *p=cold.Begin()+4*(y*desc.size.cx+x); bool bgra=format==GpuFormat::BGRA8 || format==GpuFormat::BGRA8Srgb;
     return Rgba8(p[bgra?2:0],p[1],p[bgra?0:2],p[3]);
    };
    if(cold.GetCount()==400*300*4) {
     if(format==GpuFormat::RGBA8) {
      ImageBuffer capture(desc.size);
      for(int i=0;i<capture.GetLength();++i) { const byte *p=cold.Begin()+4*i; auto& c=capture.Begin()[i]; c.r=p[0]; c.g=p[1]; c.b=p[2]; c.a=p[3]; }
      check(PNGEncoder().SaveFile("build/UiTextLayout-font-layers.png",Image(capture)),"font GPU evidence image");
     }
     int colored=0,selection_wrong=0;
     for(int y=10;y<100;++y) for(int x=10;x<130;++x) { auto p=pixel(x,y); colored+=max(p.r,max(p.g,p.b))-min(p.r,min(p.g,p.b))>30; }
     for(int y=10;y<100;++y) for(int x=200;x<320;++x) { auto p=pixel(x,y); selection_wrong+=abs((int)p.r-p.g)>1; }
     check(colored>500,"native palette produces colored pixels");
     check(selection_wrong==0,"selection recolors palette through retained coverage");
     int bitmap_changed=0;
     for(int y=160;y<184;++y) for(int x=80;x<104;++x) bitmap_changed+=pixel(x,y)!=pixel(x+60,y);
     check(bitmap_changed==0,"embedded bitmap glyph matches independent image replay");
     int mask_changed=0;
     for(int y=160;y<184;++y) for(int x=220;x<244;++x) mask_changed+=pixel(x,y)!=pixel(x+60,y);
     check(mask_changed==0,"selection uses bitmap alpha with no recolored upload");
     auto p=pixel(35,237); Cout()<<"alpha_pixel format="<<(int)format<<" rgb="<<(int)p.r<<","<<(int)p.g<<","<<(int)p.b<<EOL; bool srgb=format==GpuFormat::RGBA8Srgb || format==GpuFormat::BGRA8Srgb;
     check(p.g<=1 && abs((int)p.r-(srgb?187:127))<=2 && abs((int)p.b-(srgb?188:128))<=2,"ordered alpha layers match independent source-over equation");
     auto q=pixel(95,237);
     check(abs((int)q.r-(srgb?224:191))<=2 && abs((int)q.g-(srgb?164:95))<=2 &&
           abs((int)q.b-(srgb?207:159))<=2,"drawing opacity multiplies every native palette layer");
    }
    check(renderer.Render(scene,target) && device.ReadTexturePixels(texture,warm) && cold==warm,"warm font pixels identical");
    check(renderer.GetStats().texture_upload_count==0 && renderer.GetStats().glyph_cache_miss_count==0 &&
          renderer.GetStats().gpu_glyph_raster_count==0,"warm palette and bitmap reuse existing GPU assets");
   }
   check(device.DestroyTexture(texture)==GpuResult::Ok,"font target destroyed");
  }
  check(device.GetLiveTextureCount()==0 && device.GetLiveBufferCount()==0 && device.GetLiveCommandCount()==0 &&
        device.GetLiveShaderCount()==0 && device.GetLivePipelineCount()==0,"font layer final GPU ownership zero");
 }
 session.Close(); if(hwnd) DestroyWindow(hwnd);
 check(!session.GetReport().validation_error_count && !session.GetReport().validation_warning_count,"font layer validation clean");
 Cout()<<"font_layer_checks="<<checks<<" palette_runs="<<palette<<" passed="<<(ok?1:0)<<EOL; return ok;
}
