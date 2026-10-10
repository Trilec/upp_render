#pragma once

#include <CtrlCore/CtrlCore.h>
#include <RenderCanvas/RenderCanvas.h>

namespace Upp {

// GUI-thread lifetime guard for the existing U++ direct control traversal.
// Nested roots share one cold setup probe; frames allocate no GDI probe/backbuffer.
// The last guard restores the inherited setting. Do not change GlobalBackBuffer
// while a guard owns it. Standalone recording acquires a temporary guard.
class CtrlDisplayListPaintScope {
public:
	CtrlDisplayListPaintScope();
	~CtrlDisplayListPaintScope();
	CtrlDisplayListPaintScope(const CtrlDisplayListPaintScope&) = delete;
	void operator=(const CtrlDisplayListPaintScope&) = delete;
};

struct CtrlDisplayListRecordReport : Moveable<CtrlDisplayListRecordReport> {
	int paint_probe_count = 0; // Cold setup only while a host paint scope is held.
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
