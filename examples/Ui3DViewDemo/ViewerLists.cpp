#include "ViewerLists.h"
#include <Ui/UiIcons.h>
#include <Painter/Painter.h>
#include <algorithm>
#include <vector>

namespace Upp {

Image ViewerCameraGlyph(Color ink, int size)
{
	if(size <= 0) return Image();
	ImageBuffer image(size, size);
	{
		BufferPainter draw(image, MODE_ANTIALIASED);
		RGBA clear; clear.r = clear.g = clear.b = clear.a = 0;
		draw.Clear(clear);
		const double s = size / 16.0;
		draw.Move(2.3*s, 5*s).Line(4.5*s, 5*s).Line(5.5*s, 3.5*s)
		    .Line(10.5*s, 3.5*s).Line(11.5*s, 5*s).Line(13.7*s, 5*s)
		    .Line(13.7*s, 13*s).Line(2.3*s, 13*s).Close().Stroke(1.25*s, ink);
		draw.Ellipse(8*s, 9*s, 2.5*s, 2.5*s).Stroke(1.25*s, ink);
	}
	return Image(image);
}

Image ViewerCubeGlyph(Color ink, int size)
{
	if(size <= 0) return Image();
	ImageBuffer image(size, size);
	{
		BufferPainter draw(image, MODE_ANTIALIASED);
		RGBA clear; clear.r = clear.g = clear.b = clear.a = 0;
		draw.Clear(clear);
		const double s = size / 16.0;
		draw.Move(8*s, 1.8*s).Line(13.6*s, 4.8*s).Line(13.6*s, 11*s)
		    .Line(8*s, 14.2*s).Line(2.4*s, 11*s).Line(2.4*s, 4.8*s)
		    .Close().Stroke(1.15*s, ink);
		draw.Move(2.4*s, 4.8*s).Line(8*s, 8.1*s).Line(13.6*s, 4.8*s).Stroke(1.15*s, ink);
		draw.Move(8*s, 8.1*s).Line(8*s, 14.2*s).Stroke(1.15*s, ink);
	}
	return Image(image);
}

Image ViewerToolGlyph(int tool, Color ink, int size)
{
	if(size <= 0 || tool < 0 || tool > 3) return Image();
	ImageBuffer image(size, size);
	{
		BufferPainter draw(image, MODE_ANTIALIASED);
		RGBA clear; clear.r = clear.g = clear.b = clear.a = 0;
		draw.Clear(clear);
		const double s = size / 20.0;
		const double width = 1.55*s;
		auto line = [&](double x1, double y1, double x2, double y2) {
			draw.Move(x1*s, y1*s).Line(x2*s, y2*s).Stroke(width, ink);
		};
		switch(tool) {
		case 0:
			draw.Move(5*s, 2*s).Line(16*s, 10*s).Line(11*s, 11.5*s)
			    .Line(9*s, 17*s).Close().Stroke(width, ink);
			line(11, 11.5, 15, 17);
			break;
		case 1:
			draw.Ellipse(10*s, 10*s, 8*s, 3.3*s).Stroke(width, ink);
			draw.Ellipse(10*s, 10*s, 4.7*s, 4.7*s).Stroke(width, ink);
			draw.Move(3*s, 5*s).Line(6*s, 4.5*s).Line(5*s, 7.5*s).Stroke(width, ink);
			draw.Move(17*s, 15*s).Line(14*s, 15.5*s).Line(15*s, 12.5*s).Stroke(width, ink);
			break;
		case 2:
			line(3, 10, 17, 10); line(10, 3, 10, 17);
			draw.Move(6*s, 7*s).Line(3*s, 10*s).Line(6*s, 13*s).Stroke(width, ink);
			draw.Move(14*s, 7*s).Line(17*s, 10*s).Line(14*s, 13*s).Stroke(width, ink);
			draw.Move(7*s, 6*s).Line(10*s, 3*s).Line(13*s, 6*s).Stroke(width, ink);
			draw.Move(7*s, 14*s).Line(10*s, 17*s).Line(13*s, 14*s).Stroke(width, ink);
			break;
		case 3:
			draw.Move(7*s, 3*s).Line(3*s, 3*s).Line(3*s, 7*s).Stroke(width, ink);
			draw.Move(13*s, 3*s).Line(17*s, 3*s).Line(17*s, 7*s).Stroke(width, ink);
			draw.Move(3*s, 13*s).Line(3*s, 17*s).Line(7*s, 17*s).Stroke(width, ink);
			draw.Move(17*s, 13*s).Line(17*s, 17*s).Line(13*s, 17*s).Stroke(width, ink);
			draw.Move(7*s, 7*s).Line(13*s, 7*s).Line(13*s, 13*s)
			    .Line(7*s, 13*s).Close().Stroke(width, ink);
			break;
		}
	}
	return Image(image);
}

struct ViewerActionList::ActionState {
	struct Row {
		String id;
		Rect bounds, action;
		bool enabled = false;
	};
	ActionMode mode = Camera;
	String hot_id, pressed_id;
	std::vector<std::weak_ptr<Row>> rows;

	std::shared_ptr<Row> AddRow()
	{
		rows.erase(std::remove_if(rows.begin(), rows.end(),
		                         [](const std::weak_ptr<Row>& row) { return row.expired(); }), rows.end());
		auto row = std::make_shared<Row>();
		rows.push_back(row);
		return row;
	}
};

// The host List supplies the exact, scrolled row rectangle. Retain only those
// disposable rectangles through weak references to its recycled renderers;
// neither an item collection nor a second row/scroll layout is introduced.
class ViewerActionList::RowRender : public UiItemRender {
public:
	explicit RowRender(const std::shared_ptr<ActionState>& state)
		: actions(state), row(state->AddRow()) {}

	One<UiItemRender> Clone() const override
	{
		One<UiItemRender> copy = new RowRender(actions);
		CopyConfigurationTo(*copy);
		return copy;
	}

	Size GetContentSize() const override
	{
		Size size = content.GetContentSize();
		size.cx += DPI(30);
		return size;
	}
	Size GetMinSize() const override { return GetContentSize(); }

	void Paint(Draw& w, const UiItemRenderState& state) const override
	{
		UiItemRenderState body = state;
		// A hidden scene object is visually muted, while its eye remains operable.
		if(actions->mode == Scene && !Data().checked) body.enabled = false;
		content.Paint(w, body);
		if(row->id.IsEmpty() || row->action.IsEmpty()) return;
		const auto& style = GetStyle();
		const bool enabled = state.enabled && row->enabled;
		const bool hot = actions->hot_id == row->id;
		const bool pressed = hot && actions->pressed_id == row->id;
		const StyledState visual = !enabled ? ST_DISABLED : pressed ? ST_PRESSED : hot ? ST_HOT : ST_NORMAL;
		if(enabled && (hot || pressed)) {
			StyledMetrics metrics;
			metrics.radius = DPI(3);
			metrics.face_enabled = true;
			metrics.frame_enabled = metrics.focus_enabled = metrics.shadow.enabled = false;
			UiPaintStyledSurface(w, row->action, style.palette, metrics, StyledSkin(), visual, false, false, false);
		}
		Image icon = actions->mode == Camera ? ICON_NAVIGATION_OUTLINED_MORE_HORIZ_48()
		           : Data().checked ? ICON_ACTION_OUTLINED_VISIBILITY_48()
		                            : ICON_ACTION_OUTLINED_VISIBILITY_OFF_48();
		const int side = min(DPI(15), min(row->action.GetWidth(), row->action.GetHeight()));
		const Rect glyph = RectC(row->action.left + (row->action.GetWidth() - side) / 2,
		                         row->action.top + (row->action.GetHeight() - side) / 2, side, side);
		const Color ink = enabled ? hot ? style.palette.ink[ST_HOT] : style.muted_ink
		                          : style.palette.ink[ST_DISABLED];
		UiPaintStyledIcon(w, glyph, icon, true, true, UiIconRenderMode::MonoTint, ink, enabled);
	}

protected:
	void Layout() override
	{
		const auto& style = GetStyle();
		row->id = Data().data.Is<String>() ? AsString(Data().data) : String();
		row->enabled = Data().enabled;
		row->bounds = Bounds();
		const int side = min(DPI(26), max(0, Bounds().GetHeight() - DPI(4)));
		const int right = Bounds().right - max(DPI(2), style.metrics.content_margin.right);
		row->action = Rect(max(Bounds().left, right - side), Bounds().top + (Bounds().GetHeight() - side) / 2,
		                   max(Bounds().left, right), Bounds().top + (Bounds().GetHeight() + side) / 2);
		UiItemRenderData data = Data();
		data.has_check = false;
		data.right_text.Clear();
		UiItemRenderStyle content_style = style;
		UiReserveItemRenderDecoration(content_style, UiAlign::RIGHT, DPI(26), DPI(4));
		content.SetData(data).SetCustomStyle(content_style);
		content.PrepareLayout(Bounds(), Direction());
	}

private:
	std::shared_ptr<ActionState> actions;
	std::shared_ptr<ActionState::Row> row;
	UiItemRenderBasic content;
};

ViewerActionList::ViewerActionList(ActionMode mode)
	: action_state(std::make_shared<ActionState>())
{
	action_state->mode = mode;
	EnableRenameOnDblClick(false).EnableDragReorder(false).ShowDragHandle(false);
	RowRender render(action_state);
	SetItemRender(render);
}

ViewerActionList::~ViewerActionList()
{
	WhenItemAction.Clear();
	ClearPress(true);
}

ViewerActionList& ViewerActionList::SetActionMode(ActionMode mode)
{
	if(action_state->mode == mode) return *this;
	ClearPress(true);
	action_state->mode = mode;
	action_state->hot_id.Clear();
	Refresh();
	return *this;
}

ViewerActionList::ActionMode ViewerActionList::GetActionMode() const
{
	return action_state->mode;
}

Rect ViewerActionList::GetItemActionRect(const String& id) const
{
	if(id.IsEmpty()) return Rect(0, 0, 0, 0);
	const auto& style = GetStyle();
	const Rect viewport = UiStyledInnerRect(GetSize(), style.metrics, style.skin);
	for(const auto& weak : action_state->rows) {
		const auto row = weak.lock();
		if(row && row->id == id) return row->action & viewport;
	}
	return Rect(0, 0, 0, 0);
}

String ViewerActionList::HitItem(Point p, bool action_only) const
{
	if(!IsEnabled()) return String();
	const auto& style = GetStyle();
	if(!UiStyledInnerRect(GetSize(), style.metrics, style.skin).Contains(p)) return String();
	for(const auto& weak : action_state->rows) {
		const auto row = weak.lock();
		if(row && row->enabled && (action_only ? row->action : row->bounds).Contains(p))
			return row->id;
	}
	return String();
}

bool ViewerActionList::CanInvoke(const String& id) const
{
	if(id.IsEmpty() || !IsEnabled()) return false;
	for(int i = 0; i < Model().GetCount(); ++i) {
		const auto& item = Model().Get(i);
		if(item.data.Is<String>() && AsString(item.data) == id)
			return item.enabled && !item.group_header;
	}
	return false;
}

void ViewerActionList::Invoke(const String& id)
{
	if(!CanInvoke(id)) return;
	const String snapshot = id;
	auto callback = WhenItemAction;
	if(callback) callback(snapshot);
	// User code may have destroyed this control. No member access follows.
}

void ViewerActionList::ClearPress(bool release_capture)
{
	const bool held_action = !pressed_id.IsEmpty();
	pressed_id.Clear();
	pressed_model = nullptr;
	pressed_revision = -1;
	action_state->pressed_id.Clear();
	if(held_action) Refresh();
	// ReleaseCtrlCapture calls CancelMode, so state is already idle. CancelMode
	// itself never releases capture or calls application code.
	if(release_capture && held_action && HasCapture()) ReleaseCapture();
}

void ViewerActionList::LeftDown(Point p, dword flags)
{
	ClearPress(true);
	UiList::Layout();
	const String id = HitItem(p, true);
	if(id.IsEmpty()) { UiList::LeftDown(p, flags); return; }
	Ptr<ViewerActionList> alive(this);
	SetFocus();
	if(!alive) return;
	UiList::Layout();
	if(HitItem(p, true) != id || !CanInvoke(id)) return;
	pressed_id = id;
	pressed_model = &Model();
	pressed_revision = Model().GetRevision();
	action_state->pressed_id = action_state->hot_id = id;
	SetCapture();
	if(!alive) return;
	if(!HasCapture() || pressed_id != id || pressed_model != &Model() ||
	   pressed_revision != Model().GetRevision()) {
		const bool release = HasCapture();
		ClearPress(false);
		if(release) ReleaseCapture();
		return;
	}
	Refresh();
}

void ViewerActionList::LeftUp(Point p, dword flags)
{
	if(pressed_id.IsEmpty()) { UiList::LeftUp(p, flags); return; }
	const String id = pressed_id;
	Ptr<UiDataModelBase> model = pressed_model;
	const int revision = pressed_revision;
	UiList::Layout();
	const bool same = model == &Model() && revision == Model().GetRevision() && HitItem(p, true) == id;
	Ptr<ViewerActionList> alive(this);
	ClearPress(true);
	if(alive && same && model == &Model() && revision == Model().GetRevision()) Invoke(id);
}

void ViewerActionList::LeftDrag(Point p, dword flags)
{
	if(pressed_id.IsEmpty()) UiList::LeftDrag(p, flags);
}

void ViewerActionList::LeftDouble(Point p, dword flags)
{
	UiList::Layout();
	if(!HitItem(p, true).IsEmpty()) LeftDown(p, flags);
	else UiList::LeftDouble(p, flags);
}

void ViewerActionList::LeftTriple(Point p, dword flags)
{
	LeftDouble(p, flags);
}

void ViewerActionList::RightDown(Point p, dword)
{
	ClearPress(true);
	UiList::Layout();
	const String id = HitItem(p, false);
	if(id.IsEmpty()) return;
	Ptr<ViewerActionList> alive(this);
	SetFocus();
	if(alive) Invoke(id);
}

void ViewerActionList::MouseMove(Point p, dword flags)
{
	UiList::MouseMove(p, flags);
	const String id = HitItem(p, true);
	if(action_state->hot_id != id) {
		action_state->hot_id = id;
		Tip(id.IsEmpty() ? String() : action_state->mode == Camera
		    ? String("Camera actions (Shift+F10)") : String("Show or hide object (Space)"));
		Refresh();
	}
}

void ViewerActionList::MouseLeave()
{
	action_state->hot_id.Clear();
	Tip("");
	UiList::MouseLeave();
	Refresh();
}

void ViewerActionList::MouseWheel(Point p, int zdelta, dword flags)
{
	ClearPress(true);
	action_state->hot_id.Clear();
	UiList::MouseWheel(p, zdelta, flags);
}

bool ViewerActionList::Key(dword key, int count)
{
	if(key == K_ESCAPE && !pressed_id.IsEmpty()) { ClearPress(true); return true; }
	if(key == (K_SHIFT | K_F10) || (action_state->mode == Scene && key == K_SPACE)) {
		const int cursor = GetCursor();
		if(cursor >= 0 && cursor < Model().GetCount()) {
			const String id = AsString(Model().Get(cursor).data);
			Invoke(id);
		}
		return true;
	}
	return UiList::Key(key, count);
}

void ViewerActionList::LostFocus()
{
	ClearPress(true);
	UiList::LostFocus();
}

void ViewerActionList::CancelMode()
{
	ClearPress(false);
	UiList::CancelMode();
}

void ViewerActionList::State(int reason)
{
	if(reason == CLOSE || (reason == SHOW && !IsVisible()) || (reason == ENABLE && !IsEnabled()))
		ClearPress(true);
	UiList::State(reason);
}

}
