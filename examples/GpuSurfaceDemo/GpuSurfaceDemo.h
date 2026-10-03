#ifndef _GpuSurfaceDemo_GpuSurfaceDemo_h_
#define _GpuSurfaceDemo_GpuSurfaceDemo_h_

#include <Ui/Ui.h>
#include <GpuRender/GpuRender.h>
#include <Utilities/PropertyEditor/PropertyEditor.h>
#include <Utilities/PropertyEditor/PropertyValueEditors.h>

namespace Upp {

// Demo-only scene; GpuCtrl owns the Vulkan surface and all presentation lifetime.
// Each instance has its own scene and timer. All control/paint work stays on the GUI thread.
class ShapeSurface : public GpuCtrl {
public:
    ShapeSurface();
    ~ShapeSurface() override;
    void Configure(int count, int kind, int speed, bool animate, bool grid,
                   bool text, bool antialias, Color background);
    void Randomize();
    int GetPaintCount() const { return paint_count; }
    int GetTickCountForTest() const { return tick_count; }
    int GetItemCount() const { return items.GetCount(); }
    bool IsAntialiasing() const { return antialias; }
    bool HasCoveragePixels() const;
protected:
    void GpuPaint(GpuPainter& w) override;
    void State(int reason) override;
private:
    struct Item : Moveable<Item> {
        double x, y, vx, vy, size, aspect;
        int kind;
        Color color;
        int sprite = 0;
    };
    void PrepareSprites();
    void SyncTimer();
    void Tick();
    Vector<Item> items;
    Vector<Image> sprites;
    int sprite_dpi = 0;
    TimeCallback clock;
    int count = 24, kind = 0, speed = 100;
    bool animate = true, grid = true, text = true, antialias = true;
    Color background = Color(17, 24, 39);
    int paint_count = 0, tick_count = 0;
    int64 last_tick = 0;
};

class GpuSurfaceDemo : public TopWindow {
public:
    GpuSurfaceDemo();
    ~GpuSurfaceDemo() override;
    void Layout() override;
    void Paint(Draw& w) override;
    bool RunSmoke();
    String GenerateUsage() const;
private:
    void BuildHeader();
    void BuildPreview();
    void BuildInspector();
    void ApplyProjection();
    void ApplyTheme();
    void SelectPage(int page);
    void SetSecond(bool on);
    void UpdateStatus();
    Value Read(const char *id) const;

    PropertyEditorFactory pe_factory;
    PropertyEditorModel pe_model;
    UiTitleCard tc_header;
    UiBoxLayout box_actions { UiDirection::H };
    UiButton btn_randomize, btn_reset;
    UiToolButton btn_theme, btn_help, btn_exit;
    UiPanel pnl_preview, pnl_rail, pnl_inspector, pnl_code;
    UiLabel lbl_first, lbl_second, lbl_status;
    ShapeSurface gpu_first;
    One<ShapeSurface> gpu_second;
    UiBoxLayout box_modes { UiDirection::H };
    UiToolButton btn_inspector, btn_code, btn_copy;
    UiStack stk_pages;
    PropertyEditor pe_inspector;
    UiMultiEdit edit_code;
    TimeCallback status_clock;
    String last_status;
};

}
#endif
