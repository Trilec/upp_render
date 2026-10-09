# wgpu-native dependency checkpoint

Status: pinned official Windows x64 GNU DLL acquired and exercised through the C ABI.
Release and non-BLITZ Debug native device/surface proofs PASS on 2026-10-10.
This is a Windows dependency/host proof, not the Ui renderer migration.

## Observed source

- Upstream: https://github.com/gfx-rs/wgpu-native.git
- Tag: v29.0.1.1
- Commit: 6aed50955d934ac36049ba8d002034841633ae02
- Local source: E:/apps/github/upp_render/build/deps/wgpu-native-v29.0.1.1
- Clone job: exec-D45BEE250AB13B9058EC072D4E050F57 (exit 0).
- webgpu-headers submodule: 673658bc2bd70ec39fc55ebe6bb0173cf6d0a603
- Upstream example GLFW submodule: b35641f4a3c62aa86a0b3c983d163bc0fe36026d
- GLFW is upstream example material; it is not selected for the U++ renderer.
- Upstream toolchain file: Rust 1.93; package minimum: Rust 1.87.
- wgpu-core/types/hal declared version: 29.0.1; build uses Cargo.lock.
- Upstream licensing: MIT OR Apache-2.0; keep license files in any distribution.

Reproduction (approved Git, Render root):
```text
git clone --depth 1 --branch v29.0.1.1 --recurse-submodules --shallow-submodules https://github.com/gfx-rs/wgpu-native.git build/deps/wgpu-native-v29.0.1.1
```
Use an absent destination; inspect an existing checkout instead of recloning into
it. Confirm tag/commit/submodule identities before consuming files.

## Local raw-file identities

These hashes are of this Windows checkout's CRLF bytes, not upstream LF blobs
or release-archive hashes.

| File relative to downloaded source | SHA-256 |
| --- | --- |
| Cargo.toml | f39132a9fb68d6742318232ac6a0c98f7cdf86a32002fc7cd85cc2aa23241af0 |
| Cargo.lock | c1c7fc15e22ae40be1dbb4be2fd2a0dfb46adeb0df1830c91f9183d23b78be12 |
| rust-toolchain.toml | 8c8f4db25a68a0f05fb151c4935dd345109d86479582ab349b7b1f01e9e58bfb |
| ffi/webgpu-headers/webgpu.h | 2516cf5a7bec4385bf76ecc550d45015c1e3df77962211f3cef3f57507b2f2c8 |
| ffi/wgpu.h | 873faa1c1b63d48e4d866000fc51a163e85d0f7c6c01425c687d3f12d073ff74 |

## Build choice to qualify

Current U++ build method uses clang with x86_64-w64-mingw32 paths.
Upstream release workflow includes x86_64-pc-windows-gnu as well as MSVC.
Prefer the matching GNU release for the first link proof; inspect actual DLL,
import/static library names, CRT dependencies and matching headers before use.
No downloaded executable/library is trusted merely because its filename matches.

For lean production evaluate a locked source build with WGSL and Vulkan features
only on Windows, WGSL and Metal on Apple. Upstream defaults include additional
shader languages and backends. Cargo.toml also unconditionally enables hal DX12
and renderdoc features on Windows: --no-default-features alone is not proof those
transitive components disappear. Measure feature graph and final artifacts before
deciding whether a narrowly maintained dependency patch is justified.

The source build uses bindgen/libclang. Apple build.rs explicitly handles native
iOS and simulator SDKs via xcrun; this supports the GPU dependency build direction,
but does not supply U++ UIKit hosting or qualify any application.

## Qualified Windows artifact

Official asset:
https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-windows-x86_64-gnu-release.zip

| Artifact | Size (bytes) | SHA-256 |
| --- | ---: | --- |
| Official ZIP | 15532029 | d471e3614733c1d4ddd61bfd19868356477d0d37bf531bf8c6cb64a7f579bd2a |
| lib/wgpu_native.dll | 13919443 | 61da1bec4f888a20253d42b535f2952c51d8e9fd24efbc687318c2d20d139262 |

Downloaded with owner-approved curl; the ZIP matches GitHub's published digest.
The inspected archive contains include/webgpu headers, static/import libraries,
the DLL and release metadata. Extracted into:
build/deps/wgpu-windows-x86_64-gnu-release-v29.0.1.1
Its two header hashes exactly match the pinned source hashes above.
The probe uses those release headers and an explicit absolute DLL path, checks
wgpuGetVersion == 0x1d000101 before passing structs, and dynamically resolves
only the entry points it uses. There is no production import/static-library
decision yet. This proof establishes dynamic C ABI compatibility with U++ clang;
it does not establish static/import-library compatibility or a lean feature graph.

Native tests and reproduction: WGPU_NATIVE_PROOF.md.
Artifact bytes are not measured process memory or deployed application size.

## Current execution tools

Render now exposes Git, UMK, curl, Windows tar and project outputs.
No Cargo source build has run. Use the approved tools directly, preserve the
official ZIP/digest and list it before extracting into a fresh destination.
The native probe requires the DLL and matching headers; curl/tar are development
acquisition tools, not application runtime dependencies. Distribution must retain
upstream licenses and audit transitive notices. Source licenses are LICENSE.MIT
and LICENSE.APACHE in the pinned checkout; the binary ZIP does not include them.
Clean-machine runtime qualification for this DLL, narrowed production features,
compute, browser and Apple/iPad acceptance remain pending.
