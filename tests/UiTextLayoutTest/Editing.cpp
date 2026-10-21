#include <Ui/Ui.h>
#include <RenderCtrlBridge/RenderCtrlBridge.h>
using namespace Upp;
namespace {
WString S(std::initializer_list<int> values) { WString s; for(int cp:values) s.Cat(cp); return s; }
class EditProbe : public UiLineEdit {
public:
 int64 Hit(Point p) const { return GetMousePos(p); }
 Rect CaretAt(int pos) const { return GetCaretRect(pos); }
};
}
bool TestUnicodeEditing()
{
 bool ok=true; int checks=0;
 auto check=[&](bool pass,const char *why) { ++checks; if(!pass) { ok=false; Cout()<<"FAIL "<<why<<EOL; } };
 EditProbe edit; edit.SetRect(0,0,320,80);
 WString combining=S({'e',0x301,'x'});
 edit.SetText(combining); edit.SetCursor(3);
 check(edit.Key(K_BACKSPACE,1) && edit.GetText()==S({'e',0x301}) && edit.GetCursor()==2,"Backspace preserves combining grapheme");
 edit.Key(K_BACKSPACE,1); check(edit.IsEmpty() && edit.GetCursor()==0,"Backspace removes whole combining grapheme");
 edit.Undo(); check(edit.GetText()==S({'e',0x301}),"Unicode deletion undo");
 edit.Undo(); check(edit.GetText()==combining,"second Unicode undo");
 edit.SetText(combining); edit.SetCursor(0); edit.Key(K_DELETE,1);
 check(edit.GetText()==S({'x'}),"Delete removes combining grapheme");
 edit.SetText(combining); edit.SetCursor(0); edit.Key(K_RIGHT,1);
 check(edit.GetCursor()==2,"Right arrow skips combining interior");
 edit.Key(K_LEFT,1); check(edit.GetCursor()==0,"Left arrow returns combining start");
 edit.SetSelection(0,1); check(edit.GetSelectionW()==S({'e',0x301}),"selection expands partial grapheme");
 edit.SetText(S({0x1f469,0x200d,0x1f52c})); edit.SetCursor(3); edit.Key(K_BACKSPACE,1);
 check(edit.IsEmpty(),"Backspace removes complete emoji ZWJ sequence");
 edit.Key(0x1f600,1); check(edit.GetText()==S({0x1f600}) && edit.GetCursor()==1,"supplementary Unicode keyboard input");
 edit.SetText(S({0x915,0x94d,0x937,0x93f})); edit.SetCursor(4); edit.Key(K_BACKSPACE,1);
 check(edit.IsEmpty(),"Indic cluster deletion");
 WString arabic=S({0x633,0x644,0x627,0x645});
 edit.SetText(arabic); edit.SetCursor(0); Rect first=edit.GetCaret(); edit.Key(K_LEFT,1);
 check(edit.GetCursor()>0 && edit.GetCaret().left<first.left,"RTL left arrow moves visually left");
 for(int pos:{0,1,2,3,4}) {
  Rect caret=edit.CaretAt(pos);
  check(edit.Hit(Point(caret.left,caret.CenterPoint().y))==pos,"RTL caret and mouse hit agree");
 }
 edit.SetSelection(1,3);
 UiDisplayList list; String error; CtrlDisplayListRecordReport report;
 bool recorded=RecordCtrlDisplayList(edit,list,error,&report);
 check(recorded && !report.HasUnsupportedOperation(),"selected Arabic editor records semantic drawing");
 int shaped=0,legacy=0; std::shared_ptr<const GpuShapedText> original;
 if(recorded) for(int i=0;i<list.GetCount();++i) {
  if(list[i].type==UiDisplayOpType::DrawShapedText) {
   ++shaped; if(!original) original=list[i].shaped_text;
   check(original==list[i].shaped_text,"selection reuses full shaped snapshot");
  }
  legacy+=list[i].type==UiDisplayOpType::DrawText;
 }
 check(shaped>=2 && legacy==0,"selected editor does not split text into scalar draws");
 UiPasswordEdit password; password.SetRect(0,0,320,60);
 password.SetText(S({0x1f469,0x200d,0x1f52c})); password.SetCursor(3); password.Key(K_BACKSPACE,1);
 check(password.IsEmpty(),"masked editor uses original grapheme boundaries");
 UiMultiEdit multi; multi.SetRect(0,0,320,180);
 multi.SetText(S({'a','b','c','\n',0x5d0,0x5d1,0x5d2})); multi.SetCursor(4);
 multi.Key(K_UP,1); check(multi.GetCursor()<=3,"vertical navigation uses other line geometry");
 multi.Key(K_DOWN,1); check(multi.GetCursor()>=4,"vertical navigation returns to RTL line");
 edit.SetText(S({'o','n','e',0x3000,'t','w','o'})); edit.SetCursor(0);
 edit.Key(K_CTRL_RIGHT,1); check(edit.GetCursor()==4,"Ctrl Right skips Unicode whitespace");
 edit.Key(K_CTRL_LEFT,1); check(edit.GetCursor()==0,"Ctrl Left returns Unicode word start");
 WString many;
 for(int i=0;i<1000;++i) { many+=AsString(i).ToWString(); many.Cat('a'); many.Cat('b'); many.Cat('c'); many.Cat('\n'); }
 multi.SetText(many); multi.SetCursor(0);
 uint64 misses=GetUiTextLayoutStats().misses;
 multi.Key('x',1);
 check(GetUiTextLayoutStats().misses<=misses+2,"editing one line preserves other line layouts beyond cache capacity");
 misses=GetUiTextLayoutStats().misses;
 multi.Key(K_ENTER,1);
 check(GetUiTextLayoutStats().misses<=misses+3,"newline insertion preserves shifted line layouts");
 WString mixed=S({'A',' ',0x5d0,0x5d1,0x5d2});
 edit.SetText(mixed); edit.SetCursor(0); int previous=edit.GetCaret().left;
 bool monotonic=true;
 for(int i=0;i<20;++i) {
  edit.Key(K_RIGHT,1); int x=edit.GetCaret().left;
  monotonic=monotonic && x>=previous; previous=x;
 }
 check(monotonic,"Right arrow never loops back at a mixed-direction visual edge");
 UiLabel label; label.SetRect(0,0,320,80);
 label.SetText(ToUtf8(arabic)); label.SetSelectable(); label.Key(K_CTRL_A,1);
 UiDisplayList label_list; CtrlDisplayListRecordReport label_report;
 bool label_ok=RecordCtrlDisplayList(label,label_list,error,&label_report);
 check(label_ok && !label_report.HasUnsupportedOperation(),"selected Unicode label records");
 int label_shaped=0,label_legacy=0;
 std::shared_ptr<const GpuShapedText> label_snapshot;
 if(label_ok) for(int i=0;i<label_list.GetCount();++i) {
  if(label_list[i].type==UiDisplayOpType::DrawShapedText) {
   ++label_shaped;
   if(!label_snapshot) label_snapshot=label_list[i].shaped_text;
   check(label_snapshot==label_list[i].shaped_text && label_snapshot->GetGlyphCount()==3,"label selection retains complete Arabic context");
  }
  label_legacy+=label_list[i].type==UiDisplayOpType::DrawText;
 }
 check(label_shaped>=2 && !label_legacy,"Unicode label selection uses shaped drawing only");
 for(int key:{K_RIGHT,K_BACKSPACE,K_DELETE,K_DOWN}) {
  One<UiMultiEdit> owner; owner.Create(); owner->SetRect(0,0,320,180);
  owner->SetText(WString("abc\nxyz")); owner->SetCursor(key==K_BACKSPACE?2:0);
  if(key==K_DELETE) owner->SelectAll();
  bool valid_at_callback=true;
  owner->WhenSelection=[&] {
   valid_at_callback=owner->GetCursor()<=owner->GetText().GetCount();
   owner.Clear();
  };
  UiMultiEdit *borrowed=~owner;
  bool handled=borrowed->Key(key,1);
  check(handled && !owner && valid_at_callback,"selection callback observes valid cursor and may destroy editor during keyboard input");
 }
 Cout()<<"unicode_edit_checks="<<checks<<" passed="<<(ok?1:0)<<EOL;
 return ok;
}
