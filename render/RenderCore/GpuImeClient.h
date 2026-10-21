#pragma once
#include <Core/Core.h>
namespace Upp {
// Scalar-indexed, transient IME state; it never replaces the editable model.
// Attributes use the Windows-compatible input/target/converted values 0..5.
struct GpuImeComposition {
 WString text;
 int caret=0;
 Vector<byte> attributes;
 static bool DecodeUtf16(const Vector<char16_t>& source,int cursor,const Vector<byte>& attributes,
                         GpuImeComposition& out,String *error=nullptr);
 bool IsValid() const {
  if(text.GetCount()>32768 || caret<0 || caret>text.GetCount() ||
     (!attributes.IsEmpty() && attributes.GetCount()!=text.GetCount())) return false;
  for(int cp:text) if(cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
  for(byte attribute:attributes) if(attribute>5) return false;
  return true;
 }
};
// A root may offer an ordinary child overlay sharing its existing GPU device.
class GpuImeOverlayHost {
public:
 virtual ~GpuImeOverlayHost() {}
 virtual bool IsGpuImeOverlayReady() const=0;
};
// Optional GUI-thread capability. Native hosts deliver composition to the
// focused Ui control; normal Key/commit validation remains the control's owner.
class GpuImeClient {
public:
 virtual ~GpuImeClient() {}
 virtual bool AcceptsGpuIme() const=0;
 virtual bool UpdateGpuComposition(const GpuImeComposition& composition)=0;
 virtual bool CommitGpuComposition(const WString& text)=0;
 virtual void CancelGpuComposition()=0;
};
}
