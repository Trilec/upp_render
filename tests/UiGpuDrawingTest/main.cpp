#include <Ui/Ui.h>
#include <GpuRender/RenderCtrlBridge.h>
#ifdef flagCFONTS
#include <RenderFontWin32/RenderFontWin32.h>
#endif
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;

// Inventory drawing: enabled/disabled at two sizes, with populated table/tree/text.
// Focused edit-state checks are separate; this does not certify all input, themes,
// DPI, IME or accessibility.
static int cases = 0;
static int failures = 0;

static void Configure(Ctrl&) {}
static void Configure(UiLabel& c) { c.SetText("GPU label"); }
static void Configure(UiButton& c) { c.SetText("GPU button"); }
static void Configure(UiLineEdit& c) { c.SetData("GPU text selection"); }
static void Configure(UiMultiEdit& c) { c.SetTextUtf8("GPU multiline text\nSecond line\nThird line"); }
static void Configure(UiTable& c) {
	UiTableModel& model = c.Model(); UiModelUpdate update(model);
	model.SetSize(20, 3);
	for(int col = 0; col < 3; ++col) model.SetHeader(UITABLE_COLUMN_AXIS, col, UiTableHeader(Format("Column %d", col + 1)));
	for(int row = 0; row < 20; ++row) for(int col = 0; col < 3; ++col)
		model.SetCellValue(row, col, Format("Cell %d.%d", row + 1, col + 1));
	c.EnableInternalMutation().SetActiveCell(1, 1);
}
static void Configure(UiTree& c) {
	UiTreeModel& model = c.Model(); UiModelUpdate update(model);
	UiTreeNodeRef group = model.AddChild(model.Root(), UiModelItem("Expanded group"));
	for(int i = 0; i < 12; ++i) {
		UiModelItem item(Format("Node %d", i + 1)); item.has_check = true; item.checked = i % 2;
		model.AddChild(group, item);
	}
	c.Expand(group).SetCursor(model.GetChild(group, 2));
}

class TagProbe : public Ctrl {
	void Paint(Draw& draw) override {
		UiTagData data;
		data.text = "GPU tag";
		auto style = UiResolveTagStyle(data.role);
		auto tag = UiPrepareTag(data, style, Rect(GetSize()));
		UiPaintTag(draw, tag, IsEnabled() ? ST_NORMAL : ST_DISABLED);
	}
};

template <class Control>
static void Probe(const char *name, UiRenderer2D& renderer, const UiRenderer2DTarget& target)
{
	Control control;
	Configure(control);
	for(Size size : { Size(128, 32), Size(320, 180) }) {
		control.SetRect(Rect(size));
		for(bool enabled : { true, false }) {
			control.Enable(enabled);
			UiDisplayList list;
			String error;
			CtrlDisplayListRecordReport report;
			bool pass = RecordCtrlDisplayList(control, list, error, &report) &&
			            !report.HasUnsupportedOperation() && renderer.Render(list, target);
			cases++;
			if(!pass) failures++;
			Cout() << "UI_GPU_DRAW type=" << name << " size=" << size.cx << "x" << size.cy
			       << " enabled=" << (enabled ? 1 : 0) << " ops=" << list.GetCount()
			       << " result=" << (pass ? "PASS" : "FAIL")
			       << " error=" << error << " gpu_error=" << (pass ? String() : renderer.GetError()) << EOL;
		}
	}
}

static FARPROC WINAPI TestResolver(HMODULE, LPCSTR)
{
	return reinterpret_cast<FARPROC>(1);
}

class StyledFaceProbe : public Ctrl {
public:
	void Paint(Draw& draw) override {
		StyledPalette palette;
		palette.face[ST_NORMAL] = Color(45, 120, 210);
		palette.frame[ST_NORMAL] = Color(220, 240, 255);
		StyledMetrics metrics; metrics.radius = 8; metrics.frame_width = 2;
		UiPaintFaceFrameDash(draw, Rect(5, 5, 91, 35), palette, metrics, ST_NORMAL);
	}
};

static void CheckStyledFace(UiRenderer2D& renderer, const UiRenderer2DTarget& parent_target,
                            VulkanGpuDevice& device)
{
	bool ok = true;
	GpuTextureDesc desc; desc.size = parent_target.size; desc.format = GpuFormat::RGBA8;
	desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc;
	GpuTextureId texture;
	if(device.CreateTexture(desc, texture) != GpuResult::Ok) { failures++; return; }
	UiRenderer2DTarget target = parent_target; target.color_target = texture; target.color_format = desc.format;
	StyledFaceProbe control; control.SetRect(0, 0, 96, 40);
	UiDisplayList list; String error; CtrlDisplayListRecordReport report;
	const auto before = UiRasterCache::GetStats();
	{
		CtrlDisplayListPaintScope host;
		ok &= RecordCtrlDisplayList(control, list, error, &report);
	}
	const auto recorded = UiRasterCache::GetStats();
#ifdef flagGPUUI
	ok &= report.image_count == 0 && report.path_count == 1 &&
	      report.paint_probe_count == 0 && recorded.misses == before.misses;
#endif
	ok &= renderer.Render(list, target);
#ifdef flagGPUUI
	ok &= renderer.GetStats().gpu_path_count == 2 && renderer.GetStats().vector_raster_count == 0;
#endif
	// Ordinary Painter is the independent resolved face/frame pixel reference.
	ImagePainter reference(target.size); reference.Clear(Black());
	control.Paint(reference);
	Image expected = reference.GetResult();
	Vector<byte> pixels;
	ok &= device.ReadTexturePixels(target.color_target, pixels);
	int worst = 0; int64 total = 0;
	if(pixels.GetCount() == target.size.cx * target.size.cy * 4)
		for(int y = 0; y < 40; ++y) for(int x = 0; x < 96; ++x) {
			const RGBA& e = expected[y][x];
			const byte *p = pixels.Begin() + 4 * (y * target.size.cx + x);
			int d = max(abs((int)e.r - p[0]), max(abs((int)e.g - p[1]), abs((int)e.b - p[2])));
			worst = max(worst, d); total += d;
		}
	else ok = false;
	ok &= worst <= 64 && total <= 96 * 40 * 3;
	Cout() << "UI_GPU_STYLED_FACE max_channel_error=" << worst
	       << " mean_max_channel_error=" << Format("%.4f", total / (96.0 * 40))
	       << " semantic_paths=" << report.path_count << " images=" << report.image_count
	       << " frame_gdi_probes=" << report.paint_probe_count
	       << " result=" << (ok ? "PASS" : "FAIL") << EOL;
	ok &= device.DestroyTexture(texture) == GpuResult::Ok;
	if(!ok) failures++;
}

static void CheckTableEditing(UiRenderer2D& renderer, const UiRenderer2DTarget& target)
{
	UiTable table; Configure(table); table.SetRect(0, 0, 320, 180);
	CtrlDisplayListPaintScope host;
	auto replay = [&] {
		UiDisplayList list; String error; CtrlDisplayListRecordReport report;
		return RecordCtrlDisplayList(table, list, error, &report) && report.paint_probe_count == 0 &&
		       !report.HasUnsupportedOperation() && renderer.Render(list, target);
	};
	bool ok = table.BeginEdit() && table.IsEditing() && replay();
	ok &= table.CommitEditValue("GPU edit committed") && !table.IsEditing() &&
	      AsString(table.Model().GetCellValue(1, 1)) == "GPU edit committed" && replay();
	ok &= table.BeginEdit();
	table.CancelEdit();
	ok &= !table.IsEditing() && AsString(table.Model().GetCellValue(1, 1)) == "GPU edit committed" && replay();
	Cout() << "UI_GPU_TABLE_EDIT commit_cancel_and_inline_drawing=" << (ok ? "PASS" : "FAIL") << EOL;
	if(!ok) failures++;
}

GUI_APP_MAIN
{
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	HWND hwnd = CreateWindowExW(0, L"STATIC", L"UiGpuDrawingTest", WS_POPUP,
	                            0, 0, 320, 180, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	VulkanSurfaceSession session;
	GpuNativeWindowDesc window;
	window.kind = GpuNativeWindowKind::Win32;
	window.handle = (uintptr_t)hwnd;
	bool ok = hwnd && session.Open(true, window, &TestResolver) && session.IsReady();
	if(ok) {
		VulkanGpuDevice device(session);
		GpuTextureDesc desc;
		desc.size = Size(320, 180);
		desc.format = GpuFormat::RGBA8Srgb;
		desc.usage = GpuTextureUsage_ColorAttachment;
		GpuTextureId texture;
		ok = device.IsReady() && device.CreateTexture(desc, texture) == GpuResult::Ok;
		if(ok) {
			UiRenderer2D renderer(device);
			UiRenderer2DTarget target;
			target.color_target = texture; target.size = desc.size; target.color_format = desc.format;
			target.load_op = GpuLoadOp::Clear; target.store_op = GpuStoreOp::Store;
			target.clear_color.alpha = 1;
			CheckStyledFace(renderer, target, device);
			CheckTableEditing(renderer, target);
			Probe<TagProbe>("UiTag", renderer, target);
			Probe<UiMediaCard>("UiMediaCard", renderer, target);
			Probe<UiLabel>("UiLabel", renderer, target);
			Probe<UiButton>("UiButton", renderer, target);
			Probe<UiToolButton>("UiToolButton", renderer, target);
			Probe<UiSplitButton>("UiSplitButton", renderer, target);
			Probe<UiCheckBox>("UiCheckBox", renderer, target);
			Probe<UiRadioButton>("UiRadioButton", renderer, target);
			Probe<UiToggle>("UiToggle", renderer, target);
			Probe<UiBreadcrumbs>("UiBreadcrumbs", renderer, target);
			Probe<UiLineEdit>("UiLineEdit", renderer, target);
			Probe<UiIntEdit>("UiIntEdit", renderer, target);
			Probe<UiFloatEdit>("UiFloatEdit", renderer, target);
			Probe<UiPasswordEdit>("UiPasswordEdit", renderer, target);
			Probe<UiMultiEdit>("UiMultiEdit", renderer, target);
			Probe<UiMaskEdit>("UiMaskEdit", renderer, target);
			Probe<UiSlider>("UiSlider", renderer, target);
			Probe<UiRangeSlider>("UiRangeSlider", renderer, target);
			Probe<UiSliderEdit>("UiSliderEdit", renderer, target);
			Probe<UiRangeSliderEdit>("UiRangeSliderEdit", renderer, target);
			Probe<UiRangeSegments>("UiRangeSegments", renderer, target);
			Probe<UiScrollBar>("UiScrollBar", renderer, target);
			Probe<UiProgressBar>("UiProgressBar", renderer, target);
			Probe<UiProgressRing>("UiProgressRing", renderer, target);
			Probe<UiChartRing>("UiChartRing", renderer, target);
			Probe<UiMatrixSelector>("UiMatrixSelector", renderer, target);
			Probe<UiColorMatrix>("UiColorMatrix", renderer, target);
			Probe<UiDateTime>("UiDateTime", renderer, target);
			Probe<UiColorPicker>("UiColorPicker", renderer, target);
			Probe<UiDropdown>("UiDropdown", renderer, target);
			Probe<UiMenu>("UiMenu", renderer, target);
			Probe<UiPanel>("UiPanel", renderer, target);
			Probe<UiDirectContentHost>("UiDirectContentHost", renderer, target);
			Probe<UiGroupPanel>("UiGroupPanel", renderer, target);
			Probe<UiTitleCard>("UiTitleCard", renderer, target);
			Probe<UiStack>("UiStack", renderer, target);
			Probe<UiAccordion>("UiAccordion", renderer, target);
			Probe<UiScrollPanel>("UiScrollPanel", renderer, target);
			Probe<UiTab>("UiTab", renderer, target);
			Probe<UiSplitter>("UiSplitter", renderer, target);
			Probe<UiQuadSplitter>("UiQuadSplitter", renderer, target);
			Probe<UiAbsoluteLayout>("UiAbsoluteLayout", renderer, target);
			Probe<UiGridLayout>("UiGridLayout", renderer, target);
			Probe<UiBoxLayout>("UiBoxLayout", renderer, target);
			Probe<UiList>("UiList", renderer, target);
			Probe<UiTree>("UiTree", renderer, target);
			Probe<UiTable>("UiTable", renderer, target);
			Probe<UiGallery>("UiGallery", renderer, target);
			Probe<UiDoc>("UiDoc", renderer, target);
			Probe<UiBezierCurveEditor>("UiBezierCurveEditor", renderer, target);
			Probe<UiBezierCurveField>("UiBezierCurveField", renderer, target);
			Probe<UiNodeGraph>("UiNodeGraph", renderer, target);
			Probe<UiColorProbe>("UiColorProbe", renderer, target);
			Probe<UiPlaybackBar>("UiPlaybackBar", renderer, target);
			renderer.Close();
			ok &= device.DestroyTexture(texture) == GpuResult::Ok;
		}
		ok &= device.GetLiveBufferCount() == 0 && device.GetLiveTextureCount() == 0 &&
		      device.GetLivePipelineCount() == 0 && device.GetLiveShaderCount() == 0 &&
		      device.GetLiveCommandCount() == 0 && device.GetLiveAllocationBytes() == 0;
	}
	else
		Cout() << "session_error=" << session.GetError() << EOL;
	session.Close();
	if(hwnd) DestroyWindow(hwnd);
	const auto report = session.GetReport();
	const auto native = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	ok &= report.validation_warning_count == 0 && report.validation_error_count == 0 &&
	      native.runtime_live_count == 0 && native.device_live_count == 0 &&
	      native.instance_live_count == 0 && native.surface_live_count == 0 &&
	      VulkanGpuDevice::GetSharedImmutableAllocationBytes() == 0;
	ok &= cases == 216 && failures == 0;
#ifdef flagCFONTS
	const auto fonts = GetRenderFontWin32Stats();
	ok &= fonts.legacy_gdi_font_requests == 0 && fonts.error.IsEmpty();
	Cout() << "font_backend=DirectWrite legacy_gdi_font_requests=" << fonts.legacy_gdi_font_requests
	       << " face_cache_entries=" << fonts.face_cache_entries << "/" << fonts.face_cache_limit << EOL;
#endif
	Cout() << "UI_GPU_DRAW_SUMMARY painted_inventory_entries=54 cases=" << cases
	       << " failures=" << failures << " os_file_dialog=HOST_SERVICE"
	       << " validation_warnings=" << report.validation_warning_count
	       << " validation_errors=" << report.validation_error_count
	       << " result=" << (ok ? "PASS" : "FAIL") << EOL;
	if(!ok) SetExitCode(1);
}
