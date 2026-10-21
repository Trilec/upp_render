#include <Ui/Ui.h>
#include <imm.h>
#include <GpuRender/Win32GpuIme.h>
#include <GpuRender/GpuRender.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <RenderCtrlBridge/RenderCtrlBridge.h>
using namespace Upp;
namespace {
WString S(std::initializer_list<int> values) { WString s; for(int cp:values) s.Cat(cp); return s; }
}
bool TestUiTextImageGpu(const UiDisplayList& list,const char *path);
bool TestImeComposition()
{
 bool ok=true; int checks=0;
 auto check=[&](bool pass,const char *why) { ++checks; if(!pass) { ok=false; Cout()<<"FAIL ime "<<why<<EOL; } };
 GpuImeComposition value; value.text=S({0x633,0x644,0x627,0x645}); value.caret=2;
 value.attributes.SetCount(4,ATTR_INPUT); value.attributes[1]=value.attributes[2]=ATTR_TARGET_CONVERTED;
 check(value.IsValid(),"bounded scalar payload");
 Vector<char16_t> utf16; for(int v:{65,0xd83d,0xde00,66}) utf16.Add((char16_t)v);
 Vector<byte> attrs; for(int v:{0,1,1,2}) attrs.Add((byte)v);
 GpuImeComposition decoded;
 check(GpuImeComposition::DecodeUtf16(utf16,2,attrs,decoded) && decoded.text==S({'A',0x1f600,'B'}) &&
       decoded.caret==1 && decoded.attributes.GetCount()==3 && decoded.attributes[1]==1,"UTF16 cursor inside surrogate snaps to scalar start");
 check(GpuImeComposition::DecodeUtf16(utf16,3,attrs,decoded) && decoded.caret==2,"UTF16 cursor after surrogate");
 Vector<char16_t> invalid; invalid.Add((char16_t)0xd800);
 WString unchanged=decoded.text;
 check(!GpuImeComposition::DecodeUtf16(invalid,1,Vector<byte>(),decoded) && decoded.text==unchanged,"invalid native text rejected without partial mutation");
 attrs.Remove(0); check(!GpuImeComposition::DecodeUtf16(utf16,2,attrs,decoded),"native attributes must match UTF16 units");
 UiLineEdit edit; edit.SetRect(0,0,360,50); edit.SetText(WString("before")); edit.SetSelection(1,4);
 WString original=edit.GetText(); Rect caret_before=edit.GetCaret();
 check(edit.AcceptsGpuIme() && edit.UpdateGpuComposition(value) && edit.GetText()==original,"preedit leaves model and selection unchanged");
 Rect composition_caret=edit.GetCaret(); check(!IsNull(composition_caret) && composition_caret.GetHeight()>0,"composition caret geometry");
 UiDisplayList list; String error; CtrlDisplayListRecordReport report;
 check(RecordCtrlDisplayList(edit,list,error,&report) && !report.HasUnsupportedOperation(),"preedit records semantic glyphs");
 bool shaped=false; for(int i=0;i<list.GetCount();++i)
  if(list[i].type==UiDisplayOpType::DrawShapedText && list[i].shaped_text->GetGlyphCount()==3) shaped=true;
 check(shaped,"Arabic preedit is a complete contextual glyph run");
 check(TestUiTextImageGpu(list,"build/UiTextLayout-ime.png"),"preedit actual Vulkan pixels and GPU resource cleanup");
 edit.CancelGpuComposition(); check(edit.GetText()==original && edit.GetCaret()==caret_before,"cancel restores caret without editing text");
 edit.UpdateGpuComposition(value); check(edit.CommitGpuComposition(value.text) && edit.GetText()==WString("b")+value.text+WString("re"),"IME commit replaces selected text");
 edit.Undo(); check(edit.GetText()==original,"IME commit and replacement undo together");
 edit.SetReadOnly(); check(!edit.AcceptsGpuIme() && !edit.UpdateGpuComposition(value) && !edit.CommitGpuComposition(value.text),"readonly input rejects composition");
 UiPasswordEdit password; password.SetRect(0,0,300,50);
 check(!password.AcceptsGpuIme() && !password.UpdateGpuComposition(value),"password does not expose composition text");
 UiIntEdit integer; integer.SetRect(0,0,300,50); integer.SetText(WString());
 check(integer.CommitGpuComposition(WString("a12")) && integer.GetText()==WString("12"),"numeric IME commit retains derived Key filter");
 integer.SetText(WString()); integer.CommitGpuComposition(S({0x1f600}));
 check(integer.GetText().IsEmpty(),"numeric IME rejects supplementary non-digits");
 UiDoc doc; doc.SetRect(0,0,640,350); doc.SetText("before"); doc.SetSelection(UiDocRange(1,4)); doc.Layout();
 check(doc.UpdateGpuComposition(value) && doc.GetText()=="before","document preedit is transient");
 check(RecordCtrlDisplayList(doc,list,error,&report),"document preedit recording");
 check(doc.CommitGpuComposition(WString()) && doc.GetText()=="before","empty result does not remove document selection");
 check(doc.CommitGpuComposition(value.text) && doc.GetTextW()==WString("b")+value.text+WString("re"),"document IME commit");
 doc.Undo(); check(doc.GetText()=="before","document IME transaction undo");
 GpuImeComposition bad; bad.text.Cat(0xd800); bad.caret=1;
 check(!edit.UpdateGpuComposition(bad) && !doc.UpdateGpuComposition(bad),"invalid payload rejected by controls");
 One<UiLineEdit> closing; closing.Create(); closing->SetRect(0,0,300,40);
 closing->WhenChange=[&] { closing.Clear(); };
 auto *client=dynamic_cast<GpuImeClient *>(~closing);
 check(client && client->CommitGpuComposition(WString("ab")) && !closing,"commit callback may delete originating editor");
 // Exercise the actual native root/child lifetime and message seam. These
 // generated messages do not substitute for live OS input-method acceptance.
 {
  const int devices=VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics().device_live_count;
  GpuTopWindow root; root.SetRequireGpu().SetValidation(true).SetRect(0,0,700,420);
  UiLineEdit field,other; field.SetRect(12,12,500,36); other.SetRect(12,70,500,36);
  root.Add(field); root.Add(other); root.Open(); field.SetFocus(); Ctrl::ProcessEvents();
  check(root.IsGpuReady() && field.HasFocus(),"native Vulkan host and focused editor");
  auto children=[&]() { int n=0; for(Ctrl *c=root.GetFirstChild();c;c=c->GetNext()) ++n; return n; };
  const int child_count=children(),host_devices=VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics().device_live_count;
  Rect normal=field.GetCaret();
  check(field.UpdateGpuComposition(value) && field.GetCaret().bottom>field.GetSize().cy &&
        children()==child_count+1 &&
        VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics().device_live_count==host_devices,
        "native preedit root child escapes compact field and shares device");
  field.SetReadOnly();
  check(children()==child_count && field.GetCaret()==normal,"read-only transition removes active overlay");
  field.SetEditable(); field.UpdateGpuComposition(value); field.Disable();
  check(children()==child_count && !field.AcceptsGpuIme(),"disabled transition removes active overlay");
  field.Enable(); field.SetFocus(); field.UpdateGpuComposition(value); other.SetFocus();
  check(children()==child_count,"focus transfer cancels active overlay");
  field.SetFocus(); field.UpdateGpuComposition(value);
  LPARAM flags=ISC_SHOWUICOMPOSITIONWINDOW; LRESULT result=123;
  check(!HandleWin32GpuIme(root,root.GetHWND(),WM_IME_SETCONTEXT,1,flags,result) &&
        !(flags&ISC_SHOWUICOMPOSITIONWINDOW),"host suppresses legacy application-owned composition popup");
  flags=0; check(HandleWin32GpuIme(root,root.GetHWND(),WM_IME_ENDCOMPOSITION,0,flags,result) &&
                result==0 && children()==child_count,"native end-composition cancels overlay");
  field.SetText(WString()); field.SetFocus();
  SendMessageW(root.GetHWND(),WM_CHAR,0xd83d,1);
  SendMessageW(root.GetHWND(),WM_CHAR,0xde00,1);
  check(field.GetText()==S({0x1f600}),"native UTF16 WM_CHAR pair inserts one scalar");
  field.UpdateGpuComposition(value); root.Close();
  check(children()==child_count &&
        VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics().device_live_count==devices,
        "closing native root releases composition and GPU device");
 }
 doc.ReadOnly(false); Rect doc_caret=doc.GetCaret(); doc.UpdateGpuComposition(value); doc.Disable();
 check(!doc.AcceptsGpuIme() && doc.GetCaret()==doc_caret,"document disabled transition cancels preedit");
 Cout()<<"ime_composition_checks="<<checks<<" passed="<<(ok?1:0)<<EOL; return ok;
}
