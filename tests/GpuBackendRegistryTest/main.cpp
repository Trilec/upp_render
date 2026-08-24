#include <RenderRhi/RenderRhiBackend.h>

using namespace Upp;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ok &= Check(IsGpuPresentationBackendRegistered(GpuBackendKind::Vulkan),
	            "compiled Vulkan presentation provider should register itself");
	ok &= Check(FindGpuPresentationBackend(GpuBackendKind::Vulkan) != nullptr,
	            "registered Vulkan provider should be discoverable through the neutral registry");
	ok &= Check(!IsGpuPresentationBackendRegistered(GpuBackendKind::Unknown),
	            "Unknown must never be a registered presentation backend");

	if(ok) {
		Cout() << "GpuBackendRegistryTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
