#pragma once

#include <Core/Core.h>
#include <RenderCore/RenderCore.h>

namespace Upp {

class UiDisplayList;

enum class UiPathVerb {
	MoveTo,
	LineTo,
	QuadraticTo,
	CubicTo,
	Close,
};

struct UiPathCommand : Moveable<UiPathCommand> {
	UiPathVerb verb = UiPathVerb::MoveTo;
	Pointf p1 = Pointf(0, 0);
	Pointf p2 = Pointf(0, 0);
	Pointf p3 = Pointf(0, 0);

	bool operator==(const UiPathCommand& other) const;
	bool operator!=(const UiPathCommand& other) const { return !(*this == other); }
};

class UiPath : Moveable<UiPath> {
public:
	UiPath() = default;
	UiPath(const UiPath& other) : commands(other.commands, 1) {}
	UiPath(UiPath&& other) = default;
	UiPath& operator=(const UiPath& other) {
		if(this != &other)
			commands = Vector<UiPathCommand>(other.commands, 1);
		return *this;
	}
	UiPath& operator=(UiPath&& other) = default;

	UiPath& MoveTo(Pointf p);
	UiPath& LineTo(Pointf p);
	UiPath& QuadraticTo(Pointf control, Pointf end);
	UiPath& CubicTo(Pointf control1, Pointf control2, Pointf end);
	UiPath& Close();
	void Clear() { commands.Clear(); }

	bool IsEmpty() const { return commands.IsEmpty(); }
	int GetCount() const { return commands.GetCount(); }
	const UiPathCommand& operator[](int i) const { return commands[i]; }
	Rectf GetControlBounds() const;
	String Dump() const;

	bool operator==(const UiPath& other) const;
	bool operator!=(const UiPath& other) const { return !(*this == other); }

private:
	Vector<UiPathCommand> commands;
};

enum class UiFillRule {
	NonZero,
	EvenOdd,
};

enum class UiGradientSpread {
	Pad,
	Repeat,
	Reflect,
};

enum class UiPaintKind {
	Solid,
	LinearGradient,
	RadialGradient,
};

struct UiGradientStop : Moveable<UiGradientStop> {
	double position = 0;
	Rgba8 color;

	UiGradientStop() = default;
	UiGradientStop(double p, Rgba8 c) : position(p), color(c) {}

	bool operator==(const UiGradientStop& other) const;
	bool operator!=(const UiGradientStop& other) const { return !(*this == other); }
};

struct UiPaint : Moveable<UiPaint> {
	UiPaintKind kind = UiPaintKind::Solid;
	Rgba8 color;
	Pointf p0 = Pointf(0, 0); // linear start / radial focal point
	Pointf p1 = Pointf(0, 0); // linear end / radial centre
	double radius = 0;
	UiGradientSpread spread = UiGradientSpread::Pad;
	Vector<UiGradientStop> stops;

	UiPaint() = default;
	UiPaint(const UiPaint& other)
		: kind(other.kind), color(other.color), p0(other.p0), p1(other.p1),
		  radius(other.radius), spread(other.spread), stops(other.stops, 1) {}
	UiPaint(UiPaint&& other) = default;
	UiPaint& operator=(const UiPaint& other) {
		if(this != &other) {
			kind = other.kind;
			color = other.color;
			p0 = other.p0;
			p1 = other.p1;
			radius = other.radius;
			spread = other.spread;
			stops = Vector<UiGradientStop>(other.stops, 1);
		}
		return *this;
	}
	UiPaint& operator=(UiPaint&& other) = default;

	static UiPaint Solid(Rgba8 color);
	static UiPaint Linear(Pointf start, Pointf end, Rgba8 start_color, Rgba8 end_color,
	                      UiGradientSpread spread = UiGradientSpread::Pad);
	static UiPaint Radial(Pointf focal, Pointf centre, double radius,
	                      Rgba8 inner_color, Rgba8 outer_color,
	                      UiGradientSpread spread = UiGradientSpread::Pad);
	UiPaint& AddStop(double position, Rgba8 color);

	bool IsValid(String *reason = nullptr) const;
	String Dump() const;
	bool operator==(const UiPaint& other) const;
	bool operator!=(const UiPaint& other) const { return !(*this == other); }
};

enum class UiLineCap {
	Butt,
	Square,
	Round,
};

enum class UiLineJoin {
	Miter,
	Round,
	Bevel,
};

struct UiStrokeStyle : Moveable<UiStrokeStyle> {
	double width = 1;
	UiLineCap cap = UiLineCap::Butt;
	UiLineJoin join = UiLineJoin::Miter;
	double miter_limit = 10;
	Vector<double> dash;
	double dash_offset = 0;

	UiStrokeStyle() = default;
	UiStrokeStyle(const UiStrokeStyle& other)
		: width(other.width), cap(other.cap), join(other.join), miter_limit(other.miter_limit),
		  dash(other.dash, 1), dash_offset(other.dash_offset) {}
	UiStrokeStyle(UiStrokeStyle&& other) = default;
	UiStrokeStyle& operator=(const UiStrokeStyle& other) {
		if(this != &other) {
			width = other.width;
			cap = other.cap;
			join = other.join;
			miter_limit = other.miter_limit;
			dash = Vector<double>(other.dash, 1);
			dash_offset = other.dash_offset;
		}
		return *this;
	}
	UiStrokeStyle& operator=(UiStrokeStyle&& other) = default;

	bool IsValid(String *reason = nullptr) const;
	String Dump() const;
	bool operator==(const UiStrokeStyle& other) const;
	bool operator!=(const UiStrokeStyle& other) const { return !(*this == other); }
};

// Backend-neutral drawing contract used for recording.
class UiCanvas {
public:
	virtual ~UiCanvas() {}

	virtual void Save() = 0;
	virtual void Restore() = 0;

	virtual void ClipRect(const Rectf& rect) = 0;
	virtual void ConcatTransform(const Transform2D& transform) = 0;

	virtual void FillRect(const Rectf& rect, Rgba8 color) = 0;
	virtual void StrokeRect(const Rectf& rect, double width, Rgba8 color) = 0;
	virtual void FillRoundedRect(const struct RoundedRect& rect, Rgba8 color) = 0;
	virtual void DrawImage(const Rectf& rect, const Image& image) = 0;
	virtual void DrawText(const Pointf& point, const WString& text, Font font, Rgba8 color) = 0;
	virtual void FillPath(const UiPath& path, const UiPaint& paint,
	                      UiFillRule rule = UiFillRule::NonZero) = 0;
	virtual void StrokePath(const UiPath& path, const UiPaint& paint,
	                        const UiStrokeStyle& style) = 0;
	virtual void DrawSvg(const Rectf& rect, const String& svg) = 0;
};

enum class UiDisplayOpType {
	Save,
	Restore,
	ClipRect,
	ConcatTransform,
	FillRect,
	StrokeRect,
	FillRoundedRect,
	DrawImage,
	DrawText,
	FillPath,
	StrokePath,
	DrawSvg,
};

struct UiDisplayOp : Moveable<UiDisplayOp> {
	UiDisplayOpType type = UiDisplayOpType::Save;
	Rectf rect = Rectf(0, 0, 0, 0);
	Pointf point = Pointf(0, 0);
	double width = 0;
	Transform2D transform;
	Rgba8 color;
	struct RoundedRect rounded;
	Image image;
	WString text;
	Font font;
	UiPath path;
	UiPaint paint;
	UiFillRule fill_rule = UiFillRule::NonZero;
	UiStrokeStyle stroke;
	String svg;

	bool operator==(const UiDisplayOp& other) const;
	bool operator!=(const UiDisplayOp& other) const { return !(*this == other); }
};

// Immutable display list produced by the builder.
class UiDisplayList {
public:
	UiDisplayList();

	bool IsValid() const { return valid; }
	const String& GetError() const { return error; }
	int GetCount() const { return ops.GetCount(); }
	const UiDisplayOp& operator[](int i) const { return ops[i]; }
	String Dump() const;

private:
	Vector<UiDisplayOp> ops;
	bool valid = true;
	String error;

	void SetValid(Vector<UiDisplayOp>&& source);
	void SetInvalid(String message, Vector<UiDisplayOp>&& source);

	friend class UiDisplayListBuilder;
	friend class SoftwareUiRenderer;
};

// Records UiCanvas operations into a display list.
class UiDisplayListBuilder : public UiCanvas {
public:
	UiDisplayListBuilder();

	void Save() override;
	void Restore() override;
	void ClipRect(const Rectf& rect) override;
	void ConcatTransform(const Transform2D& transform) override;
	void FillRect(const Rectf& rect, Rgba8 color) override;
	void StrokeRect(const Rectf& rect, double width, Rgba8 color) override;
	void FillRoundedRect(const struct RoundedRect& rect, Rgba8 color) override;
	void DrawImage(const Rectf& rect, const Image& image) override;
	void DrawText(const Pointf& point, const WString& text, Font font, Rgba8 color) override;
	void FillPath(const UiPath& path, const UiPaint& paint,
	              UiFillRule rule = UiFillRule::NonZero) override;
	void StrokePath(const UiPath& path, const UiPaint& paint,
	                const UiStrokeStyle& style) override;
	void DrawSvg(const Rectf& rect, const String& svg) override;

	bool Finish(UiDisplayList& out);
	bool IsFinished() const { return finished; }
	const String& GetError() const { return error; }
	int GetSaveDepth() const { return save_depth; }

private:
	Vector<UiDisplayOp> ops;
	int save_depth = 0;
	bool finished = false;
	String error;

	bool CanRecord();
	void Fail(const String& message);
	void Append(const UiDisplayOp& op);
};

}
