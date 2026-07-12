#include <RenderVulkan/RenderVulkan.h>

using namespace Upp;

static bool Check(bool cond, const char *msg)
{
	if(!cond)
		Cout() << "FAIL: " << msg << EOL;
	return cond;
}

static bool TestPreflight()
{
	VulkanPreflight probe;
	VulkanPreflightReport report = probe.Run(false);
	return Check(report.loader_available, "loader should be available") &&
	       Check(report.instance_created, "instance should be created") &&
	       Check(!report.devices.IsEmpty(), "physical devices should be enumerated") &&
	       Check(report.suitable_device_count > 0, "at least one suitable device expected") &&
	       Check(report.clean_shutdown, "preflight should cleanly shut down") &&
	       Check(report.status == VulkanProbeStatus::Ok, "preflight should pass");
}

static bool TestBootstrap(bool validation)
{
	VulkanBootstrap probe;
	VulkanBootstrapReport report = probe.Run(validation, true);
	if(!Check(report.status == VulkanProbeStatus::Ok, "bootstrap should pass")) return false;
	if(!Check(report.logical_device_created, "logical device should be created")) return false;
	if(!Check(report.graphics_queue_acquired, "graphics queue should be acquired")) return false;
	if(!Check(report.selected_device.suitable, "selected device should be suitable")) return false;
	if(!Check(report.selected_device.dynamic_rendering, "dynamic rendering should be enabled")) return false;
	if(!Check(report.selected_device.synchronization2, "synchronization2 should be enabled")) return false;
	if(!Check(report.selected_device.selected_queue_family_index >= 0, "queue family should be selected")) return false;
	if(validation) {
		if(!Check(report.validation_warning_count == 0, "validation warnings should be zero")) return false;
		if(!Check(report.validation_error_count == 0, "validation errors should be zero")) return false;
	}
	return Check(report.clean_shutdown, "bootstrap should cleanly shut down");
}

static bool TestRepeat()
{
	String device_name;
	int queue_family = -1;
	for(int i = 0; i < 10; ++i) {
		VulkanBootstrap probe;
		VulkanBootstrapReport report = probe.Run(true, true);
		if(!Check(report.status == VulkanProbeStatus::Ok, "repeat cycle should pass")) return false;
		if(!Check(report.validation_error_count == 0, "repeat cycle should have no validation errors")) return false;
		if(i == 0) {
			device_name = report.selected_device.name;
			queue_family = report.selected_device.selected_queue_family_index;
		}
		else {
			if(!Check(report.selected_device.name == device_name, "selected device should remain stable")) return false;
			if(!Check(report.selected_device.selected_queue_family_index == queue_family, "queue family should remain stable")) return false;
		}
		if(!Check(report.clean_shutdown, "repeat cycle should cleanly shut down")) return false;
	}
	return true;
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ok &= TestPreflight();
	ok &= TestBootstrap(false);
	ok &= TestBootstrap(true);
	ok &= TestRepeat();
	if(ok) {
		Cout() << "RenderVulkanTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
