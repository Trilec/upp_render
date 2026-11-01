# Windows frame pacing

Updated2026-10-22. Core and Animation remain read-only.

## Implemented correction

The old Gallery supplied Animation's global timer with Core's coarse msecs() time and observed approximately64distinct updates/s at a120FPS target. A precise native wake alone did not change the clock consumed by animation progress.

GpuTopWindow now offers WhenGpuFrame(double steady_ms) on the UI thread. Its opt-in240Hz native clock coalesces notifications, skips hidden/iconic roots and joins at shutdown. The callback is copied and guarded by Ptr because it may destroy its window; a focused native lifecycle test passes. Existing TimerProc(msecs()) calls remain in their original clock domain.

Gallery uses the installed Animation::State::Step(now) API with explicit steady time. It uses deadlines at the selected rate, skips missed updates and maintains pause/resume/reset time. It does not schedule the same state through the global Animation timer. This integration changes no dependency source or Core symbol.

Benchmark callbacks no longer force a repaint on every sample. Distinct progress updates, scene records and actual presentation are reported separately. Cadence passes only at >=90% of requested Animation FPS; repeated presentation of unchanged motion cannot satisfy it.

## Qualified baseline

See qualification/history-2026-11-01.zip, entry docs/VULKAN_UI_FINISH_QUALIFICATION.json for exact artifacts/jobs/reports.

|120FPS target|Distinct updates/s|Presented FPS|CPU acquire/replay/present median/p99|
|---|---:|---:|---:|
|96 particles+grid|120.00|120.30|5.04/8.31ms|
|512 particles+grid|120.02|122.22|6.26/10.65ms|
|512 particles+grid,300s|119.92|119.87|6.15/14.27ms|

Status/control changes can produce additional presentations. The300second soak's timer p99/max17.98/79.40ms passes unchanged50/100ms bounds. Private early/late means decreased747,520B; no fallback/GDI font requests occurred and final native ownership was ZERO.

This is measured responsiveness/throughput for the stated workload, not GPU execution time, input-to-photon latency or an every-frame8.33ms guarantee. Existing backend submissions still wait for completion. The separate historical4K compute callback outlier has not been requalified by these whole-UI results.

Text shaping/editing integration is now authorized and active; rerun affected load tests after that change.
