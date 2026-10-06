#pragma once

#include <RenderCanvas/RenderCanvas.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

struct UiRenderer2DTarget : Moveable<UiRenderer2DTarget> {
	GpuTextureId color_target;
	Size size = Size(0, 0);
	GpuFormat color_format = GpuFormat::Unknown;
	GpuLoadOp load_op = GpuLoadOp::Clear;
	GpuStoreOp store_op = GpuStoreOp::Store;
	GpuClearColor clear_color;
};

// Retained pixel payload limits, independent of backend allocation alignment.
// Entries used by a frame are pinned until replay completes; oversized frames
// fail explicitly rather than evicting a texture referenced by that frame.
struct UiRenderer2DCacheLimits {
	int64 image_bytes = 64 * 1024 * 1024;
	int image_entries = 4096;
	int64 vector_bytes = 32 * 1024 * 1024;
	int vector_entries = 4096;
	int64 glyph_bytes = 16 * 1024 * 1024;
	int glyph_entries = 8192;
};

struct UiRenderer2DStats : Moveable<UiRenderer2DStats> {
	int display_op_count = 0;
	int primitive_count = 0;
	int emitted_primitive_count = 0;
	int clipped_primitive_count = 0;
	int image_count = 0;
	int text_run_count = 0;
	int glyph_count = 0;
	int glyph_cache_miss_count = 0;
	int glyph_atlas_page_count = 0;
	int glyph_atlas_upload_count = 0;
	int vector_op_count = 0;
	int vector_path_count = 0;
	int gradient_count = 0;
	int svg_count = 0;
	int vector_cache_miss_count = 0;
	int vector_cache_entry_count = 0;
	int vector_raster_count = 0;
	int texture_upload_count = 0;
	int triangle_count = 0;
	int vertex_count = 0;
	int textured_vertex_count = 0;
	int translucent_vertex_count = 0;
	int draw_count = 0;
	int batch_count = 0;
	int64 uploaded_bytes = 0;
	int64 image_upload_bytes = 0;
	int64 glyph_cache_bytes = 0;
	int glyph_cache_entry_count = 0;
	int glyph_cache_reset_count = 0;
	int64 image_cache_bytes = 0;
	int64 vector_cache_bytes = 0;
	int image_cache_entry_count = 0;
	int image_cache_hit_count = 0;
	int image_cache_eviction_count = 0;
	int vector_cache_eviction_count = 0;
	int64 vertex_buffer_capacity = 0;
	int64 textured_vertex_buffer_capacity = 0;
	bool vertex_buffer_grew = false;
	bool textured_vertex_buffer_grew = false;
};

// Backend-neutral 2D renderer. The GpuDevice must outlive this object.
// Vector/SVG content is antialiased by the shared U++ Painter authority into
// cached Images, then flows through the same sampled-image GPU path already
// used by ordinary DrawImage content. No second GPU texture ownership tree.
class UiRenderer2D {
public:
	explicit UiRenderer2D(GpuDevice& device);
	~UiRenderer2D();

	UiRenderer2D(const UiRenderer2D&) = delete;
	UiRenderer2D& operator=(const UiRenderer2D&) = delete;

	bool Render(const UiDisplayList& list, const UiRenderer2DTarget& target);
	bool RenderFrame(const UiDisplayList& list, const GpuFrameInfo& frame,
	                 const GpuClearColor& clear_color = GpuClearColor());
	void Close();
	// Configure between frames. Zero rejects new cached content.
	void SetCacheLimits(const UiRenderer2DCacheLimits& limits);
	const UiRenderer2DCacheLimits& GetCacheLimits() const { return cache_limits; }

	bool IsReady() const { return ready; }
	const String& GetError() const { return error; }
	const UiRenderer2DStats& GetStats() const { return stats; }

private:
	struct Vertex : Moveable<Vertex> {
		float x = 0;
		float y = 0;
		float r = 0;
		float g = 0;
		float b = 0;
		float a = 1;
	};

	struct TexturedVertex : Moveable<TexturedVertex> {
		float x = 0;
		float y = 0;
		float u = 0;
		float v = 0;
		float r = 1;
		float g = 1;
		float b = 1;
		float a = 1;
	};

	struct PipelineEntry : Moveable<PipelineEntry> {
		GpuFormat format = GpuFormat::Unknown;
		bool textured = false;
		bool alpha_mask = false;
		GpuBlendMode blend_mode = GpuBlendMode::SourceOver;
		GpuPipelineId pipeline;
	};

	struct ImageCacheEntry : Moveable<ImageCacheEntry> {
		int64 serial = 0;
		GpuFormat format = GpuFormat::Unknown;
		Size size = Size(0, 0);
		GpuTextureId texture;
		uint64 last_frame = 0;
	};

	enum class BatchKind {
		Solid,
		Invert,
		Image,
		ImageMask,
	};

	struct Batch : Moveable<Batch> {
		BatchKind kind = BatchKind::Solid;
		int first_vertex = 0;
		int vertex_count = 0;
		GpuTextureId texture;
	};

	struct ReplayState : Moveable<ReplayState> {
		Transform2D transform;
		bool has_clip = false;
		Rectf clip = Rectf(0, 0, 0, 0);
	};

	struct GlyphDraw : Moveable<GlyphDraw> {
		GpuTextureId texture;
		Rectf uv = Rectf(0, 0, 0, 0);
		Pointf offset = Pointf(0, 0);
		Size size = Size(0, 0);
		double advance = 0;
		bool drawable = false;
	};

	struct TextImpl {
		static constexpr int ATLAS_SIZE = 1024;
		static constexpr int ATLAS_PADDING = 1;

		struct AtlasPage : Moveable<AtlasPage> {
			GpuTextureId texture;
			int cursor_x = ATLAS_PADDING;
			int cursor_y = ATLAS_PADDING;
			int row_height = 0;
		};

		struct GlyphEntry : Moveable<GlyphEntry> {
			int page = -1;
			Rect pixel_rect;
			Pointf offset = Pointf(0, 0);
			Size size = Size(0, 0);
			double advance = 0;
			bool drawable = false;
		};

		Vector<AtlasPage> pages;
		VectorMap<String, GlyphEntry> glyphs;
	};

	struct TextCleanup {
		UiRenderer2D *owner = nullptr;
		~TextCleanup();
	};

	struct VectorRaster : Moveable<VectorRaster> {
		Image image;
		Rgba8 tint = Rgba8(255, 255, 255, 255);
		bool alpha_mask = false;
		Rectf local_rect = Rectf(0, 0, 0, 0);
		bool drawable = false;
	};

	struct VectorImpl {
		struct CacheEntry : Moveable<CacheEntry> {
			UiDisplayOp op;
			Image image;
			Rectf local_rect = Rectf(0, 0, 0, 0);
			int raster_scale = 1;
			uint64 last_frame = 0;
		};

		Vector<CacheEntry> cache;
	};

	struct VectorCleanup {
		UiRenderer2D *owner = nullptr;
		~VectorCleanup();
	};

	GpuFormat working_color_format = GpuFormat::Unknown;
	float ColorChannel(byte channel) const;
	GpuDevice *device = nullptr;
	bool ready = false;
	String error;
	GpuShaderId vertex_shader;
	GpuShaderId fragment_shader;
	GpuShaderId textured_vertex_shader;
	GpuShaderId textured_fragment_shader;
	GpuShaderId mask_fragment_shader;
	GpuBufferId vertex_buffer;
	GpuBufferId textured_vertex_buffer;
	int64 vertex_buffer_capacity = 0;
	int64 textured_vertex_buffer_capacity = 0;
	Vector<PipelineEntry> pipelines;
	Vector<ImageCacheEntry> image_cache;
	Vector<Vertex> vertices;
	Vector<TexturedVertex> textured_vertices;
	Vector<Batch> batches;
	UiRenderer2DStats stats;
	UiRenderer2DCacheLimits cache_limits;
	uint64 cache_frame = 0;
	int frame_image_hits = 0;
	int frame_image_evictions = 0;
	int frame_vector_evictions = 0;
	int64 frame_image_upload_bytes = 0;
	bool RenderInternal(const UiDisplayList& list, const UiRenderer2DTarget& target);
	int64 ImageCacheBytes() const;
	int64 VectorCacheBytes() const;
	bool ReserveImageCache(int64 bytes);
	bool ReserveVectorCache(int64 bytes);
	void TrimCaches();
	void UpdateCacheStats();
	TextImpl *text_impl = nullptr;
	TextCleanup text_cleanup;
	VectorImpl *vector_impl = nullptr;
	VectorCleanup vector_cleanup;

	static Image PrepareImagePixels(const Image& image, GpuFormat format);
	bool EnsureShaders(bool textured, bool alpha_mask = false);
	bool EnsurePipeline(GpuFormat format, bool textured, GpuPipelineId& out,
	                   GpuBlendMode blend_mode = GpuBlendMode::SourceOver, bool alpha_mask = false);
	bool EnsureVertexBuffer(bool textured, int64 required_bytes);
	bool EnsureImageTexture(const Image& image, GpuTextureId& out);
	bool EnsureGlyph(Font font, int ch, GlyphDraw& out);
	bool EnsureVectorRaster(const UiDisplayOp& op, const Transform2D& transform, VectorRaster& out,
	                        UiRenderer2DStats& vector_stats);
	bool MaterializeVectorList(const UiDisplayList& source, UiDisplayList& out,
	                           UiRenderer2DStats& vector_stats);
	bool BuildGeometry(const UiDisplayList& list, Size target_size);
	bool Submit(const UiRenderer2DTarget& target, GpuPipelineId solid_pipeline,
	            GpuPipelineId invert_pipeline, GpuPipelineId textured_pipeline, GpuPipelineId mask_pipeline);
	bool Fail(const String& message);

	TextImpl& Text();
	void DestroyTextExtension();
	VectorImpl& VectorCache();
	void DestroyVectorExtension();

	// Byte-preserved Stage-4/image implementation entry points wrapped by Stage 5.
	bool BuildGeometryBase(const UiDisplayList& list, Size target_size);
	bool RenderBase(const UiDisplayList& list, const UiRenderer2DTarget& target);
	bool RenderFrameBase(const UiDisplayList& list, const GpuFrameInfo& frame,
	                     const GpuClearColor& clear_color);
	void CloseBase();
};

}
