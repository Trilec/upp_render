#!/usr/bin/env bash
# Build and exercise the embedded viewport using the local U++ and Vulkan SDKs.
set -euo pipefail

if [[ $(uname -s) != Darwin ]]; then
    echo "Run this script on macOS." >&2
    exit 2
fi
root=$(cd "$(dirname "$0")/.." && pwd)
uppsrc=${UPP_SRC:-"$HOME/upp/uppsrc"}
umk=${UMK:-umk}
method=${UPP_METHOD:-CLANG}
ui=${UPP_UI:-"$root/../upp_Ui"}
animation=${UPP_ANIMATION:-"$root/../upp_animation"}
out="$root/build/macos"

command -v "$umk" >/dev/null || { echo "Set UMK to your installed umk executable." >&2; exit 2; }
for package in "$uppsrc/Core/Core.upp" "$ui/Ui/Ui.upp" "$animation/Animation/Animation.upp"; do
    [[ -f "$package" ]] || { echo "Missing package: $package (check UPP_SRC, UPP_UI and UPP_ANIMATION)." >&2; exit 2; }
done
: "${VULKAN_SDK:?Source the macOS Vulkan SDK setup-env.sh first}"
[[ -f "$VULKAN_SDK/include/vulkan/vulkan_metal.h" && -f "$VULKAN_SDK/lib/libvulkan.1.dylib" ]] || {
    echo "The SDK needs Vulkan headers and libvulkan.1.dylib under VULKAN_SDK." >&2
    exit 2
}
export CPATH="$VULKAN_SDK/include${CPATH:+:$CPATH}"
export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
nests="$root/render,$root/examples,$root/tests,$root/tools,$ui,$animation,$uppsrc"
mkdir -p "$out"
cd "$out"
build() {
    "$umk" "$nests" "$1" "$method" --out-dir "$out/cache" -r "+$2" "$out/$1"
}
run_gui() {
    local name=$1
    shift
    "$out/$name.app/Contents/MacOS/$name" "$@"
}

if [[ $# -gt 1 || ( $# -eq 1 && $1 != --validation ) ]]; then
    echo "Usage: bash tools/test-macos.sh [--validation]" >&2
    exit 2
fi
for package in RenderSceneTest Ui3DViewCameraTest RenderRhiTest; do
    build "$package" CONSOLE
    "$out/$package" | tee "$package.log"
done
build VulkanProbe CONSOLE
"$out/VulkanProbe" --create-device "$@" | tee VulkanProbe.log
build GpuCtrlPresentationTest GUI
if [[ $# -eq 0 ]]; then
    run_gui GpuCtrlPresentationTest --no-validation
else
    run_gui GpuCtrlPresentationTest
fi | tee GpuCtrlPresentationTest.log
build Ui3DViewDemo GUI
run_gui Ui3DViewDemo --smoke "$@" | tee Ui3DViewDemo-smoke.log
echo "For an interactive run in this SDK environment: \"$out/Ui3DViewDemo.app/Contents/MacOS/Ui3DViewDemo\"."
