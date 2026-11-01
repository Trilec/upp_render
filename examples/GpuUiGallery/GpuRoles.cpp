#include "GpuRoles.h"
#include <Shared/UiTheme/CineSpaceTheme.h>
#include <Ui/UiIcons.h>

namespace Upp {
namespace {
using namespace CineSpace;
const UiRole roles[]={UiRole::Standard,UiRole::Subtle,UiRole::Accent,UiRole::Alert};
const char* roleNames[]={"Standard","Subtle","Accent","Alert"};
const char* profileNames[]={"Olive","Legacy / slate + mint","CineCast R9","CineWrite R4.1","CineFlow v30","CineStage R3"};
const ThemeVisualProfile profiles[]={ThemeVisualProfile::Legacy,ThemeVisualProfile::CineCastR9,
    ThemeVisualProfile::CineWriteR41,ThemeVisualProfile::CineFlowV30,ThemeVisualProfile::CineStageR3};
String ProfileTitle(ThemeVisualProfile profile,UiThemeMode mode)
{
    int index=0; for(int i=0;i<5;++i) if(profiles[i]==profile) index=i;
    return String(mode==UiThemeMode::Dark?"DARK / ":"LIGHT / ")+profileNames[index+1];
}
String OliveTitle(UiThemeMode mode) { return String(mode==UiThemeMode::Dark?"DARK / ":"LIGHT / ")+"Olive"; }
void Place(Ctrl& c,int x,int y,int w,int h) { c.SetRect(DPI(x),DPI(y),DPI(w),DPI(h)); }

class UIStatusStrip : public Ctrl {
    const CineSpaceTheme* theme=nullptr;
public:
    void Apply(const CineSpaceTheme& t) { theme=&t; Refresh(); }
    void Paint(Draw& w) override
    {
        if(!theme) return;
        const char* texts[]={"Neutral","Ready","Warning","Error","Pending"};
        int x=DPI(2);
        for(int i=0;i<5;i++) {
            UiTagData d(texts[i],UiRole::Standard,UiTagVariant::Filled);
            auto s=theme->Tag((ThemeStatus)i); Size sz=UiMeasureTag(d,s);
            auto p=UiPrepareTag(d,s,RectC(x,DPI(2),sz.cx,sz.cy));
            UiPaintTag(w,p,IsEnabled()?ST_NORMAL:ST_DISABLED); x+=sz.cx+DPI(6);
        }
    }
};

class UIPreviewPane : public ParentCtrl {
public:
    CineSpaceTheme theme;
    UiLabel title, explanation, commandLabel, fieldLabel, toggleLabel, tabLabel, metricLabel, statusLabel, events;
    UiButton commands[4], selected, disabled, menuButton;
    UiToolButton icons[4];
    UiLineEdit edit, invalid, readonly;
    UiDropdown dropdown;
    UiToggle toggle;
    UiCheckBox check, mixed;
    UiRadioButton radioA, radioB;
    UiMenu menu;
    UiTab tabs;
    ParentCtrl tabA,tabB;
    UiLabel tabTextA,tabTextB;
    UiSlider slider;
    UiProgressBar progress;
    UiRangeSegments segments;
    UIStatusStrip chips;
    UiRole currentRole=UiRole::Standard;
    int actionCount=0;
    bool forceInvalid=true;
    bool current_olive=false;
    int content_height=DPI(810);
    int ContentHeight() const { return content_height; }
    int LayoutChoices(int top,int inner)
    {
        Ctrl* controls[]={&toggle,&check,&mixed,&radioA,&radioB};
        const int preferred[]={48,102,92,92,94};
        int left=DPI(16),right=left+DPI(inner),x=left,y=DPI(top),row_height=0;
        for(int i=0;i<5;++i) {
            Size minimum=controls[i]->GetMinSize();
            int width=max(DPI(preferred[i]),minimum.cx),height=max(DPI(28),minimum.cy);
            if(x>left&&x+width>right) { x=left;y+=row_height+DPI(8);row_height=0; }
            controls[i]->SetRect(x,y,width,height);
            x+=width+DPI(8);row_height=max(row_height,height);
        }
        // Return a logical bottom rounded up; Place converts it exactly once.
        return ((y+row_height)*100+DPI(100)-1)/DPI(100);
    }

    UIPreviewPane(UiThemeMode mode)
    {
        theme.SetMode(mode);
        Add(title); Add(explanation); Add(commandLabel); Add(fieldLabel); Add(toggleLabel);
        Add(tabLabel); Add(metricLabel); Add(statusLabel); Add(events);
        for(int i=0;i<4;i++) {
            Add(commands[i]); commands[i].SetText(roleNames[i]);
            commands[i].WhenAction=[this,i] { Record(String(roleNames[i])+" command"); };
            Add(icons[i]); icons[i].SetIcon(ICON_DESIGN_SETTINGS_48()).SetIconRenderMode(UiIconRenderMode::MonoTint).SetIconSize(DPI(16),DPI(16));
            icons[i].Tip(String(roleNames[i])+" icon action");
            icons[i].WhenAction=[this,i] { Record(String(roleNames[i])+" icon"); };
        }
        Add(selected); selected.SetText("Selected").SetCheckable().SetChecked();
        selected.WhenAction=[this] { ApplyRole(currentRole); Record("Selection toggled"); };
        Add(disabled); disabled.SetText("Disabled"); disabled.Disable();
        Add(menuButton); menuButton.SetText("Open menu").SetIcon(ICON_DESIGN_FOLDER_48()).SetIconRenderMode(UiIconRenderMode::MonoTint).SetIconSize(DPI(16),DPI(16));
        menuButton.WhenAction=[this] { menu.PopUp(&menuButton,menuButton.GetScreenRect().BottomLeft()); };
        UiMenuItem command("Record action","record"); command.icon=ICON_DESIGN_CHECK_SMALL_48();
        command.icon_render_mode=UiIconRenderMode::MonoTint;
        auto& model=menu.Model(); model.AddChild(model.Root(),command);
        UiMenuItem opt("Checked item","check"); opt.checkable=true; opt.checked=true;
        model.AddChild(model.Root(),opt);
        auto sub=model.AddChild(model.Root(),UiMenuItem("Submenu"));
        model.AddChild(sub,UiMenuItem("Nested action","nested"));
        model.AddChild(model.Root(),UiMenuItem("Unavailable command","disabled",false));
        menu.WhenAction=[this](UiMenuNodeRef,const UiMenuItem& item) { Record(item.text); };
        Add(edit); edit.SetTextUtf8("Keep this draft while switching theme");
        Add(invalid); invalid.SetTextUtf8(""); invalid.SetPlaceholder("Required name");
        invalid.WhenChange=[this] { forceInvalid=TrimBoth(invalid.GetTextUtf8()).IsEmpty(); ApplyRole(currentRole); };
        Add(readonly); readonly.SetTextUtf8("Read-only identity: Version 004"); readonly.SetReadOnly();
        Add(dropdown);
        for(int i=0;i<18;i++) dropdown.Add(Format("Option %02d",i+1),i,i!=4);
        dropdown.Select(2).SetPopupMaxItems(8).ShowDragHandle(false);
        dropdown.WhenSelect=[this](int) { Record("Dropdown selection"); };
        Add(toggle); toggle.SetOn(true);
        toggle.WhenAction=[this] { ApplyRole(currentRole); Record("Toggle"); };
        Add(check); check.SetText("Checked").SetChecked();
        Add(mixed); mixed.SetText("Mixed").SetTriState().SetState(UICHECK_INDETERMINATE);
        Add(radioA); radioA.SetText("Choice A").SetChecked();
        Add(radioB); radioB.SetText("Choice B");
        radioA.WhenAction=[this] { radioB.SetChecked(false); Record("Choice A"); };
        radioB.WhenAction=[this] { radioA.SetChecked(false); Record("Choice B"); };
        Add(tabs); tabs.Add(tabA,"Preview"); tabs.Add(tabB,"Details");
        tabA.Add(tabTextA.SizePos()); tabTextA.SetText("Selected tab retains this content.");
        tabB.Add(tabTextB.SizePos()); tabTextB.SetText("Tab change is local; model state is retained.");
        Add(slider); slider.SetRange(0,100).SetValue(64).ExpandTrack();
        Add(progress); progress.Set(64,100).Percent();
        slider.WhenChanging=[this] { progress.Set((int)slider.GetValue(),100); };
        Add(segments); Vector<UiRangeSegment> spans;
        spans.Add(UiRangeSegment(25,"Setup")); spans.Add(UiRangeSegment(45,"Development"));
        spans.Add(UiRangeSegment(30,"Resolution")); segments.SetRange(0,100).SetSegments(spans);
        segments.ShowLabels().ShowSegmentValues().SetValueDisplay(UiRangeSegments::ValueDisplay::Percent);
        segments.WhenAction=[this] { Record("Range boundary"); };
        Add(chips);
        title.SetText(mode==UiThemeMode::Dark?"DARK / slate + mint":"LIGHT / companion proposal");
        explanation.SetText("Live native controls. Hover, press, Tab, open menus, edit and resize.");
        commandLabel.SetText("ROLE EMPHASIS  /  ordinary commands and icon actions");
        fieldLabel.SetText("FIELDS  /  editable, validation, read-only, real popup");
        toggleLabel.SetText("CHOICE  /  selection and checked state are separate from role");
        tabLabel.SetText("TABS  /  selected page and focus");
        metricLabel.SetText("VALUES  /  slider, progress and data series");
        statusLabel.SetText("STATUS  /  text + semantic colour, independent of role");
        ApplyRole(currentRole); Record("Ready");
    }
    void SetSamplesEnabled(bool on)
    {
        Ctrl* controls[]={&commands[0],&commands[1],&commands[2],&commands[3],
            &icons[0],&icons[1],&icons[2],&icons[3],&selected,&disabled,&menuButton,
            &edit,&invalid,&readonly,&dropdown,&toggle,&check,&mixed,&radioA,&radioB,
            &tabs,&slider,&progress,&segments,&chips};
        for(auto* c:controls) c->Enable(on);
        disabled.Disable();
    }
    void Record(const String& s) { events.SetText(Format("%d actions / ",actionCount++)+s); }
    void ApplyRole(UiRole role)
    {
        currentRole=role;
        selected.SetText(selected.IsChecked()?"Selected":"Not selected");
        title.SetText(current_olive?OliveTitle(theme.GetMode()):ProfileTitle(theme.GetVisualProfile(),theme.GetMode()));
        fieldLabel.SetText(forceInvalid?"FIELDS / name is required (validation error)":"FIELDS / required name accepted");
        UiLabel* labels[]={&title,&explanation,&commandLabel,&fieldLabel,&toggleLabel,&tabLabel,&metricLabel,&statusLabel,&events,&tabTextA,&tabTextB};
        for(auto* l:labels) l->SetCustomStyle(theme.Label());
        fieldLabel.SetCustomStyle(theme.Label(forceInvalid?UiRole::Alert:UiRole::Standard));
        if(theme.GetVisualProfile()==ThemeVisualProfile::CineFlowV30) {
            auto section=theme.Label();section.font=theme.Tokens().caption;
            for(UiLabel* label:{&commandLabel,&toggleLabel,&tabLabel,&metricLabel,&statusLabel})
                label->SetCustomStyle(section);
            section=theme.Label(forceInvalid?UiRole::Alert:UiRole::Standard);
            section.font=theme.Tokens().caption;fieldLabel.SetCustomStyle(section);
        }
        auto heading=theme.Label(); heading.font=theme.Tokens().title; title.SetCustomStyle(heading);
        auto caption=theme.Label(UiRole::Subtle); caption.font=theme.Tokens().caption;
        explanation.SetCustomStyle(caption); events.SetCustomStyle(caption);
        for(int i=0;i<4;i++) { commands[i].SetCustomStyle(theme.Button(roles[i]));
            icons[i].SetCustomStyle(theme.ToolButton(roles[i])); }
        selected.SetCustomStyle(theme.Button(role,selected.IsChecked()));
        disabled.SetCustomStyle(theme.Button(role));
        menuButton.SetCustomStyle(theme.Button(role)); menu.SetCustomStyle(theme.Menu(role));
        edit.SetCustomStyle(theme.Edit(role)); invalid.SetCustomStyle(theme.Edit(role,forceInvalid));
        readonly.SetCustomStyle(theme.Edit(UiRole::Subtle));
        dropdown.SetCustomStyle(theme.Dropdown(role));
        toggle.SetCustomStyle(theme.Toggle(role,(bool)toggle.GetData()));
        check.SetCustomStyle(theme.CheckBox(role)); mixed.SetCustomStyle(theme.CheckBox(role));
        radioA.SetCustomStyle(theme.RadioButton(role)); radioB.SetCustomStyle(theme.RadioButton(role));
        tabs.SetCustomStyle(theme.Tab(UITAB_UNDERLINE,role)); slider.SetCustomStyle(theme.Slider(role));
        progress.SetCustomStyle(theme.ProgressBar(role)); segments.SetCustomStyle(theme.RangeSegments(role));
        chips.Apply(theme); Refresh();
    }
    void Paint(Draw& w) override { w.DrawRect(GetSize(),theme.Tokens().panel); }
    void Layout() override
    {
        const int width=GetSize().cx*100/DPI(100), inner=max(440,width-32), col=(inner-18)/4;
        auto logical=[](int px) { return (px*100+DPI(100)-1)/DPI(100); };
        int button=28;
        for(auto& c:commands) button=max(button,logical(c.GetMinSize().cy));
        for(auto& c:icons) button=max(button,logical(c.GetMinSize().cy));
        for(UiButton* c:{&selected,&disabled,&menuButton}) button=max(button,logical(c->GetMinSize().cy));
        int field=28;
        for(UiBaseEdit* c:{&edit,&invalid,&readonly}) field=max(field,logical(c->GetMinSize().cy));
        field=max(field,logical(dropdown.GetMinSize().cy));
        int y=12;
        Place(title,16,y,inner,30); y+=34;
        Place(explanation,16,y,inner,20); y+=28;
        Place(commandLabel,16,y,inner,24); y+=26;
        for(int i=0;i<4;++i) Place(commands[i],16+i*(col+6),y,col,button);
        y+=button+6;
        for(int i=0;i<4;++i) Place(icons[i],16+i*(col+6),y,col,button);
        y+=button+8;
        Place(selected,16,y,col,button); Place(disabled,22+col,y,col,button);
        Place(menuButton,28+col*2,y,col*2+6,button); y+=button+12;
        Place(fieldLabel,16,y,inner,24); y+=26;
        Place(edit,16,y,inner,field); y+=field+6;
        Place(invalid,16,y,inner/2-4,field); Place(readonly,20+inner/2,y,inner/2-4,field); y+=field+6;
        Place(dropdown,16,y,inner,field); y+=field+12;
        Place(toggleLabel,16,y,inner,24); y+=26;
        y=LayoutChoices(y,inner)+12;
        Place(tabLabel,16,y,inner,24); y+=26;
        Place(tabs,16,y,inner,84); y+=96;
        Place(metricLabel,16,y,inner,24); y+=26;
        const int valueHeight=max(32,max(logical(slider.GetMinSize().cy),logical(progress.GetMinSize().cy)));
        Place(slider,16,y,inner/2-8,valueHeight);
        Place(progress,24+inner/2,y,inner/2-8,valueHeight); y+=valueHeight+10;
        const int rangeHeight=max(70,logical(segments.GetMinSize().cy));
        Place(segments,16,y,inner,rangeHeight); y+=rangeHeight+12;
        Place(statusLabel,16,y,inner,24); y+=26;
        Place(chips,16,y,inner,28); y+=36;
        Place(events,16,y,inner,24);
        content_height=DPI(y+32);
    }
};

class RolesScroll : public UiScrollPanel {
public:
    explicit RolesScroll(UIPreviewPane& pane) : pane_(pane)
    {
        SetScrollMode(UIPANELSCROLL_AUTO);
        Content().Add(pane_);
    }
    void Layout() override
    {
        for(int pass=0;pass<3;++pass) {
            UiScrollPanel::Layout();
            const Rect view=GetViewportRect();
            pane_.SetRect(0,0,max(DPI(480),view.GetWidth()),max(view.GetHeight(),pane_.ContentHeight()));
            pane_.Layout();
            if(pane_.GetSize().cy==max(view.GetHeight(),pane_.ContentHeight())) break;
        }
        UiScrollPanel::Layout();
    }
private:
    UIPreviewPane& pane_; // Owned by the enclosing Impl and outlives this host.
};

bool PaneFieldsFit(Ctrl& root,String& report)
{
    bool ok=true;
    auto visit=[&](auto&& self,Ctrl& c)->void {
        if(dynamic_cast<UiBaseEdit*>(&c)||dynamic_cast<UiDropdown*>(&c)||dynamic_cast<UiToggle*>(&c)) {
            const Size minimum=c.GetMinSize(),actual=c.GetSize();
            if(actual.cy+1<minimum.cy) {
                report<<"roles_clipped_field size="<<actual<<" minimum="<<minimum<<"\n"; ok=false;
            }
        }
        for(Ctrl* child=c.GetFirstChild();child;child=child->GetNext()) self(self,*child);
    };
    visit(visit,root);
    return ok;
}
}

struct GpuRolesPage::Impl {
    GpuRolesPage& owner;
    UiLabel profile_label;
    UiDropdown profile;
    UiButton role_buttons[4];
    UIPreviewPane dark{UiThemeMode::Dark},light{UiThemeMode::Light};
    RolesScroll dark_scroll{dark},light_scroll{light};
    int active_profile=0,active_role=0;

    explicit Impl(GpuRolesPage& page) : owner(page)
    {
        profile_label.SetText("Theme profile");
        owner.Add(profile_label); owner.Add(profile);
        for(int i=0;i<6;++i) profile.Add(profileNames[i],i);
        profile.Select(0);
        profile.WhenSelect=[this](int index) { active_profile=index; Apply(); };
        for(int i=0;i<4;++i) {
            owner.Add(role_buttons[i]);
            role_buttons[i].SetText(roleNames[i]).SetCheckable();
            role_buttons[i].WhenAction=[this,i] { active_role=i; Apply(); };
        }
        owner.Add(dark_scroll); owner.Add(light_scroll);
        Apply();
    }
    void Apply()
    {
        for(UIPreviewPane* pane:{&dark,&light}) {
            pane->current_olive=active_profile==0;
            pane->theme.UseCurrentOlive(active_profile==0);
            if(active_profile) pane->theme.SetVisualProfile(profiles[active_profile-1]);
            pane->ApplyRole(roles[active_role]);
        }
        dark.theme.Apply(dark_scroll); light.theme.Apply(light_scroll);
        for(int i=0;i<4;++i) {
            role_buttons[i].SetCustomStyle(UiTheme::ResolveButton(roles[i]));
            role_buttons[i].SetChecked(i==active_role);
        }
        owner.Layout(); owner.Refresh();
    }
    void Layout()
    {
        const Size size=owner.GetSize(); const int pad=DPI(8),gap=DPI(10);
        const int header=max(DPI(34),profile.GetMinSize().cy);
        profile_label.SetRect(pad,pad,DPI(94),header);
        profile.SetRect(pad+DPI(98),pad,DPI(230),header);
        const int roleWidth=max(DPI(72),min(DPI(100),(size.cx-DPI(370)-3*gap)/4));
        for(int i=0;i<4;++i) role_buttons[i].SetRect(DPI(350)+i*(roleWidth+gap),pad,roleWidth,header);
        const int top=2*pad+header,width=max(0,(size.cx-2*pad-gap)/2),height=max(0,size.cy-top-pad);
        dark_scroll.SetRect(pad,top,width,height);
        light_scroll.SetRect(pad+width+gap,top,width,height);
        dark_scroll.Layout(); light_scroll.Layout();
    }
};
GpuRolesPage::GpuRolesPage() { impl_.Create(*this); }
GpuRolesPage::~GpuRolesPage() {}
void GpuRolesPage::Layout() { UiPanel::Layout(); if(impl_) impl_->Layout(); }
bool GpuRolesPage::Qualify(String& report)
{
    auto& p=*impl_;
    const String saved=p.dark.edit.GetTextUtf8();
    p.dark.edit.SetTextUtf8("Retained Vulkan draft");
    bool ok=true; int cases=0;
    for(int profile=0;profile<6;++profile) for(int role=0;role<4;++role) {
        p.active_profile=profile; p.active_role=role; p.Apply();
        ok &= PaneFieldsFit(p.dark,report) && PaneFieldsFit(p.light,report);
        ok &= p.dark.edit.GetTextUtf8()=="Retained Vulkan draft" && p.dark.dropdown.GetSelection()==2;
        ok &= p.dark.disabled.IsEnabled()==false && p.dark.theme.GetMode()==UiThemeMode::Dark &&
              p.light.theme.GetMode()==UiThemeMode::Light;
        cases+=2;
    }
    p.dark.edit.SetTextUtf8(saved); p.active_profile=0; p.active_role=0; p.Apply();
    report<<"roles_states="<<(ok?"PASS":"FAIL")<<" profiles=6 roles=4 modes=2 cases="<<cases
          <<" retained_edit/selection/disabled fields_fit\n";
    return ok;
}
}
