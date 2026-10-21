#include "RenderGpu2D.h"
#include <RenderVector/RenderVector.h>
#include <cmath>
#include "../../shaders/BoxBlurSpirv.h"

namespace Upp {
namespace {
bool StateOp(UiDisplayOpType type)
{
	return type==UiDisplayOpType::Save || type==UiDisplayOpType::Restore ||
	       type==UiDisplayOpType::ClipRect || type==UiDisplayOpType::ConcatTransform;
}
bool Chunk(const UiDisplayList& source,int from,int end,UiDisplayList& out)
{
	UiDisplayListBuilder b;
	for(int i=0;i<from;++i) if(StateOp(source[i].type)) ReplayUiDisplayOp(source[i],b);
	for(int i=from;i<end;++i) ReplayUiDisplayOp(source[i],b);
	while(b.GetSaveDepth()>0) b.Restore();
	return b.Finish(out);
}
void AddStats(UiRenderer2DStats& total,const UiRenderer2DStats& s)
{
#define ADD(x) total.x += s.x
	ADD(primitive_count); ADD(emitted_primitive_count); ADD(clipped_primitive_count);
	ADD(image_count); ADD(text_run_count); ADD(glyph_count); ADD(glyph_cache_miss_count);
	ADD(glyph_atlas_upload_count); ADD(gpu_glyph_raster_count); ADD(cpu_glyph_raster_count);
	ADD(vector_op_count); ADD(vector_path_count); ADD(gradient_count); ADD(svg_count);
	ADD(vector_cache_miss_count); ADD(vector_raster_count); ADD(gpu_vector_raster_count);
	ADD(gpu_vector_draw_count); ADD(gpu_path_count); ADD(gpu_path_vertex_count);
	ADD(gpu_path_coverage_draw_count); ADD(gpu_path_coverage_render_count);
	ADD(texture_upload_count); ADD(triangle_count); ADD(vertex_count); ADD(textured_vertex_count);
	ADD(draw_count); ADD(batch_count); ADD(uploaded_bytes);
#undef ADD
}
}

bool UiRenderer2D::EnsureClipLayer(int depth,const UiRenderer2DTarget& target,int& slot)
{
	if(depth<0 || depth>=4 || target.size.cx<=0 || target.size.cy<=0 ||
	   target.size.cx>4096 || target.size.cy>4096)
		return Fail("GPU clip layer exceeds the depth/dimension budget");
	for(int i=0;i<clip_layers.GetCount();++i) {
		auto& layer=clip_layers[i];
		if(!layer.busy && layer.colour.IsValid() && layer.mask.IsValid() &&
		   layer.size==target.size && layer.format==target.color_format) {
			slot=i; layer.busy=true; layer.last_use=++clip_use_clock; return true;
		}
	}
	auto release=[&](ClipLayer& layer) {
		if(layer.colour.IsValid()) {
			device->DestroyTexture(layer.colour);
			clip_layer_bytes-=(int64)layer.size.cx*layer.size.cy*8;
		}
		if(layer.mask.IsValid()) device->DestroyTexture(layer.mask);
		layer.colour=layer.mask=GpuTextureId();
	};
	auto oldest=[&]() {
		int found=-1;
		for(int i=0;i<clip_layers.GetCount();++i)
			if(!clip_layers[i].busy && (found<0 || clip_layers[i].last_use<clip_layers[found].last_use)) found=i;
		return found;
	};
	const int64 bytes=(int64)target.size.cx*target.size.cy*8;
	if(bytes>64*1024*1024) return Fail("GPU clip layer exceeds the 64 MiB pixel budget");
	while(clip_layer_bytes+bytes>64*1024*1024) {
		int i=oldest(); if(i<0) return Fail("active GPU clip layers exceed the pixel budget");
		release(clip_layers[i]); clip_layers[i].last_use=++clip_use_clock;
		// Empty entries are reusable, so no further release can reclaim them.
		bool reclaimable=false;
		for(const auto& layer:clip_layers) reclaimable |= !layer.busy && layer.colour.IsValid();
		if(!reclaimable && clip_layer_bytes+bytes>64*1024*1024)
			return Fail("active GPU clip layers exceed the pixel budget");
	}
	slot=-1;
	for(int i=0;i<clip_layers.GetCount();++i)
		if(!clip_layers[i].busy && !clip_layers[i].colour.IsValid()) { slot=i; break; }
	if(slot<0) {
		if(clip_layers.GetCount()<32) { slot=clip_layers.GetCount(); clip_layers.Add(); }
		else { slot=oldest(); if(slot<0) return Fail("GPU clip pool is fully active"); release(clip_layers[slot]); }
	}
	auto& layer=clip_layers[slot]; layer.size=target.size; layer.format=target.color_format;
	GpuTextureDesc d; d.size=target.size; d.format=target.color_format;
	d.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_Sampled;
	if(device->CreateTexture(d,layer.colour)!=GpuResult::Ok) return Fail(device->GetLastError());
	if(device->CreateTexture(d,layer.mask)!=GpuResult::Ok) {
		device->DestroyTexture(layer.colour); layer.colour=GpuTextureId();
		return Fail(device->GetLastError());
	}
	clip_layer_bytes+=bytes; layer.busy=true; layer.last_use=++clip_use_clock;
	return true;
}
bool UiRenderer2D::EnsureBlurPipeline(GpuFormat format, GpuPipelineId& out)
{
	for(const auto& entry:blur_pipelines) if(entry.format==format) { out=entry.pipeline; return true; }
	if(!EnsureShaders(true)) return false;
	if(!blur_fragment_shader.IsValid()) {
		GpuShaderDesc shader; shader.stage=GpuShaderStage::Fragment; shader.format=GpuShaderFormat::SpirV;
		shader.code=String(reinterpret_cast<const char *>(BOX_BLUR_SPIRV),sizeof(BOX_BLUR_SPIRV));
		shader.label="Ui bounded box blur";
		if(device->CreateShader(shader,blur_fragment_shader)!=GpuResult::Ok) return Fail(device->GetLastError());
	}
	GpuPipelineDesc p; p.vertex_shader=textured_vertex_shader; p.fragment_shader=blur_fragment_shader;
	p.color_format=format; p.vertex_layout=GpuVertexLayout::Position2Uv2Color4F;
	p.sampled_texture_count=1; p.blend_mode=GpuBlendMode::Opaque;
	p.sampler_filter=GpuSamplerFilter::Nearest; p.sampler_address=GpuSamplerAddressMode::ClampToEdge;
	if(device->CreatePipeline(p,out)!=GpuResult::Ok) return Fail(device->GetLastError());
	auto& entry=blur_pipelines.Add(); entry.format=format; entry.pipeline=out;
	return true;
}
void UiRenderer2D::CloseClipLayers()
{
	for(auto& layer:clip_layers) {
		if(layer.colour.IsValid()) device->DestroyTexture(layer.colour);
		if(layer.mask.IsValid()) device->DestroyTexture(layer.mask);
	}
	clip_layers.Clear(); clip_layer_bytes=0;
	for(const auto& p:blur_pipelines) device->DestroyPipeline(p.pipeline);
	blur_pipelines.Clear();
	if(blur_fragment_shader.IsValid()) device->DestroyShader(blur_fragment_shader);
	blur_fragment_shader=GpuShaderId();
}
bool UiRenderer2D::RenderClippedList(const UiDisplayList& list,const UiRenderer2DTarget& target,int clip)
{
	if(++frame_clip_layers>64) return Fail("GPU frame exceeds the 64 clip-layer work budget");
	const int depth=clip_depth++;
	struct Guard { int& depth; bool active=true; ~Guard() { if(active) --depth; } } guard{clip_depth};

	int saved=0;
	for(int i=0;i<clip;++i) {
		if(list[i].type==UiDisplayOpType::Save) ++saved;
		if(list[i].type==UiDisplayOpType::Restore) --saved;
	}
	int end=list.GetCount(), nesting=saved;
	if(saved) for(int i=clip+1;i<list.GetCount();++i) {
		if(list[i].type==UiDisplayOpType::Save) ++nesting;
		else if(list[i].type==UiDisplayOpType::Restore && nesting--==saved) { end=i; break; }
	}
	UiRenderer2DStats total;
	UiDisplayList part;
	if(!Chunk(list,0,clip,part) || !RenderInternal(part,target)) return false;
	AddStats(total,stats);
	Rect bounds(target.origin,target.size);
	if(list[clip].type==UiDisplayOpType::ClipPath) {
		struct State : Moveable<State> { Transform2D transform; Rectf clip; };
		State state; state.clip=Rectf(bounds); Vector<State> stack;
		auto transformed=[&](Rectf r) {
			Rectf box; bool first=true;
			for(Pointf p:{r.TopLeft(),r.TopRight(),r.BottomLeft(),r.BottomRight()}) {
				p=state.transform.TransformPoint(p);
				if(first) { box=Rectf(p,p); first=false; }
				else { box.left=min(box.left,p.x); box.top=min(box.top,p.y);
				       box.right=max(box.right,p.x); box.bottom=max(box.bottom,p.y); }
			}
			return box;
		};
		for(int i=0;i<clip;++i) {
			const auto& op=list[i];
			if(op.type==UiDisplayOpType::Save) stack.Add(state);
			else if(op.type==UiDisplayOpType::Restore && !stack.IsEmpty()) state=stack.Pop();
			else if(op.type==UiDisplayOpType::ConcatTransform) state.transform=state.transform*op.transform;
			else if(op.type==UiDisplayOpType::ClipRect) {
				Rectf r=transformed(op.rect); state.clip &= r;
			}
		}
		Rectf r=transformed(list[clip].path.GetControlBounds());
		if(!std::isfinite(r.left)||!std::isfinite(r.top)||!std::isfinite(r.right)||!std::isfinite(r.bottom) ||
		   fabs(r.left)>1e7||fabs(r.top)>1e7||fabs(r.right)>1e7||fabs(r.bottom)>1e7)
			return Fail("GPU clip bounds exceed the coordinate budget");
		r.Inflate(1,1); r &= state.clip;
		bounds &= Rect((int)floor(r.left),(int)floor(r.top),(int)ceil(r.right),(int)ceil(r.bottom));
	}
	UiRenderer2DTarget output=target; output.load_op=GpuLoadOp::Load;
	if(bounds.IsEmpty()) {
		--clip_depth; guard.active=false;
		if(end<list.GetCount()) {
			if(!Chunk(list,end+1,list.GetCount(),part) || !RenderInternal(part,output)) return false;
			AddStats(total,stats);
		}
		stats=total; stats.display_op_count=list.GetCount(); return true;
	}
	UiRenderer2DTarget body=target; body.size=bounds.GetSize(); body.origin=bounds.TopLeft();
	int slot;
	if(!EnsureClipLayer(depth,body,slot)) return false;
	ClipLayer& layer=clip_layers[slot];
	struct Release { bool& busy; ~Release() { busy=false; } } release{layer.busy};
	body.color_target=layer.colour; body.load_op=GpuLoadOp::Clear; body.clear_color=GpuClearColor();
	body.clear_color.alpha=0;
	if(!Chunk(list,clip+1,end,part) || !RenderInternal(part,body)) return false;
	AddStats(total,stats);

	GpuTextureId composed=layer.colour;
	if(list[clip].type==UiDisplayOpType::BlurLayer) {
		UiRenderer2DTarget filtered=body; filtered.color_target=layer.mask; filtered.color_format=layer.format;
		if(!RenderTexturePass(layer.colour,filtered,GpuBlendMode::Opaque,true)) return false;
		AddStats(total,stats); composed=layer.mask;
	} else {
	UiDisplayListBuilder b;
	for(int i=0;i<clip;++i) if(StateOp(list[i].type)) ReplayUiDisplayOp(list[i],b);
	b.FillPath(list[clip].path,UiPaint::Solid(Rgba8(255,255,255)),list[clip].fill_rule);
	while(b.GetSaveDepth()>0) b.Restore();
	if(!b.Finish(part)) return Fail("GPU clip mask list failed");
	UiRenderer2DTarget mask=body; mask.color_target=layer.mask; mask.color_format=layer.format;
	if(!RenderInternal(part,mask)) return false;
	AddStats(total,stats);
	body.load_op=GpuLoadOp::Load;
	if(!RenderTexture(layer.mask,body,GpuBlendMode::DestinationIn)) return false;
	AddStats(total,stats);
	}
	if(!RenderTexturePass(composed,output,GpuBlendMode::PremultipliedSourceOver,false,&bounds)) return false;
	AddStats(total,stats);
	layer.busy=false;
	--clip_depth; guard.active=false;
	if(end<list.GetCount()) {
		if(!Chunk(list,end+1,list.GetCount(),part) || !RenderInternal(part,output)) return false;
		AddStats(total,stats);
	}
	stats=total; stats.display_op_count=list.GetCount();
	return true;
}
}
