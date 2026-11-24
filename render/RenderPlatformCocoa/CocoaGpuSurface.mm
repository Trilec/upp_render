#include <CtrlCore/CocoMM.h>
#include "CocoaGpuSurface.h"
#include <cmath>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CAShapeLayer.h>
#import <QuartzCore/CATransaction.h>

// Input stays in the Cocoa top view, where U++ hit-tests its logical Ctrl tree.
@interface UppRenderMetalView : NSView {
@public
	Upp::CocoaGpuSurface *surface;
}
@end

@implementation UppRenderMetalView
- (BOOL)isFlipped { return YES; }
- (NSView *)hitTest:(NSPoint)point { return nil; }
- (CALayer *)makeBackingLayer { return [CAMetalLayer layer]; }
- (void)viewDidChangeBackingProperties {
	[super viewDidChangeBackingProperties];
	if(surface) surface->SyncGeometry();
}
@end

namespace Upp {

CocoaGpuSurface::CocoaGpuSurface()
{
	NoWantFocus();
	Transparent();
}

CocoaGpuSurface::~CocoaGpuSurface()
{
	CloseNative();
}

void CocoaGpuSurface::OpenNative()
{
	Ctrl *top = GetTopCtrl();
	NSView *parent = top ? (NSView *)top->GetNSView() : nil;
	if(!parent) {
		error = "GPU surface requires an open Cocoa top view";
		return;
	}
	UppRenderMetalView *view = [[UppRenderMetalView alloc] initWithFrame:NSZeroRect];
	[view setWantsLayer:YES];
	[parent addSubview:view];
	native = view;
	view->surface = this;
	error.Clear();
	SyncGeometry();
}

void CocoaGpuSurface::CloseNative()
{
	clock.Kill();
	paint.Kill();
	pending = false;
	UppRenderMetalView *view = (UppRenderMetalView *)native;
	if(view) {
		view->surface = nullptr;
		[view removeFromSuperview];
		[view release];
	}
	native = nullptr;
	mask_dirty = true;
	drawable = Size(0, 0);
	last_frame = last_clip = Null;
	last_unit = last_scale = 0;
}

bool CocoaGpuSurface::IsNativeHostReady() const
{
	return native && [(NSView *)native window];
}

bool CocoaGpuSurface::Describe(GpuNativeWindowDesc& desc, String& message) const
{
	if(!IsNativeHostReady()) {
		message = "GPU surface has no Cocoa window";
		return false;
	}
	desc.kind = GpuNativeWindowKind::MetalLayer;
	desc.handle = reinterpret_cast<uintptr_t>([(NSView *)native layer]);
	return true;
}

void CocoaGpuSurface::SetOverlayRects(const Vector<Rect>& rects)
{
	if(overlays == rects) return;
	overlays = clone(rects);
	mask_dirty = true;
	SyncGeometry();
}

void CocoaGpuSurface::SyncGeometry()
{
	if(!IsNativeHostReady()) return;
	NSView *view = (NSView *)native;
	// U++ scales Ctrl coordinates by DPI(1). Cocoa sizes the drawable in
	// backing pixels, which can change independently when moving displays.
	const double unit = 1.0 / DPI(1);
	const double scale = [[view window] backingScaleFactor];
	const Rect bounds = GetScreenView();
	const Rect frame = bounds - GetTopCtrl()->GetScreenRect().TopLeft();
	const Rect clip = IsVisible() ? (GetVisibleScreenRect() & bounds) - bounds.TopLeft() : Rect(0, 0, 0, 0);
	if(!mask_dirty && frame == last_frame && clip == last_clip && unit == last_unit && scale == last_scale)
		return;
	mask_dirty = false;
	last_frame = frame;
	last_clip = clip;
	last_unit = unit;
	last_scale = scale;
	CAMetalLayer *layer = (CAMetalLayer *)[view layer];
	[CATransaction begin];
	[CATransaction setDisableActions:YES];
	[view setHidden:clip.IsEmpty()];
	[view setFrame:NSMakeRect(frame.left * unit, frame.top * unit, frame.Width() * unit, frame.Height() * unit)];
	drawable = Size(int(std::lround(frame.Width() * unit * scale)), int(std::lround(frame.Height() * unit * scale)));
	[layer setContentsScale:scale];
	if(!drawable.IsEmpty()) [layer setDrawableSize:CGSizeMake(drawable.cx, drawable.cy)];
	// Cut out ordinary Ui controls so their pixels remain visible underneath.
	Vector<Rect> visible;
	if(!clip.IsEmpty()) visible.Add(clip);
	for(const Rect& overlay : overlays) Subtract(visible, overlay);
	CGMutablePathRef path = CGPathCreateMutable();
	for(const Rect& tile : visible) {
		double y = [layer isGeometryFlipped] ? tile.top : frame.Height() - tile.bottom;
		CGPathAddRect(path, nullptr, CGRectMake(tile.left * unit, y * unit, tile.Width() * unit, tile.Height() * unit));
	}
	CAShapeLayer *mask = [CAShapeLayer layer];
	mask.frame = layer.bounds;
	mask.path = path;
	layer.mask = mask;
	CGPathRelease(path);
	[CATransaction commit];
	RequestNativeRefresh();
}

void CocoaGpuSurface::RequestNativeRefresh()
{
	if(pending || !IsNativeHostReady() || !IsVisible() || last_clip.IsEmpty()) return;
	pending = true;
	paint.Post([this] {
		pending = false;
		if(IsNativeHostReady() && IsVisible() && !last_clip.IsEmpty()) WhenPaint();
	});
}

bool CocoaGpuSurface::SetFrameRate(int fps)
{
	clock.Kill();
	if(fps) clock.Set(-max(1, 1000 / fps), [this] { RequestNativeRefresh(); });
	return true;
}

void CocoaGpuSurface::State(int reason)
{
	Ctrl::State(reason);
	if(reason == CLOSE) {
		WhenState(reason);
		CloseNative();
		return;
	}
	if(reason == OPEN && !native) OpenNative();
	SyncGeometry();
	WhenState(reason);
}

void CocoaGpuSurface::Layout()
{
	Ctrl::Layout();
	SyncGeometry();
}

Image CocoaGpuSurface::MouseEvent(int event, Point pt, int delta, dword flags)
{
	Ctrl *owner = GetParent();
	if(!owner) return Ctrl::MouseEvent(event, pt, delta, flags);
	return owner->MouseEvent(event, pt + (GetScreenView().TopLeft() - owner->GetScreenView().TopLeft()), delta, flags);
}

}
