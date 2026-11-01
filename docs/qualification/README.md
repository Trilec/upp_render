# Windows/Vulkan qualification

[Current executable identity](windows-vulkan.json) records the stable
`bin/win64/UppVulkanUiDemo.exe` SHA-256, source pins and acceptance boundaries.
[Refresh evidence](refresh-2026-11-01.json) contains exact build/test jobs,
before/after picker checks and retained failed/interrupted runs.

The refreshed automated UI suite passed with validation enabled, zero CPU
vector/glyph rasters, zero GDI font requests and zero native ownership at exit.
The owner waived the repeated five-minute soak after closing/using the partial
run. Stable private-memory samples are observations, not a complete soak pass.
Physical acceptance remains8/10 (80%); see the [manual checklist](../WINDOWS_VULKAN_MANUAL_ACCEPTANCE.md).

## History

[history-2026-11-01.zip](history-2026-11-01.zip) preserves18 older plans, restart
notes, audits and qualification/machine reports, including the24 October Caro
pass. Archive size:199,148 bytes; uncompressed:1,264,893 bytes.
SHA-256: `946b7c14f82a2762d6a471c2cc9eda374a470851a64db01bd5c20e52b8650e38`.

CRC and all18 archived byte streams were checked against their source files
before removing/replacing those files. Extract into an empty scratch directory
to inspect the original `docs/` paths. These are historical records, not current
launch instructions. Source Git history remains available too.

| Archived file | Original bytes | SHA-256 |
| --- | ---: | --- |
| docs/ACTIVE_WORK.md | 13434 | `3a2ad0c6a72fc1c9f65c1cb89c2139ab3ecd8aeaea557b7f6b2ae9f04132ebb4` |
| docs/PROJECT_PLAN.md | 13253 | `e14b0a77317c6a95221f9a4319e5da7056df4f95e9854d9477229f278b55aa4f` |
| docs/VULKAN_UI_FINISHING.md | 9783 | `b5dca207fee65cfb97f6ec8c82acf33cd89ffb0692c1af26a83338b70bb51bb0` |
| docs/WINDOWS_VULKAN_RESTART.md | 7476 | `bee65e95612ac16b2ceaa793e99b953c65031b349891676dbbada8a6172bd991` |
| docs/DEMO_ROADMAP.md | 752 | `da6067a3ea7ccbf3edff4e57e5a8bb0d7d13b585e65fc81cc13f4e1bcef9ab76` |
| docs/VULKAN_UI_FINISH_QUALIFICATION.json | 69092 | `79e5682a1aa356caf5a8fb1c7e7860682ecb9dafb7999fdfa061087599093bcc` |
| docs/VULKAN_VECTOR_GLYPH_QUALIFICATION.json | 39938 | `70734d15dfdda6907a74531202c82c977799b6fe69e9c89f0cfd2e051ace13e8` |
| docs/VulkanReport_RTX_4070Ti.json | 247068 | `78bec5f70ed1d8292977f4e8377a986fd3562e170cb790dc13ab0580df56c036` |
| docs/WINDOWS_VULKAN_CARO_FINAL_SOAK_2026-10-24.txt | 4433 | `7547faa4500036503c4c24fea3b249487916de009cf049a1c557a803526b0b95` |
| docs/WINDOWS_VULKAN_CARO_QUALIFICATION.json | 339410 | `55d788652da35434aeac9e1a25c0af64e467f2c0f35e1efcced59688ead1b4c3` |
| docs/WINDOWS_VULKAN_FINAL_QUALIFICATION.json | 249629 | `8f5b5737e4adb092b04fe277ccfad01a4a6f93e9e2f1c35a41a4d05e004afe39` |
| docs/WINDOWS_VULKAN_LATENCY_INVESTIGATION.json | 149281 | `adc4d99dbd8e7545a554eb5e07c3e03a218639d76f8c89de422aa885356e6f98` |
| docs/WINDOWS_VULKAN_TEXT_QUALIFICATION.json | 78278 | `154f8f2389fc0059106c64d2e9004ae8ae518ed841826b42723a15a93188fb46` |
| docs/WINDOWS_VULKAN_V1.md | 9338 | `74794f2e3cb49f56bf82270318c76f48cb8c5ed637e38ae413e5a44b164c70d5` |
| docs/RC1_QUALIFICATION.md | 7309 | `cbfee08a140149460451aae299c5a545cb065c8e6af8718903b914fa34f09774` |
| docs/WGPU_RECOVERY.md | 2518 | `723742b367e1fa328e63e2a1ac9f2d9dd1fc878798c3ddecbbed1394cc533fa4` |
| docs/WGPU_NATIVE_PROOF.md | 6117 | `2a36be2eeb65c6ac556bdd89a10d90336d30a9cbc7020b5b40260529c0b18d36` |
| docs/UI2_DRAWING_AUDIT.md | 17784 | `dac48990fc4fa614ab73e9f11849cc785ce253142d271964978169f19c683853` |
