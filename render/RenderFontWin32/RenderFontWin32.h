#pragma once

#include <Draw/Draw.h>

namespace Upp {

// Enabled by the existing U++ CFONTS assembly hook. No DWrite/COM types escape.
struct RenderFontWin32Stats {
	int face_cache_entries = 0;
	int face_cache_limit = 64;
	uint64 face_cache_misses = 0;
	uint64 metric_requests = 0;
	uint64 outline_requests = 0;
	uint64 legacy_gdi_font_requests = 0;
	String error;
};
RenderFontWin32Stats GetRenderFontWin32Stats();
void ClearRenderFontWin32Cache();

}
