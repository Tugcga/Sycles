#include "session/session.h"
#include "scene/pass.h"
#include "scene/integrator.h"

#include <xsi_application.h>

#include "../../input/input_devices.h"
#include "../cyc_scene/cyc_scene.h"
#include "cyc_session.h"

void set_session_samples(ccl::SessionParams &session_params, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
	session_params.samples = render_parameters.GetValue("sampling_render_samples", eval_time);
	session_params.samples = std::min(session_params.samples, ccl::Integrator::MAX_SAMPLES);
	session_params.sample_offset = render_parameters.GetValue("sampling_advanced_offset", eval_time);
	session_params.sample_offset = std::clamp(session_params.sample_offset, 0, ccl::Integrator::MAX_SAMPLES);
	session_params.samples = std::clamp(session_params.samples, 0, ccl::Integrator::MAX_SAMPLES - session_params.sample_offset);

	session_params.time_limit = render_parameters.GetValue("sampling_render_time_limit", eval_time);
}

ccl::SessionParams get_session_params(const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
	ccl::SessionParams session_params;

	session_params.background = true;

	int threads_count = render_parameters.GetValue("performance_threads_count", eval_time);
	int threads_mode = render_parameters.GetValue("performance_threads_mode", eval_time);
	if (threads_mode == 0)
	{
		threads_count = 0;
	}

	session_params.threads = threads_count;

	// find matching device 
	bool is_cpu = true;
	ccl::DeviceType cpu_device_type = ccl::DeviceType::DEVICE_CPU;
	ccl::vector<ccl::DeviceInfo> available_devices = get_available_devices();

	// find selected devices
	std::vector<size_t> selected_indices;
	for (size_t i = 0; i < available_devices.size(); i++)
	{
		bool is_select = render_parameters.GetValue("device_" + XSI::CString(i), eval_time);
		if (is_select)
		{
			selected_indices.push_back(i);
		}
	}

	bool use_cpu = false;
	size_t selected_count = selected_indices.size();
	if (selected_count <= 1)
	{
		if (selected_indices.size() == 0)
		{  // nothing is selected, select the first device (it should be cpu)
			session_params.device = available_devices[0];
		}
		else if (selected_indices.size() == 1)
		{  // only one device selected, set it
			session_params.device = available_devices[selected_indices[0]];
		}

		if (session_params.device.type == cpu_device_type)
		{
			use_cpu = true;
		}
	}
	else
	{  // select multiple devices
		ccl::vector<ccl::DeviceInfo> used_devices;

		bool select_optix = false;
		bool select_cuda = false;
		bool select_non_optix = false;
		for (size_t i = 0; i < selected_count; i++)
		{
			ccl::DeviceInfo selected_device = available_devices[selected_indices[i]];
			ccl::DeviceType type = selected_device.type;
			if ((select_optix == false && select_cuda == false) ||  // try to skip adding both cuda and optix devices (optix + cpu or cuda + cpu works fine)
				(select_optix && type != ccl::DEVICE_CUDA) ||
				(select_cuda && type != ccl::DEVICE_OPTIX))
			{
				used_devices.push_back(selected_device);
			}

			if (type == ccl::DEVICE_OPTIX)
			{
				select_optix = true;
			}
			else
			{
				select_non_optix = true;
			}

			if (type == ccl::DEVICE_CUDA)
			{
				select_cuda = true;
			}
		}

		session_params.device = ccl::Device::get_multi_device(used_devices, session_params.threads, session_params.background);

		use_cpu = false;
	}

	session_params.experimental = true;
	set_session_samples(session_params, render_parameters, eval_time);

	session_params.pixel_size = 1;
	session_params.tile_size = std::max(8, (int)render_parameters.GetValue("performance_memory_tile_size", eval_time));
	session_params.use_auto_tile = render_parameters.GetValue("performance_memory_use_auto_tile", eval_time);

	int shading_system = render_parameters.GetValue("options_shaders_system", eval_time);
	bool client_want_osl = shading_system == 1;
	if (use_cpu && client_want_osl)
	{
		session_params.shadingsystem = ccl::ShadingSystem::SHADINGSYSTEM_OSL;
	}
	else
	{
		session_params.shadingsystem = ccl::ShadingSystem::SHADINGSYSTEM_SVM;
	}
	if (client_want_osl && !use_cpu)
	{
		log_message(XSI::CString("OSL shading system supports only for CPU-rendering. Switched to SVM shading system."), XSI::siWarningMsg);
	}

	session_params.use_profiling = false;
	bool is_details = render_parameters.GetValue("options_logging_log_details", eval_time);
	bool is_profiling = render_parameters.GetValue("options_logging_log_profiling", eval_time);
	if (is_details && is_profiling)
	{
		session_params.use_profiling = true;
	}
	else
	{
		session_params.use_profiling = false;
	}

	return session_params;
}

ccl::SceneParams get_scene_params(RenderType render_type, const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time)
{
	ccl::SceneParams scene_params;
	scene_params.shadingsystem = render_parameters.GetValue("options_shaders_system", eval_time) == 0 ? ccl::ShadingSystem::SHADINGSYSTEM_SVM : ccl::ShadingSystem::SHADINGSYSTEM_OSL;

	// for region and shaderball use dynamic bvh (in particular, it des not apply transform to meshes)
	scene_params.bvh_type = (render_type == RenderType_Region || render_type == RenderType_Shaderball) ? ccl::BVHType::BVH_TYPE_DYNAMIC : ccl::BVHType::BVH_TYPE_STATIC;
	scene_params.use_bvh_spatial_split = render_parameters.GetValue("performance_acceleration_use_spatial_split", eval_time);
	scene_params.use_bvh_compact_structure = render_parameters.GetValue("performance_acceleration_use_compact_bvh", eval_time);

	scene_params.hair_shape = render_parameters.GetValue("performance_curves_type", eval_time) == 1 ? ccl::CurveShapeType::CURVE_THICK : ccl::CurveShapeType::CURVE_RIBBON;
	scene_params.hair_subdivisions = render_parameters.GetValue("performance_curves_subdivs", eval_time);

	scene_params.background = true;

	return scene_params;
}

ccl::BufferParams get_buffer_params(int full_width, int full_height, int offset_x, int offset_y, int width, int height)
{
	ccl::BufferParams buffer_params;

	buffer_params.full_width = full_width;
	buffer_params.full_height = full_height;
	buffer_params.full_x = offset_x;
	buffer_params.full_y = offset_y;
	buffer_params.width = width;
	buffer_params.height = height;

	buffer_params.window_width = buffer_params.width;
	buffer_params.window_height = buffer_params.height;

	return buffer_params;
}

ccl::Session* create_session(ccl::SessionParams session_params, ccl::SceneParams scene_params)
{
	ccl::Session* session = new ccl::Session(session_params, scene_params);
	
	return session;
}

void sync_session(ccl::Session* session, UpdateContext* update_context, OutputContext *output_context, RenderVisualBuffer *visual_buffer)
{
	// at first we should sync the camera, because it contains proper value for motion steps
	sync_camera(session->scene, update_context);

	sync_scene(session->scene, update_context);
}