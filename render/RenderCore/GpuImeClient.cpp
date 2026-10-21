#include "GpuImeClient.h"
namespace Upp {
bool GpuImeComposition::DecodeUtf16(const Vector<char16_t>& source,int cursor,const Vector<byte>& attrs,
                                    GpuImeComposition& out,String *error)
{
 auto fail=[&](const char *why) { if(error) *error=why; return false; };
 if(source.GetCount()>65536 || cursor<0 || cursor>source.GetCount() ||
    (!attrs.IsEmpty() && attrs.GetCount()!=source.GetCount())) return fail("invalid IME UTF16 payload bounds");
 GpuImeComposition value; value.caret=0;
 for(int i=0;i<source.GetCount();) {
  if(value.text.GetCount()>=32768) return fail("IME composition exceeds scalar budget");
  int first=i,cp=source[i++];
  if(cp>=0xd800 && cp<=0xdbff) {
   if(i>=source.GetCount() || source[i]<0xdc00 || source[i]>0xdfff) return fail("unpaired IME high surrogate");
   cp=0x10000+((cp-0xd800)<<10)+(source[i++]-0xdc00);
  }
  else if(cp>=0xdc00 && cp<=0xdfff) return fail("unpaired IME low surrogate");
  if(cursor>=i) value.caret=value.text.GetCount()+1;
  value.text.Cat(cp);
  if(!attrs.IsEmpty()) value.attributes.Add(attrs[first]<=5?attrs[first]:(byte)0);
 }
 if(!value.IsValid()) return fail("invalid decoded IME composition");
 out.text=value.text; out.caret=value.caret; out.attributes=pick(value.attributes);
 if(error) error->Clear(); return true;
}
}
