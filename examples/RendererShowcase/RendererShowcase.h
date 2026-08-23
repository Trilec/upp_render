#pragma once

#include <Ui/Ui.h>
#include <Utilities/PropertyEditor/PropertyEditor.h>
#include <GpuRender/GpuRender.h>
#include <RenderSoftware/RenderSoftware.h>
#include <RendererShowcaseScene/RendererShowcaseScene.h>

namespace Upp {

class RendererSoftwarePreview : public Ctrl {
public:
	Function<bool(Size, UiDisplayList&, Rgba8&, String&)> WhenBuildFrame;
	void Paint(Draw& w) override;
};

class RendererShowcase : public TopWindow {
public:
	typedef RendererShowcase CLASSNAME;
	RendererShowcase();
	void Layout() override;
private:
	void BuildHeader();
	void BuildInspector();
	void Wire();
	void ApplyProjection();
	void SetPreviewMode(const String& mode);
	void ResetProperties();
	bool BuildScene(Size size, UiDisplayList& list, Rgba8& background, String& error) const;
	RendererShowcaseSettings GetSettings() const;
	Value PropertyValue(const String& id) const;
	UiTitleCard header;
	UiBoxLayout header_actions { UiDirection::H };
	UiLabel status;
	UiButton btn_gpu;
	UiButton btn_software;
	UiButton btn_reset;
	UiButton btn_exit;
	UiPanel preview_panel;
	UiStack preview_stack;
	GpuCtrl gpu_preview;
	RendererSoftwarePreview software_preview;
	UiLabel preview_caption;
	UiPanel inspector_panel;
	UiLabel inspector_title;
	UiLabel inspector_subtitle;
	PropertyEditor inspector;
	PropertyEditorModel model;
	Image demo_image;
};

}
