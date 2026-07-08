#pragma once

#include <Core/Core.h>
#include <Draw/Draw.h>

namespace Upp {

// Eight-bit RGBA color with explicit value semantics.
struct Rgba8 : Moveable<Rgba8> {
	byte r = 0;
	byte g = 0;
	byte b = 0;
	byte a = 255;

	Rgba8() = default;
	explicit Rgba8(byte red, byte green, byte blue, byte alpha = 255)
		: r(red), g(green), b(blue), a(alpha)
	{}

	bool operator==(const Rgba8& other) const {
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}

	bool operator!=(const Rgba8& other) const {
		return !(*this == other);
	}

	Color ToColor() const { RGBA rgba; rgba.r = r; rgba.g = g; rgba.b = b; rgba.a = a; return Color(rgba); }
	static Rgba8 FromColor(Color c) { RGBA rgba = c; return Rgba8(rgba.r, rgba.g, rgba.b, rgba.a); }
	static Rgba8 Opaque(byte red, byte green, byte blue) { return Rgba8(red, green, blue, 255); }
	static Rgba8 Transparent(byte red = 0, byte green = 0, byte blue = 0) { return Rgba8(red, green, blue, 0); }
};

// 2D affine transform using the same row-vector convention as U++ Painter.
struct Transform2D : Moveable<Transform2D> {
	Pointf x;
	Pointf y;
	Pointf t;

	Transform2D();

	Pointf TransformPoint(const Pointf& p) const {
		return Pointf(p.x * x.x + p.y * x.y + t.x,
		              p.x * y.x + p.y * y.y + t.y);
	}

	bool operator==(const Transform2D& other) const;
	bool operator!=(const Transform2D& other) const { return !(*this == other); }

	static Transform2D Identity();
	static Transform2D Translation(double dx, double dy);
	static Transform2D Scale(double sx, double sy);
	static Transform2D Scale(double s);
};

Transform2D operator*(const Transform2D& a, const Transform2D& b);

// Rectangle bounds plus a uniform corner radius.
struct RoundedRect : Moveable<struct RoundedRect> {
	Rectf rect;
	double radius = 0;

	RoundedRect();
	explicit RoundedRect(const Rectf& r, double corner_radius);

	bool operator==(const RoundedRect& other) const;
	bool operator!=(const RoundedRect& other) const { return !(*this == other); }
};

}
