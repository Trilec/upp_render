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

// Records resolved U++ control painting through the public Ctrl::DrawCtrl path
// into the neutral immutable display-list contract. U++ itself remains the
// authority for recursive control/frame traversal, layout, state, focus and
// theme resolution; this bridge translates only the resulting SystemDraw
// operations.
//
// Unsupported Draw semantics fail explicitly instead of being silently dropped.
// The current production capture adapter is Win32; other platforms fail
// explicitly until their equivalent SystemDraw path is validated.
bool RecordCtrlDisplayList(Ctrl& ctrl, UiDisplayList& out, String& error,
                           CtrlDisplayListRecordReport *report = nullptr);

}
