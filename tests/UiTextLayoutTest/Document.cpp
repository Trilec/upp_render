#include <Ui/Ui.h>
#include <RenderCtrlBridge/RenderCtrlBridge.h>
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <plugin/png/png.h>
using namespace Upp;
namespace {
WString S(std::initializer_list<int> values) { WString s; for(int cp:values) s.Cat(cp); return s; }
bool NativeGpuDocument(const UiDisplayList& list,const char *path="build/UiTextLayout-document.png") {
 HWND hwnd=CreateWindowExW(0,L"STATIC",L"Document offscreen qualification",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 auto resolver=[](HMODULE,LPCSTR)->FARPROC { return reinterpret_cast<FARPROC>(1); };
 VulkanSurfaceSession session; GpuNativeWindowDesc native; native.kind=GpuNativeWindowKind::Win32; native.handle=(uintptr_t)hwnd;
 bool ok=hwnd && session.Open(true,native,resolver);
 if(ok) {
  VulkanGpuDevice device(session); GpuTextureDesc desc; desc.size=Size(640,420); desc.format=GpuFormat::RGBA8;
  desc.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_TransferSrc; GpuTextureId target;
  ok=device.CreateTexture(desc,target)==GpuResult::Ok;
  if(ok) {
   {
    UiRenderer2D renderer(device); UiRenderer2DTarget output; output.color_target=target; output.size=desc.size; output.color_format=desc.format;
    output.clear_color.red=output.clear_color.green=output.clear_color.blue=output.clear_color.alpha=1;
    Vector<byte> pixels; ok=renderer.Render(list,output) && device.ReadTexturePixels(target,pixels);
    if(ok) {
     ImageBuffer image(640,420);
     for(int y=0;y<420;++y) for(int x=0;x<640;++x) {
      const byte *p=pixels.Begin()+4*(y*640+x); auto& c=image[y][x]; c.r=p[0]; c.g=p[1]; c.b=p[2]; c.a=p[3];
     }
     ok=PNGEncoder().SaveFile(path,Image(image));
    }
    ok=ok && renderer.GetStats().cpu_glyph_raster_count==0 && renderer.GetStats().vector_raster_count==0;
   }
   device.DestroyTexture(target);
  }
  ok=ok && device.GetLiveTextureCount()==0 && device.GetLiveBufferCount()==0 && device.GetLiveCommandCount()==0 && device.GetLiveShaderCount()==0 && device.GetLivePipelineCount()==0;
 }
 session.Close(); if(hwnd) DestroyWindow(hwnd);
 return ok && session.GetReport().validation_error_count==0 && session.GetReport().validation_warning_count==0;
}
}
bool TestUiTextImageGpu(const UiDisplayList& list,const char *path) { return NativeGpuDocument(list,path); }
bool TestDocumentText()
{
 bool ok=true; int checks=0;
 auto check=[&](bool pass,const char *why) { ++checks; if(!pass) { ok=false; Cout()<<"FAIL document "<<why<<EOL; } };
 UiDoc doc; doc.SetRect(0,0,640,420); doc.Layout();
 WString combining=S({'e',0x301,'x'});
 doc.SetText(ToUtf8(combining)); doc.SetSelection(UiDocRange(3,3)); doc.Key(K_BACKSPACE,1);
 check(doc.GetTextW()==S({'e',0x301}),"Backspace trailing scalar");
 doc.Key(K_BACKSPACE,1); check(doc.GetTextW().IsEmpty(),"Backspace whole grapheme");
 doc.Undo(); check(doc.GetTextW()==S({'e',0x301}),"grapheme deletion undo");
 doc.SetText(ToUtf8(combining)); doc.SetSelection(UiDocRange(0,0)); doc.Key(K_DELETE,1);
 check(doc.GetTextW()==S({'x'}),"Delete whole grapheme");
 doc.SetText(ToUtf8(combining)); doc.SetSelection(UiDocRange(0,1));
 check(doc.GetSelection().anchor==0 && doc.GetSelection().caret==2,"partial grapheme selection expands");
 doc.SetSelection(UiDocRange(1,1)); check(doc.GetSelection().anchor==0 && doc.GetSelection().caret==0,"collapsed interior caret stays collapsed");
 doc.SetSelection(UiDocRange(0,0)); doc.Key(K_RIGHT,1); check(doc.GetSelection().caret==2,"Right skips combining interior");
 doc.SetText(ToUtf8(S({0x1f469,0x200d,0x1f52c}))); doc.SetSelection(UiDocRange(3,3)); doc.Key(K_BACKSPACE,1);
 check(doc.GetTextW().IsEmpty(),"emoji sequence deletion");
 doc.SetText(ToUtf8(S({0x915,0x94d,0x937,0x93f}))); doc.SetSelection(UiDocRange(4,4)); doc.Key(K_BACKSPACE,1);
 check(doc.GetTextW().IsEmpty(),"Indic conjunct deletion");
 WString arabic=S({0x633,0x644,0x627,0x645});
 doc.SetText(ToUtf8(arabic)); doc.Layout(); doc.SetSelection(UiDocRange(0,0)); Rect before=doc.GetCaretRect(); doc.Key(K_LEFT,1);
 check(doc.GetSelection().caret>0 && doc.GetCaretRect().left<before.left,"Arabic visual left");
 for(int pos=0;pos<=4;++pos) {
  doc.SetSelection(UiDocRange(pos,pos)); Point at=doc.PointAtPos(pos);
  check(doc.PosAtPoint(at+Point(0,2))==pos,"Arabic caret and hit round trip");
 }
 WString words=S({'o','n','e',0x2003,0x633,0x644,0x627,0x645});
 doc.SetText(ToUtf8(words)); doc.SetSelection(UiDocRange(0,0)); doc.Key(K_CTRL_RIGHT,1);
 check(doc.GetSelection().caret==4,"Unicode whitespace word movement");
 WString text("Styled Arabic: "); text.Cat(arabic); text.Cat("\nIndic: "); text.Cat(S({0x915,0x94d,0x937,0x93f})); text.Cat("\n"); text.Cat(WString("Wrapped paragraph of mixed text ")); text.Cat(arabic);
 doc.SetText(ToUtf8(text)); UiDocTextStyle style; style.flags=UiDocTextStyle::BOLD|UiDocTextStyle::UNDERLINE; style.ink=Blue(); style.font_height=24;
 check(doc.Model().SetStyle(UiDocRange(0,6),style).ok,"document styled model");
 doc.ConfigureMetadataType("note",Image(),Color(38,140,100));
 String metadata=doc.AddMetadata(UiDocRange(15,17),"note","Unicode metadata",ToUtf8(arabic)+String(" ")+ToUtf8(S({0x915,0x94d,0x937,0x93f})));
 check(!metadata.IsEmpty() && doc.SetMetadataExpanded(metadata,true),"expanded metadata fixture");
 doc.ShowLineNumbers(true);
 doc.SetSelection(UiDocRange(15,17)); doc.Layout();
 UiDisplayList list; String error; CtrlDisplayListRecordReport report;
 const auto helper_before=UiRasterCache::GetStats();
 bool recorded=RecordCtrlDisplayList(doc,list,error,&report);
 check(UiRasterCache::GetStats().misses==helper_before.misses,"document metadata avoids helper raster images");
 check(recorded && !report.HasUnsupportedOperation(),"styled document semantic recording");
 int legacy=0,shaped=0,max_glyphs=0;
 for(int i=0;i<list.GetCount();++i) {
  legacy+=list[i].type==UiDisplayOpType::DrawText;
  if(list[i].type==UiDisplayOpType::DrawShapedText) { ++shaped; max_glyphs=max(max_glyphs,list[i].shaped_text->GetGlyphCount()); }
 }
 check(shaped>=3 && max_glyphs>10 && legacy==0,"document draws complete shaped lines without scalar text");
 check(recorded && NativeGpuDocument(list),"styled document actual Vulkan pixels and GPU ownership");
 UiLabel label; label.SetRect(0,0,500,80); label.EnableRich().SetSelectable();
 label.AddBulletSpan().AddTextSpan(ToUtf8(arabic.Left(2)),Red()).AddTextSpan(ToUtf8(arabic.Mid(2)),Blue());
 label.Layout(); UiDisplayList rich;
 WString rich_text; rich_text.Cat(0xfffc); rich_text.Cat(arabic);
 Font rich_font=label.GetStyle().metrics.use_text_font?label.GetStyle().metrics.text_font:label.GetStyle().font; rich_font.RealizeStd();
 Vector<UiTextStyleRange> rich_styles;
 auto& left_style=rich_styles.Add(); left_style.from=1; left_style.length=2; left_style.font=rich_font; left_style.ink=Red();
 auto& right_style=rich_styles.Add(); right_style.from=3; right_style.length=2; right_style.font=rich_font; right_style.ink=Blue();
 Vector<UiTextInlineObject> rich_objects; auto& bullet=rich_objects.Add(); bullet.position=0; bullet.size=Sizef(DPI(6),DPI(6));
 bullet.baseline=min<double>(DPI(6),max(0.0,(DPI(6)-rich_font.GetHeight())*.5+rich_font.GetAscent()));
 auto rich_native=UiTextLayout::CreateStyled(rich_text,rich_font,rich_styles,rich_objects);
 if(!rich_native.IsValid()) Cout()<<"direct_rich_error="<<rich_native.GetError()<<" font="<<rich_font<<" baseline="<<bullet.baseline<<EOL;
 check(rich_native.IsValid(),"bullet and split Arabic native layout");
 bool rich_ok=RecordCtrlDisplayList(label,rich,error,&report);
 if(!rich_ok) Cout()<<"rich_error="<<error<<" unsupported="<<report.unsupported_operation<<EOL;
 check(rich_ok,"rich label recording");
 int rich_runs=0; bool contextual=false;
 for(int i=0;i<rich.GetCount();++i) if(rich[i].type==UiDisplayOpType::DrawShapedText) {
  ++rich_runs; contextual|=rich[i].shaped_text->GetGlyphCount()<4;
 }
 check(rich_runs==1 && contextual,"rich styles share contextual Arabic line shaping");
 UiDoc table_doc; table_doc.SetRect(0,0,640,420); table_doc.Layout();
 String id=table_doc.InsertTable(1,1,0); UiDocTableCell cell; auto& run=cell.runs.Add(); run.type="text"; run.text=combining;
 check(!id.IsEmpty() && table_doc.Model().SetTableCell(id,0,0,cell),"Unicode table cell model");
 table_doc.Layout(); auto lines=table_doc.GetLayoutLines();
 if(!lines.IsEmpty()) {
  Point click(lines[0].rect.left+table_doc.GetStyle().table_cell_padding+1,lines[0].rect.bottom+table_doc.GetStyle().table_cell_padding+3);
  table_doc.LeftDown(click,0); table_doc.LeftUp(click,0); table_doc.Key(K_DELETE,1);
  UiDocTable changed; check(table_doc.GetTable(id,changed) && changed.rows[0].cells[0].runs.GetCount()==1 &&
       changed.rows[0].cells[0].runs[0].text==S({'x'}),"table Delete removes whole grapheme");
 } else check(false,"table line layout");
 Cout()<<"document_text_checks="<<checks<<" passed="<<(ok?1:0)<<EOL; return ok;
}
