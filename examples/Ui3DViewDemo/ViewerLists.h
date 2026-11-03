// Copyright (c) 2026 upp_render contributors. Apache-2.0; see repository LICENSE.
#ifndef _Ui3DViewDemo_ViewerLists_h_
#define _Ui3DViewDemo_ViewerLists_h_

#include <Ui/UiList.h>
#include <memory>

namespace Upp {

// Small native vector glyphs for the standard UiModelItem::icon field. Callers
// preserve their color; these contain no application controls or scene data.
Image ViewerCameraGlyph(Color ink, int size = DPI(16));
Image ViewerCubeGlyph(Color ink, int size = DPI(16));
// 0 Select pointer, 1 Orbit, 2 Pan arrows, 3 Frame corners. Other IDs return an empty image.
Image ViewerToolGlyph(int tool, Color ink, int size = DPI(16));

// App-side presentation adapter: UiList still owns its model, selection,
// scrolling and recycled row renderers. All rows use UiItemRenderBasic for
// their content. Only the trailing, independently clickable action is added.
//
// UiModelItem::data is a nonempty stable String ID. Scene rows use checked for
// visibility, with has_check=false and List style.show_checks=true. The native
// List projection clears checked when show_checks is false; our row renderer
// suppresses the check itself while preserving its boolean data. Hidden
// objects remain enabled so their eye can show them again. The host supplies
// the title/description and optional colored icon through the standard model.
// All methods and events are GUI-thread-only. Setters do not emit events.
class ViewerActionList : public UiList {
public:
	typedef ViewerActionList CLASSNAME;
	enum ActionMode { Camera, Scene };

	explicit ViewerActionList(ActionMode mode = Camera);
	~ViewerActionList() override;
	ViewerActionList& SetActionMode(ActionMode mode);
	ActionMode GetActionMode() const;

	// Returns local, viewport-clipped geometry, or an empty Rect when the row is
	// not prepared/visible. A menu can use GetScreenRect().TopLeft()+rect.BottomLeft().
	Rect GetItemActionRect(const String& id) const;

	// Mouse release over the same action that was pressed; no selection/cursor
	// mutation. Keyboard: Shift+F10 in either mode, Space in
	// Scene mode. Right-click on a row invokes the same action. Normal Enter and
	// Camera Space retain UiList's WhenAction contract. The callback may rebuild
	// the model or destroy this control; its ID is an owned snapshot.
	Event<const String&> WhenItemAction;

	void LeftDown(Point p, dword flags) override;
	void LeftUp(Point p, dword flags) override;
	void LeftDrag(Point p, dword flags) override;
	void LeftDouble(Point p, dword flags) override;
	void LeftTriple(Point p, dword flags) override;
	void RightDown(Point p, dword flags) override;
	void MouseMove(Point p, dword flags) override;
	void MouseLeave() override;
	void MouseWheel(Point p, int zdelta, dword flags) override;
	bool Key(dword key, int count) override;
	void LostFocus() override;
	void CancelMode() override;
	void State(int reason) override;

private:
	struct ActionState;
	class RowRender;
	std::shared_ptr<ActionState> action_state;
	String pressed_id;
	Ptr<UiDataModelBase> pressed_model;
	int pressed_revision = -1;

	String HitItem(Point p, bool action_only) const;
	bool CanInvoke(const String& id) const;
	void Invoke(const String& id);
	void ClearPress(bool release_capture);
};

}
#endif
