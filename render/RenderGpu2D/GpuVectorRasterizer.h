#ifndef _RenderGpu2D_GpuVectorRasterizer_h_
#define _RenderGpu2D_GpuVectorRasterizer_h_

#include <RenderCanvas/RenderCanvas.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

// Private GPU coverage engine. CPU work prepares bounded edge geometry only;
// coverage, gradients and final premultiplied pixels are generated on the GPU.
struct GpuVectorEdge : Moveable<GpuVectorEdge> { float x0, y0, x1, y1; };
struct GpuVectorStop : Moveable<GpuVectorStop> { float rgba[4], position, padding[3] = {}; };
struct GpuVectorGeometry {
	Vector<GpuVectorEdge> edges;
	Vector<uint32> rows;
	Vector<GpuVectorStop> stops;
	Rect bounds = Rect(0, 0, 0, 0);
	struct Parameters {
		uint32 width = 0, height = 0, evenodd = 0, kind = 0;
		float inverse_x[4] = {}, inverse_y[4] = {}, points[4] = {};
		uint32 spread = 0, stop_count = 0, srgb = 0, reserved = 0;
	} parameters;
};

// Fill curves use a 0.175 px bound; strokes preserve Painter's curve/dash/cap
// geometry policy without invoking its rasterizer. At most 8192 edges,
// 4096 pixels per axis and 1 Gi intersection work units. Failure is explicit.
bool PrepareGpuVectorGeometry(const UiDisplayOp& op, const Transform2D& transform,
                              GpuVectorGeometry& out, String& error);

class GpuVectorRasterizer {
public:
	explicit GpuVectorRasterizer(GpuDevice& device) : device(device) {}
	~GpuVectorRasterizer();
	GpuVectorRasterizer(const GpuVectorRasterizer&) = delete;
	GpuVectorRasterizer& operator=(const GpuVectorRasterizer&) = delete;
	bool IsSupported();
	bool Render(const GpuVectorGeometry& geometry, GpuTextureId texture, Point origin,
	            bool srgb, String& error);
	// Cold glyphs share bounded storage ranges and one completed submission.
	bool QueueRender(GpuVectorGeometry&& geometry, GpuTextureId texture, Point origin,
	                 bool srgb, String& error);
	bool Flush(String& error);
	void DiscardPending();
	int64 GetSubmitCount() const { return submissions; }
	int64 GetScratchBytes() const;
private:
	bool EnsureProgram(String& error);
	bool EnsureBuffer(int slot, int64 bytes, String& error);
	GpuDevice& device;
	GpuShaderId shader;
	GpuPipelineId pipeline;
	GpuBufferId buffers[4];
	int64 capacities[4] = {};
	int supported = -1;
	GpuComputeCapabilities capabilities;
	struct Pending {
		GpuVectorGeometry geometry;
		GpuTextureId texture;
		Point origin;
		bool srgb = false;
		int64 offsets[4] = {}, sizes[4] = {};
	};
	Array<Pending> pending;
	int64 pending_sizes[4] = {}, submissions = 0;
};

}
#endif
