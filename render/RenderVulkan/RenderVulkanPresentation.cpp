#include "RenderVulkanRhi.h"

#include <RenderRhi/RenderRhiBackend.h>
#include <memory>

namespace Upp {

namespace {

class VulkanPresentationContext : public GpuPresentationBackendContext {
public:
	VulkanSurfaceSessionGroup group;
};

class VulkanPresentationSession : public GpuPresentationBackendSession {
public:
	explicit VulkanPresentationSession(VulkanPresentationContext& context)
		: session(context.group)
	{
	}

	~VulkanPresentationSession() override
	{
		Close();
	}

	bool Open(bool request_validation, const GpuNativeWindowDesc& native_window,
	          String& out_error) override
	{
		Close();
		error.Clear();
		out_error.Clear();
		if(!session.Open(request_validation, native_window)) {
			error = session.GetError();
			out_error = error;
			return false;
		}

		device.reset(new VulkanGpuDevice(session));
		if(!device->IsReady()) {
			String failure = device->GetError();
			device.reset();
			session.Close();
			error = failure.IsEmpty() ? String("VulkanGpuDevice initialization failed") : failure;
			out_error = error;
			return false;
		}

		return true;
	}

	void Close() override
	{
		device.reset();
		session.Close();
		error.Clear();
	}

	bool IsReady() const override
	{
		return session.IsReady() && device && device->IsReady();
	}

	String GetError() const override
	{
		if(!error.IsEmpty())
			return error;
		if(device && !device->GetError().IsEmpty())
			return device->GetError();
		return session.GetError();
	}

	GpuDevice *GetDevice() override
	{
		return IsReady() ? device.get() : nullptr;
	}

private:
	VulkanSurfaceSession session;
	std::unique_ptr<VulkanGpuDevice> device;
	String error;
};

class VulkanPresentationBackend : public GpuPresentationBackend {
public:
	One<GpuPresentationBackendContext> CreateContext(String& error) override
	{
		error.Clear();
		return new VulkanPresentationContext;
	}

	One<GpuPresentationBackendSession> CreateSession(GpuPresentationBackendContext& context,
	                                                  String& error) override
	{
		error.Clear();
		return new VulkanPresentationSession(static_cast<VulkanPresentationContext&>(context));
	}
};

static VulkanPresentationBackend s_vulkan_backend;
static bool s_vulkan_backend_registered =
	RegisterGpuPresentationBackend(GpuBackendKind::Vulkan, s_vulkan_backend);

} // namespace

}
