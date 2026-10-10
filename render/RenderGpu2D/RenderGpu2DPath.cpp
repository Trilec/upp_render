#include "RenderGpu2DPath.h"
#include <cmath>

namespace Upp {
namespace {

constexpr int MAX_POINTS = 512;
constexpr int MAX_DEPTH = 12;
constexpr double CURVE_ERROR = 0.35; // Final device-pixel flattening budget.
constexpr double EPS = 1e-8;

double Cross(Pointf a, Pointf b) { return a.x * b.y - a.y * b.x; }
double Dot(Pointf a, Pointf b) { return a.x * b.x + a.y * b.y; }
double VectorLength(Pointf a) { return sqrt(Dot(a, a)); }
bool Finite(Pointf p) { return std::isfinite(p.x) && std::isfinite(p.y); }

bool AddPoint(Vector<Pointf>& points, Pointf p)
{
	if(!Finite(p) || fabs(p.x) > 1e7 || fabs(p.y) > 1e7)
		return false;
	if(!points.IsEmpty() && VectorLength(p - points.Top()) < EPS)
		return true;
	if(points.GetCount() >= MAX_POINTS)
		return false;
	points.Add(p);
	return true;
}

double DistanceToSegment(Pointf p, Pointf a, Pointf b)
{
	Pointf d = b - a;
	double square = Dot(d, d);
	double t = square > EPS * EPS ? minmax(Dot(p - a, d) / square, 0.0, 1.0) : 0;
	return VectorLength(p - (a + t * d));
}

bool Cubic(Vector<Pointf>& points, Pointf a, Pointf b, Pointf c, Pointf d, int depth)
{
	// Control-hull distance to the chord bounds the complete Bezier segment.
	if(max(DistanceToSegment(b, a, d), DistanceToSegment(c, a, d)) <= CURVE_ERROR)
		return AddPoint(points, d);
	if(depth == MAX_DEPTH)
		return false;
	Pointf ab = (a + b) * 0.5, bc = (b + c) * 0.5, cd = (c + d) * 0.5;
	Pointf abc = (ab + bc) * 0.5, bcd = (bc + cd) * 0.5;
	Pointf middle = (abc + bcd) * 0.5;
	return Cubic(points, a, ab, abc, middle, depth + 1) &&
	       Cubic(points, middle, bcd, cd, d, depth + 1);
}

bool Flatten(const UiPath& path, const Transform2D& transform, Vector<Pointf>& points,
             bool& closed)
{
	bool started = false;
	closed = false;
	Pointf current(0, 0);
	for(int i = 0; i < path.GetCount(); ++i) {
		const UiPathCommand& command = path[i];
		if(closed) return false; // Exactly one contour, including no second Move.
		Pointf p = transform.TransformPoint(command.p1);
		switch(command.verb) {
		case UiPathVerb::MoveTo:
			if(started || !AddPoint(points, p)) return false;
			started = true;
			current = p;
			break;
		case UiPathVerb::LineTo:
			if(!started || !AddPoint(points, p)) return false;
			current = p;
			break;
		case UiPathVerb::QuadraticTo: {
			Pointf end = transform.TransformPoint(command.p2);
			if(!started || !Cubic(points, current, current + (p - current) * (2.0 / 3.0),
			                     end + (p - end) * (2.0 / 3.0), end, 0)) return false;
			current = end;
			break;
		}
		case UiPathVerb::CubicTo: {
			Pointf c = transform.TransformPoint(command.p2);
			Pointf end = transform.TransformPoint(command.p3);
			if(!started || !Cubic(points, current, p, c, end, 0)) return false;
			current = end;
			break;
		}
		case UiPathVerb::Close:
			if(!started) return false;
			closed = true;
			break;
		}
	}
	if(points.GetCount() > 1 && VectorLength(points.Top() - points[0]) < EPS)
		points.Drop();
	return points.GetCount() >= 3;
}

// Reject reflex turns, degeneracy and multiple winding/self-crossing contours.
// A same-sign turn test alone would incorrectly accept a pentagram.
bool Convex(Vector<Pointf>& points)
{
	double area = 0;
	for(int i = 0; i < points.GetCount(); ++i)
		area += Cross(points[i] - points[0], points[(i + 1) % points.GetCount()] - points[0]);
	if(!std::isfinite(area) || fabs(area) < EPS) return false;
	if(area < 0) Reverse(points);
	double turning = 0;
	for(int i = 0; i < points.GetCount(); ++i) {
		Pointf a = points[i] - points[(i + points.GetCount() - 1) % points.GetCount()];
		Pointf b = points[(i + 1) % points.GetCount()] - points[i];
		double cross = Cross(a, b);
		if(cross < -EPS || VectorLength(a) < EPS || VectorLength(b) < EPS) return false;
		turning += atan2(cross, Dot(a, b));
	}
	return fabs(turning - 2 * M_PI) < 1e-5;
}

bool Offset(const Vector<Pointf>& source, double distance, double miter_limit,
            Vector<Pointf>& result)
{
	int count = source.GetCount();
	for(int i = 0; i < count; ++i) {
		Pointf a = source[i] - source[(i + count - 1) % count];
		Pointf b = source[(i + 1) % count] - source[i];
		double la = VectorLength(a), lb = VectorLength(b);
		if(la < EPS || lb < EPS) return false;
		Pointf na(a.y / la, -a.x / la), nb(b.y / lb, -b.x / lb);
		double denominator = 1 + Dot(na, nb);
		if(denominator < EPS) return false;
		Pointf miter = (na + nb) / denominator;
		if(VectorLength(miter) > miter_limit) return false;
		Pointf p = source[i] + distance * miter;
		if(!Finite(p)) return false;
		result.Add(p);
	}
	// An inward offset may collapse or invert even though each corner is finite.
	// Require it to remain inside every original half-plane at that distance.
	if(distance < 0)
		for(Pointf p : result)
			for(int i = 0; i < count; ++i) {
				Pointf edge = source[(i + 1) % count] - source[i];
				if(Cross(edge, p - source[i]) / VectorLength(edge) < -distance - 1e-5)
					return false;
			}
	return true;
}

void Triangle(GpuPathMesh& mesh, Pointf a, double ca, Pointf b, double cb, Pointf c, double cc)
{
	if(fabs(Cross(b - a, c - a)) < EPS) return;
	GpuPathVertex& va = mesh.triangles.Add(); va.point = a; va.coverage = ca;
	GpuPathVertex& vb = mesh.triangles.Add(); vb.point = b; vb.coverage = cb;
	GpuPathVertex& vc = mesh.triangles.Add(); vc.point = c; vc.coverage = cc;
}

void Ring(GpuPathMesh& mesh, const Vector<Pointf>& a, double ca,
          const Vector<Pointf>& b, double cb)
{
	for(int i = 0; i < a.GetCount(); ++i) {
		int j = (i + 1) % a.GetCount();
		Triangle(mesh, a[i], ca, a[j], ca, b[j], cb);
		Triangle(mesh, a[i], ca, b[j], cb, b[i], cb);
	}
}

bool PrepareStraightLine(const UiDisplayOp& op, const Transform2D& transform, GpuPathMesh& out)
{
	if(op.path.GetCount() != 2 || op.path[0].verb != UiPathVerb::MoveTo ||
	   op.path[1].verb != UiPathVerb::LineTo || !op.stroke.IsValid() ||
	   !op.stroke.dash.IsEmpty() || op.stroke.cap == UiLineCap::Round)
		return false;
	Pointf ex(transform.x.x, transform.y.x), ey(transform.x.y, transform.y.y);
	double sx = VectorLength(ex), sy = VectorLength(ey);
	if(sx < EPS || fabs(sx - sy) > 1e-6 * max(sx, sy) ||
	   fabs(Dot(ex, ey)) > 1e-6 * sx * sy)
		return false;
	const double half = op.stroke.width * sx * 0.5;
	if(!std::isfinite(half) || half < 0.5) return false;
	Pointf a = transform.TransformPoint(op.path[0].p1);
	Pointf b = transform.TransformPoint(op.path[1].p1);
	if(!Finite(a) || !Finite(b) || max(max(fabs(a.x), fabs(a.y)), max(fabs(b.x), fabs(b.y))) > 1e7)
		return false;
	const double length = VectorLength(b - a);
	if(length < EPS) return false;
	Pointf direction = (b - a) / length;
	Pointf normal(direction.y, -direction.x);
	if(op.stroke.cap == UiLineCap::Square) {
		a -= direction * half;
		b += direction * half;
	}
	Vector<Pointf> contour, outer, inner;
	if(!AddPoint(contour, a + normal * half) || !AddPoint(contour, b + normal * half) ||
	   !AddPoint(contour, b - normal * half) || !AddPoint(contour, a - normal * half) ||
	   !Convex(contour) || !Offset(contour, 0.5, 10, outer) || !Offset(contour, -0.5, 10, inner))
		return false;
	for(int i = 1; i + 1 < inner.GetCount(); ++i)
		Triangle(out, inner[0], 1, inner[i], 1, inner[i + 1], 1);
	Ring(out, inner, 1, outer, 0);
	return true;
}
}

bool PrepareGpuConvexPath(const UiDisplayOp& op, const Transform2D& transform,
                          GpuPathMesh& out)
{
	out.triangles.Clear();
	if((op.type != UiDisplayOpType::FillPath && op.type != UiDisplayOpType::StrokePath) ||
	   op.paint.kind != UiPaintKind::Solid)
		return false;
	if(op.type == UiDisplayOpType::StrokePath && op.path.GetCount() == 2)
		return PrepareStraightLine(op, transform, out);
	Vector<Pointf> contour;
	bool closed;
	if(!Flatten(op.path, transform, contour, closed) || !Convex(contour))
		return false;
	Vector<Pointf> outer, inner;
	if(op.type == UiDisplayOpType::FillPath) {
		if(!Offset(contour, 0.5, 10, outer) || !Offset(contour, -0.5, 10, inner))
			return false;
		for(int i = 1; i + 1 < inner.GetCount(); ++i)
			Triangle(out, inner[0], 1, inner[i], 1, inner[i + 1], 1);
		Ring(out, inner, 1, outer, 0);
		return true;
	}
	if(!closed || !op.stroke.dash.IsEmpty() || op.stroke.join != UiLineJoin::Miter ||
	   !op.stroke.IsValid())
		return false;
	// Local stroke widths under a similarity transform have one exact device width.
	// Anisotropic/sheared stroke geometry keeps the established Painter path.
	Pointf ex(transform.x.x, transform.y.x), ey(transform.x.y, transform.y.y);
	double sx = VectorLength(ex), sy = VectorLength(ey);
	if(sx < EPS || fabs(sx - sy) > 1e-6 * max(sx, sy) ||
	   fabs(Dot(ex, ey)) > 1e-6 * sx * sy)
		return false;
	double half = op.stroke.width * sx * 0.5;
	if(!std::isfinite(half) || half < 0.5) return false;
	Vector<Pointf> outer_edge, inner_edge;
	if(!Offset(contour, half + 0.5, op.stroke.miter_limit, outer) ||
	   !Offset(contour, half - 0.5, op.stroke.miter_limit, outer_edge) ||
	   !Offset(contour, -half + 0.5, op.stroke.miter_limit, inner_edge) ||
	   !Offset(contour, -half - 0.5, op.stroke.miter_limit, inner))
		return false;
	Ring(out, outer, 0, outer_edge, 1);
	Ring(out, outer_edge, 1, inner_edge, 1);
	Ring(out, inner_edge, 1, inner, 0);
	return true;
}
}
