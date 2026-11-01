#ifndef _GpuUiGallery_GpuRoles_h_
#define _GpuUiGallery_GpuRoles_h_

#include <Ui/Ui.h>

namespace Upp {
// The Cinespace Roles & states composition, hosted by the Vulkan control tree.
// Only the shared typed theme adapter is reused; no other demo is a dependency.
class GpuRolesPage : public UiPanel {
public:
    GpuRolesPage();
    ~GpuRolesPage() override;
    void Layout() override;
    bool Qualify(String& report);
private:
    struct Impl;
    One<Impl> impl_;
};
}
#endif
