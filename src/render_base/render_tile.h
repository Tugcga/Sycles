#pragma once
#include <xsi_renderercontext.h>

#include <vector>

#include "../utilities/math.h"
#include "../utilities/logs.h"
#include "write_tile_pixel.h"

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

	bool GetScanlineRGBA(unsigned int in_row, XSI::siImageBitDepth in_bit_depth, unsigned char *out_scanline) const
	{
		int pixel_size = 0;

		// one line store pixels channels (all four r, g, b and a)
		// so, each ixels contains four components
		if (in_bit_depth == XSI::siImageBitDepthInteger8) { pixel_size = 4; }  // if one component spend 1 byte, then pixel is 4 bytes
		else if (in_bit_depth == XSI::siImageBitDepthInteger16) { pixel_size = 8; }  // 16 bit integer is 2 bytes, so the pixel is 8 bytes
		else if (in_bit_depth == XSI::siImageBitDepthFloat32) { pixel_size = 16; }  // 32 bit float is 4 bytes, so the pixels is 16 bytes
		else { return false; }

		void* scanline;
		size_t index_shift = static_cast<size_t>(in_row) * width;  // start of the tile pixels line
		for (unsigned int i = 0; i < width; i++)
		{
			scanline = (void*)(&out_scanline[pixel_size * i]);
			// scanline is a pointer to the start of the pixels data
			size_t index = index_shift + i;
			if (in_bit_depth == XSI::siImageBitDepthInteger8) {write_int8_pixel(scanline, index, pixels, is_srgb, components); }
			else if (in_bit_depth == XSI::siImageBitDepthInteger16) { write_int16_pixel(scanline, index, pixels, is_srgb, components); }
			else if (in_bit_depth == XSI::siImageBitDepthFloat32) { write_float32_pixel(scanline, index, pixels, is_srgb, components); }
		}

		return true;
	}

private:
	unsigned int offset_x, offset_y, width, height;
	std::vector<float> pixels;
	bool is_srgb;
	int components;
};