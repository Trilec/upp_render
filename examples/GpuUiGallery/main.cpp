#include <GpuRender/GpuRender.h>
#ifdef flagCFONTS
#include <RenderFontWin32/RenderFontWin32.h>
#endif
#include <Ui/Ui.h>
#include <Animation/Animation.h>
#include <Utilities/PropertyEditor/PropertyEditor.h>
#include <Utilities/PropertyEditor/PropertyValueEditors.h>

#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <chrono>
#ifdef PLATFORM_WIN32
#include <psapi.h>
#endif

using namespace Upp;

namespace {

static String BenchmarkReportPath(bool heavy, bool soak, bool required)
{
	return GetExeDirFile(String("GpuUiGallery-") + (required ? "required-" : "") +
	                     (soak ? "soak" : heavy ? "load" : "normal") + ".txt");
}

enum SceneMode {
	SCENE_ORBIT = 0,
	SCENE_FLOW,
	SCENE_PULSE,
	SCENE_SWIRL,
	SCENE_MODE_COUNT,
};

String SceneModeName(int mode)
{
	switch(mode) {
	case SCENE_FLOW:  return "Flow";
	case SCENE_PULSE: return "Pulse";
	case SCENE_SWIRL: return "Swirl";
	default:          return "Orbit";
	}
}

class ParticleSceneCtrl : public Ctrl {
public:
	ParticleSceneCtrl() : motion_(*this)
	{
		NoWantFocus();

		ImageBuffer badge(44, 44);
		BufferPainter d(badge, MODE_ANTIALIASED);
		d.Rectangle(0, 0, 44, 44).Fill(Color(27, 35, 52));
		d.Ellipse(22, 22, 16, 16).Fill(Color(86, 190, 255)).Stroke(2, Color(210, 238, 255));
		d.Ellipse(20.5, 17.5, 4.5, 4.5).Fill(Color(255, 216, 112));
		badge_ = badge;

		motion_.Duration(60000).Loop().Ease([](double progress) { return progress; })
			.OnUpdate([=](double progress) {
				double elapsed = progress - motion_progress_;
				if(elapsed < 0) elapsed += 1; // next scheduler loop
				motion_progress_ = progress;
				phase_ += elapsed * 60.0 * speed_;
				Refresh();
			}).Play();
	}

	void SetMode(int mode)              { mode_ = clamp(mode, 0, SCENE_MODE_COUNT - 1); Refresh(); }
	void SetSpeed(double speed)         { speed_ = clamp(speed, 0.10, 4.00); }
	void SetParticleCount(int count)    { particle_count_ = clamp(count, 6, 512); Refresh(); }
	void SetMotionRadius(int radius)    { motion_radius_ = clamp(radius, 20, 100); Refresh(); }
	void SetParticleSize(int size)      { particle_size_ = clamp(size, 3, 18); Refresh(); }
	void SetBackground(Color color)     { background_ = color; Refresh(); }
	void SetPrimaryColor(Color color)   { primary_ = color; Refresh(); }
	void SetSecondaryColor(Color color) { secondary_ = color; Refresh(); }
	void ShowGrid(bool on)              { show_grid_ = on; Refresh(); }
	void SetPaused(bool on) {
		if(paused_ == on) return;
		paused_ = on;
		if(on) motion_.Pause(); else motion_.Resume();
		Refresh();
	}
	void TogglePaused()                 { SetPaused(!paused_); }

	int GetMode() const              { return mode_; }
	double GetSpeed() const          { return speed_; }
	int GetParticleCount() const     { return particle_count_; }
	int GetMotionRadius() const      { return motion_radius_; }
	int GetParticleSize() const      { return particle_size_; }
	Color GetBackground() const      { return background_; }
	Color GetPrimaryColor() const    { return primary_; }
	Color GetSecondaryColor() const  { return secondary_; }
	bool IsGridVisible() const       { return show_grid_; }
	bool IsPaused() const            { return paused_; }

	void ResetDefaults()
	{
		mode_ = SCENE_ORBIT;
		speed_ = 1.0;
		particle_count_ = 28;
		motion_radius_ = 72;
		particle_size_ = 8;
		background_ = Color(18, 24, 36);
		primary_ = Color(91, 173, 255);
		secondary_ = Color(194, 137, 255);
		show_grid_ = true;
		paused_ = false;
		phase_ = 0.0;
		motion_progress_ = 0.0;
		motion_.Replay();
		Refresh();
	}

	void Paint(Draw& w) override
	{
		const Size sz = GetSize();
		w.DrawRect(sz, background_);

		if(show_grid_) {
			const Color grid = Blend(background_, White(), 28);
			for(int x = 24; x < sz.cx; x += 48)
				w.DrawLine(x, 0, x, sz.cy, 1, grid);
			for(int y = 24; y < sz.cy; y += 48)
				w.DrawLine(0, y, sz.cx, y, 1, grid);
		}

		const double cx = sz.cx * 0.50;
		const double cy = sz.cy * 0.55;
		const double max_rx = max(28.0, sz.cx * 0.42);
		const double max_ry = max(24.0, sz.cy * 0.34);
		const double radius_scale = motion_radius_ / 100.0;
		const double rx = max_rx * radius_scale;
		const double ry = max_ry * radius_scale;

		for(int i = 0; i < particle_count_; ++i) {
			const double q = particle_count_ > 1 ? (double)i / (particle_count_ - 1) : 0.0;
			const double p = i * 0.731;
			double x = cx;
			double y = cy;
			double pulse = 1.0;

			switch(mode_) {
			case SCENE_FLOW: {
				double u = fmod(phase_ * (0.11 + (i % 5) * 0.009) + i * 0.083, 1.0);
				x = 20.0 + u * max(1, sz.cx - 40);
				y = cy + sin(u * 6.283185307 + p) * ry * (0.25 + 0.70 * q);
				break;
			}
			case SCENE_PULSE: {
				const int cols = 10;
				const int row = i / cols;
				const int col = i % cols;
				const int rows = max(1, (particle_count_ + cols - 1) / cols);
				x = 38.0 + col * max(1.0, (sz.cx - 76.0) / max(1, cols - 1));
				y = 78.0 + row * max(1.0, (sz.cy - 130.0) / max(1, rows - 1));
				pulse = 0.65 + 0.45 * (0.5 + 0.5 * sin(phase_ * 1.8 + p));
				break;
			}
			case SCENE_SWIRL: {
				const double a = p + phase_ * (0.55 + (i % 4) * 0.035);
				const double rr = 0.18 + 0.82 * q;
				x = cx + cos(a) * rx * rr;
				y = cy + sin(a * 1.07) * ry * rr;
				break;
			}
			default: {
				const double a = phase_ * (0.34 + (i % 3) * 0.025) + p;
				const double rr = 0.52 + 0.48 * ((i % 7) / 6.0);
				x = cx + sin(a) * rx * rr;
				y = cy + cos(a * 0.91 + p * 0.13) * ry * rr;
				break;
			}
			}

			const Color color = Blend(primary_, secondary_, (int)(q * 255));
			const int r = max(2, (int)(particle_size_ * pulse + (i % 3)));
			w.DrawEllipse(RectC((int)x - r, (int)y - r, 2 * r, 2 * r),
			              color, 1, Blend(color, White(), 150));
		}

		String summary = SceneModeName(mode_) + "  |  " + AsString(particle_count_)
		               + " particles  |  " + Format("%.2f", speed_) + "x";
		if(paused_)
			summary << "  |  paused";

		w.DrawImage(max(8, sz.cx - 58), 14, badge_);
		w.DrawText(16, 14, "GPU Scene Inspector", SansSerif(15).Bold(), Color(232, 239, 248));
		w.DrawText(16, 36, summary, SansSerif(12), Color(166, 181, 202));
		w.DrawText(16, max(54, sz.cy - 24),
		           "Driven live by upp_Ui controls and PropertyEditor",
		           SansSerif(11).Bold(), Color(166, 231, 209));

		const Rect arc_box = RectC(max(16, sz.cx - 132), max(70, sz.cy - 62), 86, 36);
		w.DrawArc(arc_box, arc_box.CenterRight(), arc_box.CenterLeft(), 2, secondary_);
	}

private:
	Animation motion_;
	double motion_progress_ = 0.0;
	Image badge_;
	int mode_ = SCENE_ORBIT;
	double speed_ = 1.0;
	int particle_count_ = 28;
	int motion_radius_ = 72;
	int particle_size_ = 8;
	Color background_ = Color(18, 24, 36);
	Color primary_ = Color(91, 173, 255);
	Color secondary_ = Color(194, 137, 255);
	bool show_grid_ = true;
	bool paused_ = false;
	double phase_ = 0.0;
};

class GalleryDialog : public GpuTopWindow {
public:
	GalleryDialog()
	{
		Title("Vulkan UI workspace").Sizeable().Zoomable().SetRect(0, 0, 900, 620);
		SetMinSize(Size(DPI(680), DPI(480)));
		message_.SetText("Populated Ui controls in a second GPU window");
		note_.SetData("Edit cells, scroll, expand the tree, and type in the notes below.");
		notes_.SetTextUtf8("Vulkan UI notes\n\nSelect text, type, undo and scroll.\nThe scene and inspector retain their state when this window closes.");
		close_.SetText("Close");
		close_.WhenAction = [=] { Close(); };
		{
			UiTableModel& model = table_.Model();
			UiModelUpdate update(model);
			model.SetSize(2000, 3);
			model.SetHeader(UITABLE_COLUMN_AXIS, 0, UiTableHeader("Item"));
			model.SetHeader(UITABLE_COLUMN_AXIS, 1, UiTableHeader("State"));
			model.SetHeader(UITABLE_COLUMN_AXIS, 2, UiTableHeader("Value"));
			for(int r = 0; r < 2000; ++r) {
				model.SetCellValue(r, 0, Format("Render item %04d", r + 1));
				model.SetCellValue(r, 1, r % 3 ? "Ready" : "Review");
				model.SetCellValue(r, 2, r * 7);
			}
		}
		table_.EnableInternalMutation().SetActiveCell(0, 0);
		table_.SetColumnWidth(0, DPI(220)); table_.SetColumnWidth(1, DPI(130));
		{
			UiTreeModel& model = tree_.Model();
			UiModelUpdate update(model);
			for(int group = 0; group < 4; ++group) {
				UiTreeNodeRef parent = model.AddChild(model.Root(), UiModelItem(Format("Group %d", group + 1)));
				for(int i = 0; i < 12; ++i) {
					UiModelItem item(Format("Item %d.%d", group + 1, i + 1));
					item.has_check = true; item.checked = i % 3 == 0; item.editable = true;
					model.AddChild(parent, item);
				}
				tree_.Expand(parent);
			}
		}
		tree_.EnableInternalMutation().EnableRenameOnDblClick();
		Add(message_.HSizePos(DPI(20), DPI(20)).TopPos(DPI(18), DPI(30)));
		Add(note_.HSizePos(DPI(20), DPI(20)).TopPos(DPI(57), DPI(32)));
		Add(tree_.LeftPos(DPI(20), DPI(200)).VSizePos(DPI(108), DPI(210)));
		Add(table_.HSizePos(DPI(236), DPI(20)).VSizePos(DPI(108), DPI(210)));
		Add(notes_.HSizePos(DPI(20), DPI(20)).BottomPos(DPI(64), DPI(130)));
		Add(close_.RightPos(DPI(20), DPI(100)).BottomPos(DPI(18), DPI(32)));
		SetTimeCallback(-250, [=] {
			if(IsGpuRequired() && !GetGpuError().IsEmpty()) { SetExitCode(1); Close(); }
		});
	}

	void StartQualification(String& report)
	{
		qualification_report_ = &report;
		SetTimeCallback(-100, [=] { QualificationTick(); }, 990);
	}
	bool QualificationPassed() const { return qualification_passed_; }

private:
	void QualificationTick()
	{
		if(++qualification_ticks_ > 150 || !GetGpuError().IsEmpty() || GetSoftwareFallbackCount()) {
			FinishQualification(false, "workspace GPU frame or timeout"); return;
		}
		const auto stats = GetGpuStats();
		if(!IsGpuReady() || stats.presented_frames <= qualification_frame_) return;
		qualification_frame_ = stats.presented_frames;
		bool ok = true;
		switch(qualification_step_++) {
		case 0: {
			const auto native = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			ok = native.device_live_count == 1 && native.surface_live_count >= 2 && native.swapchain_live_count >= 2;
			if(qualification_report_) *qualification_report_ << "workspace_native_devices=" << AsString(native.device_live_count)
				<< " surfaces=" << AsString(native.surface_live_count) << " swapchains=" << AsString(native.swapchain_live_count) << "\n";
			note_.SetFocus(); note_.Key(K_CTRL_A, 1); note_.Key('V', 1);
			ok &= note_.HasFocus() && note_.GetTextUtf8() == "V";
			break;
		}
		case 1: {
			notes_.SetFocus(); notes_.Key(K_CTRL_A, 1); notes_.Key('N', 1);
			ok = notes_.HasFocus() && notes_.GetTextUtf8() == "N";
			UiTreeNodeRef first = tree_.Model().GetChild(tree_.Model().Root(), 0);
			tree_.SetCursor(first); tree_.SetFocus(); tree_.Key(K_DOWN, 1);
			ok &= tree_.GetCursor().id != first.id;
			table_.SetFocus(); table_.SetActiveCell(0, 0); table_.Key(K_RIGHT, 1);
			ok &= table_.GetActiveCell().col == 1;
			break;
		}
		case 2:
			table_.BeginEdit(); ok = table_.IsEditing();
			break;
		case 3:
			table_.CommitEditValue("GPU committed");
			ok = !table_.IsEditing() && table_.Model().GetCellValue(0, 1) == Value("GPU committed");
			table_.BeginEdit(); ok &= table_.IsEditing();
			break;
		case 4:
			table_.CancelEdit();
			ok = !table_.IsEditing() && table_.Model().GetCellValue(0, 1) == Value("GPU committed");
			SetRect(Rect(GetRect().TopLeft(), Size(DPI(760), DPI(540))));
			qualification_theme_ = UiTheme::GetContext();
			{ UiThemeContext c = qualification_theme_;
			  c.mode = c.mode == UiThemeMode::Dark ? UiThemeMode::Light : UiThemeMode::Dark;
			  UiTheme::Set(c); Ctrl::SwapDarkLight(); Refresh(); }
			break;
		case 5:
			UiTheme::Set(qualification_theme_); Ctrl::SwapDarkLight(); Refresh();
			break;
		default:
			FinishQualification(true, "focus/text/tree/table edit commit-cancel/resize/light-dark");
			return;
		}
		if(!ok) FinishQualification(false, Format("workspace step %d", qualification_step_ - 1));
	}

	void FinishQualification(bool pass, const String& detail)
	{
		qualification_passed_ = pass;
		if(qualification_report_) *qualification_report_ << "workspace=" << (pass ? "PASS " : "FAIL ") << detail << "\n";
		KillTimeCallback(990);
		Close();
	}

	String *qualification_report_ = nullptr; // Borrowed only during the caller's modal Run.
	UiThemeContext qualification_theme_;
	bool qualification_passed_ = false;
	int qualification_step_ = 0, qualification_ticks_ = 0;
	uint64 qualification_frame_ = 0;
	UiLabel message_;
	UiLineEdit note_;
	UiTree tree_;
	UiTable table_;
	UiMultiEdit notes_;
	UiButton close_;
};

class GpuUiGallery : public GpuTopWindow {
public:
	GpuUiGallery()
	{
		SetAsyncPresentation();
		SetFrameClock();
		Title("GpuRender - upp_Ui GPU Scene Inspector")
		    .Sizeable().Zoomable().SetRect(0, 0, 1220, 760);

		RegisterPropertyEditorV1Editors(property_factory_);

		heading_.SetText("upp_Ui controls driving a live Vulkan-composited scene");
		subheading_.SetText("Dropdown, slider, menu and PropertyEditor all change the running custom Draw scene.");

		BuildMenu();
		BuildToolbar();
		BuildProperties();

		scene_.Tip("Live custom Draw scene recorded through the GpuTopWindow root compositor.");
		mode_.Tip("Choose an animation. The popup is a real upp_Ui transient window.");
		speed_.Tip("Drag while the scene is running to change animation speed continuously.");

		status_.SetText("Ready - use the controls to drive the scene.");
		gpu_state_.SetText("GPU status will update while the window is open.");

		Add(heading_);
		Add(subheading_);
		Add(menu_);
		Add(mode_label_);
		Add(mode_);
		Add(speed_label_);
		Add(speed_);
		Add(speed_value_);
		Add(pause_);
		Add(reset_);
		Add(open_dialog_);
		Add(scene_);
		Add(property_title_);
		Add(properties_);
		Add(status_);
		Add(gpu_state_);

		SetTimeCallback(-250, [=] {
			String e = GetGpuError();
			String state = e.IsEmpty() ? String(IsGpuReady() ? "Vulkan root compositor active" : "GPU initialization pending")
			                         : "GPU compositor: " + e;
			const auto stats = GetGpuStats();
			const double now = NowMs();
			if(!live_fps_epoch_) { live_fps_epoch_ = now; live_fps_frames_ = stats.presented_frames; }
			if(now - live_fps_epoch_ >= 1000) {
				live_fps_ = (stats.presented_frames - live_fps_frames_) * 1000.0 / (now - live_fps_epoch_);
				live_fps_epoch_ = now; live_fps_frames_ = stats.presented_frames;
			}
			if(IsGpuReady()) state << Format(" | %.1f FPS | replay %.2f ms | GPU paths %d | CPU rasters %d",
			                                live_fps_, stats.replay_ms, stats.renderer.gpu_path_count,
			                                stats.renderer.vector_raster_count);
			gpu_state_.SetText(state);
			if(IsGpuRequired() && !e.IsEmpty()) {
				SaveFile(GetExeDirFile("GpuUiGallery-gpu-failure.txt"), e + "\n");
				SetExitCode(1);
				Close();
			}
		});

		SyncAllControlsFromScene();
	}

	void SetDemoScene(int particles, bool grid)
	{
		scene_.SetParticleCount(particles);
		scene_.ShowGrid(grid);
		SyncAllControlsFromScene();
	}
	void StartQualification()
	{
		qualification_started_ = NowMs();
		SaveFile(GetExeDirFile("GpuUiGallery-qualification.txt"), "qualification=RUNNING\n");
		SetTimeCallback(-100, [=] { QualificationTick(); }, 990);
	}
	void StartBenchmark(bool heavy, bool soak = false, int seconds = 300)
	{
		scene_.SetParticleCount(heavy ? 512 : 96);
		SyncAllControlsFromScene();
		benchmark_heavy_ = heavy;
		benchmark_soak_ = soak;
		benchmark_seconds_ = seconds;
		SaveFile(BenchmarkReportPath(heavy, soak, IsGpuRequired()), "benchmark_status=RUNNING\n");
		benchmark_started_ = NowMs();
		benchmark_due_ = benchmark_started_ + 16;
		SetTimeCallback(-16, [=] { BenchmarkTick(); }, 987);
	}

	void Layout() override
	{
		GpuTopWindow::Layout();

		const Rect r = GetSize();
		const int margin = DPI(18);
		const int gap = DPI(12);
		const int heading_h = DPI(28);
		const int subheading_h = DPI(22);
		const int menu_h = DPI(34);
		const int toolbar_h = DPI(36);
		const int status_h = DPI(22);
		const int inspector_w = min(DPI(360), max(DPI(300), r.GetWidth() / 3));

		int y = margin;
		heading_.SetRect(margin, y, max(0, r.GetWidth() - 2 * margin), heading_h);
		y += heading_h;
		subheading_.SetRect(margin, y, max(0, r.GetWidth() - 2 * margin), subheading_h);
		y += subheading_h + DPI(8);
		menu_.SetRect(margin, y, max(0, r.GetWidth() - 2 * margin), menu_h);
		y += menu_h + DPI(10);

		int x = margin;
		mode_label_.SetRect(x, y + DPI(6), DPI(72), DPI(24));
		x += DPI(76);
		mode_.SetRect(x, y, DPI(170), toolbar_h);
		x += DPI(184);
		speed_label_.SetRect(x, y + DPI(6), DPI(52), DPI(24));
		x += DPI(56);

		const int button_space = DPI(300);
		const int speed_value_w = DPI(58);
		const int slider_w = max(DPI(120), r.GetWidth() - x - margin - button_space - speed_value_w);
		speed_.SetRect(x, y, slider_w, toolbar_h);
		x += slider_w + DPI(6);
		speed_value_.SetRect(x, y + DPI(6), speed_value_w, DPI(24));

		int bx = max(x + speed_value_w + DPI(8), r.GetWidth() - margin - button_space);
		pause_.SetRect(bx, y, DPI(82), toolbar_h);
		reset_.SetRect(bx + DPI(90), y, DPI(82), toolbar_h);
		open_dialog_.SetRect(bx + DPI(180), y, DPI(120), toolbar_h);

		y += toolbar_h + gap;
		const int bottom = r.GetHeight() - margin - status_h * 2 - DPI(8);
		const int body_h = max(0, bottom - y);
		const int scene_w = max(DPI(280), r.GetWidth() - 2 * margin - gap - inspector_w);

		scene_.SetRect(margin, y, scene_w, body_h);
		property_title_.SetRect(margin + scene_w + gap, y, inspector_w, DPI(28));
		properties_.SetRect(margin + scene_w + gap, y + DPI(30), inspector_w, max(0, body_h - DPI(30)));

		status_.SetRect(margin, bottom + DPI(5), max(0, r.GetWidth() - 2 * margin), status_h);
		gpu_state_.SetRect(margin, bottom + DPI(5) + status_h, max(0, r.GetWidth() - 2 * margin), status_h);
	}

private:
	void BuildMenu()
	{
		UiMenuModel& model = menu_.Model();
		UiMenuNodeRef root = model.Root();

		UiMenuNodeRef scene = model.AddChild(root, UiMenuItem("Scene"));
		model.AddChild(scene, UiMenuItem("Reset"));
		model.AddChild(scene, UiMenuItem("Pause / Resume"));
		model.AddChild(scene, UiMenuItem("Open GPU dialog"));

		UiMenuNodeRef animation = model.AddChild(root, UiMenuItem("Animation"));
		model.AddChild(animation, UiMenuItem("Orbit"));
		model.AddChild(animation, UiMenuItem("Flow"));
		model.AddChild(animation, UiMenuItem("Pulse"));
		model.AddChild(animation, UiMenuItem("Swirl"));

		UiMenuNodeRef view = model.AddChild(root, UiMenuItem("View"));
		model.AddChild(view, UiMenuItem("Toggle grid"));
		model.AddChild(view, UiMenuItem("Light / Dark"));
		model.AddChild(view, UiMenuItem("Reset colours"));

		menu_.SetMenuBarMode();
		menu_.WhenAction = [=](UiMenuNodeRef, const UiMenuItem& item) {
			const String action = item.text;
			if(action == "Reset")
				ResetScene();
			else if(action == "Pause / Resume")
				TogglePause();
			else if(action == "Open GPU dialog")
				OpenDialog();
			else if(action == "Light / Dark") {
				UiThemeContext context = UiTheme::GetContext();
				context.mode = context.mode == UiThemeMode::Dark ? UiThemeMode::Light : UiThemeMode::Dark;
				UiTheme::Set(context);
				Ctrl::SwapDarkLight();
				Refresh();
			}
			else if(action == "Toggle grid") {
				scene_.ShowGrid(!scene_.IsGridVisible());
				property_model_.SetValue("show_grid", scene_.IsGridVisible(), false);
				properties_.RefreshModel();
				SetStatus("Menu toggled the scene grid.");
			}
			else if(action == "Reset colours") {
				scene_.SetBackground(Color(18, 24, 36));
				scene_.SetPrimaryColor(Color(91, 173, 255));
				scene_.SetSecondaryColor(Color(194, 137, 255));
				SyncPropertyModelFromScene();
				SetStatus("Menu reset the scene colours.");
			}
			else {
				for(int i = 0; i < SCENE_MODE_COUNT; ++i)
					if(action == SceneModeName(i)) {
						SetAnimationMode(i, "UiMenu");
						break;
					}
			}
		};
	}

	void BuildToolbar()
	{
		mode_label_.SetText("Animation");
		mode_.Add("Orbit", SCENE_ORBIT);
		mode_.Add("Flow", SCENE_FLOW);
		mode_.Add("Pulse", SCENE_PULSE);
		mode_.Add("Swirl", SCENE_SWIRL);
		mode_.Select(SCENE_ORBIT);
		mode_.WhenSelect = [=](int index) {
			SetAnimationMode(index, "UiDropdown");
		};

		speed_label_.SetText("Speed");
		speed_.SetRange(0.25, 3.00)
		      .SetStep(0.05)
		      .SetValue(1.00)
		      .ExpandTrack();
		speed_.WhenChanging = [=] { ApplySpeed("UiSlider"); };
		speed_.WhenAction = [=] { ApplySpeed("UiSlider"); };

		pause_.SetText("Pause");
		pause_.WhenAction = [=] { TogglePause(); };

		reset_.SetText("Reset");
		reset_.WhenAction = [=] { ResetScene(); };

		open_dialog_.SetText("GPU dialog");
		open_dialog_.WhenAction = [=] { OpenDialog(); };
	}

	void BuildProperties()
	{
		property_title_.SetText("Scene properties");

		property_model_.AddSliderInt("animation_fps", "Animation target", Animation::GetFPS(), 30, 240, 1, "Motion")
		              .SetInlineEditor(true).SetUnit("fps").SetImpact(PropertyImpactPaint);
		property_model_.AddSliderInt("particle_count", "Particle count", 28, 6, 512, 2, "Motion")
		              .SetInlineEditor(true).SetImpact(PropertyImpactPaint);
		property_model_.AddSliderInt("motion_radius", "Motion radius", 72, 20, 100, 1, "Motion")
		              .SetInlineEditor(true).SetUnit("%").SetImpact(PropertyImpactPaint);
		property_model_.AddSliderInt("particle_size", "Particle size", 8, 3, 18, 1, "Motion")
		              .SetInlineEditor(true).SetUnit("px").SetImpact(PropertyImpactPaint);

		property_model_.AddBoolean("show_grid", "Show grid", true, "Appearance")
		              .SetImpact(PropertyImpactPaint);
		property_model_.AddColor("background", "Background", scene_.GetBackground(), "Appearance")
		              .SetImpact(PropertyImpactPaint);
		property_model_.AddColor("primary", "Primary colour", scene_.GetPrimaryColor(), "Appearance")
		              .SetImpact(PropertyImpactPaint);
		property_model_.AddColor("secondary", "Secondary colour", scene_.GetSecondaryColor(), "Appearance")
		              .SetImpact(PropertyImpactPaint);

		property_model_.SetGroupSubtitle("Motion", "live geometry and animation workload");
		property_model_.SetGroupSubtitle("Appearance", "presentation changes applied immediately");
		property_model_.StructureChanged();

		properties_.SetFactory(&property_factory_);
		properties_.SetModel(&property_model_);
		properties_.SetLabelRatio(46);

		PropertyEditorStyle style = PropertyEditorStyle::System();
		style.show_group_summaries = true;
		properties_.SetStyle(style);

		auto changed = [=](String, Value) {
			ApplyPropertyProjection();
			SetStatus("PropertyEditor updated the live scene.");
		};
		properties_.WhenPreview = changed;
		properties_.WhenCommit = changed;
	}

	Value PropertyValue(const String& id, const Value& fallback = Value()) const
	{
		const PropertyEditorItem *item = property_model_.Find(id);
		return item ? item->value : fallback;
	}

	void ApplyPropertyProjection()
	{
		Animation::SetFPS((int)PropertyValue("animation_fps", Animation::GetFPS()));
		scene_.SetParticleCount((int)PropertyValue("particle_count", 28));
		scene_.SetMotionRadius((int)PropertyValue("motion_radius", 72));
		scene_.SetParticleSize((int)PropertyValue("particle_size", 8));
		scene_.ShowGrid((bool)PropertyValue("show_grid", true));
		scene_.SetBackground(Color(PropertyValue("background", Color(18, 24, 36))));
		scene_.SetPrimaryColor(Color(PropertyValue("primary", Color(91, 173, 255))));
		scene_.SetSecondaryColor(Color(PropertyValue("secondary", Color(194, 137, 255))));
	}

	void SyncPropertyModelFromScene()
	{
		property_model_.SetValue("animation_fps", Animation::GetFPS(), false);
		property_model_.SetValue("particle_count", scene_.GetParticleCount(), false);
		property_model_.SetValue("motion_radius", scene_.GetMotionRadius(), false);
		property_model_.SetValue("particle_size", scene_.GetParticleSize(), false);
		property_model_.SetValue("show_grid", scene_.IsGridVisible(), false);
		property_model_.SetValue("background", scene_.GetBackground(), false);
		property_model_.SetValue("primary", scene_.GetPrimaryColor(), false);
		property_model_.SetValue("secondary", scene_.GetSecondaryColor(), false);
		properties_.RefreshModel();
	}

	void SyncAllControlsFromScene()
	{
		mode_.Select(scene_.GetMode());
		speed_.SetValue(scene_.GetSpeed());
		UpdateSpeedLabel();
		UpdatePauseButton();
		SyncPropertyModelFromScene();
	}

	void SetAnimationMode(int mode, const String& source)
	{
		mode = clamp(mode, 0, SCENE_MODE_COUNT - 1);
		scene_.SetMode(mode);
		if(mode_.GetSelection() != mode)
			mode_.Select(mode);
		SetStatus(source + " selected " + SceneModeName(mode) + " animation.");
	}

	void ApplySpeed(const String& source)
	{
		scene_.SetSpeed(speed_.GetValue());
		UpdateSpeedLabel();
		SetStatus(source + " changed animation speed to " + Format("%.2f", scene_.GetSpeed()) + "x.");
	}

	void UpdateSpeedLabel()
	{
		speed_value_.SetText(Format("%.2f", scene_.GetSpeed()) + "x");
	}

	void TogglePause()
	{
		scene_.TogglePaused();
		UpdatePauseButton();
		SetStatus(scene_.IsPaused() ? "Scene paused." : "Scene resumed.");
	}

	void UpdatePauseButton()
	{
		pause_.SetText(scene_.IsPaused() ? "Resume" : "Pause");
	}

	void ResetScene()
	{
		scene_.ResetDefaults();
		SyncAllControlsFromScene();
		SetStatus("Scene reset to defaults.");
	}

	void OpenDialog()
	{
		GalleryDialog dlg;
		dlg.SetRequireGpu(IsGpuRequired()).SetValidation(IsValidationRequested()).SetAsyncPresentation();
		dlg.Run();
		SetStatus("Modal GpuTopWindow closed; the scene state was preserved.");
	}

	void SetStatus(const String& text)
	{
		status_.SetText(text);
	}

private:
	void QualificationTick()
	{
		if(NowMs() - qualification_started_ > 30000 || !GetGpuError().IsEmpty() || GetSoftwareFallbackCount()) {
			FinishQualification(false, "root GPU frame or timeout"); return;
		}
		if(!IsGpuReady() || GetGpuStats().presented_frames <= qualification_frame_) return;
		qualification_frame_ = GetGpuStats().presented_frames;
		if(qualification_step_++ == 0) {
			pause_.WhenAction();
			bool ok = scene_.IsPaused();
			pause_.WhenAction(); ok &= !scene_.IsPaused();
			speed_.SetValue(1.75); speed_.WhenAction(); ok &= scene_.GetSpeed() == 1.75;
			mode_.WhenSelect(SCENE_PULSE); ok &= scene_.GetMode() == SCENE_PULSE;
			property_model_.SetValue("particle_count", 512, false);
			properties_.WhenCommit("particle_count", 512); ok &= scene_.GetParticleCount() == 512;
			if(!ok) { FinishQualification(false, "root control callbacks"); return; }
			KillTimeCallback(990); // Modal Run pumps timers; prevent root test re-entry.
			for(int i = 0; i < 3; ++i) {
				GalleryDialog dlg;
				dlg.SetRequireGpu().SetValidation(IsValidationRequested()).SetAsyncPresentation();
				dlg.StartQualification(qualification_report_); dlg.Run();
				if(!dlg.QualificationPassed()) { FinishQualification(false, "workspace"); return; }
			}
			qualification_report_ << "workspace_open_edit_close_cycles=3\n";
			qualification_frame_ = GetGpuStats().presented_frames;
			SetTimeCallback(-100, [=] { QualificationTick(); }, 990);
			RequestGpuRefresh();
			return;
		}
		const bool preserved = scene_.GetParticleCount() == 512 && scene_.GetSpeed() == 1.75 &&
		                       scene_.GetMode() == SCENE_PULSE && !scene_.IsPaused();
		FinishQualification(preserved, "pause/resume/speed/mode/inspector/modal state retained");
	}

	void FinishQualification(bool pass, const String& detail)
	{
		qualification_report_ << "root=" << (pass ? "PASS " : "FAIL ") << detail << "\n"
		                      << "software_fallback_count=" << AsString(GetSoftwareFallbackCount()) << "\n"
		                      << "gpu_error=" << GetGpuError() << "\n"
		                      << "qualification=" << (pass ? "PASS" : "FAIL") << "\n";
		SaveFile(GetExeDirFile("GpuUiGallery-qualification.txt"), qualification_report_);
		if(!pass) SetExitCode(1);
		KillTimeCallback(990); Close();
	}

	String qualification_report_;
	double qualification_started_ = 0;
	uint64 qualification_frame_ = 0;
	int qualification_step_ = 0;
	static double NowMs()
	{
		return std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	static uint64 PrivateBytes()
	{
#ifdef PLATFORM_WIN32
		using Query = BOOL (WINAPI *)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
		auto query = (Query)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo");
		PROCESS_MEMORY_COUNTERS_EX counters {};
		counters.cb = sizeof(counters);
		if(query && query(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&counters, sizeof(counters)))
			return counters.PrivateUsage;
#endif
		return 0;
	}

	static double Percentile(Vector<double>& samples, double p)
	{
		if(samples.IsEmpty()) return -1;
		Sort(samples);
		return samples[min(samples.GetCount() - 1, (int)ceil(p * samples.GetCount()) - 1)];
	}

	void BenchmarkTick()
	{
		const double now = NowMs();
		const double duration = now - benchmark_started_;
		if(duration >= 4000) {
			benchmark_delays_.Add(max(0.0, now - benchmark_due_));
			const GpuPresentationStats stats = GetGpuStats();
			if(!benchmark_measure_started_) {
				benchmark_measure_started_ = now;
				benchmark_start_frame_ = stats.presented_frames;
			}
			if(stats.presented_frames != benchmark_last_frame_) {
				benchmark_replays_.Add(stats.acquire_ms + stats.replay_ms + stats.present_ms);
				benchmark_last_frame_ = stats.presented_frames;
			}
			const uint64 private_bytes = PrivateBytes();
			benchmark_private_peak_ = max(benchmark_private_peak_, private_bytes);
			if(benchmark_soak_ && duration >= benchmark_next_memory_) {
				benchmark_memory_.Add(private_bytes);
				benchmark_heap_.Add((uint64)MemoryUsedKb() * 1024);
				benchmark_next_memory_ = duration + 10000;
			}
			benchmark_image_peak_ = max(benchmark_image_peak_, stats.renderer.image_cache_bytes);
			benchmark_image_entries_ = max(benchmark_image_entries_, stats.renderer.image_cache_entry_count);
			benchmark_vector_peak_ = max(benchmark_vector_peak_, stats.renderer.vector_cache_bytes);
		}
		benchmark_due_ = now + 16;
		RequestGpuRefresh();
		if(duration < (benchmark_soak_ ? benchmark_seconds_ * 1000 + 4000 : 24000)) return;
		KillTimeCallback(987);
		const GpuPresentationStats stats = GetGpuStats();
		const double p99 = Percentile(benchmark_delays_, 0.99);
		const double worst = Percentile(benchmark_delays_, 1.0);
		const bool pass = IsGpuReady() && GetGpuError().IsEmpty() &&
		                  stats.presented_frames >= 50 && (!IsGpuRequired() || GetSoftwareFallbackCount() == 0) && p99 >= 0 && p99 <= 50 && worst <= 100;
		uint64 early = 0, late = 0;
		if(benchmark_soak_ && benchmark_memory_.GetCount() >= 8) {
			const int n = benchmark_memory_.GetCount();
			for(int i = 0; i < 4; ++i) {
				early += benchmark_memory_[i]; late += benchmark_memory_[n - 4 + i];
			}
			early /= 4; late /= 4;
		}
		const bool plateau = !benchmark_soak_ || (early > 0 && late <= early + 32 * 1024 * 1024);
		String report;
		report << "GpuUiGallery Windows/Vulkan " << (benchmark_heavy_ ? "512 particles" : "96 particles") << "\n"
		       << "warmup_ms=4000 measured_ms=" << Format("%.2f", duration - 4000) << "\n"
		       << "presented_frames=" << AsString(stats.presented_frames)
		       << " dropped_frames=" << AsString(stats.dropped_frames)
		       << " pending_frames=" << stats.pending_frames << "\n"
		       << "measured_presented_frames=" << AsString(stats.presented_frames - benchmark_start_frame_)
		       << " throughput_elapsed_ms=" << Format("%.2f", now - benchmark_measure_started_)
		       << " presented_fps=" << Format("%.2f", (stats.presented_frames - benchmark_start_frame_) * 1000.0 /
		                                                   max(1.0, now - benchmark_measure_started_)) << "\n"
		       << "adapter=" << stats.adapter_name << "\n"
		       << "cold_first_successful_frame_cpu_ms=" << Format("%.2f", stats.first_frame_cpu_ms) << "\n"
		       << "cold_texture_uploads=" << stats.first_renderer.texture_upload_count
		       << " cold_glyph_misses=" << stats.first_renderer.glyph_cache_miss_count
		       << " cold_vector_rasters=" << stats.first_renderer.vector_raster_count
		       << " cold_gpu_coverage_renders=" << stats.first_renderer.gpu_path_coverage_render_count << "\n"
		       << "frame_clock_active=" << (IsFrameClockActive() ? 1 : 0) << "\n"
		       << "timer_delay_ms p50=" << Format("%.2f", Percentile(benchmark_delays_, 0.50))
		       << " p95=" << Format("%.2f", Percentile(benchmark_delays_, 0.95))
		       << " p99=" << Format("%.2f", p99) << " max=" << Format("%.2f", worst) << "\n"
		       << "acquire_replay_present_cpu_ms p50=" << Format("%.2f", Percentile(benchmark_replays_, 0.50))
		       << " p95=" << Format("%.2f", Percentile(benchmark_replays_, 0.95))
		       << " p99=" << Format("%.2f", Percentile(benchmark_replays_, 0.99))
		       << " max=" << Format("%.2f", Percentile(benchmark_replays_, 1.0)) << "\n"
		       << "root_record_ms last=" << Format("%.2f", stats.record_ms)
		       << " lifetime_max=" << Format("%.2f", stats.record_max_ms) << "\n"
		       << "root_enqueue_ms last=" << Format("%.2f", stats.enqueue_ms)
		       << " lifetime_max=" << Format("%.2f", stats.enqueue_max_ms) << "\n"
		       << "process_private_peak_bytes=" << AsString(benchmark_private_peak_) << "\n"
		       << "image_cache_entry_peak=" << benchmark_image_entries_ << "\n"
		       << "image_pixel_payload_peak_bytes=" << AsString(benchmark_image_peak_) << "\n"
		       << "vector_pixel_payload_peak_bytes=" << AsString(benchmark_vector_peak_) << "\n"
		       << "gpu_paths_last=" << stats.renderer.gpu_path_count << "\n"
		       << "gpu_path_vertices_last=" << stats.renderer.gpu_path_vertex_count << "\n"
		       << "gpu_coverage_draws_last=" << stats.renderer.gpu_path_coverage_draw_count << "\n"
		       << "gpu_coverage_renders_last=" << stats.renderer.gpu_path_coverage_render_count << "\n"
		       << "gpu_coverage_atlas_bytes=" << AsString(stats.renderer.gpu_path_coverage_bytes) << "\n"
		       << "gpu_path_cache_misses_last=" << stats.renderer.gpu_path_cache_miss_count << "\n"
		       << "gpu_path_cache_entries_last=" << stats.renderer.gpu_path_cache_entry_count << "\n"
		       << "gpu_path_cache_payload_bytes_last=" << AsString(stats.renderer.gpu_path_cache_bytes) << "\n"
		       << "cpu_vector_rasters_last=" << stats.renderer.vector_raster_count << "\n"
		       << "cpu_glyph_misses_last=" << stats.renderer.glyph_cache_miss_count << "\n"
		       << "ui_shared_raster_cache_bytes=" << AsString(UiRasterCache::GetStats().bytes) << "\n"
		       << "ui_shared_raster_cache_misses_total=" << AsString(UiRasterCache::GetStats().misses) << "\n"
		       << "ui_software_layer_allocations_total=" << AsString(UiGetRenderLayerStats().allocations) << "\n"
		       << "vertex_buffer_capacity_bytes_last=" << AsString(stats.renderer.vertex_buffer_capacity) << "\n"
		       << "textured_vertex_buffer_capacity_bytes_last=" << AsString(stats.renderer.textured_vertex_buffer_capacity) << "\n"
		       << "grid=" << (scene_.IsGridVisible() ? 1 : 0) << "\n"
		       << "animation_driver=upp_animation scheduler_fps=" << Animation::GetFPS() << "\n"
		       << "gpu_required=" << (IsGpuRequired() ? 1 : 0) << "\n"
		       << "software_fallback_count=" << AsString(GetSoftwareFallbackCount()) << "\n"
		       << "validation_requested=" << (IsValidationRequested() ? 1 : 0) << "\n"
		       << "gpu_timestamp_ms=unavailable\n"
		       << "gpu_error=" << GetGpuError() << "\n"
		       << "responsiveness=" << (pass ? "PASS" : "FAIL") << "\n";
		report << "soak=" << (benchmark_soak_ ? 1 : 0) << "\n";
		if(benchmark_soak_) {
			report << "memory_sample_interval_ms=10000 early_mean_bytes=" << AsString(early)
			       << " late_mean_bytes=" << AsString(late) << " allowed_growth_bytes=33554432\n"
			       << "memory_plateau=" << (plateau ? "PASS" : "FAIL") << "\n";
			for(uint64 bytes : benchmark_memory_) report << "private_sample_bytes=" << AsString(bytes) << "\n";
			for(uint64 bytes : benchmark_heap_) report << "upp_heap_sample_bytes=" << AsString(bytes) << "\n";
		}
		const String path = BenchmarkReportPath(benchmark_heavy_, benchmark_soak_, IsGpuRequired());
		if(!SaveFile(path, report)) SetExitCode(2);
		else if(!pass || !plateau) SetExitCode(1);
		Close();
	}

	bool benchmark_heavy_ = false;
	bool benchmark_soak_ = false;
	int benchmark_seconds_ = 300;
	Vector<uint64> benchmark_heap_;
	double benchmark_next_memory_ = 30000;
	Vector<uint64> benchmark_memory_;
	double live_fps_epoch_ = 0, live_fps_ = 0;
	uint64 live_fps_frames_ = 0;
	double benchmark_started_ = 0;
	double benchmark_measure_started_ = 0;
	uint64 benchmark_start_frame_ = 0;
	double benchmark_due_ = 0;
	uint64 benchmark_last_frame_ = 0;
	uint64 benchmark_private_peak_ = 0;
	int benchmark_image_entries_ = 0;
	int64 benchmark_image_peak_ = 0;
	int64 benchmark_vector_peak_ = 0;
	Vector<double> benchmark_delays_;
	Vector<double> benchmark_replays_;
	UiLabel heading_;
	UiLabel subheading_;
	UiMenu menu_;

	UiLabel mode_label_;
	UiDropdown mode_;
	UiLabel speed_label_;
	UiSlider speed_;
	UiLabel speed_value_;
	UiButton pause_;
	UiButton reset_;
	UiButton open_dialog_;

	ParticleSceneCtrl scene_;

	UiLabel property_title_;
	PropertyEditor properties_;
	PropertyEditorFactory property_factory_;
	PropertyEditorModel property_model_;

	UiLabel status_;
	UiLabel gpu_state_;
};

} // namespace

GUI_APP_MAIN
{
	bool benchmark = false;
	bool qualification = false;
	bool heavy = false;
	bool soak = false;
	int seconds = 300;
	bool validation = false;
	bool require_gpu = false;
	bool grid = true;
	int particles = 28;
	int animation_fps = 120;
	for(const String& arg : CommandLine()) {
		if(arg == "--benchmark" || arg == "--benchmark-load") {
			benchmark = true;
			heavy = arg == "--benchmark-load";
		}
		if(arg == "--benchmark-soak") { benchmark = true; heavy = true; soak = true; }
		if(arg.StartsWith("--benchmark-seconds=")) {
			seconds = ScanInt(arg.Mid(20));
			if(IsNull(seconds) || seconds < 20 || seconds > 300) { SetExitCode(2); return; }
		}
		if(arg.StartsWith("--particles=")) {
			particles = ScanInt(arg.Mid(12));
			if(IsNull(particles) || particles < 6 || particles > 512) { SetExitCode(2); return; }
		}
		if(arg.StartsWith("--animation-fps=")) {
			animation_fps = ScanInt(arg.Mid(16));
			if(IsNull(animation_fps) || animation_fps < 30 || animation_fps > 240) { SetExitCode(2); return; }
		}
		if(arg == "--qualify-ui") { qualification = true; require_gpu = true; }
		if(arg == "--no-grid") grid = false;
		if(arg == "--validation") validation = true;
		if(arg == "--require-gpu") require_gpu = true;
	}
	if(qualification && benchmark) { SetExitCode(2); return; }
	Animation::SetFPS(animation_fps);
	{
		GpuUiGallery app;
		app.SetDemoScene(particles, grid);
		if(validation) app.SetValidation();
		if(require_gpu) app.SetRequireGpu();
		if(benchmark) app.StartBenchmark(heavy, soak, seconds);
		if(qualification) app.StartQualification();
		app.Run();
	}
	Animation::Finalize();
	if(benchmark || qualification) {
		const auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		bool zero = d.runtime_live_count == 0 && d.instance_live_count == 0 &&
		            d.device_live_count == 0 && d.surface_live_count == 0 && d.swapchain_live_count == 0 &&
		            VulkanGpuDevice::GetSharedImmutableAllocationBytes() == 0;
		String path = qualification ? GetExeDirFile("GpuUiGallery-qualification.txt") : BenchmarkReportPath(heavy, soak, require_gpu);
		String report = LoadFile(path);
		report << "final_native_ownership=" << (zero ? "ZERO" : "NONZERO") << "\n";
#ifdef flagCFONTS
		const auto fonts = GetRenderFontWin32Stats();
		report << "font_backend=DirectWrite legacy_gdi_font_requests=" << AsString(fonts.legacy_gdi_font_requests)
		       << " face_cache_entries=" << fonts.face_cache_entries << "/" << fonts.face_cache_limit
		       << " font_error=" << fonts.error << "\n";
		if(fonts.legacy_gdi_font_requests || !fonts.error.IsEmpty()) SetExitCode(1);
#endif
		SaveFile(path, report);
		if(!zero || (qualification ? report.Find("qualification=PASS") < 0 : report.Find("responsiveness=PASS") < 0) ||
		   (soak && report.Find("memory_plateau=PASS") < 0)) SetExitCode(1);
	}
}
