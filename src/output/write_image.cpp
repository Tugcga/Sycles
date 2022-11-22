#include <vector>
#include <fstream>

#include <xsi_string.h>

#include "../utilities/logs.h"
#include "../render_cycles/cyc_output/output_context.h"
#include "write_image.h"
#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../utilities/stb_image_write.h"

#define TINYEXR_IMPLEMENTATION
#include "../utilities/tinyexr.h"

OIIO::TypeDesc xsi_key_to_data_type(int key)
{
	if (key == 3) { return OIIO::TypeDesc::UINT8; }
	else if (key == 4) { return OIIO::TypeDesc::UINT16; }
	else if (key == 5) { return OIIO::TypeDesc::UINT32; }
	else if (key == 20) { return OIIO::TypeDesc::HALF; }
	else if (key == 21) { return OIIO::TypeDesc::FLOAT; }
	else { return OIIO::TypeDesc::UINT8; }
}

float clamp_float(float value, float min, float max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

void write_output_ppm(size_t width, size_t height, size_t components, const std::string &file_path, float* output_pixels)
{
	std::ofstream file(file_path);
	file << "P3\n" << width << ' ' << height << "\n255\n";
	for (int j = height - 1; j >= 0; --j)
	{
		for (int i = 0; i < width; ++i)
		{
			float r = 256 * clamp_float(output_pixels[(j * width + i) * components], 0.0, 0.99);
			float g = 256 * clamp_float(output_pixels[(j * width + i) * components + 1], 0.0, 0.99);
			float b = 256 * clamp_float(output_pixels[(j * width + i) * components + 2], 0.0, 0.99);

			file << static_cast<int>(r) << ' '
				 << static_cast<int>(g) << ' '
				 << static_cast<int>(b) << '\n';
		}
	}

	file.close();
}

void write_output_pfm(size_t width, size_t height, size_t components, const std::string &file_path, float* pixels)
{
	std::ofstream file(file_path, std::ios::binary);

	if (!file.fail())
	{
		file << "PF" << std::endl;
		file << width << " " << height << std::endl;
		file << "-1.0" << std::endl;

		for (int h = 0; h < height; ++h)
		{
			for (int w = 0; w < width; ++w)
			{
				for (int c = 0; c < 3; ++c)
				{
					const float x = pixels[(h * width + w) * components + c];
					file.write((char*)&x, sizeof(float));
				}
			}
		}
	}
	else
	{
		log_message(XSI::CString("Fails to save the file ") + XSI::CString(file_path.c_str()), XSI::siWarningMsg);
	}
}

bool write_output_exr(size_t width, size_t height, size_t components, const std::string &file_path, float* pixels)
{
	// we should flip vertical coordinates of pixels
	std::vector<float> flip_pixels(width * height * components, 0.0f);
	LONG row = 0;
	LONG column = 0;
	LONG flip_pixel = 0;
	for (LONG pixel = 0; pixel < width * height; pixel++)
	{
		row = pixel / width;
		column = pixel - row * width;
		flip_pixel = (height - row - 1) * width + column;
		// copy all input components
		for (LONG c = 0; c < components; c++)
		{
			flip_pixels[components * flip_pixel + c] = pixels[components * pixel + c];
		}
	}

	const char* err;
	int out = SaveEXR(pixels, width, height, components, 0, file_path.c_str(), &err);
	flip_pixels.clear();
	flip_pixels.shrink_to_fit();
	return out == 0;
}

bool write_output_hdr(size_t width, size_t height, size_t components, const std::string &file_path, float* pixels)
{
	int out = stbi_write_hdr(file_path.c_str(), width, height, components, &pixels[0]);
	return out > 0;
}

bool write_output_ldr_stb(size_t width, size_t height, size_t components, float* pixels, const std::string &file_path, const std::string &file_ext)
{
	size_t otput_pixels_size = static_cast<size_t>(width) * height * components;
	unsigned char* u_pixels = new unsigned char[otput_pixels_size];
	for (size_t y = 0; y < height; y++)
	{
		for (size_t x = 0; x < width; x++)
		{
			for (size_t c = 0; c < components; c++)
			{
				u_pixels[(y * width + x) * components + c] = static_cast<unsigned char>(int(clamp_float(pixels[(y * width + x) * components + c], 0.0f, 1.0f) * 255.99f));
			}
		}
	}

	int out = 0;

	if (file_ext == "png")
	{
		out = stbi_write_png(file_path.c_str(), width, height, components, u_pixels, width * components);
	}
	else if (file_ext == "bmp")
	{
		out = stbi_write_bmp(file_path.c_str(), width, height, components, u_pixels);
	}
	else if (file_ext == "tga")
	{
		out = stbi_write_tga(file_path.c_str(), width, height, components, u_pixels);
	}
	else if (file_ext == "jpg")
	{
		out = stbi_write_jpg(file_path.c_str(), width, height, components, u_pixels, 100);
	}

	delete[]u_pixels;

	return out > 0;
}

void write_output_oiio(size_t width, size_t height, ccl::ustring output_path, std::vector<float>& output_pixels, int write_components, ccl::TypeDesc out_type)
{
	// create output
	//std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create(output_path);
	OIIO::ImageOutput::unique_ptr out = OIIO::ImageOutput::create(output_path);

	// create spec
	OIIO::ImageSpec out_spec = OIIO::ImageSpec(width, height, write_components, out_type);

	// open to write
	out->open(std::string(output_path), out_spec);
	out->write_image(OIIO::TypeDesc::FLOAT, &output_pixels[0]);
	out->close();
}

void write_outputs(OutputContext* output_context)
{
	size_t width = output_context->get_width();
	size_t height = output_context->get_height();
	for (size_t i = 0; i < output_context->get_output_passes_count(); i++)
	{
		int buffer_components = output_context->get_output_pass_components(i);
		if (buffer_components <= 0)
		{
			continue;
		}
		int write_components = output_context->get_output_pass_write_components(i);

		// create separate array with pixels for output
		std::vector<float> output_pixels(width * height * write_components);
		float* pixels = output_context->get_output_pass_pixels(i);
		convert_with_components(width, height, buffer_components, write_components, pixels, &output_pixels[0]);

		// now we are ready to write output pixels into file
		std::string output_ext = output_context->get_output_pass_format(i).c_str();
		std::string output_path = output_context->get_output_pass_path(i).c_str();
		ccl::TypeDesc out_type = xsi_key_to_data_type(output_context->get_output_pass_bits(i));
		if (output_ext == "pfm")
		{
			write_output_pfm(width, height, write_components, output_path, &output_pixels[0]);
		}
		else if (output_ext == "ppm")
		{
			write_output_ppm(width, height, write_components, output_path, &output_pixels[0]);
		}
		else if (output_ext == "exr")
		{
			write_output_exr(width, height, write_components, output_path, &output_pixels[0]);
		}
		else if (output_ext == "png" || output_ext == "bmp" || output_ext == "tga" || output_ext == "jpg")
		{
			write_output_ldr_stb(width, height, write_components, &output_pixels[0], output_path, output_ext);
		}
		else if (output_ext == "hdr")
		{
			write_output_hdr(width, height, write_components, output_path, &output_pixels[0]);
		}
		else
		{
			log_message("Unknown output format: " + XSI::CString(output_ext.c_str()), XSI::siWarningMsg);
		}
		// there are some bags in writing image by OIIO
		// the memory is not clear after write process
		// there is an error when Softimage is closed
		// so, use manual outputs instead one universal with OpenImageIO
		//write_output_oiio(width, height, output_path, output_pixels, write_components, out_type);

		// clear output pixels
		output_pixels.clear();
		output_pixels.shrink_to_fit();
	}
}