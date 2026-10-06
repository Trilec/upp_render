#include <Ui/Ui.h>
#include <GpuRender/RenderCtrlBridge.h>
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;

// This gate covers drawing of default enabled/disabled controls at two sizes.
// It does not certify populated models, interaction, themes, IME or accessibility.
static int cases = 0;
static int failures = 0;

static void Configure(Ctrl&) {}
static void Configure(UiLabel& c) { c.SetText("GPU label"); }
static void Configure(UiButton& c) { c.SetText("GPU button"); }

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
	Cout() << "UI_GPU_DRAW_SUMMARY painted_inventory_entries=54 cases=" << cases
	       << " failures=" << failures << " os_file_dialog=HOST_SERVICE"
	       << " validation_warnings=" << report.validation_warning_count
	       << " validation_errors=" << report.validation_error_count
	       << " result=" << (ok ? "PASS" : "FAIL") << EOL;
	if(!ok) SetExitCode(1);
}
