#include <CtrlLib/CtrlLib.h>
#include <Ui/UiTextLayoutInternal.h>
#include <Ui/UiTextLayout.h>
#include <RenderCore/GpuTextLayout.h>
#include <cmath>
using namespace Upp;
using namespace Upp::UiTextInternal;
static bool ok=true;
static void Check(bool pass,const char *why) { if(!pass) { ok=false; Cout()<<"FAIL "<<why<<EOL; } }
static WString Scalars(std::initializer_list<int> values) {
	WString text; for(int value:values) text.Cat(value); return text;
}
bool TestShapedReference();
bool TestUnicodeEditing();
bool TestStyledText();
bool TestDocumentText();
GUI_APP_MAIN
{
	String error; int cases=0;
	auto breaks=[&](const WString& text,std::initializer_list<int> expected) {
		Encoding encoded; Vector<int> actual; Vector<int> want;
		for(int v:expected) want.Add(v);
		bool pass=encoded.Build(text,error);
		pass = pass && GraphemeBreaks(encoded,"en",actual,error) && actual==want;
		Check(pass,"system grapheme boundaries");
		Cout()<<"grapheme_case="<<cases++<<" actual="<<AsString(actual)<<" error="<<error<<EOL;
		for(int i=0;i<encoded.scalar_to_utf16.GetCount();++i)
			Check(encoded.utf16_to_scalar[encoded.scalar_to_utf16[i]]==i,"scalar UTF16 round trip");
	};
	breaks(Scalars({}),{0});
	breaks(Scalars({'f','i'}),{0,1,2}); // Typographic ligatures are not one editing grapheme.
	breaks(Scalars({'e',0x301,'x'}),{0,2,3});
	breaks(Scalars({0x1f469,0x200d,0x1f52c}),{0,3});
	breaks(Scalars({0x1f1ec,0x1f1e7,'x'}),{0,2,3});
	breaks(Scalars({0x915,0x94d,0x937,0x93f}),{0,4});
	breaks(Scalars({'a','\r','\n','b'}),{0,1,3,4});
	auto direction=[&](WString text,bool expected) {
		Encoding encoded; bool rtl=!expected;
		Check(encoded.Build(text,error) && BaseDirection(encoded,rtl,error) && rtl==expected,"UAX9 paragraph direction");
	};
	direction(Scalars({0x633,0x644,0x627,0x645}),true);
	direction(Scalars({'a','b','c',' ',0x5d0,0x5d1}),false);
	direction(Scalars({0x2067,0x633,0x644,0x627,0x645,0x2069,'a'}),false);
	Encoding invalid; Vector<int> bounds;
	Check(!invalid.Build(Scalars({0xd800}),error) && !GraphemeBreaks(invalid,"",bounds,error),"reject surrogate scalar");
	Check(!invalid.Build(Scalars({0x110000}),error),"reject oversized scalar");
	Encoding words; Check(words.Build(Scalars({'o','n','e',' ','t','w','o'}),error) &&
		WordBreaks(words,"en",bounds,error) && bounds.GetCount()==4 && bounds[1]==3 && bounds[2]==4,"word boundaries");
	ClearUiTextLayoutCache();
	Font font=SansSerif(24);
	WString arabic=Scalars({0x633,0x644,0x627,0x645});
	auto shaped=UiTextLayout::Create(arabic,font);
	Check(shaped.IsValid(),"Arabic layout builds");
	Cout()<<"arabic_glyphs="<<shaped.GetGlyphCount()<<" size="<<shaped.GetSize()<<" error="<<shaped.GetError()<<EOL;
	Check(shaped.GetGlyphCount()>0 && shaped.GetGlyphCount()<=arabic.GetCount(),"contextual Arabic glyph count");
	if(shaped.IsValid()) {
		Check(shaped.GetCaret(0).x>shaped.GetCaret(4).x,"RTL caret direction");
		for(int i=0;i<=4;++i)
			Cout()<<"arabic_caret="<<i<<" point="<<shaped.GetCaret(i)<<" upstream="<<shaped.GetCaret(i,true)<<EOL;
		Check(shaped.GetCaret(1).x>shaped.GetCaret(2).x,"separate editing stops inside lam-alef ligature");
		Check(!shaped.GetSelection(1,3).IsEmpty(),"Arabic selection geometry");
		auto partial=shaped.GetSelection(1,2);
		Cout()<<"partial_selection_rects="<<partial.GetCount()<<EOL;
		for(const auto& r:partial) Cout()<<"partial_rect="<<r<<" width="<<r.Width()<<EOL;
		Check(partial.GetCount()==1 && fabs(partial[0].Width()-
		      fabs(shaped.GetCaret(1).x-shaped.GetCaret(2).x))<.01,"selection clips half ligature advance");
	}
	auto latin=UiTextLayout::Create(Scalars({'f','i'}),font);
	Check(latin.IsValid() && latin.GetCaret(0).x<latin.GetCaret(2).x,"LTR layout and caret");
	Check(latin.NextGrapheme(0,1)==1 && latin.NextGrapheme(1,1)==2,"ligatures retain separate editing graphemes");
	auto combined=UiTextLayout::Create(Scalars({'e',0x301,'x'}),font);
	Check(combined.IsValid() && combined.NextGrapheme(0,1)==2 && combined.NextGrapheme(2,-1)==0,"combining mark editing boundary");
	auto indic=UiTextLayout::Create(Scalars({0x915,0x94d,0x937,0x93f}),font);
	Check(indic.IsValid() && indic.GetGlyphCount()>0 && indic.NextGrapheme(0,1)==4,"Indic shaping and grapheme");
	auto multiline=UiTextLayout::Create(Scalars({'a','\n',0x633,0x644,0x627,0x645}),font);
	Check(multiline.IsValid() && multiline.GetCaret(2).y>multiline.GetCaret(0).y &&
	      multiline.GetCaret(2).x>multiline.GetCaret(6).x,"independent paragraph direction");
	UiTextHit left; left.position=0;
	UiTextHit right=combined.MoveVisual(left,1);
	Check(right.position==2 && combined.MoveVisual(right,-1).position==0,"visual movement preserves combining graphemes");
	UiTextHit arabic_start; arabic_start.position=0;
	Check(shaped.MoveVisual(arabic_start,-1).position>0,"RTL visual left moves into logical paragraph");
	auto emoji=UiTextLayout::Create(Scalars({'A',0x1f600,'B'}),font);
	Check(emoji.IsValid() && emoji.GetCaret(2,true).x>emoji.GetCaret(1).x,"upstream caret respects supplementary scalar");
	UiTextLayoutOptions wrap; wrap.wrap=true; wrap.width=45;
	auto wrapped=UiTextLayout::Create(Scalars({'o','n','e',' ','t','w','o',' ','t','h','r','e','e'}),font,wrap);
	Check(wrapped.IsValid() && wrapped.GetCaret(12).y>0,"wrapped caret uses native line geometry");
	for(int pos=0;pos<=combined.GetLength();pos=combined.NextGrapheme(pos,1)) {
		Pointf p=combined.GetCaret(pos);
		UiTextHit hit=combined.HitTest(p+Pointf(0,.5));
		Check(hit.position==pos,"combining caret/hit round trip");
		if(pos==combined.GetLength()) break;
	}
	WString fit_source=Scalars({0x1f469,0x200d,0x1f52c,'e',0x301,'x'});
	auto fit_layout=UiTextLayout::Create(fit_source,font);
	for(int width=0;width<UiGetTextSize(fit_source,font).cx;++width) {
		WString fitted=UiFitText(fit_source,font,width);
		if(fitted.IsEmpty()) continue;
		Check(UiGetTextSize(fitted,font).cx<=width,"fitted text respects width");
		int prefix=fitted.GetCount()-3; bool boundary=prefix==0;
		for(int p=0;p<fit_source.GetCount();) {
			p=fit_layout.NextGrapheme(p,1); boundary=boundary || p==prefix;
		}
		Check(boundary,"elision never splits emoji or combining grapheme");
	}
	const auto before=GetUiTextLayoutStats();
	auto cached=UiTextLayout::Create(arabic,font);
	Check(cached.IsValid() && cached.GetGpuText()==shaped.GetGpuText() &&
	      GetUiTextLayoutStats().hits==before.hits+1,"layout cache retains immutable snapshot");
	Check(!UiTextLayout::Create(Scalars({0xd800}),font).IsValid(),"layout rejects invalid scalar");
	UiTextLayoutOptions bad; bad.width=0;
	Check(!UiTextLayout::Create(arabic,font,bad).IsValid(),"layout options fail explicitly");
	ok=TestShapedReference() && ok;
	ok=TestUnicodeEditing() && ok;
	ok=TestStyledText() && ok;
	ok=TestDocumentText() && ok;
	Font variable=SansSerif(28).FaceName("Bahnschrift");
	auto variable_regular=UiTextLayout::Create(WString("Variable"),variable);
	auto variable_bold=UiTextLayout::Create(WString("Variable"),Font(variable).Bold());
	Check(variable.GetFaceName()=="Bahnschrift" && variable_regular.IsValid() && variable_bold.IsValid(),
	      "installed variable font layout");
	if(variable_regular.IsValid() && variable_bold.IsValid())
		Check(variable_regular.GetGpuText()->runs[0].face->GetIdentity()!=variable_bold.GetGpuText()->runs[0].face->GetIdentity(),
		      "font variation and style use distinct atlas identities");
	Check(UiTextLayout::Create(WString("Default font"),Font(Null)).IsValid(),"null font inherits standard font");
	GpuShapedText empty; Check(empty.IsValid(),"empty shaped snapshot valid");
	GpuShapedText invalidRun; invalidRun.runs.Add();
	Check(!invalidRun.IsValid(&error),"missing font source rejected");
	Cout()<<"unicode_version="<<UnicodeVersion()<<" grapheme_cases="<<cases<<EOL;
	if(ok) Cout()<<"UiTextLayoutTest Unicode foundation passed"<<EOL; else SetExitCode(1);
}
