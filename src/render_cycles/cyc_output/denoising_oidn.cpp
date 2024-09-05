#include "OpenImageDenoise/oidn.hpp"


#include "denoising.h"

void log_oidn_error(const char* message)
{
	log_message("[OIDN error]: " + XSI::CString(message), XSI::siWarningMsg);
}

void error_callback(void* user_ptr, oidn::Error error, const char* message)
{
	log_oidn_error(message);
}

std::vector<float> denoise_buffer_oidn(ImageBuffer* buffer, OutputContext* output_context, bool use_albedo, bool use_normal)
{
	size_t width = buffer->get_width();
	size_t height = buffer->get_height();

	oidn::DeviceType device_type = oidn::DeviceType::CPU;

	oidn::DeviceRef device = oidn::newDevice(device_type);
	const char* error_message;
	if (device.getError(error_message) != oidn::Error::None)
	{
		log_oidn_error(error_message);
	}

	device.set("setAffinity", false);
	device.commit();

	oidn::FilterRef filter = device.newFilter("RT");
	// convert to 3 channels per pixel (4-channels are not supported)
	std::vector<float> original_pixels = buffer->convert_channel_pixels(3);
	filter.setImage("color", original_pixels.data(), oidn::Format::Float3, width, height);

	std::vector<float> albedo_pixels;
	std::vector<float> normal_pixels;

	// next additional channels
	if (use_albedo)
	{
		// we should find albedo pass in output context and get pixels from this pass
		albedo_pixels = get_pixels_from_passes(output_context, ccl::PassType::PASS_DENOISING_ALBEDO);
		if (albedo_pixels.size() == width * height * ULONG(3))
		{
			filter.setImage("albedo", albedo_pixels.data(), oidn::Format::Float3, width, height);
		}
	}

	// the same for normal
	if (use_normal)
	{
		normal_pixels = get_pixels_from_passes(output_context, ccl::PassType::PASS_DENOISING_NORMAL);
		if (normal_pixels.size() == width * height * ULONG(3))
		{
			filter.setImage("normal", normal_pixels.data(), oidn::Format::Float3, width, height);
		}
	}

	// denoised pixels also contains only 3 channels
	std::vector<float> denoised_pixels(width * height * ULONG(3));
	filter.setImage("output", denoised_pixels.data(), oidn::Format::Float3, width, height);
	filter.set("hdr", true);
	filter.set("srgb", false);
	filter.set("cleanAux", true);

	filter.commit();
	filter.execute();

	albedo_pixels.clear();
	normal_pixels.clear();
	albedo_pixels.shrink_to_fit();
	normal_pixels.shrink_to_fit();

	return denoised_pixels;
}