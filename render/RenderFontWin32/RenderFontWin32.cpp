#include "RenderFontWin32.h"

#if defined(PLATFORM_WIN32) && defined(flagCFONTS)
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>

namespace Upp {
namespace {

template <class T> struct FontCom : Moveable<FontCom<T>> {
	T *p = nullptr;
	FontCom() = default;
	FontCom(FontCom&& other) noexcept : p(other.p) { other.p = nullptr; }
	FontCom(const FontCom&) = delete;
	FontCom& operator=(const FontCom&) = delete;
	~FontCom() { if(p) p->Release(); }
	T **Put() { ASSERT(!p); return &p; }
	T *operator->() const { return p; }
};

struct FontFaceEntry : Moveable<FontFaceEntry> {
	int64 key = 0;
	uint64 used = 0;
	FontCom<IDWriteFontFace> face;
	DWRITE_FONT_METRICS metrics{};
	double em = 12;
	double xscale = 1;
	bool symbol = false;
	bool fixedpitch = false;
};

struct NativeFontEntry : Moveable<NativeFontEntry> {
	int64 key = 0;
	int angle = 0;
	HFONT handle = nullptr;
	~NativeFontEntry() { if(handle) ::DeleteObject(handle); }
};

struct FontService {
	Mutex mutex;
	FontCom<IDWriteFactory> factory;
	FontCom<IDWriteFontCollection> collection;
	Vector<FontFaceEntry> faces;
	Vector<NativeFontEntry> native;
	RenderFontWin32Stats stats;
	uint64 serial = 0;

	FontService() {
		if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
		                             __uuidof(IDWriteFactory),
		                             reinterpret_cast<IUnknown **>(factory.Put()))) ||
		   FAILED(factory->GetSystemFontCollection(collection.Put(), FALSE)))
			Panic("RenderFontWin32: DirectWrite system font collection unavailable");
	}
	bool Error(const char *message) { stats.error = message; return false; }

	FontFaceEntry& Face(Font font, const String& resolved_name) {
		font.Underline(false).Strikeout(false);
		const int64 key = font.AsInt64();
		for(auto& entry : faces)
			if(entry.key == key) {
				entry.used = ++serial;
				return entry;
			}
		FontFaceEntry entry;
		entry.key = key;
		entry.used = ++serial;
		entry.em = font.GetHeight() ? max(1, abs(font.GetHeight())) : 12;
		WString name = resolved_name.ToWString();
		Buffer<char16> family_name(2 * name.GetCount() + 1);
		int count = ToUtf16(family_name, name.Begin(), name.GetCount());
		family_name[count] = 0;
		UINT32 index = 0;
		BOOL exists = FALSE;
		if(FAILED(collection->FindFamilyName(reinterpret_cast<WCHAR *>(~family_name), &index, &exists)) || !exists)
			collection->FindFamilyName(L"Arial", &index, &exists);
		FontCom<IDWriteFontFamily> family;
		FontCom<IDWriteFont> dwfont;
		if(!exists || FAILED(collection->GetFontFamily(index, family.Put())) ||
		   FAILED(family->GetFirstMatchingFont(font.IsBold() ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
		                                    DWRITE_FONT_STRETCH_NORMAL,
		                                    font.IsItalic() ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
		                                    dwfont.Put())) ||
		   FAILED(dwfont->CreateFontFace(entry.face.Put())))
			Panic("RenderFontWin32: cannot resolve font face");
		entry.symbol = dwfont->IsSymbolFont();
		entry.face->GetMetrics(&entry.metrics);
		const UINT32 cp[2] = { 'i', 'M' };
		UINT16 glyph[2]{};
		DWRITE_GLYPH_METRICS metrics[2]{};
		entry.face->GetGlyphIndices(cp, 2, glyph);
		entry.face->GetDesignGlyphMetrics(glyph, 2, metrics);
		entry.fixedpitch = metrics[0].advanceWidth == metrics[1].advanceWidth;
		if(font.GetWidth() != 0 && metrics[1].advanceWidth)
			entry.xscale = abs(font.GetWidth()) * entry.metrics.designUnitsPerEm /
			               (entry.em * metrics[1].advanceWidth);
		if(faces.GetCount() >= stats.face_cache_limit) {
			int oldest = 0;
			for(int i = 1; i < faces.GetCount(); ++i)
				if(faces[i].used < faces[oldest].used) oldest = i;
			faces.Remove(oldest);
		}
		stats.face_cache_misses++;
		return faces.Add(pick(entry));
	}

	UINT16 Glyph(FontFaceEntry& entry, int chr) {
		if(chr < 0 || chr > 0x10ffff || (chr >= 0xd800 && chr <= 0xdfff)) return 0;
		UINT32 cp = chr;
		UINT16 glyph = 0;
		entry.face->GetGlyphIndices(&cp, 1, &glyph);
		if(!glyph && entry.symbol && chr < 256) {
			cp = chr + 0xf000;
			entry.face->GetGlyphIndices(&cp, 1, &glyph);
		}
		return glyph;
	}
};

FontService& Fonts() { static FontService service; return service; }

int RoundMetric(double value) { return minmax<int>(fround(value), -32767, 32767); }

class OutlineSink final : public IDWriteGeometrySink {
	FontGlyphConsumer& out;
	Pointf origin;
	double xscale;
	ULONG refs = 1;
public:
	OutlineSink(FontGlyphConsumer& consumer, Pointf origin, double xscale)
		: out(consumer), origin(origin), xscale(xscale) {}
	Pointf Point(D2D1_POINT_2F p) const { return origin + Pointf(p.x * xscale, p.y); }
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
		static const GUID sink_iid = {0x2cd9069e, 0x12e2, 0x11dc, {0x9f,0xed,0x00,0x11,0x43,0xa0,0x55,0xf9}};
		static const GUID unknown_iid = {0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
		if(!object) return E_POINTER;
		*object = nullptr;
		if(IsEqualIID(iid, sink_iid) || IsEqualIID(iid, unknown_iid)) {
			*object = static_cast<IDWriteGeometrySink *>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
	ULONG STDMETHODCALLTYPE Release() override { ASSERT(refs > 1); return --refs; }
	void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE) noexcept override {}
	void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT) noexcept override {}
	void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F p, D2D1_FIGURE_BEGIN) noexcept override { out.Move(Point(p)); }
	void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F *p, UINT count) noexcept override {
		for(UINT i = 0; i < count; ++i) out.Line(Point(p[i]));
	}
	void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT *p, UINT count) noexcept override {
		for(UINT i = 0; i < count; ++i) out.Cubic(Point(p[i].point1), Point(p[i].point2), Point(p[i].point3));
	}
	void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END end) noexcept override { if(end == D2D1_FIGURE_END_CLOSED) out.Close(); }
	HRESULT STDMETHODCALLTYPE Close() noexcept override { return S_OK; }
};

String FamilyName(IDWriteFontFamily *family) {
	FontCom<IDWriteLocalizedStrings> names;
	if(FAILED(family->GetFamilyNames(names.Put()))) return String();
	UINT32 index = 0, length = 0;
	BOOL exists = FALSE;
	names->FindLocaleName(L"en-us", &index, &exists);
	if(!exists) index = 0;
	if(FAILED(names->GetStringLength(index, &length)) || length > 1024) return String();
	Buffer<WCHAR> buffer(length + 1);
	if(FAILED(names->GetString(index, buffer, length + 1))) return String();
	return FromSystemCharsetW(buffer);
}

} // namespace

RenderFontWin32Stats GetRenderFontWin32Stats() {
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	RenderFontWin32Stats result = service.stats;
	result.face_cache_entries = service.faces.GetCount();
	return result;
}

void ClearRenderFontWin32Cache() {
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	service.faces.Clear();
	service.native.Clear();
}

Vector<FaceInfo> GetAllFacesSys() {
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	Vector<FaceInfo> out;
	out.Add().name = "STDFONT";
	auto add = [&](String name) {
		if(name.IsEmpty()) return;
		for(const auto& face : out) if(face.name == name) return;
		FaceInfo& face = out.Add();
		face.name = name;
		face.info = Font::SCALEABLE;
		String lower = ToLower(name);
		if(lower.Find("courier") >= 0 || lower.Find("mono") >= 0 || lower == "consolas")
			face.info |= Font::FIXEDPITCH;
		if(lower.Find("times") >= 0 || lower.Find("serif") >= 0)
			face.info |= Font::SERIFSTYLE;
		if(lower == "symbol" || lower == "wingdings")
			face.info |= Font::SPECIAL;
	};
	// Preserve U++'s public generic/symbol face indices.
	for(const char *name : {"Times New Roman", "Arial", "Courier New", "Symbol", "Wingdings", "Tahoma"})
		add(name);
	const UINT32 count = service.collection->GetFontFamilyCount();
	for(UINT32 i = 0; i < count; ++i) {
		FontCom<IDWriteFontFamily> family;
		if(SUCCEEDED(service.collection->GetFontFamily(i, family.Put()))) add(FamilyName(family.p));
	}
	return out;
}

CommonFontInfo GetFontInfoSys(Font font) {
	font.RealizeStd();
	const String name = font.GetFaceName();
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	FontFaceEntry& entry = service.Face(font, name);
	service.stats.metric_requests++;
	CommonFontInfo result{};
	DWRITE_FONT_METRICS metrics = entry.metrics;
	entry.face->GetGdiCompatibleMetrics((FLOAT)entry.em, 1, nullptr, &metrics);
	const double scale = entry.em / max(1, (int)metrics.designUnitsPerEm);
	result.ascent = max(1, (int)ceil(metrics.ascent * scale));
	result.descent = max(0, (int)ceil(metrics.descent * scale));
	result.internal = max(0, result.ascent + result.descent - (int)entry.em);
	result.external = max(0, fround(metrics.lineGap * scale));
	result.firstchar = 0;
	result.charcount = 0x110000;
	result.default_char = '?';
	result.fonti = entry.face->GetIndex();
	result.fixedpitch = entry.fixedpitch;
	result.ttf = result.scaleable = true;
	const UINT32 cp[2] = { 'x', 'M' };
	UINT16 glyph[2]{};
	DWRITE_GLYPH_METRICS gm[2]{};
	entry.face->GetGlyphIndices(cp, 2, glyph);
	entry.face->GetDesignGlyphMetrics(glyph, 2, gm);
	result.avewidth = max(1, RoundMetric(gm[0].advanceWidth * scale * entry.xscale));
	result.maxwidth = max(result.avewidth, RoundMetric(metrics.designUnitsPerEm * scale * entry.xscale * 2));
	result.overhang = font.IsItalic() ? max(1, fround(entry.em * 0.25)) : 0;
	result.spacebefore = RoundMetric(gm[0].leftSideBearing * scale * entry.xscale);
	result.spaceafter = RoundMetric(gm[0].rightSideBearing * scale * entry.xscale);
	return result;
}

GlyphInfo GetGlyphInfoSys(Font font, int chr) {
	font.RealizeStd();
	const String name = font.GetFaceName();
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	FontFaceEntry& entry = service.Face(font, name);
	service.stats.metric_requests++;
	GlyphInfo result{};
	result.width = (int16)0x8000;
	result.lspc = -2;
	UINT16 glyph = service.Glyph(entry, chr);
	if(!glyph) return result;
	DWRITE_GLYPH_METRICS metrics{};
	if(FAILED(entry.face->GetGdiCompatibleGlyphMetrics((FLOAT)entry.em, 1, nullptr, FALSE, &glyph, 1, &metrics))) {
		service.Error("DirectWrite glyph metrics failed");
		return result;
	}
	const double scale = entry.em / max(1, (int)entry.metrics.designUnitsPerEm) * entry.xscale;
	result.width = (int16)max(0, RoundMetric(metrics.advanceWidth * scale));
	result.lspc = (int16)RoundMetric(metrics.leftSideBearing * scale);
	result.rspc = (int16)RoundMetric(metrics.rightSideBearing * scale);
	result.glyphi = glyph;
	return result;
}

void RenderCharacterSys(FontGlyphConsumer& out, double x, double y, int chr, Font font) {
	// Resolve baseline before locking the service: Font metrics use the same service.
	font.RealizeStd();
	const int ascent = font.GetAscent();
	const String name = font.GetFaceName();
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	FontFaceEntry& entry = service.Face(font, name);
	UINT16 glyph = service.Glyph(entry, chr);
	if(!glyph) return;
	service.stats.outline_requests++;
	OutlineSink sink(out, Pointf(x, y + ascent), entry.xscale);
	if(FAILED(entry.face->GetGlyphRunOutline((FLOAT)entry.em, &glyph, nullptr, nullptr, 1, FALSE, FALSE, &sink)))
		service.Error("DirectWrite glyph outline failed");
}

String GetFontDataSys(Font font, const char *table, int offset, int size) {
	if(offset < 0 || size <= 0) return String();
	font.RealizeStd();
	const String name = font.GetFaceName();
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	FontFaceEntry& entry = service.Face(font, name);
	constexpr int MAX_DATA = 64 * 1024 * 1024;
	if(table) {
		const UINT32 tag = UINT32((byte)table[0]) | (UINT32((byte)table[1]) << 8) |
		                   (UINT32((byte)table[2]) << 16) | (UINT32((byte)table[3]) << 24);
		const void *data = nullptr;
		UINT32 length = 0;
		void *context = nullptr;
		BOOL exists = FALSE;
		if(FAILED(entry.face->TryGetFontTable(tag, &data, &length, &context, &exists)) || !exists)
			return String();
		String result;
		if((UINT32)offset <= length)
			result = String((const char *)data + offset, min(min(size, MAX_DATA), (int)(length - offset)));
		entry.face->ReleaseFontTable(context);
		return result;
	}
	UINT32 count = 0;
	if(FAILED(entry.face->GetFiles(&count, nullptr)) || count != 1) return String();
	FontCom<IDWriteFontFile> file;
	if(FAILED(entry.face->GetFiles(&count, file.Put()))) return String();
	const void *key = nullptr;
	UINT32 key_size = 0;
	FontCom<IDWriteFontFileLoader> loader;
	FontCom<IDWriteFontFileStream> stream;
	if(FAILED(file->GetReferenceKey(&key, &key_size)) || FAILED(file->GetLoader(loader.Put())) ||
	   FAILED(loader->CreateStreamFromKey(key, key_size, stream.Put()))) return String();
	UINT64 length = 0;
	if(FAILED(stream->GetFileSize(&length)) || (UINT64)offset > length) return String();
	UINT64 bytes = min((UINT64)min(size, MAX_DATA), length - offset);
	if(!bytes) return String();
	const void *fragment = nullptr;
	void *context = nullptr;
	if(FAILED(stream->ReadFileFragment(&fragment, offset, bytes, &context))) return String();
	String result((const char *)fragment, (int)bytes);
	stream->ReleaseFileFragment(context);
	return result;
}

// CtrlCore still links its ordinary SystemDraw entry point. This compatibility
// cache is used only if native GDI text is actually requested; strict GPU hosts
// can detect every such request with the counter above.
HFONT GetWin32Font(Font font, int angle) {
	font.RealizeStd();
	WString name = font.GetFaceName().ToWString();
	FontService& service = Fonts();
	Mutex::Lock lock(service.mutex);
	service.stats.legacy_gdi_font_requests++;
	for(auto& entry : service.native)
		if(entry.key == font.AsInt64() && entry.angle == angle) return entry.handle;
	if(service.native.GetCount() >= 64) service.native.Remove(0);
	NativeFontEntry& entry = service.native.Add();
	entry.key = font.AsInt64();
	entry.angle = angle;
	entry.handle = CreateFontW(font.GetHeight() ? -max(1, abs(font.GetHeight())) : -12, font.GetWidth(), angle, angle,
	                          font.IsBold() ? FW_BOLD : FW_NORMAL, font.IsItalic(),
	                          font.IsUnderline(), font.IsStrikeout(),
	                          font.GetFace() == Font::SYMBOL ? SYMBOL_CHARSET : DEFAULT_CHARSET,
	                          OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
	                          font.IsNonAntiAliased() ? NONANTIALIASED_QUALITY : DEFAULT_QUALITY,
	                          DEFAULT_PITCH | FF_DONTCARE, ToSystemCharsetW(name.ToString()));
	return entry.handle;
}

} // namespace Upp
#else
#error RenderFontWin32 requires Windows and the CFONTS assembly flag
#endif
