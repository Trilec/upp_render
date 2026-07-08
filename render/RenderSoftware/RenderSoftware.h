#pragma once

#include <Painter/Painter.h>
#include <RenderCanvas/RenderCanvas.h>

namespace Upp {

// Software reference replay for UiDisplayList.
class SoftwareUiRenderer {
public:
	bool Replay(const UiDisplayList& list, Painter& painter);
	const String& GetError() const { return error; }

private:
	String error;
};

}
