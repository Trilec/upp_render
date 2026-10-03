#include <GpuRender/GpuRender.h>
#include <Ui/Ui.h>
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
	ParticleSceneCtrl()
	{
		NoWantFocus();

		ImageDraw d(44, 44);
		d.DrawRect(0, 0, 44, 44, Color(27, 35, 52));
		d.DrawEllipse(6, 6, 32, 32, Color(86, 190, 255), 2, Color(210, 238, 255));
		d.DrawEllipse(16, 13, 9, 9, Color(255, 216, 112));
		badge_ = d;

		SetTimeCallback(-33, [=] {
			if(!paused_) {
				phase_ += 0.033 * speed_;
				Refresh();
			}
		});
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
	void SetPaused(bool on)             { paused_ = on; Refresh(); }
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
		Title("GPU modal dialog").SetRect(0, 0, 440, 220);
		message_.SetText("Another GpuTopWindow, sharing the application GPU domain.");
		note_.SetData("The scene keeps its state after this dialog closes.");
		close_.SetText("Close");
		close_.WhenAction = [=] { Close(); };

		Add(message_.HSizePos(22, 22).TopPos(22, 30));
		Add(note_.HSizePos(22, 22).TopPos(70, 32));
		Add(close_.RightPos(22, 100).BottomPos(20, 34));
	}

private:
	UiLabel message_;
	UiLineEdit note_;
	UiButton close_;
};

class GpuUiGallery : public GpuTopWindow {
public:
	GpuUiGallery()
	{
		SetAsyncPresentation();
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
			gpu_state_.SetText(e.IsEmpty() ? "Vulkan root compositor active" : "GPU compositor: " + e);
		});

		SyncAllControlsFromScene();
	}

	void StartBenchmark(bool heavy, bool soak = false, int seconds = 300)
	{
		scene_.SetParticleCount(heavy ? 512 : 96);
		benchmark_heavy_ = heavy;
		benchmark_soak_ = soak;
		benchmark_seconds_ = seconds;
		SaveFile(GetExeDirFile(soak ? "GpuUiGallery-soak.txt" : heavy ? "GpuUiGallery-load.txt" : "GpuUiGallery-normal.txt"), "benchmark_status=RUNNING\n");
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

		property_model_.AddSliderInt("particle_count", "Particle count", 28, 6, 96, 2, "Motion")
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
		dlg.Run();
		SetStatus("Modal GpuTopWindow closed; the scene state was preserved.");
	}

	void SetStatus(const String& text)
	{
		status_.SetText(text);
	}

private:
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
		                  stats.presented_frames >= 50 && p99 >= 0 && p99 <= 50 && worst <= 100;
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
		       << "timer_delay_ms p50=" << Format("%.2f", Percentile(benchmark_delays_, 0.50))
		       << " p95=" << Format("%.2f", Percentile(benchmark_delays_, 0.95))
		       << " p99=" << Format("%.2f", p99) << " max=" << Format("%.2f", worst) << "\n"
		       << "acquire_replay_present_cpu_ms p50=" << Format("%.2f", Percentile(benchmark_replays_, 0.50))
		       << " p95=" << Format("%.2f", Percentile(benchmark_replays_, 0.95))
		       << " p99=" << Format("%.2f", Percentile(benchmark_replays_, 0.99))
		       << " max=" << Format("%.2f", Percentile(benchmark_replays_, 1.0)) << "\n"
		       << "process_private_peak_bytes=" << AsString(benchmark_private_peak_) << "\n"
		       << "image_cache_entry_peak=" << benchmark_image_entries_ << "\n"
		       << "image_pixel_payload_peak_bytes=" << AsString(benchmark_image_peak_) << "\n"
		       << "vector_pixel_payload_peak_bytes=" << AsString(benchmark_vector_peak_) << "\n"
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
		const String path = GetExeDirFile(benchmark_soak_ ? "GpuUiGallery-soak.txt" : benchmark_heavy_ ? "GpuUiGallery-load.txt" : "GpuUiGallery-normal.txt");
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
	double benchmark_started_ = 0;
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
	bool heavy = false;
	bool soak = false;
	int seconds = 300;
	bool validation = false;
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
		if(arg == "--validation") validation = true;
	}
	{
		GpuUiGallery app;
		if(validation) app.SetValidation();
		if(benchmark) app.StartBenchmark(heavy, soak, seconds);
		app.Run();
	}
	if(benchmark) {
		const auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		bool zero = d.runtime_live_count == 0 && d.instance_live_count == 0 &&
		            d.device_live_count == 0 && d.surface_live_count == 0 && d.swapchain_live_count == 0 &&
		            VulkanGpuDevice::GetSharedImmutableAllocationBytes() == 0;
		String path = GetExeDirFile(soak ? "GpuUiGallery-soak.txt" : heavy ? "GpuUiGallery-load.txt" : "GpuUiGallery-normal.txt");
		String report = LoadFile(path);
		report << "final_native_ownership=" << (zero ? "ZERO" : "NONZERO") << "\n";
		SaveFile(path, report);
		if(!zero || report.Find("responsiveness=PASS") < 0 ||
		   (soak && report.Find("memory_plateau=PASS") < 0)) SetExitCode(1);
	}
}
