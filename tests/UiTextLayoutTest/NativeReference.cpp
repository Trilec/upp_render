#include <CtrlLib/CtrlLib.h>
#include <Ui/UiTextLayout.h>
#include <Ui/UiTextLayoutInternal.h>
#include <RenderSoftware/RenderSoftware.h>
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <plugin/png/png.h>
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>

using namespace Upp;
namespace {
template <class T> struct RefCom {
 T *p=nullptr; ~RefCom() { if(p) p->Release(); }
 T **Put() { return &p; } T *operator->() const { return p; }
};
struct GeometryCommand : Moveable<GeometryCommand> { int kind=0; Pointf p[3]{}; };
struct GlyphGeometry : FontGlyphConsumer {
 Vector<GeometryCommand>& commands; Pointf origin;
 GlyphGeometry(Vector<GeometryCommand>& c,Pointf p):commands(c),origin(p) {}
 void Add(int kind,Pointf a=Pointf(0,0),Pointf b=Pointf(0,0),Pointf c=Pointf(0,0)) {
  auto& out=commands.Add(); out.kind=kind;
  if(kind!=4) { out.p[0]=a+origin; if(kind==2||kind==3) out.p[1]=b+origin; if(kind==3) out.p[2]=c+origin; }
 }
 void Move(Pointf p) override { Add(0,p); } void Line(Pointf p) override { Add(1,p); }
 void Quadratic(Pointf a,Pointf b) override { Add(2,a,b); }
 void Cubic(Pointf a,Pointf b,Pointf c) override { Add(3,a,b,c); }
 void Close() override { Add(4); }
};
class ReferenceSink : public IDWriteGeometrySink {
 Painter& painter; ULONG refs=1; GlyphGeometry geometry;
public:
 bool used=false;
private:
public:
 ReferenceSink(Painter& painter,Vector<GeometryCommand>& c,Pointf at):painter(painter),geometry(c,at) {}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **value) override {
  static const GUID sink={0x2cd9069e,0x12e2,0x11dc,{0x9f,0xed,0x00,0x11,0x43,0xa0,0x55,0xf9}};
  if(!value) return E_POINTER; *value=nullptr;
  if(IsEqualIID(iid,sink)||IsEqualIID(iid,__uuidof(IUnknown))) { *value=this; AddRef(); return S_OK; }
  return E_NOINTERFACE;
 }
 ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
 ULONG STDMETHODCALLTYPE Release() override { return --refs; }
 void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE mode) noexcept override { painter.EvenOdd(mode==D2D1_FILL_MODE_ALTERNATE); }
 void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT) noexcept override {}
 void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F p,D2D1_FIGURE_BEGIN) noexcept override { used=true; painter.Move(p.x,p.y); geometry.Move(Pointf(p.x,p.y)); }
 void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F *p,UINT count) noexcept override { for(UINT i=0;i<count;++i) { painter.Line(p[i].x,p[i].y); geometry.Line(Pointf(p[i].x,p[i].y)); } }
 void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT *p,UINT count) noexcept override {
  for(UINT i=0;i<count;++i) { Pointf a(p[i].point1.x,p[i].point1.y),b(p[i].point2.x,p[i].point2.y),c(p[i].point3.x,p[i].point3.y); painter.Cubic(a,b,c); geometry.Cubic(a,b,c); }
 }
 void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END end) noexcept override { if(end==D2D1_FIGURE_END_CLOSED) { painter.Close(); geometry.Close(); } }
 HRESULT STDMETHODCALLTYPE Close() noexcept override { return S_OK; }
};
class ReferenceRenderer : public IDWriteTextRenderer {
 Painter& painter; ULONG refs=1; Vector<GeometryCommand>& commands;
public:
 ReferenceRenderer(Painter& painter,Vector<GeometryCommand>& commands):painter(painter),commands(commands) {}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **value) override {
  if(!value) return E_POINTER; *value=nullptr;
  if(IsEqualIID(iid,__uuidof(IUnknown))||IsEqualIID(iid,__uuidof(IDWriteTextRenderer))||IsEqualIID(iid,__uuidof(IDWritePixelSnapping))) {
   *value=static_cast<IDWriteTextRenderer *>(this); AddRef(); return S_OK;
  }
  return E_NOINTERFACE;
 }
 ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
 ULONG STDMETHODCALLTYPE Release() override { return --refs; }
 HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *,BOOL *v) override { *v=TRUE; return S_OK; }
 HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *,DWRITE_MATRIX *v) override { *v={1,0,0,1,0,0}; return S_OK; }
 HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *,FLOAT *v) override { *v=1; return S_OK; }
 HRESULT STDMETHODCALLTYPE DrawGlyphRun(void *,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE,
  const DWRITE_GLYPH_RUN *run,const DWRITE_GLYPH_RUN_DESCRIPTION *,IUnknown *) override {
  // Independent oracle: let DirectWrite place the COMPLETE glyph run, rather
  // than reproducing the production per-glyph RTL/offset arithmetic.
  painter.Begin(); painter.Translate(x,y);
  ReferenceSink sink(painter,commands,Pointf(x,y));
  HRESULT hr=run->fontFace->GetGlyphRunOutline(run->fontEmSize,run->glyphIndices,run->glyphAdvances,
   run->glyphOffsets,run->glyphCount,run->isSideways,run->bidiLevel&1,&sink);
  if(SUCCEEDED(hr) && sink.used) painter.Fill(White());
  painter.End(); return hr;
 }
 HRESULT Decoration(FLOAT x,FLOAT y,FLOAT width,FLOAT thickness,FLOAT offset,DWRITE_READING_DIRECTION direction) {
  if(direction==DWRITE_READING_DIRECTION_RIGHT_TO_LEFT) x-=width;
  painter.Rectangle(x,y+offset,width,thickness).Fill(White()); return S_OK;
 }
 HRESULT STDMETHODCALLTYPE DrawUnderline(void *,FLOAT x,FLOAT y,const DWRITE_UNDERLINE *v,IUnknown *) override {
  return Decoration(x,y,v->width,v->thickness,v->offset,v->readingDirection);
 }
 HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *,FLOAT x,FLOAT y,const DWRITE_STRIKETHROUGH *v,IUnknown *) override {
  return Decoration(x,y,v->width,v->thickness,v->offset,v->readingDirection);
 }
 HRESULT STDMETHODCALLTYPE DrawInlineObject(void *,FLOAT,FLOAT,IDWriteInlineObject *,BOOL,BOOL,IUnknown *) override { return E_NOTIMPL; }
};
bool NativeImage(const WString& text,Font font,bool rtl,Image& out,Vector<GeometryCommand>& commands) {
 using namespace UiTextInternal;
 font.RealizeStd();
 String error; Encoding encoded,family;
 if(!encoded.Build(text,error)||!family.Build(font.GetFaceName().ToWString(),error)) return false;
 RefCom<IDWriteFactory> factory; RefCom<IDWriteTextFormat> format; RefCom<IDWriteTextLayout> layout;
 if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown **>(factory.Put())))) return false;
 if(FAILED(factory->CreateTextFormat((const WCHAR *)family.utf16.Begin(),nullptr,
  font.IsBold()?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,
  font.IsItalic()?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,
  (FLOAT)abs(font.GetHeight()),L"en-US",format.Put()))) return false;
 format->SetReadingDirection(rtl?DWRITE_READING_DIRECTION_RIGHT_TO_LEFT:DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
 format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
 if(FAILED(factory->CreateTextLayout((const WCHAR *)encoded.utf16.Begin(),encoded.Length16(),format.p,1000000,1000,layout.Put()))) return false;
 DWRITE_TEXT_METRICS metrics{}; if(FAILED(layout->GetMetrics(&metrics))) return false;
 layout->SetMaxWidth(max(1.0f,metrics.widthIncludingTrailingWhitespace));
 DWRITE_TEXT_RANGE range{0,(UINT32)encoded.Length16()};
 if(font.IsUnderline()) layout->SetUnderline(TRUE,range);
 if(font.IsStrikeout()) layout->SetStrikethrough(TRUE,range);
 ImagePainter painter(Size(512,96)); painter.Clear(RGBA{0,0,0,255});
 ReferenceRenderer renderer(painter,commands);
 if(FAILED(layout->Draw(nullptr,&renderer,12,8))) return false;
 out=painter.GetResult(); return true;
}
WString S(std::initializer_list<int> values) { WString s; for(int cp:values) s.Cat(cp); return s; }
// Independent numerical non-zero winding oracle. It samples polygons directly;
// it does not call the GPU row builder, shader, tessellator or Painter rasterizer.
struct CoverageOracle : FontGlyphConsumer {
 Vector<Pointf> starts,ends; Pointf at=Pointf(0,0),first=Pointf(0,0),origin;
 Rectf bounds=Rectf(1e100,1e100,-1e100,-1e100); bool begun=false;
 CoverageOracle(Pointf origin):origin(origin) {}
 void Bound(Pointf p) { bounds.left=min(bounds.left,p.x); bounds.top=min(bounds.top,p.y); bounds.right=max(bounds.right,p.x); bounds.bottom=max(bounds.bottom,p.y); }
 void Edge(Pointf a,Pointf b) { a+=origin; b+=origin; if(a!=b) { starts.Add(a); ends.Add(b); } Bound(a); Bound(b); }
 void Move(Pointf p) override { Finish(); first=at=p; begun=true; }
 void Line(Pointf p) override { if(begun) Edge(at,p); at=p; }
 struct Lines : LinearPathConsumer {
  CoverageOracle& owner; Lines(CoverageOracle& owner):owner(owner) {}
  void Move(const Pointf& p) override { owner.Line(p); }
  void Line(const Pointf& p) override { owner.Line(p); }
 };
 void Quadratic(Pointf a,Pointf b) override { Pointf start=at; Lines target(*this); ApproximateQuadratic(target,start,a,b,.3); at=b; }
 void Cubic(Pointf a,Pointf b,Pointf c) override { Pointf start=at; Lines target(*this); ApproximateCubic(target,start,a,b,c,.3); at=c; }
 void Close() override { Finish(); }
 void Finish() { if(begun) { Edge(at,first); at=first; begun=false; } }
 double Sample(Point pixel) const {
  if(bounds.right<=pixel.x || bounds.left>=pixel.x+1 || bounds.bottom<=pixel.y || bounds.top>=pixel.y+1) return 0;
  struct Crossing : Moveable<Crossing> { double x; int direction; };
  Vector<Crossing> crossings; double covered=0;
  // Integrate horizontal non-zero intervals at 128 independent vertical samples.
  // Horizontal coverage is exact for each sample; repeated contours are unioned.
  for(int y=0;y<128;++y) {
   double qy=pixel.y+(y+.5)/128; crossings.Clear();
   for(int i=0;i<starts.GetCount();++i) {
    const Pointf& a=starts[i]; const Pointf& b=ends[i];
    if((a.y<=qy && b.y>qy)||(b.y<=qy && a.y>qy)) {
     auto& hit=crossings.Add(); hit.x=a.x+(qy-a.y)*(b.x-a.x)/(b.y-a.y);
     hit.direction=b.y>a.y?1:-1;
    }
   }
   Sort(crossings,[](const Crossing& a,const Crossing& b) { return a.x<b.x; });
   int winding=0; double previous=0;
   for(const auto& hit:crossings) {
    if(winding) covered+=max(0.0,min(hit.x,(double)pixel.x+1)-max(previous,(double)pixel.x));
    winding+=hit.direction; previous=hit.x;
   }
  }
  return covered/128;
 }
};
double CoverageAt(const Array<CoverageOracle>& glyphs,const GpuShapedText& text,Point pixel) {
 double alpha=0;
 for(const auto& glyph:glyphs) { double a=glyph.Sample(pixel); alpha=a+alpha*(1-a); }
 for(const auto& decoration:text.decorations) {
  Rectf r=decoration.rect+Pointf(12,8);
  double a=max(0.0,min(r.right,(double)pixel.x+1)-max(r.left,(double)pixel.x))*
           max(0.0,min(r.bottom,(double)pixel.y+1)-max(r.top,(double)pixel.y));
  alpha=a+alpha*(1-a);
 }
 return alpha;
}
FARPROC WINAPI Resolver(HMODULE,LPCSTR) { return reinterpret_cast<FARPROC>(1); }
}
bool TestShapedReference()
{
 String report; report<<"started="<<AsString(GetSysTime())<<'\n';
 bool ok=true;
 auto check=[&](bool pass,const char *why) { if(!pass) { ok=false; report<<"FAIL "<<why<<'\n'; } return pass; };
 CoverageOracle overlap(Pointf(0,0));
 for(int i=0;i<2;++i) { overlap.Move(Pointf(0,0)); overlap.Line(Pointf(.5,0)); overlap.Line(Pointf(.5,1)); overlap.Line(Pointf(0,1)); overlap.Close(); }
 check(fabs(overlap.Sample(Point(0,0))-.5)<.0001,"independent coverage treats overlapping contours as non-zero union");
 ImagePainter overlap_reference(Size(1,1)); overlap_reference.Clear(RGBA{0,0,0,255}); overlap_reference.EvenOdd(false);
 for(int i=0;i<2;++i) overlap_reference.Move(0,0).Line(.5,0).Line(.5,1).Line(0,1).Close();
 overlap_reference.Fill(White());
 Image overlap_pixels=overlap_reference.GetResult();
 report<<"overlap_analytic=.5 painter="<<overlap_pixels[0][0].r/255.0<<'\n';
 Array<WString> fixtures;
 fixtures.Add(S({0x633,0x644,0x627,0x645}));
 fixtures.Add(S({0x633,0x64e,0x644,0x627,0x645,0x64c}));
 fixtures.Add(S({'A',' ',0x5d0,0x5d1,0x5d2,' ','4','2'}));
 fixtures.Add(S({'o','f','f','i','c','e',' ','e',0x301}));
 fixtures.Add(S({0x915,0x94d,0x937,0x93f,' ',0x92b,0x93f}));
 fixtures.Add(S({0xe44,0xe17,0xe22,' ',0xd55c,0xae00}));
 fixtures.Add(S({'A',' ',0x1f600,' ',0x4e2d}));
 fixtures.Add(WString("Variable font weight"));
 HWND hwnd=CreateWindowExW(0,L"STATIC",L"Ui shaped text offscreen qualification",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 VulkanSurfaceSession session; GpuNativeWindowDesc native;
 native.kind=GpuNativeWindowKind::Win32; native.handle=(uintptr_t)hwnd;
 bool opened=hwnd && session.Open(true,native,&Resolver);
 check(opened,"Vulkan shaping test session opens");
 if(opened) {
  VulkanGpuDevice device(session);
  check(device.IsReady(),"Vulkan shaping device ready");
  for(int sample=0;sample<fixtures.GetCount();++sample) for(int style=0;style<2;++style) {
   Font font=SansSerif(28); if(sample==7) font.FaceName("Bahnschrift"); if(style) font.Bold().Italic().Underline().Strikeout();
   UiTextInternal::Encoding encoded; String error; bool rtl=false;
   UiTextInternal::BaseDirection((encoded.Build(fixtures[sample],error),encoded),rtl,error);
   UiTextLayoutOptions options; options.locale="en-US";
   auto shaped=UiTextLayout::Create(fixtures[sample],font,options);
   Image reference; Vector<GeometryCommand> native_geometry,shaped_geometry;
   if(!check(shaped.IsValid() && NativeImage(fixtures[sample],font,rtl,reference,native_geometry),"independent native reference builds")) continue;
   for(const auto& run:shaped.GetGpuText()->runs) for(const auto& glyph:run.glyphs) {
    GlyphGeometry g(shaped_geometry,Pointf(12,8)+glyph.origin);
    run.face->GetOutline(glyph.glyph,run.em_size,g,error);
   }
   double geometry_error=0; bool matching=native_geometry.GetCount()==shaped_geometry.GetCount();
   if(matching) for(int i=0;i<native_geometry.GetCount();++i) {
    const auto& a=native_geometry[i]; const auto& b=shaped_geometry[i];
    matching=matching && a.kind==b.kind;
    for(int j=0;j<3;++j) geometry_error=max(geometry_error,max(fabs(a.p[j].x-b.p[j].x),fabs(a.p[j].y-b.p[j].y)));
   }
   report<<"geometry sample="<<sample<<" style="<<style<<" native="<<native_geometry.GetCount()<<" shaped="<<shaped_geometry.GetCount()<<" error="<<geometry_error<<'\n';
   check(matching && geometry_error<.0002,"independent whole-run outline placement");
   UiDisplayListBuilder builder;
   builder.DrawShapedText(Pointf(12,8),shaped.GetGpuText(),Rgba8(255,255,255));
   UiDisplayList list; check(builder.Finish(list),"shaped display list builds");
   ImagePainter painter(Size(512,96)); painter.Clear(RGBA{0,0,0,255});
   SoftwareUiRenderer software;
   if(!check(software.Replay(list,painter),"shaped outline replay")) continue;
   Image actual=painter.GetResult(); int maximum=0; double sum=0;
   for(int y=0;y<96;++y) for(int x=0;x<512;++x) {
    int delta=abs((int)actual[y][x].r-(int)reference[y][x].r);
    maximum=max(maximum,delta); sum+=delta;
   }
   report<<"native_outline_case="<<sample<<" style="<<style<<" max="<<maximum<<" mean="<<sum/(512*96)<<'\n';
   // Placement is checked above against native outline commands to <0.0002px.
   // Whole-run union and individual atlas glyph alpha composition differ at
   // overlapping antialiased edges. Keep this image comparison diagnostic.
   if(maximum>8) {
    PNGEncoder().SaveFile(Format("build/UiTextLayout-native-%d-%d.png",sample,style),reference);
    PNGEncoder().SaveFile(Format("build/UiTextLayout-shaped-%d-%d.png",sample,style),actual);
   }
   Array<CoverageOracle> oracle;
   for(const auto& run:shaped.GetGpuText()->runs) for(const auto& glyph:run.glyphs) {
    auto& shape=oracle.Create<CoverageOracle>(Pointf(12,8)+glyph.origin);
    check(run.face->GetOutline(glyph.glyph,run.em_size,shape,error),"independent glyph coverage outline");
    shape.Finish();
   }
   Vector<double> oracle_cache; oracle_cache.SetCount(512*96);
   for(int y=0;y<96;++y) for(int x=0;x<512;++x)
    oracle_cache[y*512+x]=CoverageAt(oracle,*shaped.GetGpuText(),Point(x,y));
   for(GpuFormat format:{GpuFormat::RGBA8,GpuFormat::BGRA8,GpuFormat::RGBA8Srgb,GpuFormat::BGRA8Srgb}) {
    GpuTextureDesc desc; desc.size=Size(512,96); desc.format=format;
    desc.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_TransferSrc;
    GpuTextureId target;
    if(!check(device.CreateTexture(desc,target)==GpuResult::Ok,"shaped GPU target creates")) continue;
    {
     UiRenderer2D renderer(device); UiRenderer2DTarget output;
     output.color_target=target; output.size=desc.size; output.color_format=format; output.clear_color.alpha=1;
     Vector<byte> pixels;
     if(check(renderer.Render(list,output) && device.ReadTexturePixels(target,pixels) && pixels.GetCount()==512*96*4,"shaped GPU pixels render")) {
      auto cold=renderer.GetStats(); double largest=0,total=0,legacy_max=0; int aa=0;
      bool srgb=format==GpuFormat::RGBA8Srgb||format==GpuFormat::BGRA8Srgb;
      for(int y=0;y<96;++y) for(int x=0;x<512;++x) {
       double v=pixels[4*(y*512+x)]/255.0;
       aa+=v>0 && v<1;
       if(srgb) v=v<=.04045?v/12.92:pow((v+.055)/1.055,2.4);
       legacy_max=max(legacy_max,fabs(v-actual[y][x].r/255.0));
       double delta=fabs(v-oracle_cache[y*512+x]);
       largest=max(largest,delta); total+=delta;
      }
      report<<"shaped_gpu_case="<<sample<<" style="<<style<<" format="<<(int)format<<" max="<<largest<<" mean="<<total/(512*96)<<" aa="<<aa<<" legacy_max="<<legacy_max<<" independently_checked="<<oracle_cache.GetCount()<<'\n';
      check(largest<=.04 && total/(512*96)<=.001 && aa>20,"GPU shaped text matches independent non-zero coverage within unchanged pixel bounds");
      if(sample==7 && format==GpuFormat::RGBA8) {
       ImageBuffer image(512,96);
       for(int y=0;y<96;++y) for(int x=0;x<512;++x) { byte v=pixels[4*(y*512+x)]; image[y][x]=RGBA{v,v,v,255}; }
       PNGEncoder().SaveFile(Format("build/UiTextLayout-variable-%d-gpu.png",style),Image(image));
       PNGEncoder().SaveFile(Format("build/UiTextLayout-variable-%d-painter.png",style),actual);
       PNGEncoder().SaveFile(Format("build/UiTextLayout-variable-%d-native.png",style),reference);
      }
      check(cold.cpu_glyph_raster_count==0 && cold.gpu_glyph_raster_count>0 && cold.texture_upload_count==0,"cold shaped glyph coverage stays on GPU");
      check(renderer.Render(list,output) && renderer.GetStats().gpu_glyph_raster_count==0 &&
       renderer.GetStats().glyph_cache_miss_count==0,"warm shaped text reuses atlas");
      Vector<byte> warm; check(device.ReadTexturePixels(target,warm) && warm==pixels,"warm shaped output is identical");
     } else report<<"renderer_error="<<renderer.GetError()<<'\n';
    }
    device.DestroyTexture(target);
   }
  }
  check(device.GetLiveBufferCount()==0 && device.GetLiveTextureCount()==0 && device.GetLiveShaderCount()==0 &&
   device.GetLivePipelineCount()==0 && device.GetLiveCommandCount()==0,"shaped text releases native GPU resources");
 }
 session.Close(); if(hwnd) DestroyWindow(hwnd);
 check(session.GetReport().validation_error_count==0 && session.GetReport().validation_warning_count==0,"shaped text has zero Vulkan validation messages");
 report<<"passed="<<(ok?1:0)<<'\n';
 SaveFile("build/UiTextLayout-qualification.txt",report);
 Cout()<<"shaped_reference_cases="<<fixtures.GetCount()*2<<" gpu_cases="<<fixtures.GetCount()*8<<" passed="<<(ok?1:0)<<'\n';
 return ok;
}
