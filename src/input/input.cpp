#include <xsi_application.h>
#include <xsi_utils.h>

#include "device/device.h"
#include "util/foreach.h"

#include "config_ini.h"
#include "../utilities/SimpleIni.h"

XSI::CString plugin_path;
void set_plugin_path(const XSI::CString &input_plugin_path)
{
	plugin_path = input_plugin_path;
}

XSI::CString get_plugin_path()
{
	return plugin_path;
}

ccl::vector<ccl::DeviceInfo> available_devices;
void find_devices()
{
	available_devices.clear();

	// cpu devices
	ccl::vector<ccl::DeviceInfo> cpu_devices = ccl::Device::available_devices(ccl::DEVICE_MASK_CPU);
	size_t index = 0;
	foreach(ccl::DeviceInfo & device, cpu_devices)
	{
		available_devices.push_back(device);
		index++;
	}

	// cuda devices
	ccl::vector<ccl::DeviceInfo> cuda_devices = ccl::Device::available_devices(ccl::DEVICE_MASK_CUDA);
	foreach(ccl::DeviceInfo & device, cuda_devices)
	{
		available_devices.push_back(device);
	}

	// optix devices
	ccl::vector<ccl::DeviceInfo> optix_devices = ccl::Device::available_devices(ccl::DEVICE_MASK_OPTIX);
	foreach(ccl::DeviceInfo & device, optix_devices)
	{
		available_devices.push_back(device);
	}

	// hip devices
	ccl::vector<ccl::DeviceInfo> hip_devices = ccl::Device::available_devices(ccl::DEVICE_MASK_HIP);
	foreach(ccl::DeviceInfo & device, hip_devices)
	{
		available_devices.push_back(device);
	}

	// metal devices
	ccl::vector<ccl::DeviceInfo> metal_devices = ccl::Device::available_devices(ccl::DEVICE_MASK_METAL);
	foreach(ccl::DeviceInfo & device, metal_devices)
	{
		available_devices.push_back(device);
	}
}

ccl::vector<ccl::DeviceInfo> get_available_devices()
{
	return available_devices;
}

XSI::CStringArray get_available_devices_names()
{
	XSI::CStringArray names_array(available_devices.size());
	for (size_t i = 0; i < available_devices.size(); i++)
	{
		names_array[i] = available_devices[i].description.c_str();
	}

	return names_array;
}

InputConfig input_config;
void read_config_ini()
{
	XSI::CString config_file_path = XSI::CUtils::BuildPath(plugin_path, "..", "..", "config.ini");
	CSimpleIniA ini;
	ini.SetUnicode(false);
	const SI_Error rc = ini.LoadFile(config_file_path.GetAsciiString());
	if (rc < 0)
	{
		input_config.is_init = false;
	}
	else
	{
		ConfigShaderball shaderball;
		const char* render_samples_str = ini.GetValue("Shaderball", "samples", "32");
		shaderball.samples = std::stoi(render_samples_str, nullptr);

		const char* max_bounces_str = ini.GetValue("Shaderball", "max_bounces", "6");
		shaderball.max_bounces = std::stoi(max_bounces_str, nullptr);

		const char* diffuse_bounces_str = ini.GetValue("Shaderball", "diffuse_bounces", "2");
		shaderball.diffuse_bounces = std::stoi(diffuse_bounces_str, nullptr);

		const char* glossy_bouncess_str = ini.GetValue("Shaderball", "glossy_bounces", "2");
		shaderball.glossy_bounces = std::stoi(glossy_bouncess_str, nullptr);

		const char* transmission_bounces_str = ini.GetValue("Shaderball", "transmission_bounces", "2");
		shaderball.transmission_bounces = std::stoi(transmission_bounces_str, nullptr);

		const char* transparent_bounces_str = ini.GetValue("Shaderball", "transparent_bounces", "2");
		shaderball.transparent_bounces = std::stoi(transparent_bounces_str, nullptr);

		const char* volume_bounces_str = ini.GetValue("Shaderball", "volume_bounces", "2");
		shaderball.volume_bounces = std::stoi(volume_bounces_str, nullptr);

		const char* clamp_direct_str = ini.GetValue("Shaderball", "clamp_direct", "1.0");
		shaderball.clamp_direct = strtof(clamp_direct_str, nullptr);

		const char* clamp_indirect_str = ini.GetValue("Shaderball", "clamp_indirect", "1.0");
		shaderball.clamp_indirect = strtof(clamp_indirect_str, nullptr);

		ConfigRender render;
		const char* devices_str = ini.GetValue("Render", "devices", "16");
		render.devices = std::stoi(devices_str, nullptr);

		ConfigSeries series;
		const char* save_intermediate_str = ini.GetValue("SeriesRendering", "save_intermediate", "0");
		const float save_intermediate_float = strtof(save_intermediate_str, nullptr);
		series.save_intermediate = save_intermediate_float >= 0.5;

		const char* save_albedo_str = ini.GetValue("SeriesRendering", "save_albedo", "1");
		const float save_albedo_float = strtof(save_albedo_str, nullptr);
		series.save_albedo = save_albedo_float >= 0.5;

		const char* save_normal_str = ini.GetValue("SeriesRendering", "save_normal", "1");
		const float save_normal_float = strtof(save_normal_str, nullptr);
		series.save_normal = save_normal_float >= 0.5;

		const char* save_beauty_str = ini.GetValue("SeriesRendering", "save_beauty", "1");
		const float save_beauty_float = strtof(save_beauty_str, nullptr);
		series.save_beauty = save_beauty_float >= 0.5;

		const char* albedo_prefix_str = ini.GetValue("SeriesRendering", "albedo_prefix", "alb");
		series.albedo_prefix = XSI::CString(albedo_prefix_str);

		const char* normal_prefix_str = ini.GetValue("SeriesRendering", "normal_prefix", "nrm");
		series.normal_prefix = XSI::CString(normal_prefix_str);

		const char* beauty_prefix_str = ini.GetValue("SeriesRendering", "beauty_prefix", "hdr");
		series.beauty_prefix = XSI::CString(beauty_prefix_str);

		const char* sampling_step_str = ini.GetValue("SeriesRendering", "sampling_step", "128");
		series.sampling_step = std::stoi(sampling_step_str, nullptr);

		const char* sampling_start_separator_str = ini.GetValue("SeriesRendering", "sampling_start_separator", ".");
		series.sampling_start_separator = XSI::CString(sampling_start_separator_str);

		const char* sampling_middle_separator_str = ini.GetValue("SeriesRendering", "sampling_middle_separator", ".");
		series.sampling_middle_separator = XSI::CString(sampling_middle_separator_str);

		const char* sampling_size_str = ini.GetValue("SeriesRendering", "sampling_size", "8");
		series.sampling_size = std::stoi(sampling_size_str, nullptr);

		const char* sampling_postfix_str = ini.GetValue("SeriesRendering", "sampling_postfix", "spp");
		series.sampling_postfix = XSI::CString(sampling_postfix_str);

		input_config.is_init = true;
		input_config.shaderball = shaderball;
		input_config.render = render;
		input_config.series = series;
	}
}

InputConfig get_input_config()
{
	return input_config;
}