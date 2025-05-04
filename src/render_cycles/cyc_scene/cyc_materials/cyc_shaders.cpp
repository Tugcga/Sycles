#include <xsi_shader.h>
#include <xsi_arrayparameter.h>
#include <xsi_time.h>
#include <xsi_shaderparameter.h>
#include <xsi_utils.h>
#include <xsi_project.h>
#include <xsi_imageclip2.h>

#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/image.h"

#include <unordered_map>
#include <string>

#include "../cyc_loaders/cyc_loaders.h"
#include "../../../input/input.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/math.h"
#include "../../../utilities/strings.h"
#include "../../../utilities/xsi_shaders.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/files_io.h"
#include "../../cyc_session/cyc_pass_utils.h"
#include "cyc_materials.h"
#include "names_converter.h"

void common_routine(ccl::Scene* scene, 
	ccl::ShaderNode* node,
	ccl::ShaderGraph* shader_graph, 
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, 
	const XSI::Shader& xsi_shader, 
	const XSI::CParameterRefArray& xsi_parameters, 
	const XSI::CTime &eval_time,
	std::vector<XSI::CStringArray>& aovs)
{
	ULONG xsi_shader_id = xsi_shader.GetObjectID();
	ccl::ustring node_name = ccl::ustring(xsi_shader.GetName().GetAsciiString());
	XSI::CString xsi_shader_progid = xsi_shader.GetProgID();

	// add node to the map
	// because when we will connect parameters, we should connect this node with other nodes
	nodes_map[xsi_shader_id] = node;
	node->name = node_name;

	// iterate over all parameters
	for (ULONG i = 0; i < xsi_parameters.GetCount(); i++)
	{
		XSI::ShaderParameter xsi_param = xsi_parameters[i];
		XSI::CString xsi_param_name = xsi_param.GetName();
		if (xsi_param.IsValid() && xsi_param_name.Length() > 0)
		{
			// check is this parameter is input or output
			bool is_input;
			XSI::siShaderParameterType param_type = xsi_shader.GetShaderParameterType(xsi_param_name, is_input);
			if (is_input)
			{
				std::string cycles_port_name = convert_port_name(xsi_shader_progid, xsi_param_name);
				if (cycles_port_name.length() > 0)
				{
					// if this parameter is input, set value and make connection
					ShaderParameterType parameter_type = get_shader_parameter_type(xsi_param);
					if (parameter_type == ShaderParameterType::ParameterType_Color3 || parameter_type == ShaderParameterType::ParameterType_Vector3 || parameter_type == ShaderParameterType::ParameterType_Color4)
					{
						sync_float3_parameter(scene, shader_graph, node, xsi_param, nodes_map, aovs, cycles_port_name, eval_time);
					}
					else
					{
						sync_float_parameter(scene, shader_graph, node, xsi_param, nodes_map, aovs, cycles_port_name, eval_time);
					}
				}
			}
		}

	}
}

std::array<float, 2> get_min_and_max(const XSI::FCurve &c1, const XSI::FCurve &c2, const XSI::FCurve &c3)
{
	std::array<float, 2> to_return;

	float min1 = c1.GetKeyAtIndex(0).GetTime();
	float max1 = c1.GetKeyAtIndex(c1.GetNumKeys() - 1).GetTime();
	float min2 = c2.GetKeyAtIndex(0).GetTime();
	float max2 = c1.GetKeyAtIndex(c1.GetNumKeys() - 1).GetTime();
	float min3 = c3.GetKeyAtIndex(0).GetTime();
	float max3 = c1.GetKeyAtIndex(c1.GetNumKeys() - 1).GetTime();

	to_return[0] = get_minimum(min1, min2, min3);
	to_return[1] = get_maximum(max1, max2, max3);

	return to_return;
}

ccl::array<ccl::float3> three_curves_to_array(const XSI::FCurve& c1, const XSI::FCurve& c2, const XSI::FCurve& c3, float min, float max, int size)
{
	ccl::array<ccl::float3> data;
	data.resize(size);
	const float range = max - min;

	for (int i = 0; i < size; i++)
	{
		float t = min + (float)i / (float)(size - 1) * range;

		data[i][0] = c1.Eval(t);
		data[i][1] = c2.Eval(t);
		data[i][2] = c3.Eval(t);
	}

	return data;
}

void form_ramp(std::vector<GradientPoint>& gradient, int size, ccl::array<ccl::float3>& colors, ccl::array<float>& alphas)
{
	ccl::array<int> order_indexes;
	order_indexes.resize(gradient.size());
	order_indexes[0] = 0;
	for (int i = 1; i < gradient.size(); i++)
	{
		float newPos = gradient[i].pos;
		int rewriteIndex = i;
		for (int j = 0; j < i; j++)
		{
			if (newPos < gradient[order_indexes[j]].pos)
			{
				rewriteIndex = j;
				j = i;
			}
		}
		for (int j = i - 1; j >= rewriteIndex; j--)
		{
			order_indexes[j + 1] = order_indexes[j];
		}
		order_indexes[rewriteIndex] = i;
	}

	colors.resize(size);
	alphas.resize(size);
	float step = 1.0 / (float)size;
	for (int i = 0; i < size; i++)
	{
		float key = step * i;

		int left = -1;
		int right = order_indexes.size();
		if (key < gradient[order_indexes[0]].pos)
		{
			left = -1;
			right = 0;
		}
		else if (key >= gradient[order_indexes[order_indexes.size() - 1]].pos)
		{
			left = order_indexes.size() - 1;
			right = order_indexes.size();
		}
		else
		{
			for (int j = 0; j < order_indexes.size() - 1; j++)
			{
				float leftPos = gradient[order_indexes[j]].pos;
				float rightPos = gradient[order_indexes[j + 1]].pos;
				if (key >= leftPos && rightPos > key)
				{
					left = j;
					right = j + 1;
					j = order_indexes.size() - 1;
				}
			}
		}

		if (left == -1)
		{
			GradientPoint first = gradient[order_indexes[0]];
			alphas[i] = first.color.GetA();
			colors[i] = color4_to_float3(first.color);
		}
		else if (right == order_indexes.size())
		{
			GradientPoint first = gradient[order_indexes[order_indexes.size() - 1]];
			alphas[i] = first.color.GetA();
			colors[i] = color4_to_float3(first.color);
		}
		else
		{
			GradientPoint first = gradient[order_indexes[left]];
			GradientPoint second = gradient[order_indexes[right]];
			float t = (key - first.pos) / (second.pos - first.pos);
			alphas[i] = interpolate_float_with_middle(first.color.GetA(), second.color.GetA(), t, first.mid);//(1 - t) * first.color.GetA() + t * second.color.GetA();
			colors[i] = color4_to_float3(interpolate_color(first.color, second.color, t, first.mid));
		}
	}
}

ccl::ShaderNode* sync_cycles_shader(ccl::Scene* scene, 
	const XSI::Shader& xsi_shader,
	const XSI::CString &shader_type, 
	const XSI::CParameterRefArray &xsi_parameters,
	const XSI::CTime &eval_time, 
	ccl::ShaderGraph* shader_graph, 
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs)
{
	if (shader_type == "DiffuseBSDF")
	{
		ccl::DiffuseBsdfNode* node = shader_graph->create_node<ccl::DiffuseBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "MetallicBSDF") {
		ccl::MetallicBsdfNode* node = shader_graph->create_node<ccl::MetallicBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString distribution = get_string_parameter_value(xsi_parameters, "distribution", eval_time);
		XSI::CString fresnel_type = get_string_parameter_value(xsi_parameters, "fresnel_type", eval_time);
		float ior_x = get_float_parameter_value(xsi_parameters, "ior_x", eval_time);
		float ior_y = get_float_parameter_value(xsi_parameters, "ior_y", eval_time);
		float ior_z = get_float_parameter_value(xsi_parameters, "ior_z", eval_time);
		float extinction_x = get_float_parameter_value(xsi_parameters, "extinction_x", eval_time);
		float extinction_y = get_float_parameter_value(xsi_parameters, "extinction_y", eval_time);
		float extinction_z = get_float_parameter_value(xsi_parameters, "extinction_z", eval_time);

		node->set_ior(ccl::make_float3(ior_x, ior_y, ior_z));
		node->set_k(ccl::make_float3(extinction_x, extinction_y, extinction_z));

		node->set_distribution(get_distribution(distribution, DistributionModes_Glass));
		node->set_fresnel_type(get_metallic_fresnel(fresnel_type));

		return node;
	}
	else if (shader_type == "PrincipledBSDF")
	{
		ccl::PrincipledBsdfNode* node = shader_graph->create_node<ccl::PrincipledBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString distribution = get_string_parameter_value(xsi_parameters, "Distribution", eval_time);
		XSI::CString ssmethod = get_string_parameter_value(xsi_parameters, "subsurface_method", eval_time);
		float radiusx = get_float_parameter_value(xsi_parameters, "RadiusX", eval_time);
		float radiusy = get_float_parameter_value(xsi_parameters, "RadiusY", eval_time);
		float radiusz = get_float_parameter_value(xsi_parameters, "RadiusZ", eval_time);

		node->set_distribution(get_distribution(distribution, DistributionModes_Principle));
		node->set_subsurface_method(get_subsurface_method(ssmethod));
		node->set_subsurface_radius(ccl::make_float3(radiusx, radiusy, radiusz));

		return node;
	}
	else if (shader_type == "TranslucentBSDF")
	{
		ccl::TranslucentBsdfNode* node = shader_graph->create_node<ccl::TranslucentBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);
		return node;
	}
	else if (shader_type == "TransparentBSDF")
	{
		ccl::TransparentBsdfNode* node = shader_graph->create_node<ccl::TransparentBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);
		return node;
	}
	else if (shader_type == "ToonBSDF")
	{
		ccl::ToonBsdfNode* node = shader_graph->create_node<ccl::ToonBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString component = get_string_parameter_value(xsi_parameters, "Component", eval_time);
		node->set_component(get_distribution(component, DistributionModes_Toon));

		return node;
	}
	else if (shader_type == "GlossyBSDF")
	{
		ccl::GlossyBsdfNode* node = shader_graph->create_node<ccl::GlossyBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString distribution = get_string_parameter_value(xsi_parameters, "Distribution", eval_time);
		node->set_distribution(get_distribution(distribution, DistributionModes_Glossy));

		return node;
	}
	else if (shader_type == "GlassBSDF")
	{
		ccl::GlassBsdfNode* node = shader_graph->create_node<ccl::GlassBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString distribution = get_string_parameter_value(xsi_parameters, "Distribution", eval_time);
		node->set_distribution(get_distribution(distribution, DistributionModes_Glass));

		return node;
	}
	else if (shader_type == "RefractionBSDF")
	{
		ccl::RefractionBsdfNode* node = shader_graph->create_node<ccl::RefractionBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString distribution = get_string_parameter_value(xsi_parameters, "Distribution", eval_time);
		node->set_distribution(get_distribution(distribution, DistributionModes_Refraction));

		return node;
	}
	else if (shader_type == "HairBSDF")
	{
		ccl::HairBsdfNode* node = shader_graph->create_node<ccl::HairBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString component = get_string_parameter_value(xsi_parameters, "Component", eval_time);
		node->set_component(get_distribution(component, DistributionModes_Hair));

		return node;
	}
	else if (shader_type == "Emission")
	{
		ccl::EmissionNode* node = shader_graph->create_node<ccl::EmissionNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "AmbientOcclusion")
	{
		ccl::AmbientOcclusionNode* node = shader_graph->create_node<ccl::AmbientOcclusionNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		int samples = get_int_parameter_value(xsi_parameters, "Samples", eval_time);
		bool inside = get_bool_parameter_value(xsi_parameters, "Inside", eval_time);
		bool local = get_bool_parameter_value(xsi_parameters, "OnlyLocal", eval_time);

		node->set_samples(samples);
		node->set_inside(inside);
		node->set_only_local(local);

		return node;
	}
	else if (shader_type == "Background")
	{
		ccl::BackgroundNode* node = shader_graph->create_node<ccl::BackgroundNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "Holdout")
	{
		ccl::HoldoutNode* node = shader_graph->create_node<ccl::HoldoutNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "AbsorptionVolume")
	{
		ccl::AbsorptionVolumeNode* node = shader_graph->create_node<ccl::AbsorptionVolumeNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "ScatterVolume")
	{
		ccl::ScatterVolumeNode* node = shader_graph->create_node<ccl::ScatterVolumeNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString phase = get_string_parameter_value(xsi_parameters, "phase", eval_time);
		node->set_phase(get_scatter_phase(phase));

		return node;
	}
	else if (shader_type == "PrincipledVolume")
	{
		ccl::PrincipledVolumeNode* node = shader_graph->create_node<ccl::PrincipledVolumeNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString density_attribute = get_string_parameter_value(xsi_parameters, "DensityAttribute", eval_time);
		XSI::CString color_attribute = get_string_parameter_value(xsi_parameters, "ColorAttribute", eval_time);
		XSI::CString temperature_attribute = get_string_parameter_value(xsi_parameters, "TemperatureAttribute", eval_time);

		node->set_density_attribute(ccl::ustring(density_attribute.GetAsciiString()));
		node->set_color_attribute(ccl::ustring(color_attribute.GetAsciiString()));
		node->set_temperature_attribute(ccl::ustring(temperature_attribute.GetAsciiString()));

		return node;
	}
	else if (shader_type == "PrincipledHairBSDF")
	{
		ccl::PrincipledHairBsdfNode* node = shader_graph->create_node<ccl::PrincipledHairBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString parameterization = get_string_parameter_value(xsi_parameters, "Parametrization", eval_time);
		XSI::CString model = get_string_parameter_value(xsi_parameters, "Model", eval_time);
		float abs_x = get_float_parameter_value(xsi_parameters, "AbsorptionCoefficientX", eval_time);
		float abs_y = get_float_parameter_value(xsi_parameters, "AbsorptionCoefficientY", eval_time);
		float abs_z = get_float_parameter_value(xsi_parameters, "AbsorptionCoefficientZ", eval_time);

		node->set_parametrization(principled_hair_parametrization(parameterization));
		node->set_model(principled_hair_model(model));  // TODO: may be the method is named in another way
		node->set_absorption_coefficient(ccl::make_float3(abs_x, abs_y, abs_z));

		return node;
	}
	else if (shader_type == "SheenBSDF")
	{
		ccl::SheenBsdfNode* node = shader_graph->create_node<ccl::SheenBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString distributeion = get_string_parameter_value(xsi_parameters, "distribution", eval_time);
		node->set_distribution(sheen_distribution(distributeion));

		return node;
	}
	else if (shader_type == "RayPortalBSDF")
	{
		ccl::RayPortalBsdfNode* node = shader_graph->create_node<ccl::RayPortalBsdfNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "SubsurfaceScattering")
	{
		ccl::SubsurfaceScatteringNode* node = shader_graph->create_node<ccl::SubsurfaceScatteringNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString falloff = get_string_parameter_value(xsi_parameters, "Falloff", eval_time);
		float radius_x = get_float_parameter_value(xsi_parameters, "RadiusX", eval_time);
		float radius_y = get_float_parameter_value(xsi_parameters, "RadiusY", eval_time);
		float radius_z = get_float_parameter_value(xsi_parameters, "RadiusZ", eval_time);

		if (falloff == "random_walk")
		{
			node->set_method(ccl::CLOSURE_BSSRDF_RANDOM_WALK_ID);
		}
		else if (falloff == "random_walk_fixed")
		{
			node->set_method(ccl::CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID);
		}
		else
		{
			node->set_method(ccl::CLOSURE_BSSRDF_BURLEY_ID);
		}
		node->set_radius(ccl::make_float3(radius_x, radius_y, radius_z));

		return node;
	}
	else if (shader_type == "ImageTexture")
	{
		ccl::ImageTextureNode* node = shader_graph->create_node<ccl::ImageTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::ImageClip2 clip = get_clip_parameter_value(xsi_parameters, "image", eval_time);
		XSI::CString file_path = "";
		XSI::CString color_space = get_string_parameter_value(xsi_parameters, "ColorSpace", eval_time);
		XSI::CString interpolation = get_string_parameter_value(xsi_parameters, "Interpolation", eval_time);
		XSI::CString projection = get_string_parameter_value(xsi_parameters, "Projection", eval_time);
		float projection_blend = get_float_parameter_value(xsi_parameters, "ProjectionBlend", eval_time);
		XSI::CString extension = get_string_parameter_value(xsi_parameters, "Extension", eval_time);
		bool premultiply_alpha = get_bool_parameter_value(xsi_parameters, "premultiply_alpha", eval_time);
		XSI::CString image_source = get_string_parameter_value(xsi_parameters, "ImageSource", eval_time);
		//XSI::CString tiles = get_string_parameter_value(xsi_parameters, "tiles", eval_time);
		int image_frames = get_int_parameter_value(xsi_parameters, "ImageFrames", eval_time);
		int start_frame = get_int_parameter_value(xsi_parameters, "ImageStartFrame", eval_time);
		int offset = get_int_parameter_value(xsi_parameters, "ImageOffset", eval_time);
		bool cyclic = get_bool_parameter_value(xsi_parameters, "ImageCyclic", eval_time);

		if (clip.IsValid())
		{
			file_path = clip.GetFileName();

			ULONG xsi_clip_id = clip.GetObjectID();
			ccl::ustring selected_colorscape = color_space == "color" ? ccl::u_colorspace_srgb : ccl::u_colorspace_raw;

			// we add each clip separately (without caching in update context)
			// because one clip in different materials can have different effects (crop, blur and so on)
			// so, we should use different images
			// get tiles
			std::map<int, XSI::CString> tile_to_path_map = sync_image_tiles(file_path);
			ccl::array<int> tiles = exctract_tiles(tile_to_path_map);

			// set node parameters before loader, because it use these parameters for define alpha
			node->set_colorspace(selected_colorscape);
			node->set_projection(projection == "flat" ? ccl::NodeImageProjection::NODE_IMAGE_PROJ_FLAT : (projection == "box" ? ccl::NodeImageProjection::NODE_IMAGE_PROJ_BOX : (projection == "sphere" ? ccl::NodeImageProjection::NODE_IMAGE_PROJ_SPHERE : (projection == "tube" ? ccl::NodeImageProjection::NODE_IMAGE_PROJ_TUBE : ccl::NodeImageProjection::NODE_IMAGE_PROJ_FLAT))));
			node->set_projection_blend(projection_blend);
			node->set_interpolation(interpolation == "Smart" ? ccl::InterpolationType::INTERPOLATION_SMART : (interpolation == "Cubic" ? ccl::InterpolationType::INTERPOLATION_CUBIC : (interpolation == "Closest" ? ccl::InterpolationType::INTERPOLATION_CLOSEST : ccl::InterpolationType::INTERPOLATION_LINEAR)));
			node->set_extension(extension == "Clip" ? ccl::ExtensionType::EXTENSION_CLIP : (extension == "Extend" ? ccl::ExtensionType::EXTENSION_EXTEND : ccl::ExtensionType::EXTENSION_REPEAT));

			node->set_alpha_type(premultiply_alpha ? ccl::ImageAlphaType::IMAGE_ALPHA_ASSOCIATED : ccl::ImageAlphaType::IMAGE_ALPHA_CHANNEL_PACKED);
			node->set_animated(false);

			if(image_source == "tiled")
			{
				// we should create array of loaders
				ccl::vector<std::unique_ptr<ccl::ImageLoader>> loaders;
				loaders.reserve(tiles.size());
				for (size_t i = 0; i < tiles.size(); i++)
				{
					int tile_value = tiles[i];
					XSI::CString tile_path = tile_to_path_map[tile_value];
					if (tile_path != file_path)
					{
						loaders.push_back(std::make_unique<XSIImageLoader>(clip, selected_colorscape, tile_value, tile_path, eval_time));
					}
					else
					{
						loaders.push_back(std::make_unique<XSIImageLoader>(clip, selected_colorscape, tile_value, "", eval_time));
					}
				}

				node->handle = scene->image_manager->add_image(std::move(loaders), node->image_params());
			}
			else
			{
				XSIImageLoader* image_loader = new XSIImageLoader(clip, selected_colorscape, 0, "", eval_time);
				node->handle = scene->image_manager->add_image(std::unique_ptr<ccl::ImageLoader>(image_loader), node->image_params());
			}

			node->set_tiles(tiles);

			return node;
		}
		else
		{
			return NULL;
		}
	}
	else if (shader_type == "EnvironmentTexture")
	{
		ccl::EnvironmentTextureNode* node = shader_graph->create_node<ccl::EnvironmentTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		node->tex_mapping.rotation = ccl::make_float3(-0.5f * XSI::MATH::PI, 0.0f, 0.0f);

		XSI::ImageClip2 clip = get_clip_parameter_value(xsi_parameters, "image", eval_time);
		XSI::CString file_path = "";

		XSI::CString color_space = get_string_parameter_value(xsi_parameters, "ColorSpace", eval_time);
		XSI::CString interpolation = get_string_parameter_value(xsi_parameters, "Interpolation", eval_time);
		XSI::CString projection = get_string_parameter_value(xsi_parameters, "Projection", eval_time);
		bool premultiply_alpha = get_bool_parameter_value(xsi_parameters, "premultiply_alpha", eval_time);

		XSI::CString image_source = get_string_parameter_value(xsi_parameters, "ImageSource", eval_time);
		int image_frames = get_int_parameter_value(xsi_parameters, "ImageFrames", eval_time);
		int start_frame = get_int_parameter_value(xsi_parameters, "ImageStartFrame", eval_time);
		int offset = get_int_parameter_value(xsi_parameters, "ImageOffset", eval_time);
		bool cyclic = get_bool_parameter_value(xsi_parameters, "ImageCyclic", eval_time);

		if (clip.IsValid())
		{
			file_path = clip.GetFileName();

			bool temp_flag = false;
			std::string filename = build_source_image_path(file_path, image_source, cyclic, start_frame, image_frames, offset, eval_time, false, temp_flag);
			ccl::ustring selected_colorscape = color_space == "color" ? ccl::u_colorspace_srgb : ccl::u_colorspace_raw;

			node->set_colorspace(selected_colorscape);
			node->set_interpolation(interpolation == "Smart" ? ccl::InterpolationType::INTERPOLATION_SMART : (interpolation == "Cubic" ? ccl::InterpolationType::INTERPOLATION_CUBIC : (interpolation == "Closest" ? ccl::InterpolationType::INTERPOLATION_CLOSEST : ccl::InterpolationType::INTERPOLATION_LINEAR)));
			node->set_projection(projection == "equirectangular" ? ccl::NodeEnvironmentProjection::NODE_ENVIRONMENT_EQUIRECTANGULAR : (projection == "mirrorball" ? ccl::NodeEnvironmentProjection::NODE_ENVIRONMENT_MIRROR_BALL : ccl::NodeEnvironmentProjection::NODE_ENVIRONMENT_EQUIRECTANGULAR));
			node->set_alpha_type(premultiply_alpha ? ccl::ImageAlphaType::IMAGE_ALPHA_ASSOCIATED : ccl::ImageAlphaType::IMAGE_ALPHA_CHANNEL_PACKED);

			XSIImageLoader* image_loader = new XSIImageLoader(clip, selected_colorscape, 0, "", eval_time);
			node->handle = scene->image_manager->add_image(std::unique_ptr<ccl::ImageLoader>(image_loader), node->image_params());

			return node;
		}
		else
		{
			return NULL;
		}
	}
	else if (shader_type == "SkyTexture")
	{
		ccl::SkyTextureNode* node = shader_graph->create_node<ccl::SkyTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		float sun_direction_x = get_float_parameter_value(xsi_parameters, "SunDirectionX", eval_time);
		float sun_direction_y = get_float_parameter_value(xsi_parameters, "SunDirectionY", eval_time);
		float sun_direction_z = get_float_parameter_value(xsi_parameters, "SunDirectionZ", eval_time);
		float turbidity = get_float_parameter_value(xsi_parameters, "Turbidity", eval_time);
		float ground_albedo = get_float_parameter_value(xsi_parameters, "GroundAlbedo", eval_time);
		float sun_size = get_float_parameter_value(xsi_parameters, "SunSize", eval_time) * XSI::MATH::PI / 180.0;
		float sun_intensity = get_float_parameter_value(xsi_parameters, "SunIntensity", eval_time);
		float sun_elevation = get_float_parameter_value(xsi_parameters, "SunElevation", eval_time) * XSI::MATH::PI / 180.0f;
		float sun_rotation = get_float_parameter_value(xsi_parameters, "SunRotation", eval_time) * XSI::MATH::PI / 180.0f;
		float altitude = get_float_parameter_value(xsi_parameters, "Altitude", eval_time);
		float air = get_float_parameter_value(xsi_parameters, "Air", eval_time);
		float dust = get_float_parameter_value(xsi_parameters, "Dust", eval_time);
		float ozone = get_float_parameter_value(xsi_parameters, "Ozone", eval_time);
		bool sun_disc = get_bool_parameter_value(xsi_parameters, "SunDisc", eval_time);

		float sun_x;
		float sun_y;
		float sun_z;
		bool is_subsun = obtain_subsub_directions(xsi_shader, sun_x, sun_y, sun_z, eval_time);

		float dirSquare = sun_direction_x * sun_direction_x + sun_direction_y * sun_direction_y + sun_direction_z * sun_direction_z;
		if (dirSquare > 0.01)
		{// normalize direction
			float s = sqrtf(dirSquare);
			sun_direction_x = sun_direction_x / s;
			sun_direction_y = sun_direction_y / s;
			sun_direction_z = sun_direction_z / s;
		}
		// apply shift transformation to sun direction
		float dx = sun_direction_x;
		float dy = -1 * sun_direction_z;
		float dz = sun_direction_y;

		node->tex_mapping.rotation = ccl::make_float3(-0.5 * XSI::MATH::PI, 0, 0);
		node->set_sky_type(type == "preetham" ? ccl::NodeSkyType::NODE_SKY_PREETHAM : (type == "hosekwilkil" ? ccl::NodeSkyType::NODE_SKY_HOSEK : ccl::NodeSkyType::NODE_SKY_NISHITA));
		if (is_subsun)
		{
			node->set_sun_direction(ccl::make_float3(sun_x, -1 * sun_z, sun_y));
			float a_sin_z = std::asin(sun_z);
			float xz_length = sqrtf(sun_x * sun_x + sun_z * sun_z);
			sun_x = sun_x / xz_length;
			sun_rotation = a_sin_z > 0 ? -std::asin(sun_x) + XSI::MATH::PI : asin(sun_x);
			sun_elevation = std::asin(sun_y);
		}
		else
		{
			node->set_sun_direction(ccl::make_float3(dx, dy, dz));
		}

		node->set_turbidity(turbidity);
		node->set_ground_albedo(ground_albedo);

		node->set_sun_disc(sun_disc);
		node->set_sun_size(sun_size);
		node->set_sun_intensity(sun_intensity);
		node->set_sun_elevation(sun_elevation);
		node->set_sun_rotation(sun_rotation);
		node->set_altitude(altitude);
		node->set_air_density(air);
		node->set_dust_density(dust);
		node->set_ozone_density(ozone);

		return node;

	}
	else if (shader_type == "NoiseTexture")
	{
		ccl::NoiseTextureNode* node = shader_graph->create_node<ccl::NoiseTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString noise_dimensions = get_string_parameter_value(xsi_parameters, "NoiseDimensions", eval_time);
		node->set_dimensions(get_dimensions_type(noise_dimensions));
		XSI::CString noise_type = get_string_parameter_value(xsi_parameters, "type", eval_time);
		node->set_type(get_noise_type(noise_type));
		bool normalize = get_bool_parameter_value(xsi_parameters, "normalize", eval_time);
		node->set_use_normalize(normalize);

		return node;
	}
	else if (shader_type == "CheckerTexture")
	{
		ccl::CheckerTextureNode* node = shader_graph->create_node<ccl::CheckerTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "BrickTexture")
	{
		ccl::BrickTextureNode* node = shader_graph->create_node<ccl::BrickTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		float offset = get_float_parameter_value(xsi_parameters, "Offset", eval_time);
		int offset_frequency = get_int_parameter_value(xsi_parameters, "OffsetFrequency", eval_time);
		float squash = get_float_parameter_value(xsi_parameters, "Squash", eval_time);
		int squash_frequency = get_int_parameter_value(xsi_parameters, "SquashFrequency", eval_time);
		node->set_offset(offset);
		node->set_offset_frequency(offset_frequency);
		node->set_squash(squash);
		node->set_squash_frequency(squash_frequency);

		return node;
	}
	else if (shader_type == "GradientTexture")
	{
		ccl::GradientTextureNode* node = shader_graph->create_node<ccl::GradientTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		node->set_gradient_type(get_gradient_type(type));

		return node;
	}
	else if (shader_type == "VoronoiTexture")
	{
		ccl::VoronoiTextureNode* node = shader_graph->create_node<ccl::VoronoiTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString dimensions = get_string_parameter_value(xsi_parameters, "VoronoiDimensions", eval_time);
		XSI::CString distance = get_string_parameter_value(xsi_parameters, "Distance", eval_time);
		XSI::CString feature = get_string_parameter_value(xsi_parameters, "Feature", eval_time);
		bool normalize = get_bool_parameter_value(xsi_parameters, "normalize", eval_time);
		node->set_dimensions(get_dimensions_type(dimensions));
		node->set_metric(voronoi_distance(distance));
		node->set_feature(voronoi_feature(feature));
		node->set_use_normalize(normalize);

		return node;
	}
	else if (shader_type == "MagicTexture")
	{
		ccl::MagicTextureNode* node = shader_graph->create_node<ccl::MagicTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		int depth = get_int_parameter_value(xsi_parameters, "Depth", eval_time);
		node->set_depth(depth);

		return node;
	}
	else if (shader_type == "WaveTexture")
	{
		ccl::WaveTextureNode* node = shader_graph->create_node<ccl::WaveTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		XSI::CString bands_direction = get_string_parameter_value(xsi_parameters, "bands_direction", eval_time);
		XSI::CString rings_direction = get_string_parameter_value(xsi_parameters, "rings_direction", eval_time);
		XSI::CString profile = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		node->set_wave_type(get_wave_type(type));
		node->set_profile(get_wave_profile(profile));
		node->set_bands_direction(get_wave_bands_direction(bands_direction));
		node->set_rings_direction(get_wave_rings_direction(rings_direction));

		return node;
	}
	else if (shader_type == "IESTexture")
	{
		ccl::IESLightNode* node = shader_graph->create_node<ccl::IESLightNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString file_path = get_string_parameter_value(xsi_parameters, "FilePath", eval_time);
		if (file_path.Length() > 0 && !XSI::CUtils::IsAbsolutePath(file_path))
		{
			file_path = XSI::CUtils::ResolvePath(XSI::CUtils::BuildPath(get_project_path(), file_path));
		}
		if (file_path.Length() > 0)
		{
			node->set_filename(OIIO::ustring(file_path.GetAsciiString()));
		}
		else
		{
			return NULL;
		}

		return node;
	}
	else if (shader_type == "WhiteNoiseTexture")
	{
		ccl::WhiteNoiseTextureNode* node = shader_graph->create_node<ccl::WhiteNoiseTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString dimensions = get_string_parameter_value(xsi_parameters, "NoiseDimensions", eval_time);
		float vector_x = get_float_parameter_value(xsi_parameters, "VectorX", eval_time);
		float vector_y = get_float_parameter_value(xsi_parameters, "VectorY", eval_time);
		float vector_z = get_float_parameter_value(xsi_parameters, "VectorZ", eval_time);

		node->set_dimensions(get_dimensions_type(dimensions));
		node->set_vector(ccl::make_float3(vector_x, vector_y, vector_z));

		return node;
	}
	else if (shader_type == "GaborTexture")
	{
		ccl::GaborTextureNode* node = shader_graph->create_node<ccl::GaborTextureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);
		XSI::CString gabor_type = get_string_parameter_value(xsi_parameters, "gabor_type", eval_time);
		float orientation_3d_x = get_float_parameter_value(xsi_parameters, "orientation_3d_x", eval_time);
		float orientation_3d_y = get_float_parameter_value(xsi_parameters, "orientation_3d_y", eval_time);
		float orientation_3d_z = get_float_parameter_value(xsi_parameters, "orientation_3d_z", eval_time);

		node->set_type(get_gabor_type(gabor_type));
		node->set_orientation_3d(ccl::make_float3(orientation_3d_x, orientation_3d_y, orientation_3d_z));

		return node;
	}
	else if (shader_type == "Normal")
	{
		ccl::NormalNode* node = shader_graph->create_node<ccl::NormalNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		float direction_x = get_float_parameter_value(xsi_parameters, "DirectionX", eval_time);
		float direction_y = get_float_parameter_value(xsi_parameters, "DirectionY", eval_time);
		float direction_z = get_float_parameter_value(xsi_parameters, "DirectionZ", eval_time);
		float normal_x = get_float_parameter_value(xsi_parameters, "NormalX", eval_time);
		float normal_y = get_float_parameter_value(xsi_parameters, "NormalY", eval_time);
		float normal_z = get_float_parameter_value(xsi_parameters, "NormalZ", eval_time);

		node->set_direction(ccl::make_float3(direction_x, direction_y, direction_z));
		node->set_normal(ccl::make_float3(normal_x, normal_y, normal_z));

		return node;
	}
	else if (shader_type == "Bump")
	{
		ccl::BumpNode* node = shader_graph->create_node<ccl::BumpNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		bool invert = get_bool_parameter_value(xsi_parameters, "Invert", eval_time);
		node->set_invert(invert);

		return node;
	}
	else if (shader_type == "Mapping")
	{
		ccl::MappingNode* node = shader_graph->create_node<ccl::MappingNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		float translation_x = get_float_parameter_value(xsi_parameters, "TranslationX", eval_time);
		float translation_y = get_float_parameter_value(xsi_parameters, "TranslationY", eval_time);
		float translation_z = get_float_parameter_value(xsi_parameters, "TranslationZ", eval_time);

		float rotation_x = get_float_parameter_value(xsi_parameters, "RotationX", eval_time);
		float rotation_y = get_float_parameter_value(xsi_parameters, "RotationY", eval_time);
		float rotation_z = get_float_parameter_value(xsi_parameters, "RotationZ", eval_time);

		float scale_x = get_float_parameter_value(xsi_parameters, "ScaleX", eval_time);
		float scale_y = get_float_parameter_value(xsi_parameters, "ScaleY", eval_time);
		float scale_z = get_float_parameter_value(xsi_parameters, "ScaleZ", eval_time);

		node->set_location(ccl::make_float3(translation_x, translation_y, translation_z));
		node->set_rotation(ccl::make_float3(DEG2RADF(rotation_x), DEG2RADF(rotation_y), DEG2RADF(rotation_z)));
		node->set_scale(ccl::make_float3(scale_x, scale_y, scale_z));
		node->set_mapping_type(type == "Point" ? ccl::NodeMappingType::NODE_MAPPING_TYPE_POINT :
			(type == "Texture" ? ccl::NodeMappingType::NODE_MAPPING_TYPE_TEXTURE :
				(type == "Vector" ? ccl::NodeMappingType::NODE_MAPPING_TYPE_VECTOR : ccl::NodeMappingType::NODE_MAPPING_TYPE_NORMAL)));

		return node;
	}
	else if (shader_type == "NormalMap")
	{
		ccl::NormalMapNode* node = shader_graph->create_node<ccl::NormalMapNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString space = get_string_parameter_value(xsi_parameters, "Space", eval_time);
		XSI::CString attribute = get_string_parameter_value(xsi_parameters, "Attribute", eval_time);

		node->set_space(get_normal_map_space(space));
		node->set_attribute(OIIO::ustring(attribute.GetAsciiString()));

		return node;
	}
	else if (shader_type == "VectorTransform")
	{
		ccl::VectorTransformNode* node = shader_graph->create_node<ccl::VectorTransformNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		XSI::CString convert_from = get_string_parameter_value(xsi_parameters, "ConvertFrom", eval_time);
		XSI::CString convert_to = get_string_parameter_value(xsi_parameters, "ConvertTo", eval_time);
		float vector_x = get_float_parameter_value(xsi_parameters, "VectorX", eval_time);
		float vector_y = get_float_parameter_value(xsi_parameters, "VectorY", eval_time);
		float vector_z = get_float_parameter_value(xsi_parameters, "VectorZ", eval_time);

		node->set_transform_type(get_vector_transform_type(type));
		node->set_convert_from(get_vector_transform_convert_space(convert_from));
		node->set_convert_to(get_vector_transform_convert_space(convert_to));
		node->set_vector(ccl::make_float3(vector_x, vector_y, vector_z));

		return node;
	}
	else if (shader_type == "VectorRotate")
	{
		ccl::VectorRotateNode* node = shader_graph->create_node<ccl::VectorRotateNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		bool invert = get_bool_parameter_value(xsi_parameters, "Invert", eval_time);
		float center_x = get_float_parameter_value(xsi_parameters, "CenterX", eval_time);
		float center_y = get_float_parameter_value(xsi_parameters, "CenterY", eval_time);
		float center_z = get_float_parameter_value(xsi_parameters, "CenterZ", eval_time);

		float axis_x = get_float_parameter_value(xsi_parameters, "AxisX", eval_time);
		float axis_y = get_float_parameter_value(xsi_parameters, "AxisY", eval_time);
		float axis_z = get_float_parameter_value(xsi_parameters, "AxisZ", eval_time);

		float rotation_x = get_float_parameter_value(xsi_parameters, "VectorRotationX", eval_time);
		float rotation_y = get_float_parameter_value(xsi_parameters, "VectorRotationY", eval_time);
		float rotation_z = get_float_parameter_value(xsi_parameters, "VectorRotationZ", eval_time);

		node->set_rotate_type(get_vector_rotate_type(type));
		node->set_invert(invert);
		node->set_center(ccl::make_float3(center_x, center_y, center_z));
		node->set_axis(ccl::make_float3(axis_x, axis_y, axis_z));
		node->set_rotation(ccl::make_float3(rotation_x, rotation_y, rotation_z));

		return node;
	}
	else if (shader_type == "VectorCurves")
	{
		ccl::VectorCurvesNode* node = shader_graph->create_node<ccl::VectorCurvesNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::FCurve x_curve = get_fcurve_parameter_value(xsi_parameters, "xCurve", eval_time);
		XSI::FCurve y_curve = get_fcurve_parameter_value(xsi_parameters, "yCurve", eval_time);
		XSI::FCurve z_curve = get_fcurve_parameter_value(xsi_parameters, "zCurve", eval_time);

		std::array<float, 2> min_max = get_min_and_max(x_curve, y_curve, z_curve);
		node->set_min_x(min_max[0]);
		node->set_max_x(min_max[1]);
		ccl::array<ccl::float3> curves_array = three_curves_to_array(x_curve, y_curve, z_curve, min_max[0], min_max[1], RAMP_TABLE_SIZE);
		node->set_curves(curves_array);

		return node;
	}
	else if (shader_type == "Displacement")
	{
		ccl::DisplacementNode* node = shader_graph->create_node<ccl::DisplacementNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString space = get_string_parameter_value(xsi_parameters, "Space", eval_time);
		node->set_space(space == "object" ? ccl::NODE_NORMAL_MAP_OBJECT : ccl::NODE_NORMAL_MAP_WORLD);

		return node;
	}
	else if (shader_type == "VectorDisplacement")
	{
		ccl::VectorDisplacementNode* node = shader_graph->create_node<ccl::VectorDisplacementNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString space = get_string_parameter_value(xsi_parameters, "Space", eval_time);
		node->set_space(space == "object" ? ccl::NODE_NORMAL_MAP_OBJECT : (space == "tangent" ? ccl::NODE_NORMAL_MAP_TANGENT : ccl::NODE_NORMAL_MAP_WORLD));

		return node;
	}
	else if (shader_type == "Geometry")
	{
		ccl::GeometryNode* node = shader_graph->create_node<ccl::GeometryNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "TextureCoordinate")
	{
		ccl::TextureCoordinateNode* node = shader_graph->create_node<ccl::TextureCoordinateNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "LightPath")
	{
		ccl::LightPathNode* node = shader_graph->create_node<ccl::LightPathNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "ObjectInfo")
	{
		ccl::ObjectInfoNode* node = shader_graph->create_node<ccl::ObjectInfoNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "VolumeInfo")
	{
		ccl::VolumeInfoNode* node = shader_graph->create_node<ccl::VolumeInfoNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "PointInfo")
	{
		ccl::PointInfoNode* node = shader_graph->create_node<ccl::PointInfoNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "ParticleInfo")
	{
		ccl::ParticleInfoNode* node = shader_graph->create_node<ccl::ParticleInfoNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "HairInfo")
	{
		ccl::HairInfoNode* node = shader_graph->create_node<ccl::HairInfoNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "Value")
	{
		ccl::ValueNode* node = shader_graph->create_node<ccl::ValueNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		float value_in = get_float_parameter_value(xsi_parameters, "ValueIn", eval_time);
		node->set_value(value_in);

		return node;
	}
	else if (shader_type == "Color")
	{
		ccl::ColorNode* node = shader_graph->create_node<ccl::ColorNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::MATH::CColor4f color = get_color_parameter_value(xsi_parameters, "ColorIn", eval_time);
		node->set_value(color4_to_float3(color));

		return node;
	}
	else if (shader_type == "Attribute")
	{
		ccl::AttributeNode* node = shader_graph->create_node<ccl::AttributeNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString attribute = get_string_parameter_value(xsi_parameters, "Attribute", eval_time);
		node->set_attribute(ccl::ustring(attribute.GetAsciiString()));

		return node;
	}
	else if (shader_type == "OutputColorAOV")
	{
		ccl::OutputAOVNode* node = shader_graph->create_node<ccl::OutputAOVNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString channel_name = get_string_parameter_value(xsi_parameters, "aov_name", eval_time);
		node->set_name(OIIO::ustring(add_prefix_to_aov_name(channel_name, true).GetAsciiString()));
		aovs[0].Add(channel_name);

		return node;
	}
	else if (shader_type == "OutputValueAOV")
	{
		ccl::OutputAOVNode* node = shader_graph->create_node<ccl::OutputAOVNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString channel_name = get_string_parameter_value(xsi_parameters, "aov_name", eval_time);
		node->set_name(OIIO::ustring(add_prefix_to_aov_name(channel_name, false).GetAsciiString()));
		aovs[1].Add(channel_name);

		return node;
	}
	else if (shader_type == "VertexColor")
	{
		ccl::VertexColorNode* node = shader_graph->create_node<ccl::VertexColorNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString name = get_string_parameter_value(xsi_parameters, "LayerName", eval_time);
		node->set_layer_name(ccl::ustring(name.GetAsciiString()));

		return node;
	}
	else if (shader_type == "Bevel")
	{
		ccl::BevelNode* node = shader_graph->create_node<ccl::BevelNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		int samples = get_int_parameter_value(xsi_parameters, "Samples", eval_time);
		node->set_samples(samples);

		return node;
	}
	else if (shader_type == "UVMap")
	{
		ccl::UVMapNode* node = shader_graph->create_node<ccl::UVMapNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString attribute = get_string_parameter_value(xsi_parameters, "Attribute", eval_time);
		node->set_attribute(ccl::ustring(attribute.GetAsciiString()));

		return node;
	}
	else if (shader_type == "Camera")
	{
		ccl::CameraNode* node = shader_graph->create_node<ccl::CameraNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "Fresnel")
	{
		ccl::FresnelNode* node = shader_graph->create_node<ccl::FresnelNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "LayerWeight")
	{
		ccl::LayerWeightNode* node = shader_graph->create_node<ccl::LayerWeightNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "Wireframe")
	{
		ccl::WireframeNode* node = shader_graph->create_node<ccl::WireframeNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		bool use_pixel = get_bool_parameter_value(xsi_parameters, "UsePixelSize", eval_time);
		node->set_use_pixel_size(use_pixel);

		return node;
	}
	else if (shader_type == "Tangent")
	{
		ccl::TangentNode* node = shader_graph->create_node<ccl::TangentNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString direction_type = get_string_parameter_value(xsi_parameters, "DirectionType", eval_time);
		XSI::CString axis = get_string_parameter_value(xsi_parameters, "Axis", eval_time);
		XSI::CString attribute = get_string_parameter_value(xsi_parameters, "Attribute", eval_time);

		node->set_direction_type(get_tangent_direction_type(direction_type));
		node->set_axis(get_tangent_axis(axis));
		node->set_attribute(OIIO::ustring(attribute.GetAsciiString()));

		return node;
	}
	else if (shader_type == "LightFalloff")
	{
		ccl::LightFalloffNode* node = shader_graph->create_node<ccl::LightFalloffNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "Invert")
	{
		ccl::InvertNode* node = shader_graph->create_node<ccl::InvertNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "MixRGB")
	{
		ccl::MixNode* node = shader_graph->create_node<ccl::MixNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		bool use_clamp = get_bool_parameter_value(xsi_parameters, "UseClamp", eval_time);

		node->set_mix_type(get_mix_type(type));
		node->set_use_clamp(use_clamp);

		return node;
	}
	else if (shader_type == "MixColor")
	{
		ccl::MixColorNode* node = shader_graph->create_node<ccl::MixColorNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		bool use_clamp = get_bool_parameter_value(xsi_parameters, "UseClamp", eval_time);
		bool use_clamp_result = get_bool_parameter_value(xsi_parameters, "UseClampResult", eval_time);

		node->set_blend_type(get_mix_type(type));
		node->set_use_clamp(use_clamp);
		node->set_use_clamp_result(use_clamp_result);

		return node;
	}
	else if (shader_type == "MixFloat")
	{
		ccl::MixFloatNode* node = shader_graph->create_node<ccl::MixFloatNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		bool use_clamp = get_bool_parameter_value(xsi_parameters, "UseClamp", eval_time);
		node->set_use_clamp(use_clamp);
		return node;
	}
	else if (shader_type == "MixVector")
	{
		ccl::MixVectorNode* node = shader_graph->create_node<ccl::MixVectorNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		bool use_clamp = get_bool_parameter_value(xsi_parameters, "UseClamp", eval_time);
		node->set_use_clamp(use_clamp);
		return node;
	}
	else if (shader_type == "MixVectorNonUniform")
	{
		ccl::MixVectorNonUniformNode* node = shader_graph->create_node<ccl::MixVectorNonUniformNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		bool use_clamp = get_bool_parameter_value(xsi_parameters, "UseClamp", eval_time);
		node->set_use_clamp(use_clamp);
		return node;
	}
	else if (shader_type == "Gamma")
	{
		ccl::GammaNode* node = shader_graph->create_node<ccl::GammaNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "BrightContrast")
	{
		ccl::BrightContrastNode* node = shader_graph->create_node<ccl::BrightContrastNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "HSV")
	{
		ccl::HSVNode* node = shader_graph->create_node<ccl::HSVNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "RGBCurves")
	{
		ccl::RGBCurvesNode* node = shader_graph->create_node<ccl::RGBCurvesNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::FCurve r_curve = get_fcurve_parameter_value(xsi_parameters, "rCurve", eval_time);
		XSI::FCurve g_curve = get_fcurve_parameter_value(xsi_parameters, "gCurve", eval_time);
		XSI::FCurve b_curve = get_fcurve_parameter_value(xsi_parameters, "bCurve", eval_time);

		std::array<float, 2> min_max = get_min_and_max(r_curve, g_curve, b_curve);
		node->set_min_x(min_max[0]);
		node->set_max_x(min_max[1]);
		ccl::array<ccl::float3> curves = three_curves_to_array(r_curve, g_curve, b_curve, min_max[0], min_max[1], RAMP_TABLE_SIZE);
		node->set_curves(curves);

		return node;
	}
	else if (shader_type == "ColorCurves")
	{
		ccl::RGBCurvesNode* node = shader_graph->create_node<ccl::RGBCurvesNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::FCurve curve = get_fcurve_parameter_value(xsi_parameters, "Curve", eval_time);

		std::array<float, 2> min_max = get_min_and_max(curve, curve, curve);
		node->set_min_x(min_max[0]);
		node->set_max_x(min_max[1]);
		ccl::array<ccl::float3> curves = three_curves_to_array(curve, curve, curve, min_max[0], min_max[1], RAMP_TABLE_SIZE);
		node->set_curves(curves);

		return node;	
	}
	else if (shader_type == "Wavelength")
	{
		ccl::WavelengthNode* node = shader_graph->create_node<ccl::WavelengthNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "Blackbody")
	{
		ccl::BlackbodyNode* node = shader_graph->create_node<ccl::BlackbodyNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "MixClosure")
	{
		ccl::MixClosureNode* node = shader_graph->create_node<ccl::MixClosureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "AddClosure")
	{
		ccl::AddClosureNode* node = shader_graph->create_node<ccl::AddClosureNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "CombineColor")
	{
		ccl::CombineColorNode* node = shader_graph->create_node<ccl::CombineColorNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString mode = get_string_parameter_value(xsi_parameters, "mode", eval_time);
		node->set_color_type(mode == "rgb" ? ccl::NODE_COMBSEP_COLOR_RGB : (mode == "hsv" ? ccl::NODE_COMBSEP_COLOR_HSV : ccl::NODE_COMBSEP_COLOR_HSL));

		return node;
	}
	else if (shader_type == "CombineRGB")
	{
		ccl::CombineRGBNode* node = shader_graph->create_node<ccl::CombineRGBNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "CombineHSV")
	{
		ccl::CombineHSVNode* node = shader_graph->create_node<ccl::CombineHSVNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "CombineXYZ")
	{
		ccl::CombineXYZNode* node = shader_graph->create_node<ccl::CombineXYZNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "SeparateColor")
	{
		ccl::SeparateColorNode* node = shader_graph->create_node<ccl::SeparateColorNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString mode = get_string_parameter_value(xsi_parameters, "mode", eval_time);
		node->set_color_type(mode == "rgb" ? ccl::NODE_COMBSEP_COLOR_RGB : (mode == "hsv" ? ccl::NODE_COMBSEP_COLOR_HSV : ccl::NODE_COMBSEP_COLOR_HSL));

		return node;
	}
	else if (shader_type == "SeparateRGB")
	{
		ccl::SeparateRGBNode* node = shader_graph->create_node<ccl::SeparateRGBNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "SeparateHSV")
	{
		ccl::SeparateHSVNode* node = shader_graph->create_node<ccl::SeparateHSVNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}
	else if (shader_type == "SeparateXYZ")
	{
		ccl::SeparateXYZNode* node = shader_graph->create_node<ccl::SeparateXYZNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		float vector_x = get_float_parameter_value(xsi_parameters, "VectorX", eval_time);
		float vector_y = get_float_parameter_value(xsi_parameters, "VectorY", eval_time);
		float vector_z = get_float_parameter_value(xsi_parameters, "VectorZ", eval_time);

		node->set_vector(ccl::make_float3(vector_x, vector_y, vector_z));

		return node;
	}
	else if (shader_type == "Math")
	{
		ccl::MathNode* node = shader_graph->create_node<ccl::MathNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		bool clamp = get_bool_parameter_value(xsi_parameters, "UseClamp", eval_time);

		node->set_math_type(get_math_type(type));
		node->set_use_clamp(clamp);

		return node;
	}
	else if (shader_type == "VectorMath")
	{
		ccl::VectorMathNode* node = shader_graph->create_node<ccl::VectorMathNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
	
		float vector1_x = get_float_parameter_value(xsi_parameters, "Vector1X", eval_time);
		float vector1_y = get_float_parameter_value(xsi_parameters, "Vector1Y", eval_time);
		float vector1_z = get_float_parameter_value(xsi_parameters, "Vector1Z", eval_time);

		float vector2_x = get_float_parameter_value(xsi_parameters, "Vector2X", eval_time);
		float vector2_y = get_float_parameter_value(xsi_parameters, "Vector2Y", eval_time);
		float vector2_z = get_float_parameter_value(xsi_parameters, "Vector2Z", eval_time);

		float vector3_x = get_float_parameter_value(xsi_parameters, "Vector3X", eval_time);
		float vector3_y = get_float_parameter_value(xsi_parameters, "Vector3Y", eval_time);
		float vector3_z = get_float_parameter_value(xsi_parameters, "Vector3Z", eval_time);

		node->set_math_type(get_vector_math(type));
		node->set_vector1(ccl::make_float3(vector1_x, vector1_y, vector1_z));
		node->set_vector2(ccl::make_float3(vector2_x, vector2_y, vector2_z));
		node->set_vector3(ccl::make_float3(vector3_x, vector3_y, vector3_z));

		return node;
	}
	else if (shader_type == "ColorRamp")
	{
		ccl::RGBRampNode* node = shader_graph->create_node<ccl::RGBRampNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		node->set_interpolate(true);
		XSI::ShaderParameter gradient_parameter = xsi_parameters.GetItem("Gradient");
		XSI::ShaderParameter gradient_parameter_final = get_source_parameter(gradient_parameter);

		XSI::CParameterRefArray g_params = gradient_parameter_final.GetParameters();
		XSI::Parameter markers_parameter = g_params.GetItem("markers");
		XSI::CParameterRefArray m_params = markers_parameter.GetParameters();
		std::vector<GradientPoint> gradient_data;
		for (int i = 0; i < m_params.GetCount(); i++)
		{
			XSI::ShaderParameter p = m_params[i];
			float pos = p.GetParameterValue("pos");
			float mid = p.GetParameterValue("mid");
			float r = p.GetParameterValue("red");
			float g = p.GetParameterValue("green");
			float b = p.GetParameterValue("blue");
			float a = p.GetParameterValue("alpha");
			if (pos > -1)
			{
				GradientPoint new_data;
				new_data.pos = pos;
				new_data.mid = mid;
				new_data.color = XSI::MATH::CColor4f(r, g, b, a);
				gradient_data.push_back(new_data);
			}
		}
		ccl::array<ccl::float3> ramp;
		ccl::array<float> ramp_alpha;
		form_ramp(gradient_data, RAMP_TABLE_SIZE, ramp, ramp_alpha);
		node->set_ramp(ramp);
		node->set_ramp_alpha(ramp_alpha);

		return node;
	}
	else if (shader_type == "MapRange")
	{
		ccl::MapRangeNode* node = shader_graph->create_node<ccl::MapRangeNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		bool clamp = get_bool_parameter_value(xsi_parameters, "Clamp", eval_time);
		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);

		node->set_clamp(clamp);
		node->set_range_type(get_map_range_type(type));

		return node;
	}
	else if (shader_type == "VectorMapRange")
	{
		ccl::VectorMapRangeNode* node = shader_graph->create_node<ccl::VectorMapRangeNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		bool clamp = get_bool_parameter_value(xsi_parameters, "Clamp", eval_time);
		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);

		float from_min_x = get_float_parameter_value(xsi_parameters, "FromMinX", eval_time);
		float from_min_y = get_float_parameter_value(xsi_parameters, "FromMinY", eval_time);
		float from_min_z = get_float_parameter_value(xsi_parameters, "FromMinZ", eval_time);

		float from_max_x = get_float_parameter_value(xsi_parameters, "FromMaxX", eval_time);
		float from_max_y = get_float_parameter_value(xsi_parameters, "FromMaxY", eval_time);
		float from_max_z = get_float_parameter_value(xsi_parameters, "FromMaxZ", eval_time);

		float to_min_x = get_float_parameter_value(xsi_parameters, "ToMinX", eval_time);
		float to_min_y = get_float_parameter_value(xsi_parameters, "ToMinY", eval_time);
		float to_min_z = get_float_parameter_value(xsi_parameters, "ToMinZ", eval_time);

		float to_max_x = get_float_parameter_value(xsi_parameters, "ToMaxX", eval_time);
		float to_max_y = get_float_parameter_value(xsi_parameters, "ToMaxY", eval_time);
		float to_max_z = get_float_parameter_value(xsi_parameters, "ToMaxZ", eval_time);

		float step_x = get_float_parameter_value(xsi_parameters, "StepsX", eval_time);
		float step_y = get_float_parameter_value(xsi_parameters, "StepsY", eval_time);
		float step_z = get_float_parameter_value(xsi_parameters, "StepsZ", eval_time);

		node->set_use_clamp(clamp);
		node->set_range_type(get_map_range_type(type));

		node->set_from_min(ccl::make_float3(from_min_x, from_min_y, from_min_z));
		node->set_from_max(ccl::make_float3(from_max_x, from_max_y, from_max_z));
		node->set_to_min(ccl::make_float3(to_min_x, to_min_y, to_min_z));
		node->set_to_max(ccl::make_float3(to_max_x, to_max_y, to_max_z));
		node->set_steps(ccl::make_float3(step_x, step_y, step_z));

		return node;
	}
	else if (shader_type == "FloatCurve")
	{
		ccl::FloatCurveNode* node = shader_graph->create_node<ccl::FloatCurveNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::FCurve curve = get_fcurve_parameter_value(xsi_parameters, "Curve", eval_time);
		float min = curve.GetKeyAtIndex(0).GetTime();
		float max = curve.GetKeyAtIndex(curve.GetNumKeys() - 1).GetTime();
		node->set_min_x(min);
		node->set_max_x(max);

		ccl::array<float> curve_mapping_curve;
		int steps = 256;// RAMP_TABLE_SIZE;
		curve_mapping_curve.resize(steps);
		const float range = max - min;
		for (int i = 0; i < steps; i++)
		{
			float t = min + (float)i * range / (float)(steps - 1);
			curve_mapping_curve[i] = curve.Eval(t);
		}
		node->set_curve(curve_mapping_curve);

		return node;
	}
	else if (shader_type == "Clamp")
	{
		ccl::ClampNode* node = shader_graph->create_node<ccl::ClampNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		XSI::CString type = get_string_parameter_value(xsi_parameters, "Type", eval_time);
		node->set_clamp_type(get_clamp_type(type));

		return node;
	}
	else if (shader_type == "RGBToBW")
	{
		ccl::RGBToBWNode* node = shader_graph->create_node<ccl::RGBToBWNode>();
		common_routine(scene, node, shader_graph, nodes_map, xsi_shader, xsi_parameters, eval_time, aovs);

		return node;
	}

	return NULL;
}