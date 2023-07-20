#include <xsi_arrayparameter.h>
#include <xsi_shader.h>
#include <xsi_shaderdef.h>
#include <xsi_shaderparamdefcontainer.h>
#include <xsi_shaderparamdef.h>
#include <xsi_color4f.h>
#include <xsi_color.h>
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

void find_texture(const XSI::ShaderParameter &xsi_texture, ULONG & source_id, bool &is_texture, bool &is_xsi_image, XSI::Shader &xsi_source_shader)
{
	is_texture = false;
	is_xsi_image = false;
	XSI::ShaderParameter xsi_source = get_source_parameter(xsi_texture, true);  // can return the same parameter, if there is no connection
	if (xsi_source.IsValid())
	{
		xsi_source_shader = xsi_source.GetParent();
		if (xsi_source_shader.IsValid())
		{
			XSI::CString source_progid = xsi_source_shader.GetProgID();
			source_id = xsi_source_shader.GetObjectID();
			if (source_progid == "CyclesShadersPlugin.CyclesImageTexture.1.0")
			{
				is_texture = true;
			}
			else if (source_progid == "Softimage.txt2d-image-explicit.1.0")
			{
				is_texture = true;
				is_xsi_image = true;
			}
		}
	}
}

ccl::ShaderNode* sync_gltf_shader(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Shader& xsi_shader, const XSI::CString& shader_type, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, std::vector<XSI::CStringArray>& aovs, const XSI::CTime& eval_time)
{
	if (shader_type == "MetallicRoughness")
	{
		XSI::CParameterRefArray xsi_gltf_params = xsi_shader.GetParameters();

		ccl::PrincipledBsdfNode* bsdf_node = shader_graph->create_node<ccl::PrincipledBsdfNode>();
		shader_graph->add(bsdf_node);

		XSI::MATH::CColor4f xsi_base_color = get_color_parameter_value(xsi_gltf_params, "baseColorFactor", eval_time);
		float xsi_metalic = get_float_parameter_value(xsi_gltf_params, "metallicFactor", eval_time);
		float xsi_roughness = get_float_parameter_value(xsi_gltf_params, "roughnessFactor", eval_time);
		float xsi_normal_scale = get_float_parameter_value(xsi_gltf_params, "scale", eval_time);
		float xsi_occlusion_strength = get_float_parameter_value(xsi_gltf_params, "strength", eval_time);
		XSI::MATH::CColor4f xsi_emissive_color = get_color_parameter_value(xsi_gltf_params, "emissiveFactor", eval_time);

		// base color is mix of the actual base color texture and occlusion
		ccl::SeparateColorNode* occlusion_separator = shader_graph->create_node<ccl::SeparateColorNode>();
		shader_graph->add(occlusion_separator);
		XSI::ShaderParameter xsi_occlusion_texture = xsi_gltf_params.GetItem("occlusionTexture");
		sync_float3_parameter(scene, shader_graph, occlusion_separator, xsi_occlusion_texture, nodes_map, aovs, "Color", eval_time);
		occlusion_separator->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f));
		// extract only R channel from occlusion color
		// conver it to the color
		ccl::CombineColorNode* occlusion_combiner = shader_graph->create_node<ccl::CombineColorNode>();
		shader_graph->add(occlusion_combiner);
		// connect r-output of the separator to all cahnnels of the combiner
		shader_graph->connect(occlusion_separator->output("Red"), occlusion_combiner->input("Red"));
		shader_graph->connect(occlusion_separator->output("Red"), occlusion_combiner->input("Green"));
		shader_graph->connect(occlusion_separator->output("Red"), occlusion_combiner->input("Blue"));

		// use occlusion combiner output (Color port) as input to the color multiplicator
		ccl::MixNode* occlusion_mix = shader_graph->create_node<ccl::MixNode>();
		shader_graph->add(occlusion_mix);
		shader_graph->connect(occlusion_combiner->output("Color"), occlusion_mix->input("Color2"));
		occlusion_mix->set_mix_type(ccl::NodeMix::NODE_MIX_MUL);
		occlusion_mix->set_fac(xsi_occlusion_strength);

		ccl::MixNode* base_mix = shader_graph->create_node<ccl::MixNode>();
		shader_graph->add(base_mix);

		XSI::ShaderParameter xsi_base_texture = xsi_gltf_params.GetItem("baseColorTexture");
		sync_float3_parameter(scene, shader_graph, base_mix, xsi_base_texture, nodes_map, aovs, "Color1", eval_time);
		base_mix->set_color1(ccl::make_float3(1.0f, 1.0f, 1.0f));
		base_mix->set_color2(color4_to_float3(xsi_base_color));
		base_mix->set_mix_type(ccl::NodeMix::NODE_MIX_MUL);
		base_mix->set_fac(1.0f);

		shader_graph->connect(base_mix->output("Color"), occlusion_mix->input("Color1"));
		shader_graph->connect(occlusion_mix->output("Color"), bsdf_node->input("Base Color"));

		// metallic is b-channel of the map
		// roughness is g-channel of the map
		ccl::SeparateColorNode* mr_separator = shader_graph->create_node<ccl::SeparateColorNode>();
		shader_graph->add(mr_separator);
		XSI::ShaderParameter xsi_mr_texture = xsi_gltf_params.GetItem("metallicRoughnessTexture");
		sync_float3_parameter(scene, shader_graph, mr_separator, xsi_mr_texture, nodes_map, aovs, "Color", eval_time);
		mr_separator->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f));

		ccl::MathNode* metallic_mix = shader_graph->create_node<ccl::MathNode>();
		shader_graph->add(metallic_mix);
		metallic_mix->set_value2(xsi_metalic);
		metallic_mix->set_math_type(ccl::NodeMathType::NODE_MATH_MULTIPLY);
		shader_graph->connect(mr_separator->output("Blue"), metallic_mix->input("Value1"));
		shader_graph->connect(metallic_mix->output("Value"), bsdf_node->input("Metallic"));

		ccl::MathNode* roughness_mix = shader_graph->create_node<ccl::MathNode>();
		shader_graph->add(roughness_mix);
		roughness_mix->set_value2(xsi_roughness);
		roughness_mix->set_math_type(ccl::NodeMathType::NODE_MATH_MULTIPLY);
		shader_graph->connect(mr_separator->output("Green"), roughness_mix->input("Value1"));
		shader_graph->connect(roughness_mix->output("Value"), bsdf_node->input("Roughness"));

		// emission
		ccl::MixNode* emission_mix = shader_graph->create_node<ccl::MixNode>();
		shader_graph->add(emission_mix);

		XSI::ShaderParameter xsi_emissive_texture = xsi_gltf_params.GetItem("emissiveTexture");
		sync_float3_parameter(scene, shader_graph, emission_mix, xsi_emissive_texture, nodes_map, aovs, "Color1", eval_time);
		emission_mix->set_color1(ccl::make_float3(1.0f, 1.0f, 1.0f));
		emission_mix->set_color2(color4_to_float3(xsi_emissive_color));
		emission_mix->set_mix_type(ccl::NodeMix::NODE_MIX_MUL);
		emission_mix->set_fac(1.0f);

		shader_graph->connect(emission_mix->output("Color"), bsdf_node->input("Emission"));

		// normals
		// check is normal map is connected
		bool is_normal_texture = false;
		bool is_normal_xsi_image = false;
		ULONG normal_source_id = 0;
		XSI::ShaderParameter xsi_normal_texture = xsi_gltf_params.GetItem("normalTexture");
		XSI::Shader xsi_normal_source_shader;
		find_texture(xsi_normal_texture, normal_source_id, is_normal_texture, is_normal_xsi_image, xsi_normal_source_shader);
		if (is_normal_texture)
		{// there is connection with texture
			ccl::NormalMapNode* normal_node = shader_graph->create_node<ccl::NormalMapNode>();
			shader_graph->add(normal_node);
			normal_node->set_strength(xsi_normal_scale);
			shader_graph->connect(normal_node->output("Normal"), bsdf_node->input("Normal"));

			if (is_normal_xsi_image)
			{
				XSI::ShaderParameter tex_param(xsi_normal_source_shader.GetParameter("tex"));
				XSI::CRef tex_source = tex_param.GetSource();
				if (tex_source.IsValid())
				{
					XSI::ImageClip2 normal_clip(tex_source);

					// manualy export image node
					ccl::ImageTextureNode* image_node = shader_graph->create_node<ccl::ImageTextureNode>();
					shader_graph->add(image_node);

					XSIImageLoader* image_node_loader = new XSIImageLoader(normal_clip, ccl::u_colorspace_raw, 0, "", eval_time);
					image_node->handle = scene->image_manager->add_image(image_node_loader, image_node->image_params());

					image_node->set_colorspace(ccl::u_colorspace_raw);
					image_node->set_projection(ccl::NodeImageProjection::NODE_IMAGE_PROJ_FLAT);
					image_node->set_projection_blend(0.0);
					image_node->set_interpolation(ccl::InterpolationType::INTERPOLATION_SMART);
					image_node->set_extension(ccl::ExtensionType::EXTENSION_REPEAT);
					image_node->set_alpha_type(ccl::ImageAlphaType::IMAGE_ALPHA_AUTO);
					image_node->set_animated(false);

					// connect
					shader_graph->connect(image_node->output("Color"), normal_node->input("Color"));
				}
			}
			else
			{
				sync_float3_parameter(scene, shader_graph, normal_node, xsi_normal_texture, nodes_map, aovs, "Color", eval_time);
			}
		}

		// finally, transparent
		XSI::CString xsi_alpha_mode = xsi_gltf_params.GetValue("alphaMode", eval_time);
		if (xsi_alpha_mode == "MASK" || xsi_alpha_mode == "BLEND")
		{
			ccl::MathNode* transparent_mix = shader_graph->create_node<ccl::MathNode>();
			shader_graph->add(transparent_mix);

			transparent_mix->set_math_type(ccl::NodeMathType::NODE_MATH_MULTIPLY);
			transparent_mix->set_value1(1.0f);
			transparent_mix->set_value2(xsi_base_color.GetA());

			// connect to Value1 the Alpha output of the base color node (if it exists)
			bool is_base_texture = false;
			bool is_base_xsi_texture = false;
			ULONG base_texture_id = 0;
			XSI::Shader xsi_base_texture_source;
			find_texture(xsi_base_texture, base_texture_id, is_base_texture, is_base_xsi_texture, xsi_base_texture_source);
			if (is_base_texture && nodes_map.contains(base_texture_id))
			{
				ccl::ImageTextureNode* base_image = static_cast<ccl::ImageTextureNode*>(nodes_map[base_texture_id]);
				shader_graph->connect(base_image->output("Alpha"), transparent_mix->input("Value1"));
			}

			shader_graph->connect(transparent_mix->output("Value"), bsdf_node->input("Alpha"));
		}

		return bsdf_node;
	}

	return NULL;
}