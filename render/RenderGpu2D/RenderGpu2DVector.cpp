#include "RenderGpu2D.h"

#include <RenderVector/RenderVector.h>
#include <cmath>

namespace Upp {

namespace {

static bool IsVectorOp(UiDisplayOpType type)
{
	return type == UiDisplayOpType::FillPath || type == UiDisplayOpType::StrokePath ||
	       type == UiDisplayOpType::DrawSvg;
}

static bool FiniteTransform(const Transform2D& transform)
{
	return std::isfinite(transform.x.x) && std::isfinite(transform.x.y) &&
	       std::isfinite(transform.y.x) && std::isfinite(transform.y.y) &&
	       std::isfinite(transform.t.x) && std::isfinite(transform.t.y);
}

static int VectorRasterScale(const Transform2D& transform)
{
	const Pointf origin = transform.TransformPoint(Pointf(0, 0));
	const Pointf ex = transform.TransformPoint(Pointf(1, 0)) - origin;
	const Pointf ey = transform.TransformPoint(Pointf(0, 1)) - origin;
	const double xx = ex.x * ex.x + ex.y * ex.y;
	const double yy = ey.x * ey.x + ey.y * ey.y;
	const double xy = ex.x * ey.x + ex.y * ey.y;
	const double discriminant = max(0.0, (xx - yy) * (xx - yy) + 4.0 * xy * xy);
	const double lambda = 0.5 * (xx + yy + sqrt(discriminant));
	const double scale = sqrt(max(1.0, lambda));
	if(!std::isfinite(scale))
		return 1;
	return minmax((int)ceil(scale), 1, 8);
}

}

UiRenderer2D::VectorCleanup::~VectorCleanup()
{
	if(owner)
		owner->DestroyVectorExtension();
}

UiRenderer2D::VectorImpl& UiRenderer2D::VectorCache()
{
	if(!vector_impl) {
		vector_impl = new VectorImpl;
		vector_cleanup.owner = this;
	}
	return *vector_impl;
}

void UiRenderer2D::DestroyVectorExtension()
{
	if(!vector_impl)
		return;
	delete vector_impl;
	vector_impl = nullptr;
	vector_cleanup.owner = nullptr;
}

bool UiRenderer2D::EnsureVectorRaster(const UiDisplayOp& op, const Transform2D& transform,
                                      VectorRaster& out, UiRenderer2DStats& vector_stats)
{
	out = VectorRaster();
	if(!IsVectorOp(op.type))
		return Fail("UiRenderer2D vector cache received a non-vector operation");
	if(!FiniteTransform(transform))
		return Fail("UiRenderer2D vector content has a non-finite transform");

	const int raster_scale = VectorRasterScale(transform);
	VectorImpl& cache = VectorCache();
	for(int i = 0; i < cache.cache.GetCount(); ++i) {
		const VectorImpl::CacheEntry& entry = cache.cache[i];
		if(entry.raster_scale == raster_scale && entry.op == op) {
			out.image = entry.image;
			out.local_rect = entry.local_rect;
			out.drawable = !entry.image.IsEmpty();
			vector_stats.vector_cache_entry_count = cache.cache.GetCount();
			return true;
		}
	}

	vector_stats.vector_cache_miss_count++;
	Image raster;
	Rectf local_rect;
	String raster_error;
	if(!RasterizeUiVectorOp(op, raster_scale, raster, local_rect, raster_error))
		return Fail("UiRenderer2D vector rasterization failed: " + raster_error);

	VectorImpl::CacheEntry& stored = cache.cache.Add();
	stored.op = op;
	stored.image = raster;
	stored.local_rect = local_rect;
	stored.raster_scale = raster_scale;
	if(!raster.IsEmpty())
		vector_stats.vector_raster_count++;
	vector_stats.vector_cache_entry_count = cache.cache.GetCount();

	out.image = stored.image;
	out.local_rect = stored.local_rect;
	out.drawable = !stored.image.IsEmpty();
	return true;
}

bool UiRenderer2D::MaterializeVectorList(const UiDisplayList& source, UiDisplayList& out,
                                         UiRenderer2DStats& vector_stats)
{
	vector_stats = UiRenderer2DStats();
	if(!source.IsValid())
		return Fail(source.GetError());

	UiDisplayListBuilder builder;
	ReplayState state;
	Vector<ReplayState> stack;

	for(int i = 0; i < source.GetCount(); ++i) {
		const UiDisplayOp& op = source[i];
		switch(op.type) {
		case UiDisplayOpType::Save:
			builder.Save();
			stack.Add(state);
			break;
		case UiDisplayOpType::Restore:
			if(stack.IsEmpty())
				return Fail("UiRenderer2D vector materialization restore without save");
			builder.Restore();
			state = stack.Pop();
			break;
		case UiDisplayOpType::ClipRect:
			builder.ClipRect(op.rect);
			break;
		case UiDisplayOpType::ConcatTransform:
			if(!FiniteTransform(op.transform))
				return Fail("UiRenderer2D vector materialization received non-finite transform");
			builder.ConcatTransform(op.transform);
			state.transform = state.transform * op.transform;
			break;
		case UiDisplayOpType::FillRect:
			builder.FillRect(op.rect, op.color);
			break;
		case UiDisplayOpType::InvertRect:
			builder.InvertRect(op.rect);
			break;
		case UiDisplayOpType::StrokeRect:
			builder.StrokeRect(op.rect, op.width, op.color);
			break;
		case UiDisplayOpType::FillRoundedRect:
			builder.FillRoundedRect(op.rounded, op.color);
			break;
		case UiDisplayOpType::DrawImage:
			builder.DrawImage(op.rect, op.image);
			break;
		case UiDisplayOpType::DrawText:
			builder.DrawText(op.point, op.text, op.font, op.color);
			break;
		case UiDisplayOpType::FillPath:
		case UiDisplayOpType::StrokePath:
		case UiDisplayOpType::DrawSvg: {
			vector_stats.vector_op_count++;
			if(op.type == UiDisplayOpType::DrawSvg)
				vector_stats.svg_count++;
			else {
				vector_stats.vector_path_count++;
				if(op.paint.kind != UiPaintKind::Solid)
					vector_stats.gradient_count++;
			}
			VectorRaster raster;
			if(!EnsureVectorRaster(op, state.transform, raster, vector_stats))
				return false;
			if(raster.drawable)
				builder.DrawImage(raster.local_rect, raster.image);
			break;
		}
		}
	}
	if(!stack.IsEmpty())
		return Fail("UiRenderer2D vector materialization ended with unbalanced save state");
	if(!builder.Finish(out))
		return Fail("UiRenderer2D vector materialization failed: " + builder.GetError());
	return true;
}

}
