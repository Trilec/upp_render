#ifndef _Ui3DViewDemo_Ui3DViewDemo_h_
#define _Ui3DViewDemo_Ui3DViewDemo_h_

#include <Ui3DView/Ui3DView.h>
#include <Ui/Ui.h>

namespace Upp {

// The demo owns its scene, selection policy and shell; Ui3DView owns only the view.
class Ui3DViewDemo : public TopWindow {
public:
	Ui3DViewDemo();
	~Ui3DViewDemo() override;
	void Layout() override;
	void Paint(Draw& draw) override;
	void SetLight(bool on);
	void SetOrthographic(bool on);
	void SetWireframe(bool on);
	bool Capture(const String& file);
	bool CheckReady(String& error) const;
	Ui3DView view;

private:
	UiTitleCard title;
	UiButton btn_projection, btn_fit, btn_grid, btn_axes, btn_wire, btn_theme, btn_help, btn_exit;
	TimeCallback status_refresh;
	bool light = false;
	String selection_name = "No object selected";
	String last_event = "Ready";
	void BuildScene();
	void ApplyOptions();
	void RefreshStatus();
};

}
#endif
