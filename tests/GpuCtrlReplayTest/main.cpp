#include <Core/Core.h>
#include <GpuRender/GpuCtrlTestHooks.h>
#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderNull/RenderNull.h>

using namespace Upp;
using namespace Upp::GpuCtrlTestHooks;

static bool Check(bool condition, const char *message) { if(!condition) Cout() << "FAIL: " << message << EOL; return condition; }
static GpuClearColor ToClearColor(Rgba8 color)
{
	const float s = 1.0f / 255.0f;
	GpuClearColor out; out.red = color.r * s; out.green = color.g * s; out.blue = color.b * s; out.alpha = color.a * s; return out;
}

CONSOLE_APP_MAIN
{
	bool ok = true; UiDisplayList list; Rgba8 background; String error;
	ok &= Check(BuildDefaultFrame(Size(200, 120), list, background, error), "default Stage-4 GpuCtrl frame should build");
	ok &= Check(error.IsEmpty() && list.IsValid(), "default GpuCtrl frame should be a valid immutable display list");
	ok &= Check(background.r == 20 && background.g == 61 && background.b == 148 && background.a == 255, "default GpuCtrl clear colour should remain deterministic");
	int saves=0, restores=0, clips=0, transforms=0, fills=0, strokes=0, rounded=0, translucent=0; bool has_general_affine=false;
	for(int i=0;i<list.GetCount();++i) {
		const UiDisplayOp& op=list[i];
		switch(op.type) {
		case UiDisplayOpType::Save: ++saves; break;
		case UiDisplayOpType::Restore: ++restores; break;
		case UiDisplayOpType::ClipRect: ++clips; break;
		case UiDisplayOpType::ConcatTransform: ++transforms; has_general_affine = has_general_affine || op.transform.x.y != 0.0 || op.transform.y.x != 0.0 || op.transform.x.x != 1.0 || op.transform.y.y != 1.0; break;
		case UiDisplayOpType::FillRect: ++fills; if(op.color.a < 255) ++translucent; break;
		case UiDisplayOpType::StrokeRect: ++strokes; if(op.color.a < 255) ++translucent; break;
		case UiDisplayOpType::FillRoundedRect: ++rounded; if(op.color.a < 255) ++translucent; break;
		}
	}
	ok &= Check(list.GetCount() == 8, "default GpuCtrl scene should contain eight deterministic operations");
	ok &= Check(saves == 1 && restores == 1 && clips == 1 && transforms == 1, "default scene should exercise Save/Restore, clipping, and transform state");
	ok &= Check(fills == 2 && strokes == 1 && rounded == 1, "default scene should exercise fill, stroke, and rounded primitives");
	ok &= Check(has_general_affine, "default scene must exercise a non-translation affine transform");
	ok &= Check(translucent >= 3, "default scene should exercise translucent source-over primitives");
	NullGpuDevice device; GpuTextureDesc target_desc; target_desc.size=Size(200,120); target_desc.format=GpuFormat::RGBA8; target_desc.usage=GpuTextureUsage_ColorAttachment; GpuTextureId target;
	ok &= Check(device.CreateTexture(target_desc, target) == GpuResult::Ok, "headless GpuCtrl target should create");
	{
		UiRenderer2D renderer(device); ok &= Check(renderer.IsReady(), "production UiRenderer2D should accept the Null validation backend");
		UiRenderer2DTarget render_target; render_target.color_target=target; render_target.size=target_desc.size; render_target.color_format=target_desc.format; render_target.load_op=GpuLoadOp::Clear; render_target.store_op=GpuStoreOp::Store; render_target.clear_color=ToClearColor(background);
		ok &= Check(renderer.Render(list, render_target), "live GpuCtrl display list should replay through UiRenderer2D");
		const UiRenderer2DStats& stats=renderer.GetStats();
		ok &= Check(stats.display_op_count == 8 && stats.primitive_count == 4, "renderer should consume the complete control scene without a private replay authority");
		ok &= Check(stats.draw_count == 1 && stats.batch_count == 1 && stats.vertex_count > 0, "compatible control primitives should render in one GPU batch");
		ok &= Check(stats.translucent_vertex_count > 0, "control-scene alpha should survive into the GPU vertex stream");
		GpuPipelineId pipeline; pipeline.value=1; GpuPipelineDesc pipeline_desc;
		ok &= Check(device.GetPipelineDesc(pipeline, pipeline_desc) && pipeline_desc.blend_mode == GpuBlendMode::SourceOver, "live control scene should use the explicit source-over pipeline contract");
	}
	ok &= Check(device.DestroyTexture(target) == GpuResult::Ok, "headless GpuCtrl target should destroy after renderer shutdown");
	UiDisplayList zero_list; Rgba8 zero_background; String zero_error;
	ok &= Check(BuildDefaultFrame(Size(0, 0), zero_list, zero_background, zero_error), "zero-size control scene should remain a valid empty display list");
	ok &= Check(zero_list.IsValid() && zero_list.GetCount() == 0 && zero_error.IsEmpty(), "zero-size control scene should not invent drawable geometry");
	if(ok) { Cout() << "GpuCtrlReplayTest passed" << EOL; return; }
	SetExitCode(1);
}
