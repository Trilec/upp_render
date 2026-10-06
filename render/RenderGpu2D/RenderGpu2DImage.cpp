#include "RenderGpu2D.h"

#include <cmath>

namespace Upp {

Image UiRenderer2D::PrepareImagePixels(const Image& image, GpuFormat format)
{
	// UNORM sampling already matches U++ premultiplied pixels; retain original bytes.
	if(image.IsEmpty() || (format != GpuFormat::RGBA8Srgb && format != GpuFormat::BGRA8Srgb))
		return image;

	ImageBuffer output(image.GetSize());
	for(int y = 0; y < image.GetHeight(); ++y) {
		const RGBA *source = image[y];
		RGBA *target = output[y];
		for(int x = 0; x < image.GetWidth(); ++x) {
			const RGBA& s = source[x];
			RGBA& d = target[x];
			d.a = s.a;
			// Hardware sRGB decoding must produce colour premultiplied in linear space.
			auto channel = [&](byte value) -> byte {
				if(!s.a) return 0;
				double c = min(1.0, (double)value / s.a);
				c = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
				c *= s.a / 255.0;
				c = c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
				return (byte)min(255, max(0, (int)std::round(c * 255)));
			};
			d.r = channel(s.r); d.g = channel(s.g); d.b = channel(s.b);
		}
	}
	return Image(output);
}

}
