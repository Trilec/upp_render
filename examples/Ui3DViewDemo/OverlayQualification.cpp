// Finite native observation of the HUD during motion. Never used by normal startup.
#include "Ui3DViewDemo.h"
#include <plugin/png/png.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <exception>
#include <thread>
#include <vector>

namespace Upp {
namespace {

void OverlayProbePump(int milliseconds)
{
	const DWORD started = GetTickCount(), duration = (DWORD)max(0, milliseconds);
	for(;;) {
		Ctrl::ProcessEvents();
		const DWORD elapsed = GetTickCount() - started;
		if(elapsed >= duration) break;
		Ctrl::GuiSleep(min(10, int(duration - elapsed)));
	}
	Ctrl::ProcessEvents();
}

UiLabel *OverlayProbeLabel(Ctrl& parent, const String& text, bool projection = false)
{
	for(Ctrl *child = parent.GetFirstChild(); child; child = child->GetNext()) {
		if(auto *label = dynamic_cast<UiLabel *>(child))
			if(label->IsVisible() && (projection ? label->GetText().Find("  |  ") >= 0 : label->GetText() == text))
				return label;
		if(auto *label = OverlayProbeLabel(*child, text, projection)) return label;
	}
	return nullptr;
}

// The worker never accesses a Ctrl, U++ image, callback or file. The GUI thread
// snapshots geometry first, keeps the window alive and joins before inspection.
struct OverlayProbeCapture {
	static constexpr int RoiCount = 4, FrameLimit = 180;
	static constexpr size_t ByteLimit = 24 * 1024 * 1024;
	enum Failure { NONE, OWNERSHIP, DC_CREATION, DIB_CREATION, SCREEN_COPY };
	HWND window = nullptr;
	RECT client {};
	std::array<RECT, RoiCount> roi {};
	std::array<size_t, RoiCount> offset {}, bytes {};
	std::array<DWORD, FrameLimit> time {};
	std::array<int, FrameLimit> phase_at_frame {};
	std::vector<unsigned char> pixels;
	size_t stride = 0;
	int capacity = 0, frames = 0, failure = NONE;
	std::atomic<bool> stop { false }, done { false }, ready { false };
	std::atomic<int> phase { 0 };
	std::atomic<int> completed_samples { 0 };
	std::thread worker;

	~OverlayProbeCapture() { Stop(); }
	void Stop()
	{
		stop.store(true);
		if(worker.joinable()) {
			// Native hit tests can consult the owning UI thread. Keep dispatching
			// while the finite worker exits, rather than joining under a GUI lock.
			while(!done.load()) OverlayProbePump(5);
			worker.join();
		}
	}
	bool OwnsRegions() const
	{
		if(!IsWindow(window) || GetForegroundWindow() != window) return false;
		RECT now;
		POINT origin { 0, 0 };
		if(!GetClientRect(window, &now) || !ClientToScreen(window, &origin) ||
		   origin.x != client.left || origin.y != client.top ||
		   now.right != client.right - client.left || now.bottom != client.bottom - client.top) return false;
		for(const RECT& r : roi) for(int y = 0; y <= 4; ++y) for(int x = 0; x <= 4; ++x) {
			const POINT p { r.left + (r.right - r.left - 1) * x / 4,
			                r.top + (r.bottom - r.top - 1) * y / 4 };
			const HWND hit = WindowFromPoint(p);
			if(!hit || GetAncestor(hit, GA_ROOT) != window) return false;
		}
		return true;
	}
	bool Start(HWND hwnd, const std::array<Rect, RoiCount>& rectangles, String& error)
	{
		window = hwnd;
		RECT local;
		POINT origin { 0, 0 };
		if(!GetClientRect(window, &local) || !ClientToScreen(window, &origin)) {
			error = "cannot snapshot capture client bounds"; return false;
		}
		client = { origin.x, origin.y, origin.x + local.right, origin.y + local.bottom };
		for(int i = 0; i < RoiCount; ++i) {
			const Rect& r = rectangles[i];
			if(r.IsEmpty() || r.left < client.left || r.top < client.top ||
			   r.right > client.right || r.bottom > client.bottom) {
				error = "HUD capture rectangle is outside the demo client"; return false;
			}
			roi[i] = { r.left, r.top, r.right, r.bottom };
			offset[i] = stride;
			bytes[i] = size_t(r.Width()) * r.Height() * 4;
			if(bytes[i] > ByteLimit || stride > ByteLimit - bytes[i]) {
				error = "HUD capture exceeds its memory bound"; return false;
			}
			stride += bytes[i];
		}
		capacity = int(std::min(size_t(FrameLimit), ByteLimit / stride));
		if(capacity < 32 || !OwnsRegions()) { error = "HUD capture ownership or sample budget unavailable"; return false; }
		try {
			pixels.resize(stride * capacity);
			worker = std::thread([this] { Run(); });
		}
		catch(const std::exception& e) { error = String("cannot start HUD capture: ") + e.what(); return false; }
		return true;
	}
	void Run()
	{
		HDC screen = GetDC(nullptr), memory = screen ? CreateCompatibleDC(screen) : nullptr;
		std::array<HBITMAP, RoiCount> bitmap {};
		std::array<void *, RoiCount> data {};
		HGDIOBJ original = nullptr;
		if(!screen || !memory) failure = DC_CREATION;
		for(int i = 0; i < RoiCount && failure == NONE; ++i) {
			BITMAPINFO info {};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = roi[i].right - roi[i].left;
			info.bmiHeader.biHeight = -(roi[i].bottom - roi[i].top);
			info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
			bitmap[i] = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &data[i], nullptr, 0);
			if(!bitmap[i] || !data[i]) failure = DIB_CREATION;
		}
		const DWORD started = GetTickCount();
		const DWORD interval = max(16, (3400 + capacity - 2) / (capacity - 1));
		while(failure == NONE && !stop.load() && frames < capacity && GetTickCount() - started < 3500) {
			const int frame_phase = phase.load();
			if(!OwnsRegions()) { failure = OWNERSHIP; break; }
			for(int i = 0; i < RoiCount; ++i) {
				HGDIOBJ previous = SelectObject(memory, bitmap[i]);
				if(!original) original = previous;
				const RECT& r = roi[i];
				if(!previous || previous == HGDI_ERROR ||
				   !BitBlt(memory, 0, 0, r.right-r.left, r.bottom-r.top, screen, r.left, r.top, SRCCOPY|CAPTUREBLT)) {
					failure = SCREEN_COPY; break;
				}
			}
			GdiFlush();
			// Never retain or publish a sample if focus, position or occlusion
			// changed while reading it. In-flight phase changes are also discarded.
			if(!OwnsRegions()) { failure = OWNERSHIP; break; }
			if(failure == NONE && frame_phase == phase.load()) {
				for(int i = 0; i < RoiCount; ++i)
					std::memcpy(pixels.data() + size_t(frames)*stride + offset[i], data[i], bytes[i]);
				time[frames] = GetTickCount() - started;
				phase_at_frame[frames++] = frame_phase;
				ready.store(true);
				completed_samples.store(frames);
			}
			Sleep(interval);
		}
		if(original && original != HGDI_ERROR) SelectObject(memory, original);
		for(HBITMAP object : bitmap) if(object) DeleteObject(object);
		if(memory) DeleteDC(memory);
		if(screen) ReleaseDC(nullptr, screen);
		done.store(true);
	}
	const unsigned char *Data(int frame, int region) const
	{
		return pixels.data() + size_t(frame)*stride + offset[region];
	}
};

// Only a successful injected down arms this cleanup. Its matching up must not
// depend on foreground ownership: losing foreground aborts further actions,
// but must never leave a button held by the diagnostic.
struct OverlayProbeButton {
	DWORD up_flag;
	bool held = false;
	explicit OverlayProbeButton(DWORD flag) : up_flag(flag) {}
	OverlayProbeButton(const OverlayProbeButton&) = delete;
	OverlayProbeButton& operator=(const OverlayProbeButton&) = delete;
	~OverlayProbeButton() { Release(); }
	void DownSent() { held = true; }
	bool Release()
	{
		if(!held) return true;
		INPUT input {}; input.type = INPUT_MOUSE; input.mi.dwFlags = up_flag;
		if(SendInput(1, &input, sizeof(INPUT)) != 1) return false;
		held = false;
		return true;
	}
};

bool OverlayProbeSamePixel(const unsigned char *a, const unsigned char *b)
{
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

bool OverlayProbeSameRegion(const OverlayProbeCapture& capture, int frame, int region)
{
	const unsigned char *a = capture.Data(0, region), *b = capture.Data(frame, region);
	for(size_t i = 0; i < capture.bytes[region]; i += 4)
		if(!OverlayProbeSamePixel(a+i, b+i)) return false;
	return true;
}

uint64 OverlayProbeRegionHash(const OverlayProbeCapture& capture, int frame, int region)
{
	uint64 hash = 1469598103934665603ULL;
	const unsigned char *data = capture.Data(frame, region);
	for(size_t i = 0; i < capture.bytes[region]; i += 4)
		for(int channel = 0; channel < 3; ++channel) hash = (hash ^ data[i+channel])*1099511628211ULL;
	return hash;
}

bool OverlayProbeTextVisible(const OverlayProbeCapture& capture, int frame, int region, Rgba8 background)
{
	// This probe fixes the dark theme. Borderless caption slots must contain
	// bright glyph pixels above their actual face color; black/face-only tiles
	// and uniform bright clears are not mistaken for visible text.
	const unsigned char *data = capture.Data(frame, region);
	size_t ink = 0;
	for(size_t i = 0; i < capture.bytes[region]; i += 4)
		if(int(data[i+2]) > int(background.r)+12 && int(data[i+1]) > int(background.g)+12 &&
		   int(data[i]) > int(background.b)+12) ++ink;
	return ink >= 4 && ink < capture.bytes[region]/8;
}

Image OverlayProbeImage(const OverlayProbeCapture& capture, int frame, int region)
{
	const RECT& r = capture.roi[region];
	const int width = r.right-r.left, height = r.bottom-r.top;
	ImageBuffer image(width, height);
	const unsigned char *source = capture.Data(frame, region);
	for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x) {
		const unsigned char *p = source + (size_t(y)*width+x)*4;
		RGBA& target = image[y][x];
		target.r = p[2]; target.g = p[1]; target.b = p[0]; target.a = 255;
	}
	image.SetKind(IMAGE_OPAQUE);
	return Image(image);
}

bool OverlayProbeSheet(const OverlayProbeCapture& capture, const String& path, int first_bad)
{
	if(!capture.frames) return false;
	const int axis_width = capture.roi[0].right-capture.roi[0].left;
	int text_width = 0, text_height = 0;
	for(int region = 1; region < 4; ++region) {
		text_width = max(text_width, int(capture.roi[region].right-capture.roi[region].left));
		text_height += capture.roi[region].bottom-capture.roi[region].top+4;
	}
	const int cell_width = axis_width+text_width+24;
	const int cell_height = max(int(capture.roi[0].bottom-capture.roi[0].top), text_height)+30;
	if(int64(cell_width)*cell_height*9 > 8000000) return false;
	ImageDraw sheet(cell_width*3, cell_height*3);
	sheet.DrawRect(0, 0, cell_width*3, cell_height*3, Color(24,29,34));
	for(int tile = 0; tile < 9; ++tile) {
		const int frame = tile == 8 && first_bad >= 0 ? first_bad : tile*(capture.frames-1)/8;
		const int x = (tile%3)*cell_width+8, y = (tile/3)*cell_height+8;
		sheet.DrawText(x, y, Format("%d ms  %s%s", int(capture.time[frame]),
		               capture.phase_at_frame[frame] ? "text updates" : "orbit",
		               frame == first_bad ? "  MISMATCH" : ""), StdFont().Height(12), White());
		sheet.DrawImage(x, y+20, OverlayProbeImage(capture, frame, 0));
		int row_y = y+20;
		for(int region : { 1, 3, 2 }) {
			sheet.DrawImage(x+axis_width+8, row_y, OverlayProbeImage(capture, frame, region));
			row_y += capture.roi[region].bottom-capture.roi[region].top+4;
		}
	}
	return PNGEncoder().SaveFile(path, Image(sheet));
}

bool OverlayProbeIdle(const Ui3DViewportChromeStats& a, const Ui3DViewportChromeStats& b)
{
	return a.native_paints == b.native_paints && a.tile_paints == b.tile_paints &&
	       a.region_updates == b.region_updates && a.z_order_updates == b.z_order_updates &&
	       a.axis_paints == b.axis_paints && a.annotation_paints == b.annotation_paints;
}

void OverlayProbeStats(String& report, const char *phase, const Ui3DViewportChromeStats& a,
	                   const Ui3DViewportChromeStats& b)
{
	report << phase << " native=" << b.native_paints-a.native_paints << " tiles=" << b.tile_paints-a.tile_paints
	       << " axis=" << b.axis_paints-a.axis_paints << " annotations=" << b.annotation_paints-a.annotation_paints
	       << " regions=" << b.region_updates-a.region_updates << " z_order=" << b.z_order_updates-a.z_order_updates
	       << " max_tile_pixels=" << b.max_tile_pixels << "\n";
}

}

bool Ui3DViewDemo::QualifyOverlays(String& report, const String& evidence_directory)
{
	const DWORD started = GetTickCount();
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char *message) {
		++checks;
		if(!ok) { ++failures; report << "FAIL: " << message << "\n"; }
	};
	auto finish = [&] {
		report << "OverlayQualification checks=" << checks << " failures=" << failures
		       << " elapsed_ms=" << GetTickCount()-started << "\n";
		return failures == 0;
	};
	RealizeDirectory(evidence_directory);
	check(DirectoryExists(evidence_directory), "overlay evidence directory is available");
	SetLight(false); ResetDemo(); SetInspectorVisible(true);
	auto& pane = panes[3];
	auto& view = pane.View();
	pane.ShowAnnotations().ShowOrientation().ShowNavigation();
	String error;
	check(view.SetDisplayMode(Ui3DDisplayMode::SolidEdges, error), "overlay probe display mode is available");
	SyncChrome(false); view.SetFocus(); OverlayProbePump(250);
	check(CheckReady(error), "overlay probe has a completed Vulkan frame");
	check(GetForegroundWindow() == GetHWND(), "overlay probe owns the foreground before native input");
	if(failures) { report << "setup_error=" << error << "\n"; return finish(); }

	UiLabel *projection = OverlayProbeLabel(pane, String(), true);
	UiLabel *units = OverlayProbeLabel(pane, "Y up  \xC2\xB7  metres");
	UiLabel *caption = OverlayProbeLabel(pane, "Perspective");
	check(projection && units && caption, "native HUD label rectangles are discoverable");
	if(!projection || !units || !caption) return finish();
	const Rect view_rect = view.GetScreenRect();
	const int side = DPI(pane.IsCompact() ? 82 : 98);
	const Point axis_origin(view_rect.right-DPI(8)-side, view_rect.top+DPI(8));
	const Rect axis_rect(axis_origin, Size(side, side));
	const Point axis_center = axis_origin + Point(side/2, side/2);
	const std::array<Rect, 4> regions { axis_rect, projection->GetScreenRect(), units->GetScreenRect(), caption->GetScreenRect() };
	const String projection_before = projection->GetText();
	const Scene3DCamera camera_before = view.GetCamera();
	const Rgba8 caption_background = view.GetOptions().background;
	POINT cursor_before {};
	GetCursorPos(&cursor_before);
	auto owns_point = [&](Point point) {
		if(GetForegroundWindow() != GetHWND() || !view.IsOpen() || view.GetScreenRect() != view_rect ||
		   !view_rect.Contains(point)) return false;
		POINT native { point.x, point.y };
		HWND hit = WindowFromPoint(native);
		return hit && GetAncestor(hit, GA_ROOT) == GetHWND();
	};
	auto modifiers_clear = [&] {
		for(int key : { VK_CONTROL, VK_SHIFT, VK_MENU }) if(GetAsyncKeyState(key) & 0x8000) return false;
		return true;
	};
	bool initially_clear = modifiers_clear();
	for(int key : { VK_LBUTTON, VK_MBUTTON, VK_RBUTTON }) if(GetAsyncKeyState(key) & 0x8000) initially_clear = false;
	check(initially_clear, "native overlay probe starts without held user buttons or modifiers");
	if(!initially_clear) return finish();

	OverlayProbeCapture capture;
	const DWORD capture_started = GetTickCount();
	const bool capture_started_ok = capture.Start(GetHWND(), regions, error);
	for(int i = 0; i < 30 && capture_started_ok && !capture.ready.load() && !capture.done.load(); ++i) OverlayProbePump(10);
	check(capture_started_ok && capture.ready.load(), "independent native HUD capture starts with an owned baseline");
	if(!capture_started_ok || !capture.ready.load()) {
		capture.Stop(); report << "capture_setup_error=" << error << " failure=" << capture.failure << "\n";
		return finish();
	}
	const auto chrome_before = pane.GetChromeStats();
	const uint64 gpu_before = view.GetViewStats().rendered_frames;
	// Starting on the real annotation exercises its pass-through route above
	// the Vulkan HWND; capture then belongs to the ordinary Ui3DView gesture.
	const Point press = regions[1].CenterPoint();
	OverlayProbeButton middle_button(MOUSEEVENTF_MIDDLEUP);
	bool down_sent = owns_point(press) && SetCursorPos(press.x, press.y);
	OverlayProbePump(20);
	POINT actual {};
	down_sent = down_sent && GetCursorPos(&actual) && std::abs(actual.x-press.x) <= 1 &&
	            std::abs(actual.y-press.y) <= 1 && owns_point(press) && modifiers_clear();
	if(down_sent) {
		INPUT input {}; input.type = INPUT_MOUSE; input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
		down_sent = SendInput(1, &input, sizeof(INPUT)) == 1;
		if(down_sent) middle_button.DownSent();
	}
	for(int i = 0; i < 20 && down_sent && !view.IsNavigating(); ++i) OverlayProbePump(10);
	check(down_sent && view.IsNavigating() && view.HasCapture(), "native MMB on an annotation forwards to viewport navigation");
	bool motion_ok = down_sent && view.IsNavigating() && view.HasCapture();
	int motions = 0, camera_changes = 0;
	Scene3DCamera previous = view.GetCamera();
	const int desktop_width = GetSystemMetrics(SM_CXVIRTUALSCREEN), desktop_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	for(int step = 0; step < 60 && motion_ok && !capture.done.load(); ++step) {
		const double t = double(step+1)/60;
		const Point point = press + Point(int(std::lround(DPI(180)*t)), int(std::lround(DPI(55)*std::sin(3.141592653589793*t))));
		motion_ok = desktop_width > 1 && desktop_height > 1 && owns_point(point) && modifiers_clear() && view.HasCapture();
		if(!motion_ok) break;
		INPUT input {};
		input.type = INPUT_MOUSE;
		input.mi.dx = LONG((int64(point.x-GetSystemMetrics(SM_XVIRTUALSCREEN))*65536+32768)/desktop_width);
		input.mi.dy = LONG((int64(point.y-GetSystemMetrics(SM_YVIRTUALSCREEN))*65536+32768)/desktop_height);
		input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE_NOCOALESCE;
		motion_ok = SendInput(1, &input, sizeof(INPUT)) == 1;
		if(!motion_ok) break;
		++motions;
		OverlayProbePump(30);
		const auto current = view.GetCamera();
		if(!ViewerCameraModel::SameCamera(previous, current)) ++camera_changes;
		previous = current;
	}
	motion_ok = motion_ok && GetForegroundWindow() == GetHWND() && view.HasCapture() &&
	            (!capture.done.load() || capture.failure == OverlayProbeCapture::NONE);
	// Matching cleanup is unconditional even if the guarded motion aborted.
	const bool up_sent = down_sent && middle_button.Release();
	if(down_sent) OverlayProbePump(40);
	if(down_sent && !up_sent) report << "input_cleanup: injected middle-up was rejected\n";
	check(motion_ok && motions >= 12 && camera_changes >= 8 && !ViewerCameraModel::SameCamera(camera_before, view.GetCamera()),
	      "bounded real native orbit produces multiple distinct camera updates");
	check(up_sent && !view.IsNavigating() && !view.HasCapture() && ViewerCameraModel::SameCamera(cameras.GetPane(3).camera, view.GetCamera()),
	      "native orbit releases capture and commits through the application model");
	const auto chrome_orbit = pane.GetChromeStats();
	check(chrome_orbit.region_updates == chrome_before.region_updates && chrome_orbit.z_order_updates == chrome_before.z_order_updates,
	      "fixed-size orbit performs no native region replacement or z-order update");
	check(chrome_orbit.native_paints > chrome_before.native_paints && chrome_orbit.axis_paints > chrome_before.axis_paints &&
	      chrome_orbit.tile_paints > chrome_before.tile_paints && view.GetViewStats().rendered_frames > gpu_before,
	      "orbit produces actual bounded overlay tiles and Vulkan frames");
	check(projection->GetText() == projection_before && !view.GetCamera().orthographic,
	      "orbit retains the projection text and P centre used by pixel invariants");
	OverlayProbeStats(report, "orbit", chrome_before, chrome_orbit);
	const Scene3DCamera orbit_camera = view.GetCamera();
	CancelGestures();
	capture.phase.store(1);
	int text_changes = 0;
	String old_text = projection->GetText();
	bool text_ok = true;
	for(int step = 0; step < 4 && !capture.done.load(); ++step) {
		if((step & 1) == 0) SetDirection((step & 2) ? Scene3DViewDirection::Top : Scene3DViewDirection::Front);
		else { text_ok = view.SetCamera(orbit_camera, error) && text_ok; CameraChanged(3); }
		text_ok = view.SetDisplayMode((step & 2) ? Ui3DDisplayMode::Wireframe : Ui3DDisplayMode::SolidEdges, error) && text_ok;
		SyncChrome(false); OverlayProbePump(20);
		// Hold each painted state for two completed samples. One capture may
		// already be in flight; the second then observes the full new state.
		const int observed = capture.completed_samples.load();
		const DWORD state_started = GetTickCount();
		while(capture.completed_samples.load() < observed + 2 && !capture.done.load() &&
		      GetTickCount()-state_started < 400) OverlayProbePump(5);
		text_ok = capture.completed_samples.load() >= observed + 2 && text_ok;
		if(projection->GetText() != old_text) ++text_changes;
		old_text = projection->GetText();
	}
	while(GetTickCount()-capture_started < 3300 && !capture.done.load()) OverlayProbePump(10);
	capture.Stop();
	const auto chrome_text = pane.GetChromeStats();
	check(text_ok && text_changes >= 3 && chrome_text.annotation_paints > chrome_orbit.annotation_paints,
	      "changed projection and direction captions cause actual annotation painting");
	check(chrome_text.region_updates == chrome_orbit.region_updates && chrome_text.z_order_updates == chrome_orbit.z_order_updates &&
	      projection->GetScreenRect() == regions[1] && units->GetScreenRect() == regions[2] && caption->GetScreenRect() == regions[3],
	      "changing text retains fixed HUD slots, native region and z-order");
	OverlayProbeStats(report, "text", chrome_orbit, chrome_text);
	check(capture.failure == OverlayProbeCapture::NONE && capture.frames >= 32 &&
	      capture.time[capture.frames-1]-capture.time[0] >= 3000,
	      "bounded capture observes at least three seconds without foreground, geometry or occlusion loss");

	int orbit_samples = 0, text_samples = 0, units_bad = 0, projection_bad = 0, centre_bad = 0, axis_changes = 0, first_bad = -1;
	int projection_blank = 0, caption_blank = 0;
	std::vector<uint64> projection_hashes, caption_hashes;
	uint64 last_axis_hash = 0;
	const int radius = max(1, DPI(7)-DPI(2)), centre = side/2, inset = DPI(10);
	bool visible_units = false, visible_projection = false, visible_centre = false;
	if(capture.frames) {
		for(int region : { 1, 2 }) {
			const unsigned char *data = capture.Data(0, region);
			bool ink = false;
			for(size_t i = 4; i < capture.bytes[region]; i += 4) ink = ink || !OverlayProbeSamePixel(data, data+i);
			if(region == 1) visible_projection = ink; else visible_units = ink;
		}
		const unsigned char *base = capture.Data(0, 0);
		const unsigned char *centre_base = base+(size_t(centre-radius)*side+centre)*4;
		for(int y = centre-radius; y <= centre+radius; ++y) for(int x = centre-radius; x <= centre+radius; ++x)
			if((x-centre)*(x-centre)+(y-centre)*(y-centre) <= radius*radius)
				visible_centre = visible_centre || !OverlayProbeSamePixel(centre_base, base+(size_t(y)*side+x)*4);
		for(int frame = 0; frame < capture.frames; ++frame) {
			bool bad = !OverlayProbeSameRegion(capture, frame, 2);
			if(bad) ++units_bad;
			if(!OverlayProbeTextVisible(capture, frame, 1, caption_background)) { ++projection_blank; bad = true; }
			if(!OverlayProbeTextVisible(capture, frame, 3, caption_background)) { ++caption_blank; bad = true; }
			if(capture.phase_at_frame[frame] == 0) {
				++orbit_samples;
				if(!OverlayProbeSameRegion(capture, frame, 1)) { ++projection_bad; bad = true; }
				const unsigned char *data = capture.Data(frame, 0);
				bool centre_same = true;
				for(int y = centre-radius; y <= centre+radius; ++y) for(int x = centre-radius; x <= centre+radius; ++x)
					if((x-centre)*(x-centre)+(y-centre)*(y-centre) <= radius*radius) {
						const size_t offset = (size_t(y)*side+x)*4;
						centre_same = centre_same && OverlayProbeSamePixel(base+offset, data+offset);
					}
				if(!centre_same) { ++centre_bad; bad = true; }
				uint64 hash = 1469598103934665603ULL;
				for(int y = inset; y < side-inset; ++y) for(int x = inset; x < side-inset; ++x)
					for(int channel = 0; channel < 3; ++channel) hash = (hash ^ data[(size_t(y)*side+x)*4+channel])*1099511628211ULL;
				if(frame && hash != last_axis_hash) ++axis_changes;
				last_axis_hash = hash;
			}
			else {
				++text_samples;
				const uint64 projection_hash = OverlayProbeRegionHash(capture, frame, 1);
				const uint64 caption_hash = OverlayProbeRegionHash(capture, frame, 3);
				if(std::find(projection_hashes.begin(), projection_hashes.end(), projection_hash) == projection_hashes.end())
					projection_hashes.push_back(projection_hash);
				if(std::find(caption_hashes.begin(), caption_hashes.end(), caption_hash) == caption_hashes.end())
					caption_hashes.push_back(caption_hash);
			}
			if(bad && first_bad < 0) first_bad = frame;
		}
	}
	check(visible_units && visible_projection && visible_centre, "baseline invariant patches contain visible text and projection glyph");
	check(orbit_samples >= 20 && text_samples >= 4 && axis_changes >= 8 && projection_hashes.size() >= 3 && caption_hashes.size() >= 3,
	      "captured axes, projection text and direction text each show multiple displayed states");
	check(units_bad == 0 && projection_bad == 0 && centre_bad == 0 && projection_blank == 0 && caption_blank == 0,
	      "constant HUD patches stay stable and every captured projection and direction caption contains text");
	report << "capture frames=" << capture.frames << " orbit_samples=" << orbit_samples << " text_samples=" << text_samples
	       << " axis_changes=" << axis_changes << " native_motions=" << motions << " camera_changes=" << camera_changes
	       << " units_mismatches=" << units_bad << " projection_mismatches=" << projection_bad << " centre_mismatches=" << centre_bad
	       << " projection_states=" << projection_hashes.size() << " caption_states=" << caption_hashes.size()
	       << " projection_blank_samples=" << projection_blank << " caption_blank_samples=" << caption_blank
	       << " first_bad_frame=" << first_bad << " worker_failure=" << capture.failure << " bytes=" << capture.pixels.size() << "\n";
	const String sheet = AppendFileName(evidence_directory, "Ui3DView-overlay-motion.png");
	check(OverlayProbeSheet(capture, sheet, first_bad), "captured native HUD contact sheet is saved after worker completion");
	report << "overlay_contact_sheet=" << sheet << "\n";

	// A real centre click after the redraw sequence must still reach the gizmo.
	const bool was_orthographic = view.GetCamera().orthographic;
	OverlayProbeButton left_button(MOUSEEVENTF_LEFTUP);
	bool click_sent = motion_ok && up_sent && !view.IsNavigating() && !view.HasCapture() && modifiers_clear() && owns_point(axis_center) &&
	                  SetCursorPos(axis_center.x, axis_center.y);
	OverlayProbePump(20);
	click_sent = click_sent && GetCursorPos(&actual) && std::abs(actual.x-axis_center.x) <= 1 &&
	             std::abs(actual.y-axis_center.y) <= 1 && owns_point(axis_center);
	if(click_sent) {
		INPUT click[2] {};
		click[0].type = click[1].type = INPUT_MOUSE;
		click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
		const UINT inserted = SendInput(2, click, sizeof(INPUT));
		click_sent = inserted == 2;
		if(inserted == 1) {
			left_button.DownSent();
			const bool released = left_button.Release();
			report << "input_cleanup: partial projection click left_up=" << released << "\n";
		}
		OverlayProbePump(100);
	}
	check(click_sent && view.GetCamera().orthographic != was_orthographic,
	      "native projection-centre click still reaches the overlay after repeated redraws");
	ResetView();
	view.SetDisplayMode(Ui3DDisplayMode::SolidEdges, error); SyncChrome(false); view.SetFocus();
	const Point idle_point = view_rect.CenterPoint();
	if(owns_point(idle_point)) SetCursorPos(idle_point.x, idle_point.y);
	OverlayProbePump(150);
	auto idle_before = pane.GetChromeStats();
	uint64 idle_gpu_before = view.GetViewStats().rendered_frames;
	int settled = 0;
	for(int i = 0; i < 10 && settled < 2; ++i) {
		OverlayProbePump(50);
		const auto now = pane.GetChromeStats();
		const uint64 gpu = view.GetViewStats().rendered_frames;
		settled = OverlayProbeIdle(idle_before, now) && gpu == idle_gpu_before ? settled+1 : 0;
		idle_before = now; idle_gpu_before = gpu;
	}
	check(settled >= 2, "overlay and Vulkan work reach a settled idle state");
	OverlayProbePump(500);
	const auto idle_after = pane.GetChromeStats();
	check(OverlayProbeIdle(idle_before, idle_after) && view.GetViewStats().rendered_frames == idle_gpu_before,
	      "idle observation causes no native HUD, tile, region, z-order or Vulkan redraws");
	OverlayProbeStats(report, "idle", idle_before, idle_after);
	const String normal = AppendFileName(evidence_directory, "Ui3DView-overlays-normal.png");
	check(Capture(normal, error), "normal native viewport screenshot is saved for visual review");
	report << "overlay_normal_capture=" << normal << " capture_error=" << error << "\n";
	CancelGestures();
	if(GetForegroundWindow() == GetHWND()) SetCursorPos(cursor_before.x, cursor_before.y);
	check(GetTickCount()-started < 15000, "focused overlay qualification finishes within fifteen seconds");
	return finish();
}

}
