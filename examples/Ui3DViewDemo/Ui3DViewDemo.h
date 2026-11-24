// Copyright (c) 2026 upp_render contributors. Apache-2.0; see repository LICENSE.
#ifndef _Ui3DViewDemo_Ui3DViewDemo_h_
#define _Ui3DViewDemo_Ui3DViewDemo_h_

#include <Ui3DViewUi/Ui3DViewport.h>
#include <Ui/Ui.h>
#include "ViewerCameraModel.h"
#include "ViewerLists.h"
#include <array>

namespace Upp {

// Application shell. The four view controls retain their identities across layout
// changes. Scene/selection/visibility, bookmarks and undo belong to this host.
class Ui3DViewDemo : public TopWindow {
public:
	Ui3DViewDemo();
	~Ui3DViewDemo() override;
	void Layout() override;
	bool Key(dword key, int count) override;
	void SetLight(bool on);
	void SetQuad(bool on);
	void SetInspectorVisible(bool on);
	void ActivatePane(int pane);
	void ResetDemo();
	void SetValidation();
	bool CheckReady(String& error) const;
#ifdef GUI_WIN
	bool Capture(const String& file, String& error);
	bool QualifyWorkspace(String& report);
	bool QualifyOverlays(String& report, const String& evidence_directory);
#endif
	Ui3DViewport& Pane(int i) { return panes.at(i); }
	const Ui3DViewport& Pane(int i) const { return panes.at(i); }
	int ActivePane() const { return active; }
	bool IsQuad() const { return quad; }
	bool IsLight() const { return light; }

private:
	// Models precede views, and therefore outlive all controls holding references.
	ViewerCameraModel cameras;
	Scene3DSceneDesc scene_description;
	std::shared_ptr<const Scene3DSnapshot> scene;
	std::vector<String> selection;
	std::array<Scene3DCamera, 4> defaults;
	std::array<Ui3DViewport, 4> panes;
	UiQuadSplitter views;
	UiPanel shell, header, toolbar, inspector, inspector_header, selection_panel, status_panel;
	UiBoxLayout root { UiDirection::V }, header_row { UiDirection::H };
	UiBoxLayout title_column { UiDirection::V }, brand_row { UiDirection::H }, tools { UiDirection::H };
	UiBoxLayout body { UiDirection::H }, status_row { UiDirection::H };
	UiScrollPanel inspector_scroll;
	UiBoxLayout inspector_column { UiDirection::V };
	UiBoxLayout inspector_root { UiDirection::V }, inspector_title_row { UiDirection::H };
	UiBoxLayout view_heading { UiDirection::H }, lens_row { UiDirection::H }, camera_heading { UiDirection::H };
	UiBoxLayout scene_heading { UiDirection::H }, selection_column { UiDirection::V };
	UiBoxLayout::ItemRef inspector_item, subtitle_item, frame_selection_item, undo_item, show_all_item;
	UiLabel logo, title, subtitle, badge, sample_scene, inspector_title, inspector_badge;
	UiLabel heading_view, view_name, view_detail, camera_binding, selection_hint;
	UiLabel lens_label, heading_cameras, heading_scene, selection_label, heading_navigation;
	UiLabel nav_text, status_text, status_detail;
	UiToolButton btn_theme, btn_help, btn_inspector;
	UiToolButton btn_select, btn_orbit, btn_pan, btn_single, btn_quad;
	UiToolButton btn_frame_all, btn_frame_selection, btn_save;
	UiToolButton btn_reset, btn_camera_undo, btn_show_all;
	UiSliderEdit lens;
	ViewerActionList camera_list { ViewerActionList::Camera }, object_list { ViewerActionList::Scene };
	UiMenu camera_menu;
	int active = 3;
	bool light = false, quad = false, inspector_visible = true, syncing = false;
	Ui3DNavigationTool navigation_tool = Ui3DNavigationTool::Select;
	String selected_camera_id, overview_camera_id, low_camera_id, last_event = "Ready";
	int lens_preview_pane = -1;
	std::array<uint64, 4> navigation_gesture {}, navigation_binding {}, last_navigation_gesture {};

	void BuildLayout();
	void ConnectEvents();
	void BuildScene();
	bool PublishScene(String& error);
	void ApplyTheme();
	void SyncChrome(bool rebuild_lists = true);
	void SyncCameras();
	void SyncSelection();
	void SetTool(Ui3DNavigationTool tool);
	void CameraChanged(int pane);
	void Navigate(int pane, const Ui3DNavigationEvent& event);
	void Select(int pane, const Ui3DSelectionRequest& request);
	void CancelGestures();
	void Frame(bool selected);
	void ResetView();
	void SetProjection(bool orthographic);
	void SetDirection(Scene3DViewDirection direction);
	void ChangeLens(bool commit);
	void SetStatus(const String& text);
	void SaveCamera();
	void RenameCamera();
	void LoadCamera(const String& id, int pane);
	void UpdateCamera();
	void RemoveCamera();
	void UndoCamera();
	void PopulateCameraMenu(UiMenu& menu, int pane);
	void OpenCameraActions(const String& id);
	bool EditName(const String& heading, String& name);
	void ToggleVisibility();
	void ToggleVisibility(const String& id);
	void ShowAll();
	void ShowHelp();
	bool ViewerHasFocus() const;
};

}
#endif
