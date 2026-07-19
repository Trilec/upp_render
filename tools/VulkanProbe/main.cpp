#include <RenderVulkan/RenderVulkan.h>

using namespace Upp;

static bool HasArg(const Vector<String>& args, const char *name)
{
	for(const String& arg : args)
		if(arg == name)
			return true;
	return false;
}

CONSOLE_APP_MAIN
{
	Vector<String> args;
	for(int i = 1; i < __argc; ++i)
		args.Add(__argv[i]);

	bool request_validation = HasArg(args, "--validation");
	bool create_device = HasArg(args, "--create-device");
	if(create_device) {
		VulkanBootstrap probe;
		VulkanBootstrapReport report = probe.Run(request_validation, true);
		Cout() << probe.Dump(report) << EOL;
		if(report.status != VulkanProbeStatus::Ok)
			SetExitCode((int)report.status + 1);
		return;
	}

	VulkanPreflight probe;
	VulkanPreflightReport report = probe.Run(request_validation);
	Cout() << probe.Dump(report) << EOL;

	if(report.status != VulkanProbeStatus::Ok)
		SetExitCode((int)report.status + 1);
}
