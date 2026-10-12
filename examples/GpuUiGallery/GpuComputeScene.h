#pragma once
#include <GpuRender/GpuRender.h>
#include <Ui/Ui.h>
#include <memory>

namespace Upp {
struct ComputeSceneState {
    bool paused = false, dither = true;
    Size work_size = Size(0,0), image_size = Size(0,0);
    int mode = 0;
    uint32 seed = 1;
    float exposure = 0;
    double phase = 0;
    uint64 dispatches = 0;
    int program_builds = 0, image_builds = 0;
    int64 retained_image_bytes = 0;
    double compute_copy_cpu_ms = 0;
    GpuComputeStats compute_stats;
    GpuSamplingStats sampling_stats;
    bool measuring = false;
    Vector<double> frame_cpu_ms;
};

// Small backend-neutral custom-content caller. Root controls and the modal frame
// remain ordinary Ui controls recorded by GpuTopWindow.
class ComputeSceneDialog : public GpuTopWindow {
public:
    explicit ComputeSceneDialog(bool validation, Size work_size = Size(0,0));
    ~ComputeSceneDialog() override;
    void Layout() override;
    void StartQualification(String& report);
    void StartBenchmark(int seconds, String& report);
    bool QualificationPassed() const { return qualification_passed; }
private:
    void Tick();
    void Finish(bool pass, const String& reason);
    void BenchmarkTick(double now);
    std::shared_ptr<ComputeSceneState> state;
    GpuCtrl image;
    UiLabel title, description, status, exposure_label;
    UiButton pause, randomize, close;
    UiCheckBox dither;
    UiDropdown mode;
    UiSlider exposure;
    double epoch = 0, started = 0, fps = 0, paused_phase = 0;
    uint64 frames = 0, qualification_frame = 0;
    int qualification_step = 0;
    bool qualification_passed = false;
    String *qualification_report = nullptr;
    String *benchmark_report = nullptr;
    int benchmark_seconds = 0;
    double benchmark_epoch = 0, benchmark_due = 0, memory_due = 0;
    uint64 benchmark_frames = 0, private_peak = 0;
    Vector<double> callback_delays;
    Vector<uint64> private_samples;
};
}
