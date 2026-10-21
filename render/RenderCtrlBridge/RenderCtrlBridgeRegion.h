#pragma once
#include <cmath>
// Private recorder adapter: represent rectangle subtraction with disjoint clips.
// Ordinary recording takes the direct path. At most 64 surviving regions are
// admitted, and each drawing operation is replayed once per disjoint region.
namespace Upp {
class CtrlRegionCanvas : public UiCanvas {
public:
    CtrlRegionCanvas(UiDisplayListBuilder& target, Size size) : target(target) { state.regions.Add(Rectf(0, 0, size.cx, size.cy)); }
    const String& GetError() const { return error.IsEmpty() ? target.GetError() : error; }
    void Save() override { stack.Add(state); target.Save(); }
    void Restore() override {
        target.Restore();
        if(!stack.IsEmpty()) { state = stack.Top(); stack.Drop(); }
    }
    void ClipRect(const Rectf& r) override { target.ClipRect(r); }
    void BlurLayer(int radius) override { target.BlurLayer(radius); }
    void ClipPath(const UiPath& p, UiFillRule rule) override { target.ClipPath(p, rule); }
    void ConcatTransform(const Transform2D& t) override { state.transform = state.transform * t; target.ConcatTransform(t); }
    bool Exclude(const Rectf& local) {
        const auto& t = state.transform;
        if(t.x.y != 0 || t.y.x != 0) { error = "ExcludeClip requires an axis-aligned Draw transform"; return false; }
        Pointf a = t.TransformPoint(local.TopLeft()), b = t.TransformPoint(local.BottomRight());
        Rectf cut(min(a.x, b.x), min(a.y, b.y), max(a.x, b.x), max(a.y, b.y));
        Vector<Rectf> result;
        auto add = [&](const Rectf& r) { if(r.right > r.left && r.bottom > r.top) result.Add(r); };
        for(const auto& r : state.regions) {
            Rectf q(max(r.left, cut.left), max(r.top, cut.top), min(r.right, cut.right), min(r.bottom, cut.bottom));
            if(q.right <= q.left || q.bottom <= q.top) add(r);
            else {
                add(Rectf(r.left, r.top, r.right, q.top)); add(Rectf(r.left, q.bottom, r.right, r.bottom));
                add(Rectf(r.left, q.top, q.left, q.bottom)); add(Rectf(q.right, q.top, r.right, q.bottom));
            }
            if(result.GetCount() > 64) { error = "ExcludeClip exceeds the 64-region recorder budget"; return false; }
        }
        state.regions = pick(result); state.excluded = true;
        return !state.regions.IsEmpty();
    }
    bool Intersects(const Rect& r) const {
        if(r.IsEmpty()) return false;
        for(const auto& q : state.regions)
            if(q.left < r.right && q.right > r.left && q.top < r.bottom && q.bottom > r.top) return true;
        return false;
    }
    void FillRect(const Rectf& r, Rgba8 c) override { Emit([&] { target.FillRect(r, c); }); }
    void InvertRect(const Rectf& r) override { Emit([&] { target.InvertRect(r); }); }
    void StrokeRect(const Rectf& r, double w, Rgba8 c) override { Emit([&] { target.StrokeRect(r, w, c); }); }
    void FillRoundedRect(const struct RoundedRect& r, Rgba8 c) override { Emit([&] { target.FillRoundedRect(r, c); }); }
    void DrawImage(const Rectf& r, const Image& image) override { Emit([&] { target.DrawImage(r, image); }); }
    void DrawImage(const Rectf& r, const Image& image, const Rect& src,
                   Rgba8 tint = Rgba8(255,255,255,255), bool mask = false) override {
        Emit([&] { target.DrawImage(r, image, src, tint, mask); });
    }
    void DrawText(const Pointf& p, const WString& s, Font f, Rgba8 c) override { Emit([&] { target.DrawText(p, s, f, c); }); }
    void DrawShapedText(const Pointf& p, const std::shared_ptr<const GpuShapedText>& s, Rgba8 c,bool override_ink=false) override {
        Emit([&] { target.DrawShapedText(p,s,c,override_ink); });
    }
    void FillPath(const UiPath& p, const UiPaint& paint, UiFillRule rule = UiFillRule::NonZero) override {
        Emit([&] { target.FillPath(p, paint, rule); });
    }
    void StrokePath(const UiPath& p, const UiPaint& paint, const UiStrokeStyle& style) override {
        Emit([&] { target.StrokePath(p, paint, style); });
    }
    void DrawSvg(const Rectf& r, const String& svg) override { Emit([&] { target.DrawSvg(r, svg); }); }
private:
    struct State : Moveable<State> {
        Transform2D transform;
        Vector<Rectf> regions;
        bool excluded = false;
        State() {}
        State(const State& other) : transform(other.transform), regions(other.regions, 1), excluded(other.excluded) {}
        State& operator=(const State& other) {
            transform = other.transform; regions = clone(other.regions); excluded = other.excluded; return *this;
        }
    };
    template <class F> void Emit(F emit) {
        if(!error.IsEmpty()) return;
        if(!state.excluded) { emit(); return; }
        const auto& t = state.transform;
        double determinant = t.x.x * t.y.y - t.x.y * t.y.x;
        if(!std::isfinite(determinant) || std::fabs(determinant) < 1e-12) {
            error = "ExcludeClip cannot replay a singular Draw transform"; return;
        }
        Transform2D inverse;
        inverse.x = Pointf(t.y.y / determinant, -t.x.y / determinant);
        inverse.y = Pointf(-t.y.x / determinant, t.x.x / determinant);
        inverse.t = Pointf(-(inverse.x.x * t.t.x + inverse.x.y * t.t.y),
                          -(inverse.y.x * t.t.x + inverse.y.y * t.t.y));
        for(const auto& clip : state.regions) {
            target.Save(); target.ConcatTransform(inverse); target.ClipRect(clip);
            target.ConcatTransform(t); emit(); target.Restore();
        }
    }
    UiDisplayListBuilder& target;
    State state;
    Vector<State> stack;
    String error;
};
}
