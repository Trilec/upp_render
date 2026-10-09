# wgpu implementation plan

Design authority: WGPU_ARCHITECTURE.md.
Preserved baseline and recovery: WGPU_RECOVERY.md.
Work on main; keep the existing Vulkan implementation intact until replacement
qualifies. New wgpu builds are opt-in during the proof phase.

## Checkpoint 0: recovery and design

- Verified complete-history Git bundle at the recorded baseline.
- Write architecture, implementation gates and recovery instructions.
- Update ACTIVE_WORK.md to distinguish accepted legacy evidence from new work.
- Verify Patch receipts/readback and Git diff hygiene. Documentation is not a GPU test.

## Gate 1: dependency and host feasibility

Candidate upstream native tag: v29.0.1.1.
Observed tag ref: 6aed50955d934ac36049ba8d002034841633ae02.
Source: https://github.com/gfx-rs/wgpu-native.git
Tag discovery job: exec-A3638BD33755AF58EFF49A4F779ECEBA.

Source is cloned and its identities are recorded in WGPU_DEPENDENCY.md.
Pin commit, submodules, Cargo.lock, toolchain and licenses. Keep downloaded/build dependency trees under ignored build/deps.
Tracked integration contains a small manifest and reproducible instructions,
not an accidental nested Git checkout or entire unreviewed dependency tree.
Do not assume latest Rust wgpu and wgpu-native have the same release/ABI.

Use matching webgpu.h, wgpu.h and library artifacts from one native release.
Try a documented official Windows x64 binary first if compatible with U++ clang
and its runtime; otherwise use a reproducible native source build. Record static
versus DLL choice, import library compatibility, distribution files, enabled
backend features and SHA-256 hashes. Do not prescribe static linking as lighter
without measuring final shipped size. Curl and tar are now approved and the matching official GNU DLL is acquired.
The isolated native probe passes Release and non-BLITZ Debug; see
WGPU_NATIVE_PROOF.md. Cargo source builds and static/import-library compatibility
remain unqualified. Do not route an unapproved tool through Git or a helper.

Build a minimal U++ console link/query programme, then a Win32 surface triangle.
Require actual adapter/backend/limits, error/device-loss callbacks, resize and
clean close. Pin working ABI before broader implementation.

Early in project scheduling, establish an
Apple build host and audit U++ UIKit availability. Require a native iPad surface
and lifecycle proof plus device/simulator linking for critical imaging libraries.
If host work is substantial, record its estimate and alternatives before
claiming this migration solves iPad. Apple hardware/toolchains are a gate,
not a Windows-test substitute.

Windows dependency/host subgates now pass: matching C ABI, hardware Vulkan device,
offscreen pixels, WGSL triangle presentation, resize/minimize/restore and three
close/reopen cycles per build. Fault injection, clean-machine DLL deployment and
Apple/iPad feasibility remain open. A Windows proof does not close the entire
cross-platform Gate 1.

## Gate 2: minimal semantic replay

Create RenderWgpu with direct private wgpu ownership; no old RHI adapter.
Reuse backend-independent canvas/list contracts after dependency review.
RenderCtrlBridge is now a separate package (CtrlCore + RenderCanvas); legacy
GpuRender includes it through a compatibility header. Keep one implementation
of control recording. The recorder still uses Win32 SystemDraw and is not a
portable-text or fully non-GDI solution.
Implement WGSL rectangle, image and alpha-mask pipelines, clipping, CPU-reference
vector coverage, formats and bounded caching. Build a small opt-in U++ root demo.
Run meaningful pixel comparisons for premultiplication, crop/scale/mask/text,
sRGB and painter order. Existing legacy tests are behaviour references; do not
mechanically port tests that only mirror old implementation classes.
Require Debug/Release and non-BLITZ link proof.

## Gate 3: real controls and lifecycle

Integrate representative Ui controls, PropertyEditor, Designer-generated layout,
theme changes, DPI, menus/tooltips/owned popups and embedded surfaces.
Test 1/2/10 surfaces, shared uploads, one-window close with siblings surviving,
resize/minimize/restore/reopen, startup/retry/device loss and explicit required-GPU
failure. Qualify populated/input states, not only empty default control painting.
Add optional worker replay only if profiling warrants it.
Measure p99 input delay, memory plateau, upload reuse and final resource release
against baseline on the same machine. Keep software fallback outcomes visible.

## Gate 4: CineView and imaging

Implement bounded staging/texture slots and completion-owned frame pins.
Generation/serial tokens travel through submit/outcome; EveryFrame backpressure
and interactive latest-frame policy are distinct. No steady-state queue-idle wait.
Retain UINT8/UINT16/HALF/FLOAT precision and native row-pitch semantics.
First built-in exposure/colour pass, then pinned OCIO shader/LUT bindings with
CPU comparisons; unchanged source plus adjustment means zero source uploads.
Add bounded async readback and a real compute operation with CPU-reference checks.
Measure cached presentation separately from decode-through playback at 1080p/4K/8K.
No claim that renderer changes fix CPU decode limits or all OpenFX plug-in ports.

## Gate 5: portable delivery and retirement

Run macOS/Metal and native iPad gates on actual Apple targets; qualify portable
text, input/lifecycle and dependencies. Build browser separately with compatible
WASM WebGPU bindings and async canvas hosting; native artifacts are not reusable
browser binaries. Browser iPad success is not native iPad qualification.

After behaviour and measured budget gates pass, make wgpu the default.
Remove unused custom GPU-provider/RHI code and Vulkan SDK requirements from that
default build. Preserve tags/bundle/reference manifests instead of permanent
dual implementations. Update examples and application docs to final APIs.

## Evidence needed before estimating remaining work

Record dependency link/surface spike time, Ui draw gaps, Apple host findings and
baseline-versus-wgpu measurements. Only then give a scope/time estimate.
Current architecture review supports a clean experiment; it does not establish
that migration takes days, is faster, or has lower footprint.

Each gate records source/dependency identity, build command, job ID, binary hash,
PASS/FAIL/SKIPPED, platform and unresolved limitations. No remote visual review,
GPU measurement or iPad acceptance may be inferred from a Windows exit code.
