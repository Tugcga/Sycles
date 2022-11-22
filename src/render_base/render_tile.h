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

inline unsigned char linear_to_srgb(float v)
{
	if (v <= 0.0f)
		return 0;
	if (v >= 1.0f)
		return 255;
	if (v <= 0.0031308f)
		return  (unsigned char)((12.92f * v * 255.0f) + 0.5f);
	return (unsigned char)(((1.055f * pow(v, 1.0f / 2.4f)) - 0.055f) * 255.0f + 0.5f);
}

inline unsigned char linear_clamp(float v)
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
	RenderTile(unsigned int in_offset_x, unsigned int in_offset_y, unsigned int in_width, unsigned int in_height, std::vector<float> &_pixels, bool _apply_srgb, int _components)
	{
		offset_x = in_offset_x;
		offset_y = in_offset_y;
		width = in_width;
		height = in_height;
		pixels = _pixels;
		is_srgb = _apply_srgb;
		components = _components;
	}

	unsigned int GetOffsetX() const { return(offset_x); }
	unsigned int GetOffsetY() const { return(offset_y); }
	unsigned int GetWidth() const { return(width); }
	unsigned int GetHeight() const { return(height); }

	bool GetScanlineRGBA(unsigned int in_uiRow, XSI::siImageBitDepth in_eBitDepth, unsigned char *out_pScanline) const
	{
		RGBA* pScanline = (RGBA*)out_pScanline;
		for (unsigned int i = 0; i < width; i++)
		{
			size_t indexShift = static_cast<size_t>(in_uiRow) * width;
			pScanline[i].a = components == 4 ? static_cast<unsigned char>(pixels[4 * (indexShift + i) + 3] * 255.0) : static_cast<unsigned char>(255.0);
			if (is_srgb)
			{
				if (components == 4)
				{
					pScanline[i].r = linear_to_srgb(pixels[4 * (indexShift + i)]);
					pScanline[i].g = linear_to_srgb(pixels[4 * (indexShift + i) + 1]);
					pScanline[i].b = linear_to_srgb(pixels[4 * (indexShift + i) + 2]);
				}
				else if (components == 3)
				{
					pScanline[i].r = linear_to_srgb(pixels[3 * (indexShift + i)]);
					pScanline[i].g = linear_to_srgb(pixels[3 * (indexShift + i) + 1]);
					pScanline[i].b = linear_to_srgb(pixels[3 * (indexShift + i) + 2]);
				}
				else
				{
					float v = linear_to_srgb(pixels[indexShift + i]);
					pScanline[i].r = static_cast<unsigned char>(v);
					pScanline[i].g = static_cast<unsigned char>(v);
					pScanline[i].b = static_cast<unsigned char>(v);
				}
			}
			else
			{
				if (components == 4)
				{
					pScanline[i].r = linear_clamp(pixels[4 * (indexShift + i)]);
					pScanline[i].g = linear_clamp(pixels[4 * (indexShift + i) + 1]);
					pScanline[i].b = linear_clamp(pixels[4 * (indexShift + i) + 2]);
				}
				else if (components == 3)
				{
					pScanline[i].r = linear_clamp(pixels[3 * (indexShift + i)]);
					pScanline[i].g = linear_clamp(pixels[3 * (indexShift + i) + 1]);
					pScanline[i].b = linear_clamp(pixels[3 * (indexShift + i) + 2]);
				}
				else
				{
					float v = linear_clamp(pixels[indexShift + i]);
					pScanline[i].r = static_cast<unsigned char>(v);
					pScanline[i].g = static_cast<unsigned char>(v);
					pScanline[i].b = static_cast<unsigned char>(v);
				}
			}
		}

		return true;
	}

private:
	unsigned int offset_x, offset_y, width, height;
	std::vector<float> pixels;
	bool is_srgb;
	int components;
};