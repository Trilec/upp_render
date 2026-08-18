#pragma once

#include <CtrlCore/CtrlCore.h>
#include <RenderCanvas/RenderCanvas.h>

namespace Upp {

struct CtrlDisplayListRecordReport : Moveable<CtrlDisplayListRecordReport> {
	int rect_count = 0;
	int line_count = 0;
	int image_count = 0;
	int text_count = 0;
	int path_count = 0;
	int clip_count = 0;
	int transform_count = 0;
	String unsupported_operation;

	bool HasUnsupportedOperation() const { return !unsupported_operation.IsEmpty(); }
};

// Records resolved ordinary U++ control painting into the neutral immutable
// display-list contract. The bridge walks the public Ctrl tree and invokes each
// control's real Paint(Draw&) plus public CtrlFrame paint/layout operations in
// U++ paint order. U++ remains the sole authority for layout, state, focus and
// theme resolution; this package only translates the resulting Draw commands.
//
// Unsupported Draw semantics fail explicitly instead of being silently dropped.
// Native child-window controls remain outside the intended root-compositor path.
bool RecordCtrlDisplayList(Ctrl& ctrl, UiDisplayList& out, String& error,
                           CtrlDisplayListRecordReport *report = nullptr);

}
