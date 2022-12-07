#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/light.h"
#include "util/hash.h"

#include <xsi_light.h>
#include <xsi_x3dobject.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_transformation.h>

#include "../../render_cycles/update_context.h"
#include "../../utilities/logs.h"
#include "../../utilities/xsi_shaders.h"
#include "../../utilities/math.h"

void sync_xsi_light(ccl::Scene* scene, ccl::Light* light, const XSI::Light &xsi_light, const XSI::CTime &eval_time)
{
	// get light shader
	XSI::CRefArray xsi_shaders = xsi_light.GetShaders();
	XSI::ShaderParameter root_parameter = get_root_shader_parameter(xsi_shaders, "LightShader", false);
	XSI::MATH::CColor4f xsi_color(0.0, 0.0, 0.0, 0.0);
	float xsi_intensity = 0.0f;
	float xsi_spread = 1.0f;  // in degrees, used only for infinite light, define the size of the sun
	float xsi_power = 250.0f;
	float xsi_umbra = 1.0f;
	if (root_parameter.IsValid())
	{
		XSI::Shader xsi_light_node = get_input_node(root_parameter);
		if (xsi_light_node.IsValid() && xsi_light_node.GetProgID() == "Softimage.soft_light.1.0")
		{
			// TODO: supports shader networks here, read connections to color and intensity

			// get color and intensity of the shader
			XSI::CParameterRefArray all_params = xsi_light_node.GetParameters();
			xsi_color = get_color_parameter_value(all_params, "color", eval_time);
			xsi_intensity = get_float_parameter_value(all_params, "intensity", eval_time);

			// and also other settings
			xsi_spread = get_float_parameter_value(all_params, "spread", eval_time);
			xsi_umbra = get_float_parameter_value(all_params, "factor", eval_time);
		}
	}

	// next create shader for the light
	ccl::Shader* light_shader = new ccl::Shader();
	light_shader->name = "Light " + std::to_string(xsi_light.GetObjectID());
	ccl::ShaderGraph* graph = new ccl::ShaderGraph();
	ccl::EmissionNode* node = new ccl::EmissionNode();
	node->set_color(color4_to_float3(xsi_color));
	node->set_strength(xsi_intensity);
	graph->add(node);
	ccl::ShaderNode* out = graph->output();
	graph->connect(node->output("Emission"), out->input("Surface"));
	light_shader->set_graph(graph);
	light_shader->tag_update(scene);

	scene->shaders.push_back(light_shader);
	int shader_index = scene->shaders.size() - 1;

	// assign the shader
	light->set_shader(light_shader);
	light->set_is_enabled(true);

	// next setup parameters from xsi light properties and for different light types
	int xsi_light_type = xsi_light.GetParameterValue("Type", eval_time);
	bool xsi_area = xsi_light.GetParameterValue("LightArea", eval_time);
	float cone = xsi_light.GetParameterValue("LightCone", eval_time);
	bool xsi_visible = xsi_light.GetParameterValue("LightAreaVisible", eval_time);
	int xsi_area_shape = xsi_light.GetParameterValue("LightAreaGeom", eval_time);
	float xsi_size_x = xsi_light.GetParameterValue("LightAreaXformSX", eval_time);
	float xsi_size_y = xsi_light.GetParameterValue("LightAreaXformSY", eval_time);

	// get light transform
	XSI::MATH::CMatrix4 xsi_tfm_matrix = xsi_light.GetKinematics().GetGlobal().GetTransform().GetMatrix4();
	XSI::MATH::CVector3 xsi_tfm_position = xsi_light.GetKinematics().GetGlobal().GetTransform().GetTranslation();

	if (xsi_area && xsi_area_shape != 3)
	{// active area, but shape is not a sphere (sphere is a simple point light)
		// in this case we always interpret light as area light
		float xsi_rotation_x = xsi_light.GetParameterValue("LightAreaXformRX", eval_time);
		float xsi_rotation_y = xsi_light.GetParameterValue("LightAreaXformRY", eval_time);
		float xsi_rotation_z = xsi_light.GetParameterValue("LightAreaXformRZ", eval_time);

		// TODO: calculate actual area transform as in SoftLux
		light->set_light_type(ccl::LightType::LIGHT_AREA);
		XSI::MATH::CVector3 xsi_axis_u = XSI::MATH::CVector3(xsi_tfm_matrix.GetValue(0, 0), xsi_tfm_matrix.GetValue(0, 1), xsi_tfm_matrix.GetValue(0, 2));
		XSI::MATH::CVector3 xsi_axis_v = XSI::MATH::CVector3(xsi_tfm_matrix.GetValue(1, 0), xsi_tfm_matrix.GetValue(1, 1), xsi_tfm_matrix.GetValue(1, 2));

		// consider different shapes
		if (xsi_area_shape == 2)
		{// disc
			// use only x size
			light->set_round(true);
			light->set_sizeu(xsi_size_x * 2.0f);
			light->set_sizev(xsi_size_x * 2.0f);
		}
		else
		{// all other shapes are rectangles
			light->set_round(false);
			light->set_sizeu(xsi_size_x);
			light->set_sizev(xsi_size_y);

			// rectangle can be rotated, so, change transform matrix
			XSI::MATH::CTransformation rotate_tfm;
			rotate_tfm.SetIdentity();
			rotate_tfm.SetRotX(xsi_rotation_x);
			rotate_tfm.SetRotY(xsi_rotation_y);
			rotate_tfm.SetRotZ(xsi_rotation_z);
			XSI::MATH::CMatrix4 rotate_matrix = rotate_tfm.GetMatrix4();
			xsi_tfm_matrix.Mul(rotate_matrix, xsi_tfm_matrix);
		}

		light->set_axisu(vector3_to_float3(xsi_axis_u));
		light->set_axisv(vector3_to_float3(xsi_axis_v));
		
		light->set_size(1);
		light->set_is_portal(false);
		light->set_spread(XSI::MATH::PI);
	}
	else
	{
		if ((xsi_area && xsi_area_shape == 3) || xsi_light_type == 0)
		{// point light
			light->set_light_type(ccl::LightType::LIGHT_POINT);
			if (xsi_area)
			{
				light->set_size(xsi_size_x);
			}
			else
			{
				light->set_size(0.001f);  // use constant value
			}
		}
		else if (xsi_light_type == 1)
		{// infinite light
			light->set_light_type(ccl::LightType::LIGHT_DISTANT);
			// set angle from spread value
			light->set_angle(xsi_spread * XSI::MATH::PI / 180.0f);
			// for infinite light set unit power
			xsi_power = 1.0f;
		}
		else if (xsi_light_type == 2)
		{// spot light
			light->set_light_type(ccl::LightType::LIGHT_SPOT);
			light->set_size(0.001f);

			light->set_spot_angle(cone * XSI::MATH::PI / 180.0);
			light->set_spot_smooth(xsi_umbra);
		}
		else
		{// unknown light type
			// set as point
			light->set_light_type(ccl::LightType::LIGHT_POINT);
			light->set_size(0.001f);
		}
	}

	XSI::MATH::CVector3 xsi_tfm_direction = XSI::MATH::CVector3(-1 * xsi_tfm_matrix.GetValue(2, 0), -1 * xsi_tfm_matrix.GetValue(2, 1), -1 * xsi_tfm_matrix.GetValue(2, 2));

	// setup common light parameters
	light->set_strength(ccl::one_float3() * xsi_power);  // for all light except infinite set constant power
	light->set_cast_shadow(true);
	light->set_use_mis(true);
	light->set_max_bounces(1024);
	light->set_use_camera(xsi_visible);
	light->set_use_diffuse(true);
	light->set_use_glossy(true);
	light->set_use_transmission(true);
	light->set_use_scatter(true);
	light->set_is_shadow_catcher(false);

	// set transform
	light->set_dir(vector3_to_float3(xsi_tfm_direction));
	light->set_co(vector3_to_float3(xsi_tfm_position));
	std::vector<float> light_tfm;

	xsi_matrix_to_cycles_array(light_tfm, xsi_tfm_matrix, false);
	light->set_tfm(get_transform(light_tfm));
	light_tfm.clear();
	light_tfm.shrink_to_fit();

	light->set_random_id(ccl::hash_uint2(ccl::hash_string(xsi_light.GetUniqueName().GetAsciiString()), 0));
}

void sync_xsi_lights(ccl::Scene* scene, const std::vector<XSI::Light> &xsi_lights, UpdateContext* update_context)
{
	XSI::CTime eval_time = update_context->get_time();
	for (size_t i = 0; i < xsi_lights.size(); i++)
	{
		XSI::Light xsi_light = xsi_lights[i];
		 // crete Cycles light
		ccl::Light* light = scene->create_node<ccl::Light>();

		// setup light parameters
		sync_xsi_light(scene, light, xsi_light, eval_time);

		// save connection between light object in the Softimage scene and in the Cycles scene
		update_context->add_light_index(xsi_light.GetObjectID(), scene->lights.size() - 1);
	}
}