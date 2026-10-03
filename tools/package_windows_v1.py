"""Package a local Windows/Vulkan candidate; does not tag, publish or delete files."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

def digest(data):
    return hashlib.sha256(data).hexdigest()

def git(root, *args):
    return subprocess.check_output(["git", "-c", "safe.directory=" + root.as_posix(),
                                   "-C", str(root), *args])

def source_files(root):
    paths = git(root, "ls-files", "-z", "--cached", "--others", "--exclude-standard").split(b"\0")
    result = {}
    for raw in paths:
        if not raw:
            continue
        name = raw.decode("utf-8")
        if name.startswith((".git/", ".klick-patch/", "build/")):
            raise RuntimeError("Unexpected excluded path: " + name)
        path = root / name
        if not path.is_file() or path.is_symlink():
            raise RuntimeError("Source file missing or symlink: " + name)
        result[name] = path.read_bytes()
    return result

def write_zip(path, files):
    with zipfile.ZipFile(path, "x", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("ZIP integrity failure")
        for name, data in files.items():
            if digest(archive.read(name)) != digest(data):
                raise RuntimeError("ZIP content mismatch: " + name)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ui", type=Path, required=True)
    parser.add_argument("--animation", type=Path, required=True)
    parser.add_argument("--uppsrc", type=Path, required=True)
    parser.add_argument("--clang", type=Path, required=True, help="Bundled clang root for runtime notices")
    parser.add_argument("--name", required=True, help="New output folder name within build")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if Path(args.name).name != args.name or args.name in (".", ".."):
        parser.error("--name must be a single folder name")
    output = root / "build" / args.name
    output.mkdir(exist_ok=False)
    source = source_files(root)
    for dependency in (args.ui, args.animation):
        if git(dependency, "status", "--porcelain").strip():
            raise RuntimeError("Dependency must be clean for a pinned candidate: " + str(dependency))
    runtime = {}
    for name in ("GpuUiGallery", "GpuSurfaceDemoRelease", "RendererShowcase"):
        runtime[name + ".exe"] = (root / "build" / (name + ".exe")).read_bytes()
    notices = {
        "LICENSE-renderer.txt": root / "LICENSE",
        "LICENSE-Ui.txt": args.ui / "LICENSE",
        "LICENSE-Animation.txt": args.animation / "LICENSE",
    }
    for package in ("Core", "Draw", "Painter", "CtrlCore", "CtrlLib", "RichText",
                    "plugin/bmp", "plugin/png", "plugin/z"):
        notices["Upp-" + package.replace("/", "-") + "-Copying.txt"] = args.uppsrc / package / "Copying"
    for relative in ("Core/lib/LICENSE.lz4", "Core/lib/LICENSE.xxhash",
                     "plugin/png/lib/LICENSE", "plugin/z/lib/LICENSE"):
        notices["Upp-" + relative.replace("/", "-") + ".txt"] = args.uppsrc / relative
    for name in ("COPYING", "COPYING.MinGW-w64.txt", "COPYING.MinGW-w64-runtime.txt",
                 "COPYING.winpthreads.txt", "COPYING.winstorecompat.txt"):
        notices["MinGW-" + name] = args.clang / "x86_64-w64-mingw32/share/mingw32" / name
    notices["LLVM-LICENSE.TXT"] = args.clang / "LICENSE.TXT"
    for name, path in notices.items():
        runtime["licenses/" + name] = path.read_bytes()
    for name in ("WINDOWS_VULKAN_V1.md", "GPU_CTRL_USAGE.md", "RELEASE_CANDIDATE.md", "RC1_QUALIFICATION.md"):
        runtime["docs/" + name] = source["docs/" + name]
    runtime["README.txt"] = (
        "Windows/Vulkan v1 local candidate\n"
        "Run GpuUiGallery.exe for the whole-Ui application, GpuSurfaceDemoRelease.exe "
        "for embedded surfaces, or RendererShowcase.exe for rendering comparisons.\n"
        "Requires Windows x64 with POPCNT and a compatible Vulkan 1.3 GPU/driver "
        "providing vulkan-1.dll. Vulkan SDK is only needed for building; validation "
        "layers are optional and required for --validation.\n"
        "Gallery: --benchmark / --benchmark-load / --benchmark-soak [--validation] write reports beside "
        "the executable and close automatically. Interactive use stays open.\n"
        "GpuSurfaceDemoRelease.exe --self-test runs deterministic surface checks.\n"
        "GPU client-area composition retains Windows/font/GDI platform dependencies.\n"
        "Source archive contains the renderer working tree; external Ui, Animation "
        "and U++ sources/toolchain are not bundled. See manifest dependency pins.\n"
        "No public release/tag or separate clean-machine installation is implied.\n"
    ).encode()
    for name in ("GpuUiGallery-normal.txt", "GpuUiGallery-load.txt", "GpuUiGallery-soak.txt",
                 "GpuSiblingLoad.txt", "v1-validation.json", "rc1-qualification-final.json"):
        path = root / "build" / name
        if not path.is_file():
            raise RuntimeError("Required evidence missing: " + name)
        data = path.read_bytes()
        if name.startswith("GpuUiGallery-") and (
                b"responsiveness=PASS" not in data or b"final_native_ownership=ZERO" not in data):
            raise RuntimeError("Incomplete or failed Gallery evidence: " + name)
        if name == "GpuUiGallery-soak.txt" and b"memory_plateau=PASS" not in data:
            raise RuntimeError("Incomplete or failed memory plateau evidence")
        if name == "GpuSiblingLoad.txt" and (
                b"sibling_responsiveness=PASS" not in data or b"final_native_ownership=ZERO" not in data):
            raise RuntimeError("Incomplete or failed sibling evidence")
        runtime["evidence/" + name] = data
    manifest = {
        "scope": "Windows/Vulkan v1 local candidate; renderer working-tree snapshot",
        "renderer_dirty": bool(git(root, "status", "--porcelain").strip()),
        "renderer_base": git(root, "rev-parse", "HEAD").decode().strip(),
        "ui": git(args.ui, "rev-parse", "HEAD").decode().strip(),
        "animation": git(args.animation, "rev-parse", "HEAD").decode().strip(),
        "toolchain": "U++ 18468; clang 21.1.1; C++17; Vulkan SDK 1.4.350.0; Windows x64",
        "source_files": {name: digest(data) for name, data in sorted(source.items())},
        "runtime_files": {name: digest(data) for name, data in sorted(runtime.items())},
        "shader_contract": "Checked-in SPIR-V arrays; no runtime shader compiler. Original generation provenance unavailable.",
        "clean_machine_install": "Not performed",
    }
    encoded = json.dumps(manifest, indent=2).encode() + b"\n"
    source["candidate-manifest.json"] = encoded
    runtime["candidate-manifest.json"] = encoded
    write_zip(output / "upp-render-windows-vulkan-v1-source.zip", source)
    write_zip(output / "upp-render-windows-vulkan-v1-runtime.zip", runtime)
    (output / "candidate-manifest.json").write_bytes(encoded)
    # Keep the user-requested branch archive with the local handoff, outside product ZIPs.
    bundle = root / "build/branches-before-cleanup-2026-10-03.bundle"
    if digest(bundle.read_bytes()) != "c00c2ae7060ce55f7d09ca76f5a5d7c8828aca25c93b4bb1918021c351934cec":
        raise RuntimeError("Branch archive identity mismatch")
    (output / bundle.name).write_bytes(bundle.read_bytes())
    hashes = {path.name: digest(path.read_bytes()) for path in sorted(output.iterdir())}
    (output / "SHA256SUMS.txt").write_text(
        "".join(value + "  " + name + "\n" for name, value in hashes.items()), encoding="utf-8")
    print(json.dumps({"output": str(output), "source_files": len(source),
                      "runtime_files": len(runtime), "sha256": hashes}, indent=2))

if __name__ == "__main__":
    main()
