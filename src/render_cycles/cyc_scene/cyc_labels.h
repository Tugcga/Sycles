#pragma once
#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>
#include <xsi_camera.h>
#include <xsi_project.h>
#include <xsi_scene.h>

#include "scene/scene.h"
#include "scene/geometry.h"
#include "scene/object.h"
#include "scene/mesh.h"
#include "scene/hair.h"

#include "../../utilities/math.h"

class LabelsContext
{
public:
	LabelsContext() { reset(); }

	~LabelsContext() { reset(); }

	void reset()
	{
		is_label_render_time = false;
		is_label_time = false;
		is_label_frame = false;
		is_label_scene = false;
		is_label_camera = false;
		is_label_samples = false;
		is_label_objects_count = false;
		is_label_lights_count = false;
		is_label_triangles_count = false;
		is_label_curves_count = false;

		render_time_value = 0.0f;
		time_value = "";
		frame_value = 0;
		scene_value = "";
		camera_value = "";
		samples_value = 0;
		objects_count_value = 0;
		lights_count_value = 0;
		triangles_count_value = 0;
		curves_count_value = 0;
	}

	bool is_labels()
	{
		return is_label_render_time ||
			is_label_time ||
			is_label_frame ||
			is_label_scene ||
			is_label_camera ||
			is_label_samples ||
			is_label_objects_count ||
			is_label_lights_count ||
			is_label_triangles_count ||
			is_label_curves_count;
	}

	void setup(ccl::Scene* scene, const XSI::CParameterRefArray& render_parameters, const XSI::Camera& xsi_camera, const XSI::CTime& eval_time)
	{
		is_label_render_time = render_parameters.GetValue("output_label_render_time");

		is_label_time = render_parameters.GetValue("output_label_time");
		if (is_label_time)
		{
			time_t t;
			t = time(nullptr);
			std::string time_string = ctime(&t);
			time_value = XSI::CString(time_string.c_str());
		}
		else
		{
			time_value = "";
		}

		is_label_frame = render_parameters.GetValue("output_label_frame");
		if (is_label_frame)
		{
			frame_value = get_frame(eval_time);
		}
		else
		{
			frame_value = 0;
		}

		is_label_scene = render_parameters.GetValue("output_label_scene");
		if (is_label_scene)
		{
			XSI::CParameterRefArray scene_params = XSI::Application().GetActiveProject().GetActiveScene().GetParameters();
			XSI::Parameter filename_parameter = scene_params.GetItem("Filename");
			XSI::CString scene_full_path = filename_parameter.GetValue(double(1));
			std::string scene_full_path_str = scene_full_path.GetAsciiString();
			size_t lss = scene_full_path_str.find_last_of("/\\");
			scene_value = scene_full_path_str.substr(lss + 1, scene_full_path_str.size()).c_str();
		}
		else
		{
			scene_value = "";
		}

		is_label_camera = render_parameters.GetValue("output_label_camera");
		if (is_label_camera)
		{
			camera_value = xsi_camera.GetName();
		}
		else
		{
			camera_value = "";
		}

		is_label_samples = render_parameters.GetValue("output_label_samples");
		if (is_label_samples)
		{
			samples_value = render_parameters.GetValue("sampling_render_samples");
		}
		else
		{
			samples_value = 0;
		}

		is_label_objects_count = render_parameters.GetValue("output_label_objects_count");
		if (is_label_objects_count)
		{
			objects_count_value = scene->objects.size();
		}
		else
		{
			objects_count_value = 0;
		}

		is_label_lights_count = render_parameters.GetValue("output_label_lights_count");
		if (is_label_lights_count)
		{
			lights_count_value = scene->lights.size();
		}
		else
		{
			lights_count_value = 0;
		}

		is_label_triangles_count = render_parameters.GetValue("output_label_triangles_count");
		
		triangles_count_value = 0;

		is_label_curves_count = render_parameters.GetValue("output_label_curves_count");
		curves_count_value = 0;
		if (is_label_curves_count)
		{
			for (size_t i = 0; i < scene->objects.size(); i++)
			{
				ccl::Geometry* geo = scene->objects[i]->get_geometry();
				if (geo->geometry_type == ccl::Geometry::HAIR)
				{
					ccl::Hair* hair = static_cast<ccl::Hair*>(geo);
					curves_count_value += hair->get_curve_first_key().size();
				}
			}
		}
	}

	void set_render_time(double value)
	{
		render_time_value = value;
	}

	// use if we should setup real samples insted value from render settings
	void set_render_samples(int value)
	{
		samples_value = value;
	}

	// calculate triangles after render process (because it properly update mesh triangles for subdivided meshes)
	void set_render_triangles(ccl::Scene* scene)
	{
		if (is_label_triangles_count)
		{
			size_t triangles_accum = 0;
			for (size_t i = 0; i < scene->objects.size(); i++)
			{
				ccl::Geometry* geo = scene->objects[i]->get_geometry();
				if (geo->geometry_type == ccl::Geometry::MESH)
				{
					ccl::Mesh* mesh = static_cast<ccl::Mesh*>(geo);
					triangles_accum += mesh->get_triangles().size();
				}
			}
			triangles_count_value = triangles_accum / 3;
		}
	}

	XSI::CString get_string()
	{
		XSI::CString to_return;
		bool is_start_init = false;
		if (is_label_render_time)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Render time: " + seconds_to_date(render_time_value);
			is_start_init = true;
		}
		if (is_label_samples)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Samples: " + XSI::CString(samples_value);
			is_start_init = true;
		}
		if (is_label_time)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Date: " + time_value;
			is_start_init = true;
		}
		if (is_label_frame)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Frame: " + XSI::CString(frame_value);
			is_start_init = true;
		}
		if (is_label_scene)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Scene: " + scene_value;
			is_start_init = true;
		}
		if (is_label_camera)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Camera: " + camera_value;
			is_start_init = true;
		}
		if (is_label_triangles_count)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Triangles: " + XSI::CString(triangles_count_value);
			is_start_init = true;
		}
		if (is_label_curves_count)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Curves: " + XSI::CString(curves_count_value);
			is_start_init = true;
		}
		if (is_label_objects_count)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Objects: " + XSI::CString(objects_count_value);
			is_start_init = true;
		}
		if (is_label_lights_count)
		{
			to_return = to_return + (is_start_init ? " | " : "") + "Lights: " + XSI::CString(lights_count_value);
			is_start_init = true;
		}

		return to_return;
	}

private:
	bool is_label_render_time;
	bool is_label_time;
	bool is_label_frame;
	bool is_label_scene;
	bool is_label_camera;
	bool is_label_samples;
	bool is_label_objects_count;
	bool is_label_lights_count;
	bool is_label_triangles_count;
	bool is_label_curves_count;

	double render_time_value;
	XSI::CString time_value;
	int frame_value;
	XSI::CString scene_value;
	XSI::CString camera_value;
	int samples_value;
	int objects_count_value;
	int lights_count_value;
	int triangles_count_value;
	int curves_count_value;
};