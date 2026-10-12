#include "GpuComputeScene.h"
#include "../../shaders/ImageSceneSpirv.h"
#include <chrono>
#ifdef PLATFORM_WIN32
#include <psapi.h>
#endif

namespace Upp {
namespace {
double NowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
uint64 PrivateBytes() {
#ifdef PLATFORM_WIN32
    using Query = BOOL (WINAPI *)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    auto query = (Query)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo");
    PROCESS_MEMORY_COUNTERS_EX counters {}; counters.cb = sizeof(counters);
    if(query && query(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&counters, sizeof(counters))) return counters.PrivateUsage;
#endif
    return 0;
}
double Percentile(Vector<double>& values, double p) {
    if(values.IsEmpty()) return -1;
    Sort(values); return values[min(values.GetCount() - 1, (int)ceil(p * values.GetCount()) - 1)];
}
class ComputeSceneContent : public GpuSurfaceContent {
public:
    explicit ComputeSceneContent(std::shared_ptr<ComputeSceneState> state) : state(std::move(state)) {}
    bool Render(GpuDevice& device, const GpuFrameInfo& frame, String& error) override {
        auto fail = [&](const String& message) { error = message + ": " + device.GetLastError(); return false; };
        if(!pipeline.IsValid()) {
            GpuComputeCapabilities caps;
            if(device.GetComputeCapabilities(caps) != GpuResult::Ok) return fail("Compute capability unavailable");
            GpuShaderDesc desc; desc.stage = GpuShaderStage::Compute; desc.format = GpuShaderFormat::SpirV;
            desc.code = String(reinterpret_cast<const char *>(IMAGE_SCENE_SPIRV), sizeof(IMAGE_SCENE_SPIRV));
            if(device.CreateShader(desc, shader) != GpuResult::Ok) return fail("Compute shader");
            GpuComputePipelineDesc program; program.shader = shader; program.storage_buffer_count = 1; program.parameter_bytes = 32;
            if(device.CreateComputePipeline(program, pipeline) != GpuResult::Ok) return fail("Compute pipeline");
            renderer.reset(new UiRenderer2D(device));
            ++state->program_builds;
        }
        const Size work = state->work_size.cx ? state->work_size : frame.size;
        if(size != work) {
            if(work.cx <= 0 || work.cy <= 0 || work.cx > 8192 || work.cy > 8192 ||
               (int64)work.cx * work.cy * 8 > 64 * 1024 * 1024)
                return fail("Compute showcase surface exceeds 64 MiB image payload or 8192 dimension bound");
            if(output.IsValid()) device.DestroyBuffer(output);
            if(texture.IsValid()) device.DestroyTexture(texture);
            output = GpuBufferId(); texture = GpuTextureId();
            GpuBufferDesc buffer; buffer.size = (int64)work.cx * work.cy * 4;
            buffer.usage = GpuBufferUsage_Storage | GpuBufferUsage_TransferSrc;
            buffer.memory = GpuBufferMemory::DeviceLocal;
            if(device.CreateBuffer(buffer, output) != GpuResult::Ok) return fail("Compute output buffer");
            GpuTextureDesc image; image.size = work; image.format = GpuFormat::RGBA8Srgb;
            image.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst;
            if(device.CreateTexture(image, texture) != GpuResult::Ok) return fail("Compute display texture");
            size = work; state->image_size = work; ++state->image_builds; state->retained_image_bytes = buffer.size * 2;
        }
        double now = NowMs();
        if(previous_ms && !state->paused) state->phase += min(0.1, max(0.0, (now - previous_ms) / 1000.0));
        previous_ms = now;
        struct Params { uint32_t width, height, mode, dither; float phase, exposure; uint32_t seed, reserved; };
        Params params{(uint32_t)size.cx, (uint32_t)size.cy, (uint32_t)state->mode, (uint32_t)state->dither,
                      (float)state->phase, state->exposure, state->seed, 0};
        GpuCommandListId commands;
        if(device.BeginCommands(commands) != GpuResult::Ok) return fail("Compute BeginCommands");
        auto retire = [&] { if(device.EndCommands(commands) == GpuResult::Ok) device.Submit(commands); };
        GpuTextureWriteDesc region; region.size = size; region.row_pitch = (int64)size.cx * 4;
        if(device.SetComputePipeline(commands, pipeline) != GpuResult::Ok ||
           device.SetStorageBuffer(commands, 0, output) != GpuResult::Ok ||
           device.SetComputeParameters(commands, &params, sizeof(params)) != GpuResult::Ok ||
           device.Dispatch(commands, (size.cx + 7) / 8, (size.cy + 7) / 8) != GpuResult::Ok ||
           device.CopyBufferToTexture(commands, output, 0, texture, region) != GpuResult::Ok) {
            String detail = device.GetLastError(); retire(); error = "Compute/copy recording: " + detail; return false;
        }
        if(device.EndCommands(commands) != GpuResult::Ok || device.Submit(commands) != GpuResult::Ok)
            return fail("Compute/copy submission");
        state->compute_copy_cpu_ms = NowMs() - now;
        ++state->dispatches;
        if(!renderer->RenderTextureFrame(texture, frame)) { error = renderer->GetError(); return false; }
        device.GetComputeStats(state->compute_stats);
        device.GetSamplingStats(state->sampling_stats);
        if(state->measuring && state->frame_cpu_ms.GetCount() < 72000) state->frame_cpu_ms.Add(NowMs() - now);
        error.Clear(); return true;
    }
    void Close(GpuDevice& device) override {
        renderer.reset();
        if(pipeline.IsValid()) device.DestroyPipeline(pipeline);
        if(shader.IsValid()) device.DestroyShader(shader);
        if(output.IsValid()) device.DestroyBuffer(output);
        if(texture.IsValid()) device.DestroyTexture(texture);
        pipeline = GpuPipelineId(); shader = GpuShaderId(); output = GpuBufferId(); texture = GpuTextureId();
        size = Size(0, 0); previous_ms = 0; state->retained_image_bytes = 0;
    }
private:
    std::shared_ptr<ComputeSceneState> state;
    std::unique_ptr<UiRenderer2D> renderer;
    GpuPipelineId pipeline;
    GpuShaderId shader;
    GpuBufferId output;
    GpuTextureId texture;
    Size size = Size(0, 0);
    double previous_ms = 0;
};
}
ComputeSceneDialog::ComputeSceneDialog(bool validation, Size work_size)
    : state(std::make_shared<ComputeSceneState>())
{
    state->work_size = work_size;
    Title("Vulkan image and compute").Sizeable().Zoomable().SetRect(0, 0, 960, 680);
    SetMinSize(Size(DPI(760), DPI(540)));
    SetRequireGpu().SetValidation(validation).SetAsyncPresentation();
    title.SetText("Compute image laboratory");
    description.SetText("GPU-generated pixels, antialiased balls and a 16-bit ramp. Resize this window and compare dithering.");
    pause.SetText("Pause"); randomize.SetText("Randomize"); close.SetText("Close");
    dither.SetText("Dither"); dither.SetData(true);
    mode.Add("Animated balls", 0); mode.Add("16-bit ramps", 1); mode.Select(0);
    exposure_label.SetText("Exposure (stops)");
    exposure.SetRange(-4, 4).SetStep(0.1).SetValue(0);
    pause.WhenAction = [=] { state->paused = !state->paused; pause.SetText(state->paused ? "Resume" : "Pause"); };
    randomize.WhenAction = [=] { ++state->seed; };
    close.WhenAction = [=] { Close(); };
    dither.WhenAction = [=] { state->dither = (bool)dither.GetData(); };
    mode.WhenSelect = [=](int index) { state->mode = index; };
    exposure.WhenAction = [=] { state->exposure = (float)exposure.GetValue(); };
    image.SetValidation(validation).SetGpuContent(std::unique_ptr<GpuSurfaceContent>(new ComputeSceneContent(state))).SetContinuousRefresh(120);
    Add(title); Add(description); Add(image); Add(pause); Add(randomize); Add(close);
    Add(dither); Add(mode); Add(exposure_label); Add(exposure); Add(status);
    started = NowMs();
    SetTimeCallback(-100, [=] { Tick(); }, 994);
}
ComputeSceneDialog::~ComputeSceneDialog() { KillTimeCallback(994); }
void ComputeSceneDialog::Layout()
{
    GpuTopWindow::Layout();
    Size size = GetSize(); int margin = DPI(18), gap = DPI(10), h = DPI(34);
    title.SetRect(margin, margin, size.cx - 2 * margin, DPI(28));
    description.SetRect(margin, margin + DPI(30), size.cx - 2 * margin, DPI(44));
    int y = margin + DPI(80);
    pause.SetRect(margin, y, DPI(82), h); randomize.SetRect(margin + DPI(92), y, DPI(110), h);
    dither.SetRect(margin + DPI(214), y, DPI(90), h); mode.SetRect(margin + DPI(314), y, DPI(170), h);
    close.SetRect(size.cx - margin - DPI(82), y, DPI(82), h);
    y += h + gap;
    exposure_label.SetRect(margin, y, DPI(130), h); exposure.SetRect(margin + DPI(140), y, size.cx - 2 * margin - DPI(140), h);
    y += h + gap;
    image.SetRect(margin, y, max(1, size.cx - 2 * margin), max(1, size.cy - y - margin - DPI(48)));
    status.SetRect(margin, size.cy - margin - DPI(42), size.cx - 2 * margin, DPI(42));
}
void ComputeSceneDialog::StartQualification(String& report) { qualification_report = &report; started = NowMs(); }
void ComputeSceneDialog::StartBenchmark(int seconds, String& report)
{
    benchmark_report = &report; benchmark_seconds = seconds; started = NowMs();
    report = "benchmark_status=RUNNING\n"; SaveFile(GetExeDirFile("GpuUiGallery-compute.txt"), report);
}
void ComputeSceneDialog::BenchmarkTick(double now)
{
    if(now - started < 4000) return;
    const auto stats = image.GetGpuStats();
    if(!benchmark_epoch) {
        benchmark_epoch = now; benchmark_frames = stats.presented_frames;
        benchmark_due = now; memory_due = now; state->measuring = true;
    }
    callback_delays.Add(max(0.0, now - benchmark_due)); benchmark_due = now + 100;
    if(now >= memory_due) {
        uint64 bytes = PrivateBytes(); private_samples.Add(bytes);
        private_peak = max(private_peak, bytes); memory_due = now + 1000;
    }
    if(now - benchmark_epoch < benchmark_seconds * 1000) return;
    state->measuring = false;
    uint64 early = 0, late = 0;
    if(private_samples.GetCount() >= 8) for(int i = 0; i < 4; ++i) {
        early += private_samples[i] / 4; late += private_samples[private_samples.GetCount() - 4 + i] / 4;
    }
    double measured_fps = (stats.presented_frames - benchmark_frames) * 1000.0 / (now - benchmark_epoch);
    double p99 = Percentile(callback_delays, .99), worst = Percentile(callback_delays, 1);
    bool plateau = early && late <= early + 32 * 1024 * 1024;
    bool responsive = p99 < 100 && worst < 500 && measured_fps >= 55;
    bool resources = state->program_builds == 1 && state->image_builds == 1 &&
                     state->compute_stats.descriptor_pools_created == 1 && state->compute_stats.live_commands == 0 &&
                     state->sampling_stats.descriptor_pools_created == 1 && state->sampling_stats.live_commands == 0 &&
                     state->retained_image_bytes <= 64 * 1024 * 1024;
    *benchmark_report = "GpuUiGallery Windows/Vulkan compute and whole-UI window\n";
    *benchmark_report << "adapter=" << stats.adapter_name << "\n"
        << "target_fps=120 presented_fps=" << Format("%.2f", measured_fps)
        << " measured_frames=" << AsString(stats.presented_frames - benchmark_frames)
        << " elapsed_ms=" << Format("%.2f", now - benchmark_epoch) << "\n"
        << "compute_copy_draw_cpu_ms p50=" << Format("%.2f", Percentile(state->frame_cpu_ms, .5))
        << " p99=" << Format("%.2f", Percentile(state->frame_cpu_ms, .99))
        << " max=" << Format("%.2f", Percentile(state->frame_cpu_ms, 1))
        << " samples=" << state->frame_cpu_ms.GetCount() << "\n"
        << "ui_callback_delay_ms p99=" << Format("%.2f", p99) << " max=" << Format("%.2f", worst) << "\n"
        << "process_private_peak_bytes=" << AsString(private_peak)
        << " early_mean=" << AsString(early) << " late_mean=" << AsString(late) << "\n"
        << "compute_image_width=" << state->image_size.cx << " height=" << state->image_size.cy << "\n"
        << "image_payload_bytes=" << AsString(state->retained_image_bytes)
        << " shader_pipeline_builds=" << state->program_builds << " image_builds=" << state->image_builds << "\n"
        << "descriptor_pools_created=" << AsString(state->compute_stats.descriptor_pools_created)
        << " reused=" << AsString(state->compute_stats.descriptor_pools_reused) << "\n"
        << "sampled_descriptor_pools_created=" << AsString(state->sampling_stats.descriptor_pools_created)
        << " reused=" << AsString(state->sampling_stats.descriptor_pools_reused) << "\n"
        << "software_fallback_count=" << GetSoftwareFallbackCount()
        << " responsiveness=" << (responsive ? "PASS" : "FAIL")
        << " memory_plateau=" << (plateau ? "PASS" : "FAIL")
        << " resource_reuse=" << (resources ? "PASS" : "FAIL") << "\n"
        << "benchmark_status=" << (responsive && plateau && resources ? "PASS" : "FAIL") << "\n";
    SaveFile(GetExeDirFile("GpuUiGallery-compute.txt"), *benchmark_report);
    KillTimeCallback(994); Close();
}
void ComputeSceneDialog::Finish(bool pass, const String& reason)
{
    qualification_passed = pass;
    if(qualification_report) {
        *qualification_report << "compute_dialog=" << (pass ? "PASS " : "FAIL ") << reason << "\n"
            << "compute_presented_frames=" << AsString(image.GetGpuStats().presented_frames)
            << " dispatches=" << AsString(state->dispatches) << " shader_pipeline_builds=" << state->program_builds
            << " image_builds=" << state->image_builds << " payload_bytes=" << AsString(state->retained_image_bytes) << "\n";
    }
    if(benchmark_report) {
        *benchmark_report = "benchmark_status=FAIL\nerror=" + reason + "\n";
        SaveFile(GetExeDirFile("GpuUiGallery-compute.txt"), *benchmark_report);
    }
    KillTimeCallback(994); Close();
}
void ComputeSceneDialog::Tick()
{
    String error = !GetGpuError().IsEmpty() ? GetGpuError() : image.GetGpuError();
    if(!error.IsEmpty()) { Finish(false, error); return; }
    const auto stats = image.GetGpuStats(); const double now = NowMs();
    if(!epoch) { epoch = now; frames = stats.presented_frames; }
    if(now - epoch >= 1000) {
        fps = (stats.presented_frames - frames) * 1000.0 / (now - epoch);
        epoch = now; frames = stats.presented_frames;
    }
    status.SetText(Format("%.1f presented FPS | compute + copy CPU %.2f ms | %d image rebuilds",
                          fps, state->compute_copy_cpu_ms, state->image_builds) +
                   "\n" + AsString(state->retained_image_bytes) + " bytes image/buffer payload | no per-frame readback");
    if(benchmark_report) { BenchmarkTick(now); return; }
    if(!qualification_report) return;
    if(now - started > 30000) { Finish(false, "timeout"); return; }
    if(!IsGpuReady() || !image.IsGpuReady() || stats.presented_frames <= qualification_frame + 5) return;
    qualification_frame = stats.presented_frames;
    switch(qualification_step++) {
    case 0:
        dither.SetData(false); dither.WhenAction();
        mode.WhenSelect(1); exposure.SetValue(1.5); exposure.WhenAction(); randomize.WhenAction();
        if(state->dither || state->mode != 1 || state->exposure != 1.5 || state->seed != 2) { Finish(false, "control callbacks"); return; }
        pause.WhenAction(); paused_phase = state->phase; SetRect(0, 0, 1020, 720); return;
    case 1:
        if(!state->paused || state->phase != paused_phase) { Finish(false, "pause"); return; }
        pause.WhenAction(); dither.SetData(true); dither.WhenAction(); mode.WhenSelect(0); return;
    default:
        Finish(state->phase > paused_phase && state->dither && state->mode == 0 &&
               state->program_builds == 1 && state->image_builds >= 2 && GetSoftwareFallbackCount() == 0,
               "controls, pause/resume, resize, one compiled program, direct GPU image");
    }
}
}
