#include <xsi_arrayparameter.h>
#include <xsi_shader.h>
#include <xsi_shaderdef.h>
#include <xsi_shaderparamdefcontainer.h>
#include <xsi_shaderparamdef.h>
#include <xsi_color4f.h>
#include <xsi_imageclip2.h>

#include "scene/shader.h"
#include "scene/shader_nodes.h"
#include "scene/shader_graph.h"
#include "scene/scene.h"
#include "scene/osl.h"

#include "../../../utilities/xsi_shaders.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "cyc_materials.h"
#include "../cyc_loaders/cyc_loaders.h"

ccl::ShaderNode* sync_xsi_shader(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Shader& xsi_shader, const XSI::CString &shader_type, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, std::vector<XSI::CStringArray>& aovs, const XSI::CTime& eval_time)
{
	if (shader_type == "sib_vector_to_color")
	{
		ccl::SeparateXYZNode* separate_node = shader_graph->create_node<ccl::SeparateXYZNode>();
		ccl::CombineRGBNode* combine_node = shader_graph->create_node<ccl::CombineRGBNode>();

		shader_graph->add(separate_node);
		shader_graph->add(combine_node);

		// use combine node as output node
		ULONG xsi_shader_id = xsi_shader.GetObjectID();
		nodes_map[xsi_shader_id] = combine_node;
		combine_node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

		shader_graph->connect(separate_node->output("X"), combine_node->input("R"));
		shader_graph->connect(separate_node->output("Y"), combine_node->input("G"));
		shader_graph->connect(separate_node->output("Z"), combine_node->input("B"));

		XSI::ShaderParameter input_param(xsi_shader.GetParameter("input"));
		sync_float3_parameter(scene, shader_graph, separate_node, input_param, nodes_map, aovs, "Vector", eval_time);
		return combine_node;
	}
	else if (shader_type == "sib_scalar_to_color")
	{
		ccl::RGBRampNode* node = shader_graph->create_node<ccl::RGBRampNode>();
		shader_graph->add(node);
		ULONG xsi_shader_id = xsi_shader.GetObjectID();
		nodes_map[xsi_shader_id] = node;
		node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

		node->set_interpolate(true);
		ccl::array<ccl::float3> ramp(2);
		ramp[0] = ccl::zero_float3();
		ramp[1] = ccl::one_float3();
		ccl::array<float> alpha_ramp(2);
		alpha_ramp[0] = 1.0f;
		alpha_ramp[1] = 1.0f;
		node->set_ramp(ramp);
		node->set_ramp_alpha(alpha_ramp);

		XSI::ShaderParameter input_param(xsi_shader.GetParameter("input"));
		sync_float_parameter(scene, shader_graph, node, input_param, nodes_map, aovs, "Fac", eval_time);

		return node;
	}
	else if (shader_type == "sib_vector_to_scalar")
	{
		ccl::SeparateXYZNode* separate_node = shader_graph->create_node<ccl::SeparateXYZNode>();  // this is input node
		ccl::MathNode* math_add_01 = shader_graph->create_node<ccl::MathNode>();
		ccl::MathNode* math_add_02 = shader_graph->create_node<ccl::MathNode>();
		ccl::MathNode* math_divide = shader_graph->create_node<ccl::MathNode>();  // this is ouput node

		// if we use this node again, then we should connect to output math node
		// so, add it to the nodes map
		shader_graph->add(separate_node);
		ULONG xsi_shader_id = xsi_shader.GetObjectID();
		nodes_map[xsi_shader_id] = math_divide;
		separate_node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

		shader_graph->add(math_add_01);
		shader_graph->add(math_add_02);
		shader_graph->add(math_divide);

		math_add_01->set_math_type(ccl::NodeMathType::NODE_MATH_ADD);
		math_add_02->set_math_type(ccl::NodeMathType::NODE_MATH_ADD);
		math_divide->set_math_type(ccl::NodeMathType::NODE_MATH_DIVIDE);
		math_divide->set_value2(3.0f);

		shader_graph->connect(separate_node->output("X"), math_add_01->input("Value1"));
		shader_graph->connect(separate_node->output("Y"), math_add_01->input("Value2"));
		shader_graph->connect(math_add_01->output("Value"), math_add_02->input("Value1"));
		shader_graph->connect(separate_node->output("Z"), math_add_02->input("Value2"));
		shader_graph->connect(math_add_02->output("Value"), math_divide->input("Value1"));

		XSI::ShaderParameter input_param(xsi_shader.GetParameter("input"));
		sync_float3_parameter(scene, shader_graph, separate_node, input_param, nodes_map, aovs, "Vector", eval_time);

		return math_divide;
	}
	else if (shader_type == "sib_color_to_scalar")
	{
		ccl::RGBToBWNode* node = shader_graph->create_node<ccl::RGBToBWNode>();
		shader_graph->add(node);
		ULONG xsi_shader_id = xsi_shader.GetObjectID();
		nodes_map[xsi_shader_id] = node;
		node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

		XSI::ShaderParameter input_param(xsi_shader.GetParameter("input"));
		sync_float3_parameter(scene, shader_graph, node, input_param, nodes_map, aovs, "Color", eval_time);

		return node;
	}
	else if (shader_type == "txt2d-image-explicit")
	{
		XSI::CParameterRefArray params = xsi_shader.GetParameters();
		XSI::ShaderParameter tex_param = params.GetItem("tex");
		XSI::ShaderParameter alt_x_param = params.GetItem("alt_x");
		XSI::ShaderParameter alt_y_param = params.GetItem("alt_y");
		XSI::ShaderParameter repeats_param = params.GetItem("repeats");
		XSI::ShaderParameter uv_param = params.GetItem("tspace_id");

		XSI::CRef tex_source = tex_param.GetSource();
		if (tex_source.IsValid() && alt_x_param.IsValid() && alt_y_param.IsValid() && repeats_param.IsValid() && uv_param.IsValid())
		{
			XSI::ImageClip2 clip(tex_source);
			XSI::CString file_path = clip.GetFileName();

			bool alt_x = alt_x_param.GetValue(eval_time);
			bool alt_y = alt_y_param.GetValue(eval_time);
			XSI::CParameterRefArray repeats_params = repeats_param.GetParameters();
			float repeat_x = ((XSI::Parameter)repeats_params[0]).GetValue(eval_time);
			float repeat_y = ((XSI::Parameter)repeats_params[1]).GetValue(eval_time);
			float repeat_z = ((XSI::Parameter)repeats_params[2]).GetValue(eval_time);

			XSI::CString uv_name = uv_param.GetValue(eval_time);

			// next setup shader nodes
			// main image node
			ccl::ImageTextureNode* node = shader_graph->create_node<ccl::ImageTextureNode>();
			shader_graph->add(node);
			// this node is output, so, add it to the nodes map
			ULONG xsi_shader_id = xsi_shader.GetObjectID();
			nodes_map[xsi_shader_id] = node;
			node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

			//node->set_filename(OIIO::ustring(file_path.GetAsciiString()));
			XSIImageLoader* image_loader = new XSIImageLoader(clip, eval_time);
			node->handle = scene->image_manager->add_image(image_loader, node->image_params());

			node->set_colorspace(ccl::u_colorspace_srgb);
			node->set_projection(ccl::NodeImageProjection::NODE_IMAGE_PROJ_FLAT);
			node->set_projection_blend(0.0);
			node->set_interpolation(ccl::InterpolationType::INTERPOLATION_SMART);
			node->set_extension(ccl::ExtensionType::EXTENSION_REPEAT);
			node->set_alpha_type(ccl::ImageAlphaType::IMAGE_ALPHA_AUTO);
			node->set_animated(false);

			// uv node
			ccl::UVMapNode* uv_node = shader_graph->create_node<ccl::UVMapNode>();
			shader_graph->add(uv_node);
			uv_node->set_attribute(ccl::ustring(uv_name.GetAsciiString()));

			// mapping node
			ccl::MappingNode* mapping = shader_graph->create_node<ccl::MappingNode>();
			shader_graph->add(mapping);
			mapping->set_mapping_type(ccl::NodeMappingType::NODE_MAPPING_TYPE_POINT);
			mapping->set_location(ccl::make_float3(alt_x ? 1.0f : 0.0, alt_y ? 1.0f : 0.0, 0.0));
			mapping->set_rotation(ccl::zero_float3());
			mapping->set_scale(ccl::make_float3(repeat_x, repeat_y, repeat_z));
			// connect uv to mapping
			shader_graph->connect(uv_node->output("UV"), mapping->input("Vector"));

			// separate node
			ccl::SeparateXYZNode* separate = shader_graph->create_node<ccl::SeparateXYZNode>();
			shader_graph->add(separate);
			// connect mapping to separate
			shader_graph->connect(mapping->output("Vector"), separate->input("Vector"));

			// combine node
			ccl::CombineXYZNode* combine = shader_graph->create_node<ccl::CombineXYZNode>();
			shader_graph->add(combine);
			// connect separate to combine
			if (alt_x)
			{// with alternation, add math ping-pong node
				ccl::MathNode* math_x = shader_graph->create_node<ccl::MathNode>();
				shader_graph->add(math_x);
				math_x->set_math_type(ccl::NODE_MATH_PINGPONG);
				math_x->set_value2(1.0);

				// connect separator to math and then to combiner
				shader_graph->connect(separate->output("X"), math_x->input("Value1"));
				shader_graph->connect(math_x->output("Value"), combine->input("X"));
			}
			else
			{
				shader_graph->connect(separate->output("X"), combine->input("X"));
			}
			if (alt_y)
			{
				ccl::MathNode* math_y = shader_graph->create_node<ccl::MathNode>();
				shader_graph->add(math_y);
				math_y->set_math_type(ccl::NODE_MATH_PINGPONG);
				math_y->set_value2(1.0);

				// connect separator to math and then to combiner
				shader_graph->connect(separate->output("Y"), math_y->input("Value1"));
				shader_graph->connect(math_y->output("Value"), combine->input("Y"));
			}
			else
			{
				shader_graph->connect(separate->output("Y"), combine->input("Y"));
			}

			// connect separate to image
			shader_graph->connect(combine->output("Vector"), node->input("Vector"));

			// nothing connect to this node
			return node;
		}
		else
		{
			return NULL;
		}
	}
	else if (shader_type == "material-lambert")
	{
		// interprete Lamberts as simple diffuse
		ccl::DiffuseBsdfNode* node = shader_graph->create_node<ccl::DiffuseBsdfNode>();

		shader_graph->add(node);
		ULONG xsi_shader_id = xsi_shader.GetObjectID();
		nodes_map[xsi_shader_id] = node;
		node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

		XSI::ShaderParameter diffuse_parameter(xsi_shader.GetParameter("diffuse"));
		sync_float3_parameter(scene, shader_graph, node, diffuse_parameter, nodes_map, aovs, "Color", eval_time);

		return node;
	}
	else if (shader_type == "material-phong")
	{
		// interprete Lamberts as principled bsdf
		ccl::PrincipledBsdfNode* node = shader_graph->create_node<ccl::PrincipledBsdfNode>();

		shader_graph->add(node);
		ULONG xsi_shader_id = xsi_shader.GetObjectID();
		nodes_map[xsi_shader_id] = node;
		node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

		XSI::CParameterRefArray xsi_parameters = xsi_shader.GetParameters();

		XSI::ShaderParameter diffuse_parameter(xsi_parameters.GetItem("diffuse"));
		sync_float3_parameter(scene, shader_graph, node, diffuse_parameter, nodes_map, aovs, "Base Color", eval_time);

		bool use_spec = get_bool_parameter_value(xsi_parameters, "specular_inuse", eval_time);
		if (use_spec)
		{
			// phong specular color interprete as specular intensity
			XSI::MATH::CColor4f spec_color = get_color_parameter_value(xsi_parameters, "specular", eval_time);
			node->set_specular((spec_color.GetR() + spec_color.GetG() + spec_color.GetB()) / 3.0f);

			// phong shiny is roughness (inverse and devided by 100)
			// shiny = 0 -> roughness = 1
			// shiny = 100 -> roughness = 0
			// so, add additional math nodes
			ccl::MathNode* math_subtract = shader_graph->create_node<ccl::MathNode>();
			ccl::MathNode* math_divide = shader_graph->create_node<ccl::MathNode>();
			ccl::ClampNode* clamp_node = shader_graph->create_node<ccl::ClampNode>();

			shader_graph->add(math_subtract);
			shader_graph->add(math_divide);
			shader_graph->add(clamp_node);

			math_subtract->set_value1(1.0f);
			math_subtract->set_math_type(ccl::NodeMathType::NODE_MATH_SUBTRACT);

			math_divide->set_value2(100.0f);
			math_divide->set_math_type(ccl::NodeMathType::NODE_MATH_DIVIDE);

			shader_graph->connect(math_divide->output("Value"), math_subtract->input("Value2"));
			shader_graph->connect(math_subtract->output("Value"), clamp_node->input("Value"));
			shader_graph->connect(clamp_node->output("Result"), node->input("Roughness"));

			XSI::ShaderParameter shiny_param = xsi_parameters.GetItem("shiny");
			sync_float_parameter(scene, shader_graph, math_divide, shiny_param, nodes_map, aovs, "Value1", eval_time);
		}
		else
		{
			// set zero specular
			node->set_specular(0.0f);
		}

		return node;
	}
	else
	{
		log_message(shader_type);
	}
	return NULL;
}