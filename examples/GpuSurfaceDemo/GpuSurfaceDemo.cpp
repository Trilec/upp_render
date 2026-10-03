#include "GpuSurfaceDemo.h"
#include <Painter/Painter.h>
#include <chrono>

namespace Upp {

ShapeSurface::ShapeSurface()
{
    Randomize();
}

ShapeSurface::~ShapeSurface()
{
    clock.Kill();
}

void ShapeSurface::Configure(int n, int k, int s, bool a, bool g, bool t, bool aa, Color bg)
{
    bool rebuild = count != n || kind != k;
    count = minmax(n, 1, 200);
    kind = minmax(k, 0, 4);
    speed = minmax(s, 0, 300);
    animate = a;
    grid = g;
    text = t;
    antialias = aa;
    background = bg;
    PrepareSprites();
    if(rebuild) Randomize();
    SyncTimer();
    RequestGpuRefresh();
}

void ShapeSurface::Randomize()
{
    items.Clear();
    PrepareSprites();
    for(int i = 0; i < count; ++i) {
        Item& b = items.Add();
        b.x = Random(10000) / 10000.0;
        b.y = Random(10000) / 10000.0;
        b.vx = (0.12 + Random(200) / 1000.0) * (Random(2) ? 1 : -1);
        b.vy = (0.12 + Random(200) / 1000.0) * (Random(2) ? 1 : -1);
        b.size = 10 + Random(15);
        b.kind = kind ? kind - 1 : i % 4;
        b.aspect = b.kind == 2 ? 2.2 : b.kind == 1 ? 1.6 : 1.0;
        int color_index = Random(6);
        b.sprite = (b.kind * 6 + color_index) * 15 + (int)b.size - 10;
        const Color colors[] = { Color(96, 165, 250), Color(52, 211, 153),
                                 Color(251, 191, 36), Color(244, 114, 182),
                                 Color(167, 139, 250), Color(34, 211, 238) };
        b.color = colors[color_index];
    }
    RequestGpuRefresh();
}

void ShapeSurface::PrepareSprites()
{
    if(sprite_dpi == DPI(100) && sprites.GetCount() == 360) return;
    sprite_dpi = DPI(100);
    sprites.Clear();
    sprites.SetCount(360);
    const Color colors[] = { Color(96, 165, 250), Color(52, 211, 153),
                             Color(251, 191, 36), Color(244, 114, 182),
                             Color(167, 139, 250), Color(34, 211, 238) };
    // Finite immutable variants: four shapes, six colours, fifteen sizes.
    // Rasterize once per DPI; randomization and animation reuse these image identities.
    for(int k = 0; k < 4; ++k)
        for(int c = 0; c < 6; ++c)
            for(int n = 0; n < 15; ++n) {
                double h = DPI(10 + n);
                double w = h * (k == 2 ? 2.2 : k == 1 ? 1.6 : 1.0);
                ImageBuffer buffer(Size((int)ceil(w) + 4, (int)ceil(h) + 4));
                buffer.SetKind(IMAGE_ALPHA);
                Fill(~buffer, RGBAZero(), buffer.GetLength());
                BufferPainter p(buffer, MODE_ANTIALIASED);
                Rectf r(2, 2, 2 + w, 2 + h);
                if(k == 0) p.Ellipse(r).Fill(colors[c]);
                else if(k == 1) p.Rectangle(r).Fill(colors[c]);
                else p.RoundedRectangle(r, k == 2 ? h / 2 : h * 0.22).Fill(colors[c]);
                if(k == 3) p.Rectangle(r).Stroke(1, White());
                p.Finish();
                sprites[(k * 6 + c) * 15 + n] = Image(buffer);
            }
}

bool ShapeSurface::HasCoveragePixels() const
{
    for(const Image& image : sprites) {
        const RGBA *pixels = ~image;
        for(int i = 0; i < image.GetLength(); ++i)
            if(pixels[i].a > 0 && pixels[i].a < 255) return true;
    }
    return false;
}

void ShapeSurface::SyncTimer()
{
    clock.Kill();
    last_tick = GetTickCount();
    if(IsOpen() && IsVisible() && animate && speed > 0)
        clock.Set(-33, [=] { Tick(); });
}

void ShapeSurface::State(int reason)
{
    if(reason == CLOSE) clock.Kill();
    GpuCtrl::State(reason);
    if(reason == OPEN || reason == SHOW)
        SyncTimer();
}

void ShapeSurface::Tick()
{
    if(!IsOpen() || !IsVisible()) { clock.Kill(); return; }
    int64 now = GetTickCount();
    double dt = min(0.1, max(0.0, (now - last_tick) / 1000.0)) * speed / 100.0;
    last_tick = now;
    for(Item& b : items) {
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        if(b.x < 0) { b.x = -b.x; b.vx = abs(b.vx); }
        if(b.x > 1) { b.x = 2 - b.x; b.vx = -abs(b.vx); }
        if(b.y < 0) { b.y = -b.y; b.vy = abs(b.vy); }
        if(b.y > 1) { b.y = 2 - b.y; b.vy = -abs(b.vy); }
    }
    ++tick_count;
    RequestGpuRefresh();
}

void ShapeSurface::GpuPaint(GpuPainter& w)
{
    ++paint_count;
    Size sz = w.GetSize();
    w.Clear(background);
    if(sz.cx <= 0 || sz.cy <= 0) return;
    if(grid) {
        Color ink = Blend(background, White(), 24);
        for(int x = 0; x < sz.cx; x += DPI(32))
            w.FillRect(Rectf(x, 0, x + 1, sz.cy), ink);
        for(int y = 0; y < sz.cy; y += DPI(32))
            w.FillRect(Rectf(0, y, sz.cx, y + 1), ink);
    }
    w.Save();
    w.ClipRect(Rectf(0, 0, sz.cx, sz.cy));
    for(const Item& b : items) {
        double bw = min((double)sz.cx, DPI(b.size) * b.aspect);
        double bh = min((double)sz.cy, (double)DPI(b.size));
        double x = b.x * max(0.0, sz.cx - bw);
        double y = b.y * max(0.0, sz.cy - bh);
        Rectf r(x, y, x + bw, y + bh);
        if(antialias) {
            const Image& sprite = sprites[b.sprite];
            double sx = bw / (DPI(b.size) * b.aspect);
            double sy = bh / DPI(b.size);
            w.DrawImage(Rectf(x - 2 * sx, y - 2 * sy,
                             x - 2 * sx + sprite.GetWidth() * sx,
                             y - 2 * sy + sprite.GetHeight() * sy), sprite);
        }
        else if(b.kind == 0) {
            struct RoundedRect rounded(r, min(bw, bh) / 2);
            w.FillRoundedRect(rounded, b.color);
        }
        else if(b.kind == 1)
            w.FillRect(r, b.color);
        else if(b.kind == 2) {
            struct RoundedRect rounded(r, bh / 2);
            w.FillRoundedRect(rounded, b.color);
        }
        else {
            struct RoundedRect rounded(r, min(bw, bh) * 0.22);
            w.FillRoundedRect(rounded, b.color);
            w.StrokeRect(r, 1, Rgba8::FromColor(White()));
        }
    }
    w.Restore();
    if(text) {
        w.DrawText(Pointf(DPI(10), DPI(10)), Format("%d bouncing shapes", items.GetCount()),
                   SansSerif(DPI(12)).Bold(), White());
        w.DrawText(Pointf(DPI(10), max(DPI(28), sz.cy - DPI(24))),
                   Format("%d x %d  |  Vulkan", sz.cx, sz.cy),
                   SansSerif(DPI(11)), Color(203, 213, 225));
    }
}

GpuSurfaceDemo::GpuSurfaceDemo()
{
    Title("GpuCtrl Demo — Vulkan surfaces").Sizeable().Zoomable();
    SetRect(80, 80, DPI(1220), DPI(780));
    UiThemeContext context = UiTheme::GetContext();
    context.preset = UiThemePreset::Minimal;
    context.mode = UiThemeMode::Light;
    UiTheme::Set(context);
    RegisterPropertyEditorV1Editors(pe_factory);
    BuildHeader();
    BuildPreview();
    BuildInspector();
    ApplyTheme();
    SelectPage(0);
    ApplyProjection();
    status_clock.Set(-500, [=] { UpdateStatus(); });
}

GpuSurfaceDemo::~GpuSurfaceDemo()
{
    status_clock.Kill();
}

void GpuSurfaceDemo::BuildHeader()
{
    Add(tc_header);
    tc_header.SetTitle("GpuCtrl")
             .SetSubTitle("A small Vulkan surface inside an ordinary Ui application")
             .ShowTitleLine(false).SetContentInset(DPI(8)).SetContentCell(box_actions);
    box_actions.SetGap(DPI(4)).SetInset(0).SetAlignItems(UiCrossAlign::Center);
    box_actions.AddSpacer(1).Expand(1);
    btn_randomize.SetText("Randomize");
    btn_reset.SetText("Reset");
    btn_theme.SetIcon(ICON_ACTION_DARK_MODE_48()).SetIconSize(DPI(16), DPI(16)).Tip("Toggle light/dark");
    btn_help.SetIcon(ICON_DESIGN_HELP_48()).SetIconSize(DPI(16), DPI(16)).Tip("About these surfaces");
    btn_exit.SetIcon(ICON_DESIGN_MODE_OFF_ON_48()).SetIconSize(DPI(16), DPI(16)).Tip("Close demo");
    box_actions.Add(btn_randomize).Fixed(DPI(100));
    box_actions.Add(btn_reset).Fixed(DPI(70));
    box_actions.Add(btn_theme).Fixed(DPI(34));
    box_actions.Add(btn_help).Fixed(DPI(34));
    box_actions.Add(btn_exit).Fixed(DPI(34));
    btn_randomize.WhenAction = [=] {
        gpu_first.Randomize();
        if(gpu_second) gpu_second->Randomize();
    };
    btn_reset.WhenAction = [=] {
        for(int i = 0; i < pe_model.GetCount(); ++i) {
            const PropertyEditorItem& item = pe_model[i];
            pe_model.SetValue(item.id, item.default_value);
        }
        pe_inspector.RefreshModel();
        ApplyProjection();
    };
    btn_theme.WhenAction = [=] {
        UiThemeContext c = UiTheme::GetContext();
        c.mode = c.mode == UiThemeMode::Dark ? UiThemeMode::Light : UiThemeMode::Dark;
        UiTheme::Set(c);
        ApplyTheme();
    };
    btn_help.WhenAction = [=] {
        PromptOK("GpuCtrl owns an embedded Vulkan surface. Ui owns the surrounding controls.\n\n"
                 "Resize the surface, add shapes, pause each scene or remove and recreate Surface B. "
                 "Both surfaces share compatible device state but have separate scenes and swapchains.\n\n"
                 "This demo uses the public 2D painter API. Rendering runs on the GUI thread; "
                 "two surfaces do not imply two render threads.\n\n"
                 "For whole-window GPU composition, see GpuUiGallery.");
    };
    btn_exit.WhenAction = [=] { Close(); };
}

void GpuSurfaceDemo::BuildPreview()
{
    Add(pnl_preview);
    pnl_preview.Add(gpu_first);
    pnl_preview.Add(lbl_first);
    pnl_preview.Add(lbl_second);
    pnl_preview.Add(lbl_status);
    lbl_first.SetText("Surface A").SetAlign(UiAlign::CENTER, UiAlign::CENTER);
    lbl_second.SetText("Surface B").SetAlign(UiAlign::CENTER, UiAlign::CENTER);
    lbl_status.SetAlign(UiAlign::CENTER, UiAlign::CENTER);
    Add(pnl_rail);
    pnl_rail.Add(box_modes);
    pnl_rail.Add(stk_pages);
    box_modes.SetGap(DPI(4)).SetInset(DPI(4)).SetAlignItems(UiCrossAlign::Center);
    btn_inspector.SetText("Inspector").SetCheckable();
    btn_code.SetText("Code").SetCheckable();
    box_modes.Add(btn_inspector).Fixed(DPI(100));
    box_modes.Add(btn_code).Fixed(DPI(80));
    stk_pages.Add(pnl_inspector, "inspector");
    stk_pages.Add(pnl_code, "code");
    pnl_inspector.Add(pe_inspector.SizePos());
    pnl_code.Add(edit_code.HSizePos(DPI(6), DPI(6)).VSizePos(DPI(42), DPI(6)));
    pnl_code.Add(btn_copy.RightPos(DPI(8), DPI(32)).TopPos(DPI(6), DPI(30)));
    edit_code.SetReadOnly();
    btn_copy.SetIcon(ICON_CONTENT_CONTENT_COPY_48()).SetIconSize(DPI(16), DPI(16)).Tip("Copy C++ usage");
    btn_copy.WhenAction = [=] { WriteClipboardText(GenerateUsage()); };
    btn_inspector.WhenAction = [=] { SelectPage(0); };
    btn_code.WhenAction = [=] { SelectPage(1); };
}

void GpuSurfaceDemo::BuildInspector()
{
    pe_model.AddNumericInt("width", "Width", 300, 64, 640, 1, "Surface").SetUnit("logical px").SetDefault(300);
    pe_model.AddNumericInt("height", "Height", 300, 64, 640, 1, "Surface").SetUnit("logical px").SetDefault(300);
    pe_model.AddBoolean("second", "Show surface B", true, "Surface").SetDefault(true);
    pe_model.AddNumericInt("count", "Shape count", 24, 1, 200, 1, "Scene").SetDefault(24);
    pe_model.AddChoice("kind", "Shape", "Mixed", "Scene")
        .AddChoice("Mixed", "Mixed").AddChoice("Balls", "Balls")
        .AddChoice("Rectangles", "Rectangles").AddChoice("Oblongs", "Oblongs")
        .AddChoice("Rounded squares", "Rounded squares").SetDefault("Mixed");
    pe_model.AddNumericInt("speed", "Speed", 100, 0, 300, 1, "Scene").SetUnit("%").SetDefault(100);
    pe_model.AddBoolean("animate_a", "Animate A", true, "Scene").SetDefault(true);
    pe_model.AddBoolean("animate_b", "Animate B", true, "Scene").SetDefault(true);
    pe_model.AddBoolean("antialias", "Antialias shapes", true, "Drawing").SetDefault(true)
        .SetHelp("Cached Painter edge coverage, displayed by Vulkan. Off uses direct single-sample geometry; this is not MSAA.");
    pe_model.AddBoolean("grid", "Show grid", true, "Drawing").SetDefault(true);
    pe_model.AddBoolean("text", "Show text", true, "Drawing").SetDefault(true);
    pe_model.AddColor("background", "Background", Color(17, 24, 39), "Drawing").SetDefault(Color(17, 24, 39));
    pe_model.StructureChanged();
    pe_inspector.SetFactory(&pe_factory);
    pe_inspector.SetModel(&pe_model);
    pe_inspector.SetLabelRatio(48);
    auto changed = [=](String, Value) { ApplyProjection(); };
    pe_inspector.WhenPreview = changed;
    pe_inspector.WhenCommit = changed;
    pe_inspector.WhenReset = [=](String id) {
        const PropertyEditorItem *item = pe_model.Find(id);
        if(item) pe_model.SetValue(id, item->default_value);
        ApplyProjection();
    };
}

Value GpuSurfaceDemo::Read(const char *id) const
{
    const PropertyEditorItem *item = pe_model.Find(id);
    return item ? item->value : Value();
}

void GpuSurfaceDemo::SetSecond(bool on)
{
    if(on && !gpu_second) {
        gpu_second.Create();
        pnl_preview.Add(*gpu_second);
    }
    if(!on && gpu_second) {
        gpu_second->Remove();
        gpu_second.Clear();
    }
    lbl_second.Show(on);
}

void GpuSurfaceDemo::ApplyProjection()
{
    String type = AsString(Read("kind"));
    int kind = type == "Balls" ? 1 : type == "Rectangles" ? 2 :
               type == "Oblongs" ? 3 : type == "Rounded squares" ? 4 : 0;
    SetSecond((bool)Read("second"));
    gpu_first.Configure((int)Read("count"), kind, (int)Read("speed"),
                        (bool)Read("animate_a"), (bool)Read("grid"), (bool)Read("text"), (bool)Read("antialias"),
                        Color(Read("background")));
    if(gpu_second)
        gpu_second->Configure((int)Read("count"), kind, (int)Read("speed"),
                              (bool)Read("animate_b"), (bool)Read("grid"), (bool)Read("text"), (bool)Read("antialias"),
                              Color(Read("background")));
    edit_code.SetData(GenerateUsage());
    RefreshLayout();
    UpdateStatus();
}

void GpuSurfaceDemo::ApplyTheme()
{
    UiTitleCard::Style title_style = UiTheme::ResolveTitleCard(UiRole::Accent);
    title_style.title_line = false;
    tc_header.SetCustomStyle(title_style);
    btn_randomize.SetCustomStyle(UiTheme::ResolveButton(UiRole::Standard));
    btn_reset.SetCustomStyle(UiTheme::ResolveButton(UiRole::Standard));
    btn_theme.SetCustomStyle(UiTheme::ResolveToolButton(UiRole::Standard));
    btn_help.SetCustomStyle(UiTheme::ResolveToolButton(UiRole::Standard));
    btn_inspector.SetCustomStyle(UiTheme::ResolveToolButton(UiRole::Standard));
    btn_code.SetCustomStyle(UiTheme::ResolveToolButton(UiRole::Standard));
    pnl_preview.SetCustomStyle(UiTheme::ResolvePanel(UiPanelRole::Surface));
    pnl_rail.SetCustomStyle(UiTheme::ResolvePanel(UiPanelRole::Subtle));
    pnl_inspector.SetCustomStyle(UiTheme::ResolvePanel(UiPanelRole::Subtle));
    pnl_code.SetCustomStyle(UiTheme::ResolvePanel(UiPanelRole::Subtle));
    lbl_first.SetCustomStyle(UiTheme::ResolveLabel(UiLabelRole::Caption));
    lbl_second.SetCustomStyle(UiTheme::ResolveLabel(UiLabelRole::Caption));
    lbl_status.SetCustomStyle(UiTheme::ResolveLabel(UiLabelRole::Caption));
    btn_exit.SetCustomStyle(UiTheme::ResolveToolButton(UiRole::Alert));
    pe_inspector.SetPaletteMode(UiTheme::GetContext().mode == UiThemeMode::Dark ?
                               PropertyEditorPaletteMode::Dark : PropertyEditorPaletteMode::Light);
    Refresh();
}

void GpuSurfaceDemo::Paint(Draw& w)
{
    bool dark = UiTheme::GetContext().mode == UiThemeMode::Dark;
    w.DrawRect(GetSize(), dark ? Color(30, 32, 36) : Color(246, 247, 249));
}

void GpuSurfaceDemo::SelectPage(int page)
{
    stk_pages.SetActivePage(page);
    btn_inspector.SetChecked(page == 0);
    btn_code.SetChecked(page == 1);
}

void GpuSurfaceDemo::Layout()
{
    Size sz = GetSize();
    int pad = DPI(12), gap = DPI(12);
    int width = max(0, sz.cx - 2 * pad);
    tc_header.SetRect(pad, pad, width, DPI(72));
    int top = pad + DPI(84), body_h = max(0, sz.cy - top - pad);
    int rail = min(width, DPI(380));
    int preview = max(0, width - rail - gap);
    pnl_preview.SetRect(pad, top, preview, body_h);
    pnl_rail.SetRect(pad + preview + gap, top, rail, body_h);
    int available_h = max(0, body_h - DPI(112));
    int cell = gpu_second ? max(0, (preview - DPI(36)) / 2) : max(0, preview - DPI(24));
    int w = min(DPI((int)Read("width")), cell);
    int h = min(DPI((int)Read("height")), available_h);
    int y = DPI(32) + max(0, (available_h - h) / 2);
    int x = gpu_second ? DPI(12) + (cell - w) / 2 : (preview - w) / 2;
    gpu_first.SetRect(x, y, w, h);
    lbl_first.SetRect(x, max(0, y - DPI(30)), w, DPI(26));
    if(gpu_second) {
        int x2 = DPI(24) + cell + (cell - w) / 2;
        gpu_second->SetRect(x2, y, w, h);
        lbl_second.SetRect(x2, max(0, y - DPI(30)), w, DPI(26));
    }
    lbl_status.SetRect(DPI(10), max(0, body_h - DPI(66)), max(0, preview - DPI(20)), DPI(54));
    box_modes.SetRect(0, 0, rail, DPI(42));
    stk_pages.SetRect(DPI(6), DPI(48), max(0, rail - DPI(12)), max(0, body_h - DPI(54)));
}

void GpuSurfaceDemo::UpdateStatus()
{
    String status;
    if(!gpu_first.GetGpuError().IsEmpty())
        status = "Surface A: " + gpu_first.GetGpuError();
    else if(gpu_second && !gpu_second->GetGpuError().IsEmpty())
        status = "Surface B: " + gpu_second->GetGpuError();
    else if(gpu_first.IsGpuReady() && (!gpu_second || gpu_second->IsGpuReady()))
        status = gpu_second ? "Vulkan ready — two independent surfaces" : "Vulkan ready — one embedded surface";
    else
        status = "Opening Vulkan surfaces...";
    if(status != last_status) {
        last_status = status;
        lbl_status.SetText(status);
    }
}

String GpuSurfaceDemo::GenerateUsage() const
{
    Color bg(Read("background"));
    // Deliberately a minimal public-API example, not a serialized animation engine.
    return Format(
        "#include <GpuRender/GpuRender.h>\n"
        "using namespace Upp;\n\n"
        "class PreviewWindow : public TopWindow {\n"
        "public:\n"
        "    PreviewWindow() {\n"
        "        Title(\"Embedded Vulkan surface\").Sizeable();\n"
        "        SetRect(0, 0, DPI(800), DPI(600));\n"
        "        Add(gpu.LeftPos(DPI(24), DPI(%d)).TopPos(DPI(24), DPI(%d)));\n"
        "        gpu.SetGpuPaint([](GpuPainter& w) {\n"
        "            w.Clear(Color(%d, %d, %d));\n"
        "            Size sz = w.GetSize();\n"
        "            w.FillRect(Rectf(12, 12, sz.cx / 2.0, sz.cy / 2.0), Color(96, 165, 250));\n"
        "            w.DrawText(Pointf(12, 40), String(\"Vulkan surface\"), SansSerif(14), White());\n"
        "        });\n"
        "        // Call gpu.RequestGpuRefresh() after changing your scene state.\n"
        "    }\n"
        "private:\n"
        "    GpuCtrl gpu;\n"
        "};\n\n"
        "GUI_APP_MAIN { PreviewWindow().Run(); }\n",
        (int)Read("width"), (int)Read("height"), bg.GetR(), bg.GetG(), bg.GetB());
}

bool GpuSurfaceDemo::RunSmoke()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *message) {
        ++checks;
        if(!ok) { ++failures; Cout() << "FAIL: " << message << EOL; }
    };
    auto pump = [&](int ms) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        // GuiSleep can return early for a queued message; iteration count is not elapsed time.
        while(std::chrono::steady_clock::now() < end) { Ctrl::ProcessEvents(); Ctrl::GuiSleep(5); }
    };
    Open();
    for(int i = 0; i < 600 && !(gpu_first.IsGpuReady() && gpu_second && gpu_second->IsGpuReady()); ++i)
        pump(5);
    pump(150);
    check(gpu_first.IsGpuReady() && gpu_first.GetGpuError().IsEmpty(), "surface A ready");
    check(gpu_second && gpu_second->IsGpuReady() && gpu_second->GetGpuError().IsEmpty(), "surface B ready");
    check(gpu_first.GetPaintCount() > 0 && gpu_second && gpu_second->GetPaintCount() > 0, "both surfaces painted");
    check(gpu_first.IsAntialiasing() && gpu_first.HasCoveragePixels(), "AA default has fractional edge coverage");
    pe_model.SetValue("antialias", false);
    ApplyProjection();
    pump(100);
    check(!gpu_first.IsAntialiasing() && gpu_first.GetGpuError().IsEmpty(), "direct geometry mode presents");
    pe_model.SetValue("antialias", true);
    ApplyProjection();
    pump(100);
    check(gpu_first.IsAntialiasing() && gpu_first.GetGpuError().IsEmpty(), "cached AA mode presents after toggle");
    pe_model.SetValue("animate_a", false);
    ApplyProjection();
    pump(50);
    int a = gpu_first.GetTickCountForTest();
    int b = gpu_second ? gpu_second->GetTickCountForTest() : 0;
    pump(200);
    check(gpu_first.GetTickCountForTest() == a, "paused A stops its timer");
    if(gpu_second && gpu_second->GetTickCountForTest() <= b)
        Cout() << "B timer diagnostic: before=" << b << " after=" << gpu_second->GetTickCountForTest()
               << " visible=" << gpu_second->IsVisible() << " open=" << gpu_second->IsOpen()
               << " ready=" << gpu_second->IsGpuReady() << EOL;
    check(gpu_second && gpu_second->GetTickCountForTest() > b, "B animates while A is paused");
    pe_model.SetValue("count", 80);
    pe_model.SetValue("kind", "Oblongs");
    pe_model.SetValue("width", 180);
    pe_model.SetValue("height", 140);
    ApplyProjection();
    pump(100);
    check(gpu_first.GetItemCount() == 80 && gpu_second && gpu_second->GetItemCount() == 80, "count projection");
    check(gpu_first.GetSize() == Size(DPI(180), DPI(140)), "independent X/Y sizing");
    gpu_first.Randomize();
    check(gpu_first.GetItemCount() == 80, "randomize preserves requested count");
    pe_model.SetValue("second", false);
    ApplyProjection();
    int paints = gpu_first.GetPaintCount();
    gpu_first.RequestGpuRefresh();
    pump(100);
    check(!gpu_second && gpu_first.IsGpuReady() && gpu_first.GetGpuError().IsEmpty() &&
          gpu_first.GetPaintCount() > paints, "A survives B destruction");
    pe_model.SetValue("second", true);
    ApplyProjection();
    pump(200);
    check(gpu_second && gpu_second->IsGpuReady() && gpu_second->GetGpuError().IsEmpty(), "B recreated");
    UiThemeContext c = UiTheme::GetContext();
    c.mode = UiThemeMode::Dark;
    UiTheme::Set(c);
    ApplyTheme();
    SelectPage(1);
    pump(100);
    check(gpu_first.IsGpuReady() && gpu_first.GetGpuError().IsEmpty(), "theme and page switching preserve A");
    SelectPage(0);
    pe_model.SetValue("animate_b", false);
    ApplyProjection();
    pump(50);
    int stopped = gpu_second ? gpu_second->GetTickCountForTest() : 0;
    pump(100);
    check(gpu_second && gpu_second->GetTickCountForTest() == stopped, "paused B stops its timer");
    Close();
    pump(50);
    check(!gpu_first.IsGpuReady() && (!gpu_second || !gpu_second->IsGpuReady()), "close releases presenter readiness");
    Cout() << Format("GpuSurfaceDemo smoke: %d checks / %d failures", checks, failures) << EOL;
    return failures == 0;
}

}
