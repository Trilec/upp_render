#include "GpuCapture.h"
#include <GpuRender/RenderCtrlBridge.h>
#include <RenderVulkan/RenderVulkanRhi.h>
#include <plugin/png/png.h>
#include <cstring>

namespace Upp {
bool CaptureGpuUiClient(GpuTopWindow& window,const String& name,String& evidence)
{
	UiDisplayList list; String detail;
	bool ok=RecordCtrlDisplayList(window,list,detail);
	VulkanSurfaceSession session;
	GpuNativeWindowDesc native; native.kind=GpuNativeWindowKind::Win32;
	native.handle=(uintptr_t)window.GetHWND();
	if(ok) ok=session.Open(true,native) && session.IsReady();
	if(ok) {
		VulkanGpuDevice device(session);
		GpuTextureDesc desc; desc.size=window.GetSize(); desc.format=GpuFormat::RGBA8Srgb;
		desc.usage=GpuTextureUsage_ColorAttachment|GpuTextureUsage_TransferSrc;
		GpuTextureId texture;
		ok=device.CreateTexture(desc,texture)==GpuResult::Ok;
		if(ok) {
			{
				UiRenderer2D renderer(device); UiRenderer2DTarget target;
				target.color_target=texture; target.size=desc.size; target.color_format=desc.format;
				target.clear_color.alpha=1;
				ok=renderer.Render(list,target);
				if(!ok) detail=renderer.GetError();
				Vector<byte> pixels;
				if(ok) ok=device.ReadTexturePixels(texture,pixels) && pixels.GetCount()==desc.size.cx*desc.size.cy*4;
				if(ok) {
					ImageBuffer image(desc.size);
					for(int y=0;y<image.GetHeight();++y) for(int x=0;x<image.GetWidth();++x) {
						const byte *p=pixels.Begin()+4*(y*image.GetWidth()+x);
						RGBA& c=image[y][x]; c.r=p[0]; c.g=p[1]; c.b=p[2]; c.a=p[3];
					}
					image.SetKind(IMAGE_OPAQUE);
					ok=PNGEncoder().SaveFile(GetExeDirFile(name+".png"),Image(image));
				}
				const auto& stats=renderer.GetStats();
				evidence<<name<<"="<<(ok?"PASS":"FAIL")<<" size="<<desc.size
				        <<" cpu_vector_rasters="<<stats.vector_raster_count
				        <<" cpu_glyph_rasters="<<stats.cpu_glyph_raster_count
				        <<" gpu_composition_layers="<<stats.gpu_clip_layer_count<<" error="<<detail<<"\n";
			}
			ok &= device.DestroyTexture(texture)==GpuResult::Ok;
		}
	}
	else evidence<<name<<"=FAIL "<<detail<<" "<<session.GetError()<<"\n";
	session.Close();
	ok &= session.GetReport().validation_warning_count==0 && session.GetReport().validation_error_count==0;
	return ok;
}
}
