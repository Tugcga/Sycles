#include <vector>
#include <fstream>

#include <xsi_string.h>

#include "OpenEXR\ImfRgbaFile.h"
#include <OpenEXR\ImfChannelList.h>
#include <OpenEXR\ImfOutputFile.h>

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

// return true if extension corresponds to the ldr image format,
// flase if it hdr
bool is_ext_ldr(std::string ext)
{
	if (ext == "png" || ext == "bmp" || ext == "tga" || ext == "jpg" || ext == "ppm")
	{
		return true;
	}
	else
	{
		return false;
	}
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
	size_t output_pixels_size = static_cast<size_t>(width) * height * components;
	unsigned char* u_pixels = new unsigned char[output_pixels_size];
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

void write_outputs_separate_passes(OutputContext* output_context, ColorTransformContext* color_transform_context, size_t width, size_t height)
{
	for (size_t i = 0; i < output_context->get_output_passes_count(); i++)
	{
		int buffer_components = output_context->get_output_pass_components(i);
		if (buffer_components <= 0)
		{
			continue;
		}
		int write_components = output_context->get_output_pass_write_components(i);
		std::string output_ext = output_context->get_output_pass_format(i).c_str();

		// create separate array with pixels for output
		std::vector<float> output_pixels(width * height * write_components);
		float* pixels = output_context->get_output_pass_pixels(i);
		bool need_flip = !(output_ext == "pfm" || output_ext == "ppm");  // pfm and ppm does not require the flip
		convert_with_components(width, height, buffer_components, write_components, need_flip, pixels, &output_pixels[0]);

		// apply color correction to ldr combined pass, if we need this
		if (output_context->get_output_pass_type(i) == ccl::PASS_COMBINED && is_ext_ldr(output_ext))
		{
			// if color correction is disabled in parameters, then transform context skip the process (it know when it should apply process)
			color_transform_context->apply(width, height, write_components, &output_pixels[0]);
		}

		// now we are ready to write output pixels into file
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

void write_multilayer_exr(size_t width, size_t height, OutputContext* output_context)
{
	int passes_count = output_context->get_output_passes_count();
	size_t pixels_count = width * height;
	if (passes_count > 0)
	{
		// create path to save output exr file
		std::string first_path = std::string(output_context->get_output_pass_path(0).c_str());
		size_t last_slash = first_path.find_last_of("/\\");
		std::string to_save_path = first_path.substr(0, last_slash + 1) + output_context->get_common_path().GetAsciiString() + +"Combined." + std::to_string(output_context->get_render_frame()) + ".exr";
		
		// prepare channel buffers for all passes
		// we should calculate how many r, g, b and a channels we have in all passes
		// and then create vectors for each channel of the corresponding size
		// also we should find channel shifts for each pass
		std::vector<size_t> r_starts(passes_count, 0);
		std::vector<size_t> g_starts(passes_count, 0);
		std::vector<size_t> b_starts(passes_count, 0);
		std::vector<size_t> a_starts(passes_count, 0);
		size_t r_channels = 0;
		size_t g_channels = 0;
		size_t b_channels = 0;
		size_t a_channels = 0;
		// start from labels (if it exists)
		if (output_context->get_is_labels())
		{
			//lables buffer always contains 4 components
			r_channels++;
			g_channels++;
			b_channels++;
			a_channels++;
		}

		for (int i = 0; i < passes_count; i++)
		{
			int components_count = output_context->get_output_pass_components(i);
			r_starts[i] = r_channels;
			g_starts[i] = g_channels;
			b_starts[i] = b_channels;
			a_starts[i] = a_channels;
			if (components_count >= 1) { r_channels++; }
			if (components_count >= 2) { g_channels++; }
			if (components_count >= 3) { b_channels++; }
			if (components_count >= 4) { a_channels++; }
		}

		// create pixels for all channels
		std::vector<float> r_pixels(r_channels * pixels_count, 0.0f);
		std::vector<float> g_pixels(g_channels * pixels_count, 0.0f);
		std::vector<float> b_pixels(b_channels * pixels_count, 0.0f);
		std::vector<float> a_pixels(a_channels * pixels_count, 0.0f);

		Imf::Header header(width, height);
		Imf::FrameBuffer frameBuffer;
		// add labels
		if (output_context->get_is_labels())
		{
			header.channels().insert("Labels.R", Imf::Channel(Imf::FLOAT));
			header.channels().insert("Labels.G", Imf::Channel(Imf::FLOAT));
			header.channels().insert("Labels.B", Imf::Channel(Imf::FLOAT));
			header.channels().insert("Labels.A", Imf::Channel(Imf::FLOAT));

			output_context->extract_lables_channel(0, &r_pixels[0], true);
			output_context->extract_lables_channel(0, &g_pixels[0], true);
			output_context->extract_lables_channel(0, &b_pixels[0], true);
			output_context->extract_lables_channel(0, &a_pixels[0], true);

			frameBuffer.insert("Labels.R", Imf::Slice(Imf::FLOAT, (char*)&r_pixels[0], sizeof(float), sizeof(float) * width));
			frameBuffer.insert("Labels.G", Imf::Slice(Imf::FLOAT, (char*)&g_pixels[0], sizeof(float), sizeof(float) * width));
			frameBuffer.insert("Labels.B", Imf::Slice(Imf::FLOAT, (char*)&b_pixels[0], sizeof(float), sizeof(float) * width));
			frameBuffer.insert("Labels.A", Imf::Slice(Imf::FLOAT, (char*)&a_pixels[0], sizeof(float), sizeof(float) * width));
		}

		for (int i = 0; i < passes_count; i++)
		{
			int components_count = output_context->get_output_pass_components(i);
			std::string pass_name = output_context->get_output_pass_name(i).c_str();

			if (components_count >= 1)
			{
				std::string channel_name = pass_name + (components_count == 1 ? ".A" : ".R");
				header.channels().insert(channel_name, Imf::Channel(Imf::FLOAT));

				// we should extract pixel channels to the proper place in the global array
				size_t r_place = pixels_count * r_starts[i];
				output_context->extract_output_channel(i, 0, &r_pixels[r_place], true);

				// for one component image we save the channel as A instead of R
				frameBuffer.insert(channel_name, Imf::Slice(Imf::FLOAT, (char*)&r_pixels[r_place], sizeof(float), sizeof(float) * width));
			}

			if (components_count >= 3)
			{
				
				header.channels().insert(pass_name + ".G", Imf::Channel(Imf::FLOAT));
				header.channels().insert(pass_name + ".B", Imf::Channel(Imf::FLOAT));

				size_t g_place = pixels_count * g_starts[i];
				size_t b_place = pixels_count * b_starts[i];
				
				output_context->extract_output_channel(i, 1, &g_pixels[g_place], true);
				output_context->extract_output_channel(i, 2, &b_pixels[b_place], true);
				
				frameBuffer.insert(pass_name + ".G", Imf::Slice(Imf::FLOAT, (char*)&g_pixels[g_place], sizeof(float), sizeof(float) * width));
				frameBuffer.insert(pass_name + ".B", Imf::Slice(Imf::FLOAT, (char*)&b_pixels[b_place], sizeof(float), sizeof(float) * width));
			}
			if (components_count >= 4)
			{
				header.channels().insert(pass_name + ".A", Imf::Channel(Imf::FLOAT));

				size_t a_place = pixels_count * a_starts[i];
				output_context->extract_output_channel(i, 3, &a_pixels[a_place], true);
				frameBuffer.insert(pass_name + ".A", Imf::Slice(Imf::FLOAT, (char*)&a_pixels[a_place], sizeof(float), sizeof(float) * width));
			}
		}

		// write output file
		Imf::OutputFile file(to_save_path.c_str(), header);

		file.setFrameBuffer(frameBuffer);
		file.writePixels(height);

		// clear global pixels
		r_pixels.clear();
		r_pixels.shrink_to_fit();
		g_pixels.clear();
		g_pixels.shrink_to_fit();
		b_pixels.clear();
		b_pixels.shrink_to_fit();
		a_pixels.clear();
		a_pixels.shrink_to_fit();

		r_starts.clear();
		r_starts.shrink_to_fit();
		g_starts.clear();
		g_starts.shrink_to_fit();
		b_starts.clear();
		b_starts.shrink_to_fit();
		a_starts.clear();
		a_starts.shrink_to_fit();
	}
}

void write_outputs(OutputContext* output_context, ColorTransformContext* color_transform_context, const XSI::CParameterRefArray& render_parameters)
{
	RenderType render_type = output_context->get_render_type();
	if (render_type == RenderType::RenderType_Pass || render_type == RenderType::RenderType_Rendermap)
	{
		size_t width = output_context->get_width();
		size_t height = output_context->get_height();

		// we should save separate passes if output combine is false or it true and save separate is also true
		bool output_exr_combine_passes = render_parameters.GetValue("output_exr_combine_passes");
		bool output_exr_render_separate_passes = render_parameters.GetValue("output_exr_render_separate_passes");

		// at first save multilayer image
		if (output_exr_combine_passes)
		{// we should save all output passes into one multilayer exr file
			write_multilayer_exr(width, height, output_context);
		}

		// next save separate images
		// but before this process, combine labels (if it exists) with each combined output pass
		output_context->overlay_labels();
		if ((output_exr_combine_passes && output_exr_render_separate_passes) || (!output_exr_combine_passes))
		{
			write_outputs_separate_passes(output_context, color_transform_context, width, height);
		}
	}
}