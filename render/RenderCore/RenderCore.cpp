#include "RenderCore.h"

namespace Upp {

Transform2D::Transform2D()
{
	x.x = 1;
	y.y = 1;
	x.y = 0;
	y.x = 0;
	t.x = 0;
	t.y = 0;
}

bool Transform2D::operator==(const Transform2D& other) const
{
	return x.x == other.x.x && x.y == other.x.y &&
	       y.x == other.y.x && y.y == other.y.y &&
	       t.x == other.t.x && t.y == other.t.y;
}

Transform2D Transform2D::Identity()
{
	return Transform2D();
}

Transform2D Transform2D::Translation(double dx, double dy)
{
	Transform2D m;
	m.t = Pointf(dx, dy);
	return m;
}

Transform2D Transform2D::Scale(double sx, double sy)
{
	Transform2D m;
	m.x.x = sx;
	m.y.y = sy;
	return m;
}

Transform2D Transform2D::Scale(double s)
{
	return Scale(s, s);
}

Transform2D operator*(const Transform2D& a, const Transform2D& b)
{
	Transform2D r;
	r.x.x = a.x.x * b.x.x + a.y.x * b.x.y;
	r.x.y = a.x.y * b.x.x + a.y.y * b.x.y;
	r.y.x = a.x.x * b.y.x + a.y.x * b.y.y;
	r.y.y = a.x.y * b.y.x + a.y.y * b.y.y;
	r.t.x = a.t.x * b.x.x + a.t.y * b.x.y + b.t.x;
	r.t.y = a.t.x * b.y.x + a.t.y * b.y.y + b.t.y;
	return r;
}

RoundedRect::RoundedRect()
{
	rect = Rectf(0, 0, 0, 0);
	radius = 0;
}

RoundedRect::RoundedRect(const Rectf& r, double corner_radius)
{
	rect = r;
	radius = corner_radius < 0 ? 0 : corner_radius;
}

bool RoundedRect::operator==(const RoundedRect& other) const
{
	return rect.left == other.rect.left && rect.top == other.rect.top &&
	       rect.right == other.rect.right && rect.bottom == other.rect.bottom &&
	       radius == other.radius;
}

}
