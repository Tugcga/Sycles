#pragma once
#include <xsi_renderercontext.h>

#include <vector>

struct RGBA
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
};

inline unsigned char linearToSRGB(float v)
{
	if (v <= 0.0f)
		return 0;
	if (v >= 1.0f)
		return 255;
	if (v <= 0.0031308f)
		return  (unsigned char)((12.92f * v * 255.0f) + 0.5f);
	return (unsigned char)(((1.055f * pow(v, 1.0f / 2.4f)) - 0.055f) * 255.0f + 0.5f);
}

inline unsigned char linearClamp(float v)
{
	if (v <= 0.0f)
		return 0;
	if (v >= 1.0f)
		return 255;
	return (unsigned char)(v * 255.0);
}

class RenderTile : public XSI::RendererImageFragment
{
public:
	RenderTile(int in_offX, int in_offY, int in_width, int in_height, std::vector<float> &_pixels, bool _apply_srgb, int _components)
	{
		offX = in_offX;
		offY = in_offY;
		width = in_width;
		height = in_height;
		fPixels = _pixels;
		isSRGB = _apply_srgb;
		components = _components;
	}

	unsigned int GetOffsetX() const { return(offX); }
	unsigned int GetOffsetY() const { return(offY); }
	unsigned int GetWidth() const { return(width); }
	unsigned int GetHeight() const { return(height); }

	bool GetScanlineRGBA(unsigned int in_uiRow, XSI::siImageBitDepth in_eBitDepth, unsigned char *out_pScanline) const
	{
		RGBA* pScanline = (RGBA*)out_pScanline;
		for (unsigned int i = 0; i < width; i++)
		{
			size_t indexShift = static_cast<size_t>(in_uiRow) * width;
			pScanline[i].a = components == 4 ? static_cast<unsigned char>(fPixels[4 * (indexShift + i) + 3] * 255.0) : static_cast<unsigned char>(255.0);
			if (isSRGB)
			{
				if (components == 4)
				{
					pScanline[i].r = linearToSRGB(fPixels[4 * (indexShift + i)]);
					pScanline[i].g = linearToSRGB(fPixels[4 * (indexShift + i) + 1]);
					pScanline[i].b = linearToSRGB(fPixels[4 * (indexShift + i) + 2]);
				}
				else
				{
					float v = linearToSRGB(fPixels[indexShift + i]);
					pScanline[i].r = static_cast<unsigned char>(v);
					pScanline[i].g = static_cast<unsigned char>(v);
					pScanline[i].b = static_cast<unsigned char>(v);
				}
			}
			else
			{
				if (components == 4)
				{
					pScanline[i].r = linearClamp(fPixels[4 * (indexShift + i)]);
					pScanline[i].g = linearClamp(fPixels[4 * (indexShift + i) + 1]);
					pScanline[i].b = linearClamp(fPixels[4 * (indexShift + i) + 2]);
				}
				else
				{
					float v = linearClamp(fPixels[indexShift + i]);
					pScanline[i].r = static_cast<unsigned char>(v);
					pScanline[i].g = static_cast<unsigned char>(v);
					pScanline[i].b = static_cast<unsigned char>(v);
				}
			}
		}

		return true;
	}

private:
	unsigned int offX, offY, width, height;
	std::vector<float> fPixels;
	bool isSRGB;
	int components;
};