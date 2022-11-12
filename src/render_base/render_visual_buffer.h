#pragma once
#include <xsi_dataarray.h>

#include <vector>

class RenderVisualBuffer
{
public:
	RenderVisualBuffer() { pixels.resize(0); };
	RenderVisualBuffer(ULONG image_width, ULONG image_height, ULONG crop_left, ULONG crop_bottom, ULONG crop_width, ULONG crop_height, ULONG coms) :
		full_width(image_width),
		full_height(image_height),
		corner_x(crop_left),
		corner_y(crop_bottom),
		width(crop_width),
		height(crop_height),
		components(coms)
	{
		pixels.resize(width * height * components);
	};
	~RenderVisualBuffer()
	{
		pixels.clear();
		pixels.shrink_to_fit();
	};

	void clear()
	{
		pixels.clear();
		pixels.shrink_to_fit();
	};

public:
	unsigned int full_width, full_height;  // full frame size
	unsigned int corner_x, corner_y;  // coordinates of the left bottom corner
	unsigned int width, height;  // actual render size
	ULONG components;

	std::vector<float> pixels;
};