#include "RenderGpu2D.h"

#include <RenderVector/RenderVector.h>
#include <cmath>

namespace Upp {

namespace {

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

static String VectorCacheKey(const UiDisplayOp& op, int raster_scale)
{
	String key;
	key << raster_scale << '|';
	switch(op.type) {
	case UiDisplayOpType::FillPath:
		key << "F|" << (op.fill_rule == UiFillRule::EvenOdd ? 'E' : 'N')
		    << '|' << op.path.Dump() << '|' << op.paint.Dump();
		break;
	case UiDisplayOpType::StrokePath:
		key << "S|" << op.path.Dump() << '|' << op.paint.Dump() << '|' << op.stroke.Dump();
		break;
	case UiDisplayOpType::DrawSvg:
		// Keep the full SVG source in the key so cache correctness never relies on
		// a finite-width hash collision assumption. The display list already owns
		// the source, and vector cache sizes are modest in normal UI workloads.
		key << "V|" << FormatDoubleFix(op.rect.left, 6) << ',' << FormatDoubleFix(op.rect.top, 6)
		    << ',' << FormatDoubleFix(op.rect.right, 6) << ',' << FormatDoubleFix(op.rect.bottom, 6)
		    << '|' << op.svg;
		break;
	default:
		key << "invalid";
		break;
	}
	return key;
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
	if(device) {
		for(int i = vector_impl->cache.GetCount() - 1; i >= 0; --i) {
			const VectorImpl::CacheEntry& entry = vector_impl->cache[i];
			if(entry.texture.IsValid())
				device->DestroyTexture(entry.texture);
		}
	}
	delete vector_impl;
	vector_impl = nullptr;
	vector_cleanup.owner = nullptr;
}

bool UiRenderer2D::EnsureVectorTexture(const UiDisplayOp& op, const Transform2D& transform,
                                       VectorDraw& out)
{
	out = VectorDraw();
	if(!device)
		return Fail("UiRenderer2D vector cache has no device");
	if((device->GetAdapterInfo().capability_flags & GpuCapability_Textures) == 0)
		return Fail("UiRenderer2D vector rendering requires texture support");
	if(op.type != UiDisplayOpType::FillPath && op.type != UiDisplayOpType::StrokePath &&
	   op.type != UiDisplayOpType::DrawSvg)
		return Fail("UiRenderer2D vector cache received a non-vector operation");

	const int raster_scale = VectorRasterScale(transform);
	const String key = VectorCacheKey(op, raster_scale);
	VectorImpl& cache = VectorCache();
	const int found = cache.cache.Find(key);
	if(found >= 0) {
		const VectorImpl::CacheEntry& entry = cache.cache[found];
		out.texture = entry.texture;
		out.local_rect = entry.local_rect;
		out.drawable = entry.texture.IsValid();
		stats.vector_texture_count = cache.cache.GetCount();
		return true;
	}

	stats.vector_cache_miss_count++;
	Image raster;
	Rectf local_rect;
	String raster_error;
	if(!RasterizeUiVectorOp(op, raster_scale, raster, local_rect, raster_error))
		return Fail("UiRenderer2D vector rasterization failed: " + raster_error);
	if(raster.IsEmpty())
		return true;

	Image straight = Unmultiply(raster);
	GpuTextureDesc desc;
	desc.size = straight.GetSize();
	desc.format = GpuFormat::RGBA8Srgb;
	desc.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst;
	desc.label = "UiRenderer2D vector raster";
	GpuTextureId texture;
	GpuResult result = device->CreateTexture(desc, texture);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D vector texture creation failed: " + DumpGpuResult(result));

	GpuTextureWriteDesc upload;
	upload.size = straight.GetSize();
	upload.row_pitch = (int64)straight.GetWidth() * (int)sizeof(RGBA);
	const int64 bytes = (int64)straight.GetLength() * (int)sizeof(RGBA);
	result = device->WriteTexture(texture, upload, straight.Begin(), bytes);
	if(result != GpuResult::Ok) {
		device->DestroyTexture(texture);
		return Fail("UiRenderer2D vector texture upload failed: " + DumpGpuResult(result));
	}

	VectorImpl::CacheEntry entry;
	entry.texture = texture;
	entry.local_rect = local_rect;
	entry.raster_scale = raster_scale;
	cache.cache.Add(key, pick(entry));
	stats.vector_texture_upload_count++;
	stats.vector_texture_count = cache.cache.GetCount();

	const VectorImpl::CacheEntry& stored = cache.cache.Top();
	out.texture = stored.texture;
	out.local_rect = stored.local_rect;
	out.drawable = true;
	return true;
}

}
