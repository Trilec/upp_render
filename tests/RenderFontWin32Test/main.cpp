#include <RenderFontWin32/RenderFontWin32.h>
#include <Painter/Painter.h>
#include <cmath>
#include <atomic>

using namespace Upp;

static bool Check(bool value, const char *message) {
	if(!value) Cout() << "FAIL: " << message << EOL;
	return value;
}

struct OutlineProbe : FontGlyphConsumer {
	int moves = 0, lines = 0, curves = 0, closes = 0;
	bool finite = true;
	void Point(Pointf p) { finite &= std::isfinite(p.x) && std::isfinite(p.y); }
	void Move(Pointf p) override { moves++; Point(p); }
	void Line(Pointf p) override { lines++; Point(p); }
	void Quadratic(Pointf a, Pointf b) override { curves++; Point(a); Point(b); }
	void Cubic(Pointf a, Pointf b, Pointf c) override { curves++; Point(a); Point(b); Point(c); }
	void Close() override { closes++; }
};

CONSOLE_APP_MAIN {
	bool ok = true;
	ok &= Check(Font::GetFaceName(Font::SERIF) == "Times New Roman" &&
	            Font::GetFaceName(Font::SANSSERIF) == "Arial" &&
	            Font::GetFaceName(Font::MONOSPACE) == "Courier New", "public generic font indices preserved");
	for(int height : {12, 18, 32, 64}) {
		for(Font font : {SansSerif(height), SansSerif(height).Bold().Italic(), Monospace(height)}) {
			ok &= Check(font.GetAscent() > 0 && font.GetDescent() >= 0 && font.GetCy() >= height,
			            "finite positive line metrics");
			ok &= Check(font.GetWidth('M') > 0 && font.GetWidth(' ') > 0, "visible/space advances");
			OutlineProbe probe;
			font.Render(probe, 2, 3, 'B');
			ok &= Check(probe.finite && probe.moves >= 2 && probe.closes >= 2 &&
			            probe.lines + probe.curves > 0, "compound B outline with finite coordinates");
		}
	}
	Font font = SansSerif(24);
	for(int cp : {0x00e9, 0x03a9, 0x0416, 0x4e2d, 0x1d400, 0x1f600}) {
		const GlyphInfo info = GetGlyphInfo(font, cp);
		ok &= Check(!info.IsMissing(), "Unicode scalar has U++ fallback or native glyph");
		ok &= Check(font.GetWidth(cp) > 0, "Unicode fallback has a positive advance");
	}
	GlyphInfo missing = GetGlyphInfoSys(font, 0x110000);
	ok &= Check(missing.IsMissing(), "out-of-range Unicode returns missing marker");
	ok &= Check(GetGlyphInfoSys(font, 0xd800).IsMissing(), "surrogate scalar rejected");
	String head = font.GetData("head", 0, 54);
	ok &= Check(head.GetCount() == 54, "OpenType head table exposed without GDI");
	ok &= Check(font.GetData("head", 8, 8) == head.Mid(8, 8), "table byte offset is exact");
	ok &= Check(font.GetData("head", 100000, 10).IsEmpty(), "table range bounded");
	ok &= Check(font.GetData(nullptr, 0, 32).GetCount() == 32, "font-file stream header exposed");
	ImagePainter painter(320, 96);
	painter.Clear(RGBAZero());
	WString text;
	for(int cp : {0x41, 0x42, 0x00e9, 0x03a9, 0x0416, 0x4e2d, 0x1f600}) text.Cat(cp);
	painter.DrawText(4, 4, text, font.Underline().Strikeout(), White());
	Image image = painter.GetResult();
	int covered = 0;
	for(int y = 0; y < image.GetHeight(); ++y)
		for(int x = 0; x < image.GetWidth(); ++x) covered += image[y][x].a != 0;
	ok &= Check(covered > 100, "Unicode/decorated text produces coverage through custom outlines");
	for(int i = 1; i <= 100; ++i) {
		OutlineProbe probe;
		SansSerif(i + 7).Render(probe, 0, 0, 'g');
	}
	std::atomic<int> thread_failures{0};
	Array<Thread> workers;
	for(int worker = 0; worker < 4; ++worker)
		workers.Add().Run([&, worker] {
			for(int i = 0; i < 100; ++i) {
				Font face = SansSerif(12 + (i + worker) % 32);
				OutlineProbe probe;
				face.Render(probe, worker, i, 'B');
				if(face.GetWidth('M') <= 0 || !probe.finite || probe.moves == 0)
					thread_failures++;
				GetRenderFontWin32Stats();
			}
		});
	for(auto& worker : workers) worker.Wait();
	ok &= Check(thread_failures.load() == 0, "concurrent metrics/outlines and diagnostics remain valid");
	RenderFontWin32Stats stats = GetRenderFontWin32Stats();
	ok &= Check(stats.face_cache_entries <= stats.face_cache_limit && stats.face_cache_misses >= 100,
	            "font-face cache remains bounded under churn");
	ok &= Check(stats.outline_requests >= 100 && stats.metric_requests > 0 && stats.error.IsEmpty(),
	            "DirectWrite metrics/outlines succeeded");
	ok &= Check(stats.legacy_gdi_font_requests == 0, "no legacy GDI text/font entry point used");
	ClearRenderFontWin32Cache();
	ok &= Check(GetRenderFontWin32Stats().face_cache_entries == 0, "explicit cache cleanup releases font faces");
	OutlineProbe recovered;
	SansSerif(24).Render(recovered, 0, 0, 'B');
	ok &= Check(recovered.moves > 0, "font service recovers after cache cleanup");
	Cout() << "font_backend=DirectWrite legacy_gdi_font_requests=" << stats.legacy_gdi_font_requests
	       << " cached_faces=" << stats.face_cache_entries << "/" << stats.face_cache_limit
	       << " covered_pixels=" << covered << EOL;
	Cout() << (ok ? "RenderFontWin32Test passed" : "RenderFontWin32Test FAILED") << EOL;
	SetExitCode(ok ? 0 : 1);
}
