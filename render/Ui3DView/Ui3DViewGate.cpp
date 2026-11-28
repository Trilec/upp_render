#include "Ui3DViewRenderer.h"
#include <algorithm>
#include <cmath>

namespace Upp {
bool ComputeUi3DViewGateLayout(const Scene3DCamera& camera,const Ui3DViewOptions& options,
                              Size viewport,Ui3DViewGateLayout& out)
{
    out=Ui3DViewGateLayout();
    if(!std::isfinite(options.matte_opacity) || options.matte_opacity<0 || options.matte_opacity>1 ||
       !std::isfinite(options.guide_opacity) || options.guide_opacity<0 || options.guide_opacity>1 ||
       !std::isfinite(options.action_safe) || !std::isfinite(options.title_safe) ||
       options.title_safe<=0 || options.title_safe>options.action_safe || options.action_safe>1)
        return false;
    const Rect gate=camera.GetGateRect(viewport);
    if(gate.IsEmpty()) return false;
    auto inset=[&](double fraction) {
        const int width=std::max(1,(int)std::lround(gate.Width()*fraction));
        const int height=std::max(1,(int)std::lround(gate.Height()*fraction));
        return RectC(gate.left+(gate.Width()-width)/2,gate.top+(gate.Height()-height)/2,width,height);
    };
    out.gate=gate; out.action_safe=inset(options.action_safe); out.title_safe=inset(options.title_safe);
    return true;
}

namespace Ui3DInternal {
bool ViewContent::DrawGate(GpuDevice& device,const GpuFrameInfo& frame,String& error)
{
    state->stats.gate_draws_last_frame=0;
    const auto& options=state->options;
    if(!options.show_gate && (!options.show_safe || options.guide_opacity == 0)) return true;
    Ui3DViewGateLayout layout;
    if(!ComputeUi3DViewGateLayout(state->camera,options,frame.size,layout)) {
        error=state->error="Invalid film/safe guide projection."; return false;
    }
    UiDisplayListBuilder builder;
    if(options.show_gate) {
        auto matte=options.matte_color;
        matte.a=(byte)std::lround(options.matte_opacity*255);
        auto fill=[&](Rect rect) {
            if(!rect.IsEmpty() && matte.a) builder.FillRect(Rectf(rect),matte);
        };
        fill(Rect(0,0,frame.size.cx,layout.gate.top));
        fill(Rect(0,layout.gate.bottom,frame.size.cx,frame.size.cy));
        fill(Rect(0,layout.gate.top,layout.gate.left,layout.gate.bottom));
        fill(Rect(layout.gate.right,layout.gate.top,frame.size.cx,layout.gate.bottom));
    }
    auto outline=[&](const Rect& rect,Rgba8 color) {
        if(rect.Width()>=2 && rect.Height()>=2)
            builder.StrokeRect(Rectf(rect.left+.5,rect.top+.5,rect.right-.5,rect.bottom-.5),1,color);
    };
    if(options.show_gate) outline(layout.gate,options.gate_color);
    if(options.show_safe && options.guide_opacity > 0) {
        auto guide = options.safe_color;
        guide.a = byte(std::lround(options.guide_opacity * 255));
        const Rect& gate = layout.gate;
        auto guide_grid = [&](double fraction) {
            for(double position : {fraction, 1. - fraction}) {
                const int x = gate.left + int(std::lround((gate.Width() - 1) * position));
                const int y = gate.top + int(std::lround((gate.Height() - 1) * position));
                builder.FillRect(Rectf(x, gate.top, x + 1, gate.bottom), guide);
                builder.FillRect(Rectf(gate.left, y, gate.right, y + 1), guide);
            }
        };
        switch(options.framing_guide) {
        case Ui3DFramingGuide::SafeAreas:
            outline(layout.action_safe, guide);
            if(layout.title_safe != layout.action_safe) outline(layout.title_safe, guide);
            break;
        case Ui3DFramingGuide::TitleSafe:
            outline(layout.title_safe, guide);
            break;
        case Ui3DFramingGuide::ActionSafe:
            outline(layout.action_safe, guide);
            break;
        case Ui3DFramingGuide::Crosshair: {
            const Point middle = gate.CenterPoint();
            const int reach = std::min({12, (gate.Width() - 1) / 2, (gate.Height() - 1) / 2});
            if(reach > 0) {
                builder.FillRect(Rectf(middle.x - reach, middle.y, middle.x + reach + 1, middle.y + 1), guide);
                builder.FillRect(Rectf(middle.x, middle.y - reach, middle.x + 1, middle.y), guide);
                builder.FillRect(Rectf(middle.x, middle.y + 1, middle.x + 1, middle.y + reach + 1), guide);
            }
            break;
        }
        case Ui3DFramingGuide::Thirds:
            guide_grid(1. / 3);
            break;
        case Ui3DFramingGuide::GoldenRatio:
            guide_grid(.381966011250105);
            break;
        }
    }
    UiDisplayList list;
    if(!builder.Finish(list)) { error=state->error=builder.GetError(); return false; }
    if(!gate_renderer) gate_renderer.reset(new UiRenderer2D(device));
    UiRenderer2DTarget target;
    target.color_target=frame.color_target; target.color_format=frame.color_format; target.size=frame.size;
    target.load_op=GpuLoadOp::Load; target.store_op=GpuStoreOp::Store;
    if(!gate_renderer->Render(list,target)) { error=state->error=gate_renderer->GetError(); return false; }
    state->stats.gate_draws_last_frame=gate_renderer->GetStats().draw_count;
    return true;
}
} }
