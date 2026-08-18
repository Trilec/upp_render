#pragma once

#include <CtrlLib/CtrlLib.h>
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

// Records resolved U++ control painting through Ctrl::DrawCtrl into the neutral
// immutable display-list contract. U++ remains the authority for control tree,
// layout, state, focus and theme resolution; this bridge translates only the
// resulting Draw operations.
//
// The current production implementation is Win32 because Ctrl::DrawCtrl routes
// recursive control painting through the platform SystemDraw abstraction. Other
// platforms fail explicitly until an equivalent SystemDraw adapter is supplied.
bool RecordCtrlDisplayList(Ctrl& ctrl, UiDisplayList& out, String& error,
                           CtrlDisplayListRecordReport *report = nullptr);

}
