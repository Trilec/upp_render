#include "Win32GpuIme.h"
#ifdef PLATFORM_WIN32
#include <imm.h>
namespace Upp {
namespace {
bool ReadUtf16(HIMC context,DWORD kind,Vector<char16_t>& result)
{
 LONG bytes=ImmGetCompositionStringW(context,kind,nullptr,0);
 if(bytes<0 || bytes>131072 || bytes%2) return false;
 result.SetCount(bytes/2);
 return !bytes || ImmGetCompositionStringW(context,kind,result.Begin(),bytes)==bytes;
}
bool ReadComposition(HIMC context,GpuImeComposition& value)
{
 Vector<char16_t> raw; Vector<byte> attrs;
 if(!ReadUtf16(context,GCS_COMPSTR,raw)) return false;
 LONG cursor=ImmGetCompositionStringW(context,GCS_CURSORPOS,nullptr,0);
 if(cursor<0) cursor=raw.GetCount();
 LONG count=ImmGetCompositionStringW(context,GCS_COMPATTR,nullptr,0);
 if(count>65536) return false;
 if(count>0) {
  attrs.SetCount(count);
  if(ImmGetCompositionStringW(context,GCS_COMPATTR,attrs.Begin(),count)!=count) return false;
 }
 return GpuImeComposition::DecodeUtf16(raw,minmax<int>(cursor,0,raw.GetCount()),attrs,value);
}
void PositionCandidate(HWND hwnd,Ctrl& focus)
{
 Ptr<Ctrl> alive(&focus);
 HIMC context=ImmGetContext(hwnd); if(!context) return;
 if(!alive) { ImmReleaseContext(hwnd,context); return; }
 Rect caret=focus.GetCaret();
 if(!IsNull(caret)) {
  Point origin=focus.GetScreenView().TopLeft();
  POINT at{origin.x+caret.left,origin.y+caret.top}; ScreenToClient(hwnd,&at);
  CANDIDATEFORM form{}; form.dwStyle=CFS_EXCLUDE;
  form.ptCurrentPos=at; form.rcArea={at.x,at.y,at.x+max(1,caret.Width()),at.y+max(1,caret.Height())};
  for(DWORD i=0;i<4;++i) { form.dwIndex=i; ImmSetCandidateWindow(context,&form); }
 }
 ImmReleaseContext(hwnd,context);
}
}
bool HandleWin32GpuIme(Ctrl& root,HWND hwnd,UINT message,WPARAM,LPARAM& lparam,LRESULT& result)
{
 if(message!=WM_IME_SETCONTEXT && message!=WM_IME_STARTCOMPOSITION &&
    message!=WM_IME_COMPOSITION && message!=WM_IME_ENDCOMPOSITION && message!=WM_KILLFOCUS) return false;
 Ptr<Ctrl> root_alive(&root),focus=Ctrl::GetFocusCtrl();
 if(!focus || focus->GetTopCtrl()!=&root) return false;
 auto *client=dynamic_cast<GpuImeClient *>(~focus); if(!client) return false;
 if(message==WM_KILLFOCUS) { client->CancelGpuComposition(); return false; }
 if(!client->AcceptsGpuIme()) {
  client->CancelGpuComposition();
  if(message==WM_IME_COMPOSITION || message==WM_IME_STARTCOMPOSITION || message==WM_IME_ENDCOMPOSITION) { result=0; return true; }
  return false;
 }
 if(message==WM_IME_SETCONTEXT) { lparam&=~(LPARAM)ISC_SHOWUICOMPOSITIONWINDOW; return false; }
 result=0;
 if(message==WM_IME_STARTCOMPOSITION || message==WM_IME_ENDCOMPOSITION || lparam==0) {
  client->CancelGpuComposition(); return true;
 }
 HIMC context=ImmGetContext(hwnd); if(!context) { client->CancelGpuComposition(); return true; }
 bool has_result=(lparam&GCS_RESULTSTR)!=0,has_composition=(lparam&(GCS_COMPSTR|GCS_COMPATTR|GCS_COMPCLAUSE|GCS_CURSORPOS))!=0;
 GpuImeComposition committed,composition; Vector<char16_t> raw; Vector<byte> no_attributes;
 bool result_valid=!has_result || (ReadUtf16(context,GCS_RESULTSTR,raw) &&
    GpuImeComposition::DecodeUtf16(raw,raw.GetCount(),no_attributes,committed));
 bool composition_valid=!has_composition || ReadComposition(context,composition);
 // Release before callbacks: a commit may synchronously destroy this window.
 ImmReleaseContext(hwnd,context);
 if(!root_alive || !focus || Ctrl::GetFocusCtrl()!=~focus || focus->GetTopCtrl()!=~root_alive) return true;
 client=dynamic_cast<GpuImeClient *>(~focus); if(!client) return true;
 if(!result_valid || !composition_valid) { client->CancelGpuComposition(); return true; }
 if(has_result) {
  client->CommitGpuComposition(committed.text);
  if(!root_alive || !focus || Ctrl::GetFocusCtrl()!=~focus) return true;
 }
 if(has_composition) {
  client=dynamic_cast<GpuImeClient *>(~focus);
  if(client) client->UpdateGpuComposition(composition);
 }
 if(root_alive && focus && Ctrl::GetFocusCtrl()==~focus && IsWindow(hwnd)) PositionCandidate(hwnd,*focus);
 return true;
}
}
#endif
