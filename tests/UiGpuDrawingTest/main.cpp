#include <Ui/Ui.h>
#include <Ui/UiFileBrowser/UiFileBrowser.h>
#include <GpuRender/RenderCtrlBridge.h>
#if defined(PLATFORM_WIN32) && defined(flagCFONTS)
#include <RenderFontWin32/RenderFontWin32.h>
#endif
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <plugin/png/png.h>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;

// Inventory drawing: enabled/disabled at two sizes, with populated table/tree/text.
// Focused edit-state checks are separate; this does not certify all input, themes,
// DPI, IME or accessibility.
static int cases = 0;
static int failures = 0;
static bool capture = false;
static int captures = 0;
static One<ImageBuffer> contact;

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
static void Probe(const char *name, UiRenderer2D& renderer, const UiRenderer2DTarget& target, VulkanGpuDevice& device)
{
	Control control;
	Configure(control);
	for(Size size : { DPI(128, 32), DPI(320, 180) }) {
		control.SetRect(Rect(size));
		for(bool enabled : { true, false }) {
			control.Enable(enabled);
			UiDisplayList list;
			String error;
			CtrlDisplayListRecordReport report;
			bool pass = RecordCtrlDisplayList(control, list, error, &report) &&
			            !report.HasUnsupportedOperation() && renderer.Render(list, target);
			if(pass && capture && enabled && size == Size(320,180)) {
				Vector<byte> pixels;
				pass &= device.ReadTexturePixels(target.color_target,pixels) && pixels.GetCount()==320*180*4;
				if(pass) {
					ImageBuffer image(320,180);
					for(int y=0;y<image.GetHeight();++y) for(int x=0;x<image.GetWidth();++x) {
						const byte *p=pixels.Begin()+4*(y*image.GetWidth()+x);
						RGBA& c=image[y][x]; c.r=p[0]; c.g=p[1]; c.b=p[2]; c.a=p[3];
					}
					image.SetKind(IMAGE_OPAQUE);
					int col=captures%6,row=captures/6;
					for(int y=0;y<180;++y) std::memcpy((*contact)[row*180+y]+col*320,image[y],320*4);
					pass &= PNGEncoder().SaveFile(GetExeDirFile(String("UiGpu-")+name+".png"),Image(image));
					++captures;
				}
			}
			cases++;
			if(!pass) failures++;
			Cout() << "UI_GPU_DRAW type=" << name << " size=" << size.cx << "x" << size.cy
			       << " enabled=" << (enabled ? 1 : 0) << " ops=" << list.GetCount()
			       << " raster_misses=" << AsString(UiRasterCache::GetStats().misses)
			       << " software_layers=" << AsString(UiGetRenderLayerStats().allocations)
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

class ShadowProbe : public Ctrl {
public:
	StyledMetrics metrics;
	void Paint(Draw& draw) override {
		StyledPalette palette; StyledSkin skin;
		UiPaintStyledBackground(draw,Rect(40,35,180,125),palette,metrics,skin,ST_NORMAL,false);
	}
};

static void CheckShadows(UiRenderer2D& renderer,const UiRenderer2DTarget& target,
                         VulkanGpuDevice& device)
{
	CtrlDisplayListPaintScope host;
	int count=0;
	for(bool inset:{false,true}) for(bool hard:{false,true}) for(int radius:{0,9}) {
		ShadowProbe control; control.SetRect(0,0,320,180);
		auto& m=control.metrics; m.face_enabled=m.frame_enabled=false; m.radius=radius;
		m.shadow.enabled=true; m.shadow.inset=inset; m.shadow.distance=8;
		m.shadow.offset_x=2; m.shadow.offset_y=1; m.shadow.alpha=160;
		m.shadow.color=Color(180,80,20); m.shadow.mode=hard?SHADOW_HARD:SHADOW_CURVE;
		UiDisplayList list; String error; CtrlDisplayListRecordReport report;
		const auto before=UiRasterCache::GetStats();
		bool ok=RecordCtrlDisplayList(control,list,error,&report) &&
		        !report.HasUnsupportedOperation() && report.paint_probe_count==0 &&
		        renderer.Render(list,target);
		const int64 bytes=renderer.GetStats().gpu_clip_layer_bytes;
		ok &= renderer.GetStats().vector_raster_count==0 &&
		      UiRasterCache::GetStats().misses==before.misses;
		// Independent invariants: coloured coverage, empty exterior and no shadow
		// at the centre of an outset cutout. Do not use the old CPU shadow as oracle.
		Vector<byte> pixels; ok &= device.ReadTexturePixels(target.color_target,pixels);
		int coloured=0;
		if(pixels.GetCount()==320*180*4) {
			for(int y=0;y<180;++y) for(int x=0;x<320;++x) {
				const byte *p=pixels.Begin()+4*(y*320+x);
				if(p[0]>p[1] && p[1]>p[2]) ++coloured;
			}
			const byte *centre=pixels.Begin()+4*(80*320+110);
			ok &= pixels[0]==0 && pixels[1]==0 && pixels[2]==0 && coloured>20;
			if(!inset) ok &= centre[0]==0 && centre[1]==0 && centre[2]==0;
		} else ok=false;
		// A repeated same-size effect must reuse retained composition textures.
		ok &= renderer.Render(list,target) && renderer.GetStats().gpu_clip_layer_bytes==bytes;
		Cout()<<"UI_GPU_SHADOW inset="<<inset<<" hard="<<hard<<" radius="<<radius
		      <<" coloured_pixels="<<coloured<<" layer_bytes="<<bytes
		      <<" result="<<(ok?"PASS":"FAIL")<<" error="<<error<<" "<<renderer.GetError()<<EOL;
		if(!ok) ++failures; ++count;
	}
	Cout()<<"UI_GPU_SHADOW cases="<<count<<EOL;
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
	capture = FindIndex(CommandLine(),String("--capture")) >= 0;
	const bool uhd=FindIndex(CommandLine(),String("--uhd"))>=0;
	if(uhd && capture) { SetExitCode(2); return; }
	if(uhd) {
		Font font=StdFont(); font.RealizeStd(); font.Height(max(1,abs(font.GetHeight()))*2);
		Font::SetStdFont(font); Ctrl::ReSkin(); SetUHDMode(true);
	}
	Cout()<<"ui_scale_mode="<<(uhd?"UHD2x":"normal")<<" dpi_unit="<<DPI(1)<<" std_font_height="<<StdFont().GetHeight()<<EOL;
	if(capture) { contact.Create(1920,1800); Fill(~*contact,RGBAZero(),contact->GetLength()); }
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
		desc.size = DPI(320, 180);
		desc.format = GpuFormat::RGBA8Srgb;
		desc.usage = GpuTextureUsage_ColorAttachment | GpuTextureUsage_TransferSrc;
		GpuTextureId texture;
		ok = device.IsReady() && device.CreateTexture(desc, texture) == GpuResult::Ok;
		if(ok) {
			UiRenderer2D renderer(device);
			UiRenderer2DTarget target;
			target.color_target = texture; target.size = desc.size; target.color_format = desc.format;
			target.load_op = GpuLoadOp::Clear; target.store_op = GpuStoreOp::Store;
			target.clear_color.alpha = 1;
			if(!uhd) {
				CheckStyledFace(renderer, target, device);
				CheckTableEditing(renderer, target);
				CheckShadows(renderer, target, device);
			}
			// The independent CPU reference above is excluded from production helper evidence.
			UiRasterCache::Clear();
			target.clear_color.red=target.clear_color.green=target.clear_color.blue=1;
			Probe<TagProbe>("UiTag", renderer, target, device);
			Probe<UiMediaCard>("UiMediaCard", renderer, target, device);
			Probe<UiLabel>("UiLabel", renderer, target, device);
			Probe<UiButton>("UiButton", renderer, target, device);
			Probe<UiToolButton>("UiToolButton", renderer, target, device);
			Probe<UiSplitButton>("UiSplitButton", renderer, target, device);
			Probe<UiCheckBox>("UiCheckBox", renderer, target, device);
			Probe<UiRadioButton>("UiRadioButton", renderer, target, device);
			Probe<UiToggle>("UiToggle", renderer, target, device);
			Probe<UiBreadcrumbs>("UiBreadcrumbs", renderer, target, device);
			Probe<UiLineEdit>("UiLineEdit", renderer, target, device);
			Probe<UiIntEdit>("UiIntEdit", renderer, target, device);
			Probe<UiFloatEdit>("UiFloatEdit", renderer, target, device);
			Probe<UiPasswordEdit>("UiPasswordEdit", renderer, target, device);
			Probe<UiMultiEdit>("UiMultiEdit", renderer, target, device);
			Probe<UiMaskEdit>("UiMaskEdit", renderer, target, device);
			Probe<UiSlider>("UiSlider", renderer, target, device);
			Probe<UiRangeSlider>("UiRangeSlider", renderer, target, device);
			Probe<UiSliderEdit>("UiSliderEdit", renderer, target, device);
			Probe<UiRangeSliderEdit>("UiRangeSliderEdit", renderer, target, device);
			Probe<UiRangeSegments>("UiRangeSegments", renderer, target, device);
			Probe<UiScrollBar>("UiScrollBar", renderer, target, device);
			Probe<UiProgressBar>("UiProgressBar", renderer, target, device);
			Probe<UiProgressRing>("UiProgressRing", renderer, target, device);
			Probe<UiChartRing>("UiChartRing", renderer, target, device);
			Probe<UiMatrixSelector>("UiMatrixSelector", renderer, target, device);
			Probe<UiColorMatrix>("UiColorMatrix", renderer, target, device);
			Probe<UiDateTime>("UiDateTime", renderer, target, device);
			Probe<UiColorPicker>("UiColorPicker", renderer, target, device);
			Probe<UiColorPickerMicro>("UiColorPickerMicro", renderer, target, device);
			Probe<UiFileBrowser>("UiFileBrowser", renderer, target, device);
			Probe<UiDropdown>("UiDropdown", renderer, target, device);
			Probe<UiMenu>("UiMenu", renderer, target, device);
			Probe<UiPanel>("UiPanel", renderer, target, device);
			Probe<UiDirectContentHost>("UiDirectContentHost", renderer, target, device);
			Probe<UiGroupPanel>("UiGroupPanel", renderer, target, device);
			Probe<UiTitleCard>("UiTitleCard", renderer, target, device);
			Probe<UiStack>("UiStack", renderer, target, device);
			Probe<UiAccordion>("UiAccordion", renderer, target, device);
			Probe<UiScrollPanel>("UiScrollPanel", renderer, target, device);
			Probe<UiTab>("UiTab", renderer, target, device);
			Probe<UiSplitter>("UiSplitter", renderer, target, device);
			Probe<UiQuadSplitter>("UiQuadSplitter", renderer, target, device);
			Probe<UiAbsoluteLayout>("UiAbsoluteLayout", renderer, target, device);
			Probe<UiGridLayout>("UiGridLayout", renderer, target, device);
			Probe<UiBoxLayout>("UiBoxLayout", renderer, target, device);
			Probe<UiList>("UiList", renderer, target, device);
			Probe<UiTree>("UiTree", renderer, target, device);
			Probe<UiTable>("UiTable", renderer, target, device);
			Probe<UiGallery>("UiGallery", renderer, target, device);
			Probe<UiDoc>("UiDoc", renderer, target, device);
			Probe<UiBezierCurveEditor>("UiBezierCurveEditor", renderer, target, device);
			Probe<UiBezierCurveField>("UiBezierCurveField", renderer, target, device);
			Probe<UiNodeGraph>("UiNodeGraph", renderer, target, device);
			Probe<UiColorProbe>("UiColorProbe", renderer, target, device);
			Probe<UiPlaybackBar>("UiPlaybackBar", renderer, target, device);
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
	ok &= cases == 224 && failures == 0;
#if defined(PLATFORM_WIN32) && defined(flagCFONTS)
	const auto fonts = GetRenderFontWin32Stats();
	ok &= fonts.legacy_gdi_font_requests == 0 && fonts.error.IsEmpty();
	Cout() << "font_backend=DirectWrite legacy_gdi_font_requests=" << fonts.legacy_gdi_font_requests
	       << " face_cache_entries=" << fonts.face_cache_entries << "/" << fonts.face_cache_limit << EOL;
#endif
	if(capture) ok &= captures==56 && PNGEncoder().SaveFile(GetExeDirFile("UiGpu-contact.png"),Image(*contact));
	const auto tags=UiRasterCache::GetTagBytes();
	for(int i=0;i<tags.GetCount();++i) {
		ok &= tags.GetKey(i).StartsWith("vector/") || tags.GetKey(i).StartsWith("icon/");
		Cout()<<"UI_RASTER_TAG "<<tags.GetKey(i)<<" bytes="<<tags[i]<<EOL;
	}
	Cout() << "UI_GPU_DRAW_SUMMARY painted_inventory_entries=56 cases=" << cases
	       << " failures=" << failures << " os_file_dialog=HOST_SERVICE"
	       << " validation_warnings=" << report.validation_warning_count
	       << " validation_errors=" << report.validation_error_count
	       << " result=" << (ok ? "PASS" : "FAIL") << EOL;
	if(!ok) SetExitCode(1);
}
