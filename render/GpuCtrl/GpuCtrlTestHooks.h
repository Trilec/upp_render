#pragma once

#include <RenderCanvas/RenderCanvas.h>

namespace Upp {
namespace GpuCtrlTestHooks {

struct ReplayColor {
	float red = 0.0f;
	float green = 0.0f;
	float blue = 0.0f;
	float alpha = 1.0f;
};

struct ReplayFillRect : Moveable<ReplayFillRect> {
	Rect rect = Rect(0, 0, 0, 0);
	ReplayColor color;
};

struct ReplayResult {
	ReplayColor background;
	Vector<ReplayFillRect> fill_rects;
	String error;
};

bool ReplayDisplayList(const UiDisplayList& list, ReplayResult& out);
bool BuildDefaultFrame(Size size, ReplayResult& out);

} // namespace GpuCtrlTestHooks
} // namespace Upp
