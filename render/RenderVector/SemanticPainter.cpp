#include "RenderVector.h"
#include <cmath>

namespace Upp {

void ReplayUiDisplayList(const UiDisplayList& list, UiCanvas& target)
{
	for(int i=0;i<list.GetCount();++i) {
		ReplayUiDisplayOp(list[i],target);
	}
}

void ReplayUiDisplayOp(const UiDisplayOp& op, UiCanvas& target)
{
	switch(op.type) {
		case UiDisplayOpType::Save: target.Save(); break;
		case UiDisplayOpType::Restore: target.Restore(); break;
		case UiDisplayOpType::BlurLayer: target.BlurLayer((int)op.width); break;
		case UiDisplayOpType::ClipPath: target.ClipPath(op.path,op.fill_rule); break;
		case UiDisplayOpType::ClipRect: target.ClipRect(op.rect); break;
		case UiDisplayOpType::ConcatTransform: target.ConcatTransform(op.transform); break;
		case UiDisplayOpType::FillRect: target.FillRect(op.rect, op.color); break;
		case UiDisplayOpType::InvertRect: target.InvertRect(op.rect); break;
		case UiDisplayOpType::StrokeRect: target.StrokeRect(op.rect, op.width, op.color); break;
		case UiDisplayOpType::FillRoundedRect: target.FillRoundedRect(op.rounded, op.color); break;
		case UiDisplayOpType::DrawImage:
			target.DrawImage(op.rect, op.image, op.image_source, op.image_tint, op.image_alpha_mask); break;
		case UiDisplayOpType::DrawText: target.DrawText(op.point, op.text, op.font, op.color); break;
		case UiDisplayOpType::FillPath: target.FillPath(op.path, op.paint, op.fill_rule); break;
		case UiDisplayOpType::StrokePath: target.StrokePath(op.path, op.paint, op.stroke); break;
		case UiDisplayOpType::DrawSvg: target.DrawSvg(op.rect, op.svg); break;
	}
}

namespace {
Transform2D Neutral(const Xform2D& m) { Transform2D t; t.x=m.x; t.y=m.y; t.t=m.t; return t; }
UiGradientSpread Spread(int s) {
	return s==GRADIENT_REPEAT ? UiGradientSpread::Repeat :
	       s==GRADIENT_REFLECT ? UiGradientSpread::Reflect : UiGradientSpread::Pad;
}

// NilPainter supplies no raster allocation. Every unsupported virtual operation
// is overridden to fail; recording is transactional, never a partial fallback.
class SemanticPainter : public NilPainter, public GpuPainterEffects {
public:
	SemanticPainter(Size size) : size(size) {}
	void BlurLayer(int radius) override { builder.BlurLayer(radius); }
	void PaintAngular(Pointf centre, double start, double sweep,
	                  Color first, Color last, double width) override {
		Emit(UiPaint::Angular(centre,start,sweep,ColorValue((RGBA)first),ColorValue((RGBA)last)),width);
	}
	bool Finish(UiDisplayList& out, String& error) {
		if(!stack.IsEmpty()) Fail("unbalanced Painter Begin/End");
		if(!failure.IsEmpty()) { error=failure; return false; }
		if(!builder.Finish(out)) { error=builder.GetError(); return false; }
		error.Clear(); return true;
	}
private:
	struct State : Moveable<State> {
		Transform2D transform;
		UiStrokeStyle stroke;
		Vector<UiGradientStop> stops;
		double opacity=1;
		bool evenodd=false;
		State() = default;
		State(const State& s) : transform(s.transform), stroke(s.stroke), stops(s.stops,1),
		                       opacity(s.opacity), evenodd(s.evenodd) {}
		State& operator=(const State& s) {
			transform=s.transform; stroke=s.stroke; stops=clone(s.stops);
			opacity=s.opacity; evenodd=s.evenodd; return *this;
		}
	};
	Size size;
	UiDisplayListBuilder builder;
	State state;
	Array<State> stack;
	UiPath path;
	Pointf current=Pointf(0,0), start=Pointf(0,0), quadratic=Pointf(0,0), cubic=Pointf(0,0);
	bool painted=false, has_drawn=false;
	String failure;
	void Fail(const char *why) { if(failure.IsEmpty()) failure=why; }
	bool Prepare() {
		if(!failure.IsEmpty()) return false;
		if(painted) { path.Clear(); current=start=quadratic=cubic=Pointf(0,0); painted=false; }
		if(path.GetCount()>=8192) { Fail("Painter path command budget exceeded"); return false; }
		return true;
	}
	Pointf Point(const Pointf& p, bool rel) {
		return Pointf(IsNull(p.x)?current.x:p.x+(rel?current.x:0),
		              IsNull(p.y)?current.y:p.y+(rel?current.y:0));
	}
	Rgba8 ColorValue(RGBA c) {
		// Painter RGBA is premultiplied; neutral UiPaint uses straight colour.
		auto straight = [&](byte v) -> byte {
			return c.a ? (byte)min(255, ((int)v * 255 + c.a / 2) / c.a) : 0;
		};
		return Rgba8(straight(c.r),straight(c.g),straight(c.b),
		             (byte)minmax((int)floor(c.a*state.opacity+.5),0,255));
	}
	UiPaint Gradient(Pointf a, RGBA ca, Pointf b, RGBA cb, double r, int style, bool radial) {
		UiPaint p = radial ? UiPaint::Radial(a,b,r,ColorValue(ca),ColorValue(cb),Spread(style)) :
		                     UiPaint::Linear(a,b,ColorValue(ca),ColorValue(cb),Spread(style));
		for(const auto& s:state.stops) {
			Rgba8 c=s.color; c.a=(byte)minmax((int)floor(c.a*state.opacity+.5),0,255);
			p.AddStop(s.position,c);
		}
		return p;
	}
	void Emit(UiPaint paint, double width=-1) {
		if(!failure.IsEmpty()) return;
		has_drawn=true;
		if(path.IsEmpty() || (width==0)) { painted=true; return; }
		if(!paint.IsValid()) { Fail("invalid Painter paint"); return; }
		builder.Save(); builder.ConcatTransform(state.transform);
		if(width<0) builder.FillPath(path,paint,state.evenodd?UiFillRule::EvenOdd:UiFillRule::NonZero);
		else { UiStrokeStyle stroke=state.stroke; stroke.width=width; builder.StrokePath(path,paint,stroke); }
		builder.Restore(); painted=true;
	}
	void ClearOp(const RGBA& c) override {
		if(has_drawn || !stack.IsEmpty()) { Fail("clearing existing Painter content requires a separate layer"); return; }
		if(c.a) { builder.Save(); builder.FillRect(Rectf(0,0,size.cx,size.cy),ColorValue(c)); builder.Restore(); }
	}
	void MoveOp(const Pointf& p, bool rel) override {
		if(!Prepare()) return; current=start=quadratic=cubic=Point(p,rel); path.MoveTo(current);
	}
	void LineOp(const Pointf& p, bool rel) override {
		if(!Prepare()) return; if(path.IsEmpty()) path.MoveTo(current);
		current=quadratic=cubic=Point(p,rel); path.LineTo(current);
	}
	void QuadraticOp(const Pointf& c, const Pointf& p, bool rel) override {
		if(!Prepare()) return; if(path.IsEmpty()) path.MoveTo(current);
		Pointf q=Point(c,rel), end=Point(p,rel); path.QuadraticTo(q,end); quadratic=q; current=end;
	}
	void QuadraticOp(const Pointf& p, bool rel) override {
		if(!Prepare()) return; QuadraticOp(rel ? current-quadratic : 2*current-quadratic,p,rel);
	}
	void CubicOp(const Pointf& a,const Pointf& b,const Pointf& p,bool rel) override {
		if(!Prepare()) return; if(path.IsEmpty()) path.MoveTo(current);
		Pointf ca=Point(a,rel),cb=Point(b,rel),end=Point(p,rel);
		path.CubicTo(ca,cb,end); cubic=cb; current=end;
	}
	void CubicOp(const Pointf& b,const Pointf& p,bool rel) override {
		if(!Prepare()) return; CubicOp(rel ? current-cubic : 2*current-cubic,b,p,rel);
	}
	void ArcOp(const Pointf& c,const Pointf& r,double angle,double sweep,bool rel) override {
		if(!Prepare()) return; DoArc(Point(c,rel),r,angle,sweep,0);
	}
	void SvgArcOp(const Pointf& r,double angle,bool large,bool sweep,const Pointf& p,bool rel) override {
		if(!Prepare()) return; Pointf previous=current,end=Point(p,rel); DoSvgArc(r,angle,large,sweep,end,previous);
	}
	void CloseOp() override { if(!path.IsEmpty()) { path.Close(); current=quadratic=cubic=start; } }
	void DivOp() override { CloseOp(); }
	void FillOp(const RGBA& c) override { Emit(UiPaint::Solid(ColorValue(c))); }
	void StrokeOp(double w,const RGBA& c) override { Emit(UiPaint::Solid(ColorValue(c)),w); }
	void FillOp(const Pointf& a,const RGBA& ca,const Pointf& b,const RGBA& cb,int s) override {
		Emit(Gradient(a,ca,b,cb,0,s,false));
	}
	void StrokeOp(double w,const Pointf& a,const RGBA& ca,const Pointf& b,const RGBA& cb,int s) override {
		Emit(Gradient(a,ca,b,cb,0,s,false),w);
	}
	void FillOp(const Pointf& a,const RGBA& ca,const Pointf& b,double r,const RGBA& cb,int s) override {
		Emit(Gradient(a,ca,b,cb,r,s,true));
	}
	void StrokeOp(double w,const Pointf& a,const RGBA& ca,const Pointf& b,double r,const RGBA& cb,int s) override {
		Emit(Gradient(a,ca,b,cb,r,s,true),w);
	}
	void MatrixPaint(const RGBA& ca,const RGBA& cb,const Xform2D& m,int s,double w,
	                 bool radial=false,Pointf f=Pointf(0,0)) {
		UiPaint p=Gradient(f,ca,radial?Pointf(0,0):Pointf(1,0),cb,1,s,radial);
		p.transform=Neutral(m); Emit(p,w);
	}
	void FillOp(const RGBA& a,const RGBA& b,const Xform2D& m,int s) override { MatrixPaint(a,b,m,s,-1); }
	void StrokeOp(double w,const RGBA& a,const RGBA& b,const Xform2D& m,int s) override { MatrixPaint(a,b,m,s,w); }
	void FillOp(const Pointf& f,const RGBA& a,const RGBA& b,const Xform2D& m,int s) override {
		MatrixPaint(a,b,m,s,-1,true,f);
	}
	void StrokeOp(double w,const Pointf& f,const RGBA& a,const RGBA& b,const Xform2D& m,int s) override {
		MatrixPaint(a,b,m,s,w,true,f);
	}
	void FillOp(const Image& image,const Xform2D& m,dword flags) override {
		if(image.IsEmpty()) { painted=true; return; }
		double det=m.x.x*m.y.y-m.x.y*m.y.x;
		if(!std::isfinite(det) || fabs(det)<1e-12) { Fail("invalid Painter image transform"); return; }
		if(flags & ~(FILL_FAST|FILL_HREFLECT|FILL_VREFLECT)) { Fail("unsupported Painter image flags"); return; }
		Rectf bounds=path.GetControlBounds(), source;
		bool first=true;
		for(Pointf q:{bounds.TopLeft(),bounds.TopRight(),bounds.BottomLeft(),bounds.BottomRight()}) {
			Pointf d=q-m.t;
			q=Pointf((m.y.y*d.x-m.x.y*d.y)/det,(-m.y.x*d.x+m.x.x*d.y)/det);
			if(!std::isfinite(q.x)||!std::isfinite(q.y)||fabs(q.x)>1e7||fabs(q.y)>1e7) {
				Fail("Painter image extent budget exceeded"); return;
			}
			if(first) { source=Rectf(q,q); first=false; }
			else { source.left=min(source.left,q.x); source.top=min(source.top,q.y);
			       source.right=max(source.right,q.x); source.bottom=max(source.bottom,q.y); }
		}
		struct Cell : Moveable<Cell> { double lo,hi; int a,b; bool flip; };
		auto axis=[&](double lo,double hi,int size,int mode,Vector<Cell>& cells) {
			auto add=[&](double x,double y,int a,int b,bool flip=false) {
				if(y>x) { Cell& c=cells.Add(); c.lo=x;c.hi=y;c.a=a;c.b=b;c.flip=flip; }
			};
			if(mode==0) add(0,size,0,size);
			else if(mode==1) {
				add(min(0.0,lo),0,0,1); add(0,size,0,size); add(size,max((double)size,hi),size-1,size);
			} else {
				int begin=(int)floor(lo/size),end=(int)ceil(hi/size);
				if(end-begin>64) { Fail("Painter image repeat budget exceeded"); return; }
				for(int i=begin;i<end;++i) add((double)i*size,(double)(i+1)*size,0,size,mode==3 && (i&1));
			}
		};
		Vector<Cell> xs,ys; axis(source.left,source.right,image.GetWidth(),flags&3,xs);
		axis(source.top,source.bottom,image.GetHeight(),(flags>>2)&3,ys);
		if((int64)xs.GetCount()*ys.GetCount()>256) { Fail("Painter image tile budget exceeded"); return; }
		bool rectangle=path.GetCount()==5 && path[0].verb==UiPathVerb::MoveTo &&
		               path[1].verb==UiPathVerb::LineTo && path[2].verb==UiPathVerb::LineTo &&
		               path[3].verb==UiPathVerb::LineTo && path[4].verb==UiPathVerb::Close;
		for(int i=0;rectangle && i<4;++i) {
			Pointf p=path[i].p1;
			rectangle=(p.x==bounds.left || p.x==bounds.right) && (p.y==bounds.top || p.y==bounds.bottom);
		}
		bool exact=rectangle && fabs(source.left)<1e-8 && fabs(source.top)<1e-8 &&
		           fabs(source.right-image.GetWidth())<1e-8 && fabs(source.bottom-image.GetHeight())<1e-8;
		builder.Save();
		if(!exact) ClipOp();
		for(const auto& x:xs) for(const auto& y:ys) {
			Transform2D mirror=Transform2D::Scale(x.flip?-1:1,y.flip?-1:1);
			mirror.t=Pointf(x.flip?x.lo+x.hi:0,y.flip?y.lo+y.hi:0);
			builder.Save(); builder.ConcatTransform(mirror*Neutral(m)*state.transform);
			builder.DrawImage(Rectf(x.lo,y.lo,x.hi,y.hi),image,Rect(x.a,y.a,x.b,y.b),
			                  Rgba8(255,255,255,(byte)minmax((int)round(state.opacity*255),0,255)));
			builder.Restore();
		}
		builder.Restore(); painted=true; has_drawn=true;
	}
	void StrokeOp(double,const Image&,const Xform2D&,dword) override { Fail("Painter image-pattern stroke requires a GPU mask"); }
	void ClipOp() override {
		if(path.IsEmpty()) { Fail("Painter clip path is empty"); return; }
		UiPath resolved;
		for(int i=0;i<path.GetCount();++i) {
			const auto& c=path[i]; const auto& t=state.transform;
			switch(c.verb) {
			case UiPathVerb::MoveTo: resolved.MoveTo(t.TransformPoint(c.p1)); break;
			case UiPathVerb::LineTo: resolved.LineTo(t.TransformPoint(c.p1)); break;
			case UiPathVerb::QuadraticTo: resolved.QuadraticTo(t.TransformPoint(c.p1),t.TransformPoint(c.p2)); break;
			case UiPathVerb::CubicTo: resolved.CubicTo(t.TransformPoint(c.p1),t.TransformPoint(c.p2),t.TransformPoint(c.p3)); break;
			case UiPathVerb::Close: resolved.Close(); break;
			}
		}
		builder.ClipPath(resolved,state.evenodd?UiFillRule::EvenOdd:UiFillRule::NonZero); painted=true;
	}

	void CharacterOp(const Pointf& p,int ch,Font f) override { PaintCharacter(*this,p,ch,f); }
	void ColorStopOp(double pos,const RGBA& c) override {
		if(state.stops.GetCount()>=30) { Fail("Painter gradient stop budget exceeded"); return; }
		double opacity=state.opacity; state.opacity=1;
		state.stops.Add(UiGradientStop(pos,ColorValue(c))); state.opacity=opacity;
	}
	void ClearStopsOp() override { state.stops.Clear(); }
	void OpacityOp(double a) override {
		if(!std::isfinite(a)||a<0||a>1) Fail("invalid Painter opacity"); else state.opacity*=a;
	}
	void LineCapOp(int c) override { state.stroke.cap=c==LINECAP_ROUND?UiLineCap::Round:c==LINECAP_SQUARE?UiLineCap::Square:UiLineCap::Butt; }
	void LineJoinOp(int c) override { state.stroke.join=c==LINEJOIN_ROUND?UiLineJoin::Round:c==LINEJOIN_BEVEL?UiLineJoin::Bevel:UiLineJoin::Miter; }
	void MiterLimitOp(double m) override { state.stroke.miter_limit=m; }
	void EvenOddOp(bool b) override { state.evenodd=b; }
	void DashOp(const Vector<double>& d,double start) override { state.stroke.dash=clone(d); state.stroke.dash_offset=start; }
	void TransformOp(const Xform2D& m) override {
		if(!path.IsEmpty() && !painted) { Fail("Painter transform changed during an unfinished path"); return; }
		state.transform=Neutral(m)*state.transform;
	}
	void BeginOp() override { stack.Add(state); builder.Save(); }
	void EndOp() override {
		if(stack.IsEmpty()) { Fail("Painter End without Begin"); return; }
		state=stack.Top(); stack.Drop(); builder.Restore(); painted=true;
	}
	void InvertOp(bool b) override { if(b) Fail("Painter inversion is unsupported"); }
	void ImageFilterOp(int) override {}
	void BeginMaskOp() override { Fail("Painter mask requires GPU composition"); }
	void BeginOnPathOp(double,bool) override { Fail("Painter text-on-path is unsupported"); }
};
}

bool RecordUiPainter(Size size,const Function<void(Painter&)>& paint,UiDisplayList& out,String& error)
{
	if(size.cx<=0||size.cy<=0||size.cx>16384||size.cy>16384) { error="invalid Painter recording bounds"; return false; }
	SemanticPainter painter(size);
	paint(painter);
	return painter.Finish(out,error);
}

bool RecordUiSvg(const UiDisplayOp& op,UiDisplayList& out,String& error)
{
	if(op.type==UiDisplayOpType::DrawSvg && op.svg.IsEmpty()) {
		UiDisplayListBuilder empty; error.Clear(); return empty.Finish(out);
	}
	if(op.type!=UiDisplayOpType::DrawSvg || op.svg.GetCount()>1024*1024 ||
	   !std::isfinite(op.rect.left) || !std::isfinite(op.rect.top) ||
	   !std::isfinite(op.rect.right) || !std::isfinite(op.rect.bottom) ||
	   op.rect.Width()<=0 || op.rect.Height()<=0 ||
	   op.rect.Width()>4096 || op.rect.Height()>4096) { error="invalid SVG recording bounds"; return false; }
	bool parsed=false;
	Size requested((int)ceil(op.rect.Width()),(int)ceil(op.rect.Height()));
	Rectf bounds=GetSVGBoundingBox(op.svg);
	if(bounds.IsEmpty() || !std::isfinite(bounds.left) || !std::isfinite(bounds.top) ||
	   !std::isfinite(bounds.right) || !std::isfinite(bounds.bottom) ||
	   bounds.Width()>1e7 || bounds.Height()>1e7) { error="invalid SVG bounds"; return false; }
	Sizef fit=GetFitSize(bounds.GetSize(),Sizef(requested.cx,requested.cy)-10.0);
	Size actual((int)ceil(fit.cx),(int)ceil(fit.cy));
	if(actual.cx<=0||actual.cy<=0) { error="empty SVG fitting bounds"; return false; }
	bool ok=RecordUiPainter(actual,[&](Painter& p) {
		p.Translate(op.rect.left,op.rect.top);
		p.Scale(op.rect.Width()/actual.cx,op.rect.Height()/actual.cy);
		p.Rectangle(0,0,actual.cx,actual.cy).Fill(White());
		p.Scale(min(actual.cx/bounds.Width(),actual.cy/bounds.Height()));
		p.Translate(-bounds.left,-bounds.top);
		parsed=RenderSVG(p,op.svg);
	},out,error);
	if(ok&&!parsed) { error="SVG parser failed"; return false; }
	return ok;
}

}
