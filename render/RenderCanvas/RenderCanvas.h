#pragma once

#include <Core/Core.h>
#include <RenderCore/RenderCore.h>

namespace Upp {

class UiDisplayList;

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
};

enum class UiDisplayOpType {
	Save,
	Restore,
	ClipRect,
	ConcatTransform,
	FillRect,
	StrokeRect,
	FillRoundedRect,
};

struct UiDisplayOp : Moveable<UiDisplayOp> {
	UiDisplayOpType type = UiDisplayOpType::Save;
	Rectf rect = Rectf(0, 0, 0, 0);
	double width = 0;
	Transform2D transform;
	Rgba8 color;
	struct RoundedRect rounded;

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
