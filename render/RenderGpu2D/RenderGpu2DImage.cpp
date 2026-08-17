#include "RenderGpu2D.h"

namespace Upp {

Image UiRenderer2D::Unmultiply(const Image& image)
{
	if(image.IsEmpty())
		return Image();

	ImageBuffer output(image.GetSize());
	for(int y = 0; y < image.GetHeight(); ++y) {
		const RGBA *source = image[y];
		RGBA *target = output[y];
		for(int x = 0; x < image.GetWidth(); ++x) {
			const RGBA& s = source[x];
			RGBA& d = target[x];
			d.a = s.a;
			if(s.a == 0) {
				d.r = d.g = d.b = 0;
				continue;
			}
			const int half_alpha = s.a / 2;
			d.r = (byte)min(255, ((int)s.r * 255 + half_alpha) / (int)s.a);
			d.g = (byte)min(255, ((int)s.g * 255 + half_alpha) / (int)s.a);
			d.b = (byte)min(255, ((int)s.b * 255 + half_alpha) / (int)s.a);
		}
	}
	return Image(output);
}

}
