#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include <xsi_material.h>
#include <xsi_time.h>
#include <xsi_shaderdef.h>
#include <xsi_texture.h>
#include <xsi_project.h>
#include <xsi_scene.h>
#include <xsi_materiallibrary.h>

#include "../../update_context.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/strings.h"
#include "../../../utilities/xsi_shaders.h"
#include "../../../utilities/math.h"
#include "cyc_materials.h"
#include "names_converter.h"

void log_ports(ccl::ShaderNode* node, bool is_inputs)
{
	if (is_inputs)
	{
		for (size_t i = 0; i < node->inputs.size(); i++)
		{
			log_message("input " + XSI::CString(i) + ": " + XSI::CString(node->inputs[i]->name().c_str()));
		}
	}
	else
	{
		for (size_t i = 0; i < node->outputs.size(); i++)
		{
			log_message("output " + XSI::CString(i) + ": " + XSI::CString(node->outputs[i]->name().c_str()));
		}
	}
}

void log_one_shader(const XSI::CString &name, ccl::ShaderNode* node)
{
	log_message("-------" + name + "-------");
	log_ports(node, true);
	log_ports(node, false);
}

void log_all_shaders()
{
	log_one_shader("CyclesCombineColor", new ccl::CombineColorNode());
	//log_one_shader("CyclesDiffuseBSDF", new ccl::DiffuseBsdfNode());
	//log_one_shader("CyclesPrincipledBSDF", new ccl::PrincipledBsdfNode());
	//log_one_shader("CyclesAnisotropicBSDF", new ccl::AnisotropicBsdfNode());
	/*log_one_shader("CyclesTranslucentBSDF", new ccl::TranslucentBsdfNode());
	log_one_shader("CyclesTransparentBSDF", new ccl::TransparentBsdfNode());
	log_one_shader("CyclesVelvetBSDF", new ccl::VelvetBsdfNode());
	log_one_shader("CyclesToonBSDF", new ccl::ToonBsdfNode());
	log_one_shader("CyclesGlossyBSDF", new ccl::GlossyBsdfNode());
	log_one_shader("CyclesGlassBSDF", new ccl::GlassBsdfNode());
	log_one_shader("CyclesRefractionBSDF", new ccl::RefractionBsdfNode());
	log_one_shader("CyclesHairBSDF", new ccl::HairBsdfNode());
	log_one_shader("CyclesEmission", new ccl::EmissionNode());
	log_one_shader("CyclesAmbientOcclusion", new ccl::AmbientOcclusionNode());
	log_one_shader("CyclesBackground", new ccl::BackgroundNode());
	log_one_shader("CyclesHoldout", new ccl::HoldoutNode());
	log_one_shader("CyclesAbsorptionVolume", new ccl::AbsorptionVolumeNode());
	log_one_shader("CyclesScatterVolume", new ccl::ScatterVolumeNode());
	log_one_shader("CyclesPrincipledVolume", new ccl::PrincipledVolumeNode());
	log_one_shader("CyclesPrincipledHairBSDF", new ccl::PrincipledHairBsdfNode());
	log_one_shader("CyclesSubsurfaceScattering", new ccl::SubsurfaceScatteringNode());
	log_one_shader("CyclesImageTexture", new ccl::ImageTextureNode());
	log_one_shader("CyclesEnvironmentTexture", new ccl::EnvironmentTextureNode());
	log_one_shader("CyclesSkyTexture", new ccl::SkyTextureNode());
	log_one_shader("CyclesNoiseTexture", new ccl::NoiseTextureNode());
	log_one_shader("CyclesCheckerTexture", new ccl::CheckerTextureNode());
	log_one_shader("CyclesBrickTexture", new ccl::BrickTextureNode());
	log_one_shader("CyclesGradientTexture", new ccl::GradientTextureNode());
	log_one_shader("CyclesVoronoiTexture", new ccl::VoronoiTextureNode());
	log_one_shader("CyclesMusgraveTexture", new ccl::MusgraveTextureNode());
	log_one_shader("CyclesMagicTexture", new ccl::MagicTextureNode());
	log_one_shader("CyclesWaveTexture", new ccl::WaveTextureNode());
	log_one_shader("CyclesIESTexture", new ccl::IESLightNode());
	log_one_shader("CyclesWhiteNoiseTexture", new ccl::WhiteNoiseTextureNode());
	log_one_shader("CyclesNormal", new ccl::NormalNode());
	log_one_shader("CyclesBump", new ccl::BumpNode());
	log_one_shader("CyclesMapping", new ccl::MappingNode());
	log_one_shader("CyclesNormalMap", new ccl::NormalMapNode());
	log_one_shader("CyclesVectorTransform", new ccl::VectorTransformNode());
	log_one_shader("CyclesVectorRotate", new ccl::VectorRotateNode());
	log_one_shader("CyclesVectorCurves", new ccl::VectorCurvesNode());
	log_one_shader("CyclesDisplacement", new ccl::DisplacementNode());
	log_one_shader("CyclesVectorDisplacement", new ccl::VectorDisplacementNode());
	log_one_shader("CyclesGeometry", new ccl::GeometryNode());
	log_one_shader("CyclesTextureCoordinate", new ccl::TextureCoordinateNode());
	log_one_shader("CyclesLightPath", new ccl::LightPathNode());
	log_one_shader("CyclesObjectInfo", new ccl::ObjectInfoNode());
	log_one_shader("CyclesVolumeInfo", new ccl::VolumeInfoNode());
	log_one_shader("CyclesPointInfo", new ccl::PointInfoNode());
	log_one_shader("CyclesParticleInfo", new ccl::ParticleInfoNode());
	log_one_shader("CyclesHairInfo", new ccl::HairInfoNode());
	log_one_shader("CyclesValue", new ccl::ValueNode());
	log_one_shader("CyclesColor", new ccl::ColorNode());
	log_one_shader("CyclesAttribute", new ccl::AttributeNode());
	log_one_shader("CyclesOutputColorAOV", new ccl::OutputAOVNode());
	log_one_shader("CyclesVertexColor", new ccl::VertexColorNode());
	log_one_shader("CyclesBevel", new ccl::BevelNode());
	log_one_shader("CyclesUVMap", new ccl::UVMapNode());
	log_one_shader("CyclesCamera", new ccl::CameraNode());
	log_one_shader("CyclesFresnel", new ccl::FresnelNode());
	log_one_shader("CyclesLayerWeight", new ccl::LayerWeightNode());
	log_one_shader("CyclesWireframe", new ccl::WireframeNode());
	log_one_shader("CyclesTangent", new ccl::TangentNode());
	log_one_shader("CyclesLightFalloff", new ccl::LightFalloffNode());
	log_one_shader("CyclesInvert", new ccl::InvertNode());
	log_one_shader("CyclesMixRGB", new ccl::MixColorNode());
	log_one_shader("CyclesGamma", new ccl::GammaNode());
	log_one_shader("CyclesBrightContrast", new ccl::BrightContrastNode());
	log_one_shader("CyclesHSV", new ccl::HSVNode());
	log_one_shader("CyclesRGBCurves", new ccl::RGBCurvesNode());
	log_one_shader("CyclesWavelength", new ccl::WavelengthNode());
	log_one_shader("CyclesBlackbody", new ccl::BlackbodyNode());
	log_one_shader("CyclesMixClosure", new ccl::MixClosureNode());
	log_one_shader("CyclesAddClosure", new ccl::AddClosureNode());
	log_one_shader("CyclesCombineRGB", new ccl::CombineRGBNode());
	log_one_shader("CyclesCombineHSV", new ccl::CombineHSVNode());
	log_one_shader("CyclesCombineXYZ", new ccl::CombineXYZNode());
	log_one_shader("CyclesSeparateRGB", new ccl::SeparateRGBNode());
	log_one_shader("CyclesSeparateHSV", new ccl::SeparateHSVNode());
	log_one_shader("CyclesSeparateXYZ", new ccl::SeparateXYZNode());
	log_one_shader("CyclesMath", new ccl::MathNode());
	log_one_shader("CyclesVectorMath", new ccl::VectorMathNode());
	log_one_shader("CyclesColorRamp", new ccl::RGBRampNode());
	log_one_shader("CyclesMapRange", new ccl::MapRangeNode());
	log_one_shader("CyclesVectorMapRange", new ccl::VectorMapRangeNode());
	log_one_shader("CyclesFloatCurve", new ccl::FloatCurveNode());
	log_one_shader("CyclesClamp", new ccl::ClampNode());
	log_one_shader("CyclesRGBToBW", new ccl::RGBToBWNode());*/
	// 87 nodes
}

int create_default_shader(ccl::Scene* scene)
{
	ccl::Shader* shader = new ccl::Shader();
	shader->name = "default_shader";
	ccl::ShaderGraph* shader_graph = new ccl::ShaderGraph();
	ccl::GlossyBsdfNode* glossy_node = shader_graph->create_node<ccl::GlossyBsdfNode>();
	shader_graph->add(glossy_node);
	glossy_node->set_roughness(0.25f);
	glossy_node->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f));

	ccl::ShaderNode* out = shader_graph->output();
	shader_graph->connect(glossy_node->output("BSDF"), out->input("Surface"));
	shader->set_graph(shader_graph);
	shader->tag_update(scene);

	scene->shaders.push_back(shader);
	return scene->shaders.size() - 1;
}

int create_emission_checker(ccl::Scene* scene, float checker_scale)
{
	ccl::Shader* shader = new ccl::Shader();
	shader->name = "emission_checker";
	ccl::ShaderGraph* shader_graph = new ccl::ShaderGraph();
	ccl::EmissionNode* emission_node = shader_graph->create_node<ccl::EmissionNode>();
	shader_graph->add(emission_node);
	emission_node->set_strength(1.0);

	ccl::CheckerTextureNode* checker_node = shader_graph->create_node<ccl::CheckerTextureNode>();
	shader_graph->add(checker_node);
	checker_node->set_scale(checker_scale);
	checker_node->set_color1(ccl::make_float3(0.2, 0.2, 0.2));
	checker_node->set_color2(ccl::make_float3(0.8, 0.8, 0.8));

	ccl::TextureCoordinateNode* uv_node = shader_graph->create_node<ccl::TextureCoordinateNode>();
	shader_graph->add(uv_node);

	ccl::ShaderNode* out = shader_graph->output();
	shader_graph->connect(emission_node->output("Emission"), out->input("Surface"));
	shader_graph->connect(checker_node->output("Color"), emission_node->input("Color"));
	shader_graph->connect(uv_node->output("UV"), checker_node->input("Vector"));
	shader->set_graph(shader_graph);
	shader->tag_update(scene);

	scene->shaders.push_back(shader);
	return scene->shaders.size() - 1;
}

// we call this method for every node from render tree to convert it to the Cycles node in the sahder graph
// here we does not add the node to the graph
ccl::ShaderNode* xsi_node_to_cycles(
	ccl::Scene* scene,  // we need scene for osl shader manager
	ccl::ShaderGraph* shader_graph,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs,
	const XSI::Shader& xsi_shader,
	const XSI::CTime& eval_time)
{
	ULONG xsi_id = xsi_shader.GetObjectID();
	if (nodes_map.contains(xsi_id))
	{
		return nodes_map[xsi_id];
	}
	else
	{
		XSI::CString out_type;
		ShadernodeType shadernode_type = get_shadernode_type(xsi_shader, out_type);

		if (shadernode_type == ShadernodeType_Cycles || shadernode_type == ShadernodeType_CyclesAOV)
		{
			XSI::CParameterRefArray params = xsi_shader.GetParameters();
			return sync_cycles_shader(scene, xsi_shader, out_type, params, eval_time, shader_graph, nodes_map, aovs);
		}
		else if (shadernode_type == ShadernodeType_OSL)
		{
			return sync_osl_shader(scene, shader_graph, xsi_shader, nodes_map, aovs, eval_time);
		}
		else
		{
			return NULL;
		}
	}
}

// make connection between Cycles node (with port name) and corresponding node (after conversation) of the xsi_parameter
bool sync_shader_parameter_connection(
	ccl::Scene* scene,
	ccl::ShaderGraph* shader_graph,
	ccl::ShaderNode* cycles_node,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs,
	const XSI::ShaderParameter& xsi_parameter,
	const std::string& cycles_name,
	const XSI::CTime& eval_time)
{
	// check is this final parameter is connected to something
	XSI::CRef source = xsi_parameter.GetSource();
	if (source.IsValid() && source.GetClassID() == XSI::siShaderParameterID)
	{// there is connection with shader parameter 
		XSI::ShaderParameter xsi_source_parameter(source);
		XSI::Shader xsi_source_node(xsi_source_parameter.GetParent());

		ccl::ShaderNode* source_node = xsi_node_to_cycles(scene, shader_graph, nodes_map, aovs, xsi_source_node, eval_time);
		if (source_node != NULL)
		{
			bool is_connect = make_nodes_connection(shader_graph, source_node, cycles_node, xsi_source_node, xsi_source_parameter.GetName(), cycles_name, eval_time);
			return is_connect;
		}
	}

	return false;
}

void sync_float_parameter(ccl::Scene* scene,
	ccl::ShaderGraph* shader_graph,
	ccl::ShaderNode* cycles_node,
	XSI::ShaderParameter& xsi_parameter,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs,
	const std::string& cycles_name,
	const XSI::CTime& eval_time)
{
	XSI::ShaderParameter xsi_finall_parameter = get_source_parameter(xsi_parameter);

	bool is_connect = sync_shader_parameter_connection(scene, shader_graph, cycles_node, nodes_map, aovs, xsi_finall_parameter, cycles_name, eval_time);

	ShaderParameterType parameter_type = get_shader_parameter_type(xsi_finall_parameter);
	XSI::Shader xsi_finall_parameter_shader = xsi_finall_parameter.GetParent();

	ccl::ShaderInput* input = cycles_node->input(cycles_name.c_str());

	if (parameter_type == ShaderParameterType::ParameterType_Float)
	{
		float float_value = get_float_parameter_value(xsi_finall_parameter_shader.GetParameters(), xsi_finall_parameter.GetName(), eval_time);
		input->set(float_value);
	}
}

void sync_float3_parameter(ccl::Scene* scene,
	ccl::ShaderGraph* shader_graph,
	ccl::ShaderNode* cycles_node,
	XSI::ShaderParameter& xsi_parameter,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs,
	const std::string& cycles_name,
	const XSI::CTime& eval_time)
{
	// return final float3 value of the parameter
	XSI::ShaderParameter xsi_finall_parameter = get_source_parameter(xsi_parameter);
	
	bool is_connect = sync_shader_parameter_connection(scene, shader_graph, cycles_node, nodes_map, aovs, xsi_finall_parameter, cycles_name, eval_time);

	ShaderParameterType parameter_type = get_shader_parameter_type(xsi_finall_parameter);
	XSI::Shader xsi_finall_parameter_shader = xsi_finall_parameter.GetParent();

	ccl::ShaderInput* input = cycles_node->input(cycles_name.c_str());

	if (parameter_type == ShaderParameterType::ParameterType_Color3 || parameter_type == ShaderParameterType::ParameterType_Vector3)
	{
		XSI::MATH::CVector3 vector_values = get_vector_parameter_value(xsi_finall_parameter_shader.GetParameters(), xsi_finall_parameter.GetName(), eval_time);
		input->set(vector3_to_float3(vector_values));
	}
	else if (parameter_type == ShaderParameterType::ParameterType_Color4 && input->type() != ccl::SocketType::CLOSURE)
	{
		// does not set value for closure Cycles input socket
		XSI::MATH::CColor4f color_values = get_color_parameter_value(xsi_finall_parameter_shader.GetParameters(), xsi_finall_parameter.GetName(), eval_time);
		input->set(color4_to_float3(color_values));
	}
}

// we call this method for different types of connection of the material: to surface port, bump (for displacement) port or volume port
// xsi_node_port_name is changeble parameter, it should be equal to the name of the port in the first node, connected to the port_name in the material
// xsi_node_prog_id should contains the prog_id of the first node
ccl::ShaderNode* sync_material_port(ccl::Scene* scene, 
	ccl::ShaderGraph* shader_graph,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	std::vector<XSI::CStringArray>& aovs,
	XSI::CString &xsi_node_port_name,
	XSI::Shader &xsi_shader,
	const XSI::ShaderParameter& material_port,
	const XSI::CTime& eval_time)
{
	XSI::ShaderParameter material_port_source = get_source_parameter(material_port, true);  // return output parameter
	if (material_port_source.IsValid())
	{
		XSI::Shader xsi_node(material_port_source.GetParent());
		if (xsi_node.IsValid())
		{
			ccl::ShaderNode* to_return = xsi_node_to_cycles(scene, shader_graph, nodes_map, aovs, xsi_node, eval_time);
			if (to_return != NULL)
			{
				xsi_node_port_name = material_port_source.GetName();
				xsi_shader = xsi_node;
			}

			return to_return;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}

ccl::ShaderGraph* material_to_graph(ccl::Scene* scene, const XSI::Material& xsi_material, const XSI::CTime& eval_time, std::vector<XSI::CStringArray>& aovs)
{
	std::unordered_map<ULONG, ccl::ShaderNode*> nodes_map;  // key - render tree shader node id, value - Cycles shader node
	ccl::ShaderGraph* shader_graph = new ccl::ShaderGraph();

	XSI::CString xsi_node_surface_port_name = "";
	XSI::Shader xsi_shader_surface;
	XSI::CParameterRefArray xsi_material_parameters = xsi_material.GetParameters();
	ccl::ShaderNode* surface_node = sync_material_port(scene, shader_graph, nodes_map, aovs, xsi_node_surface_port_name, xsi_shader_surface, xsi_material_parameters.GetItem("surface"), eval_time);
	
	XSI::CString xsi_node_volume_port_name = "";
	XSI::Shader xsi_shader_volume;
	ccl::ShaderNode* volume_node = sync_material_port(scene, shader_graph, nodes_map, aovs, xsi_node_volume_port_name, xsi_shader_volume, xsi_material_parameters.GetItem("volume"), eval_time);

	XSI::CString xsi_node_displace_port_name = "";
	XSI::Shader xsi_shader_displacement;
	ccl::ShaderNode* displacement_node = sync_material_port(scene, shader_graph, nodes_map, aovs, xsi_node_displace_port_name, xsi_shader_displacement, xsi_material_parameters.GetItem("normal"), eval_time);

	bool is_empty_surface = true;
	if (surface_node != NULL && xsi_node_surface_port_name.Length() > 0 && xsi_shader_surface.IsValid())
	{
		bool is_connect = make_nodes_connection(shader_graph, surface_node, shader_graph->output(), xsi_shader_surface, xsi_node_surface_port_name, "Surface", eval_time);
		if (is_connect)
		{
			is_empty_surface = false;
		}
	}

	bool is_empty_volume = true;
	if (volume_node != NULL && xsi_node_volume_port_name.Length() > 0 && xsi_shader_volume.IsValid())
	{
		bool is_connect = make_nodes_connection(shader_graph, volume_node, shader_graph->output(), xsi_shader_volume, xsi_node_volume_port_name, "Volume", eval_time);
		if (is_connect)
		{
			is_empty_volume = false;
		}
	}

	bool is_empty_displacement = true;
	if (displacement_node != NULL && xsi_node_displace_port_name.Length() > 0 && xsi_shader_displacement.IsValid())
	{
		bool is_connect = make_nodes_connection(shader_graph, displacement_node, shader_graph->output(), xsi_shader_displacement, xsi_node_displace_port_name, "Displacement", eval_time);
		if (is_connect)
		{
			is_empty_displacement = false;
		}
	}

	if (is_empty_surface && is_empty_volume)
	{
		// surface is empty (or invalid), create transparent node
		ccl::TransparentBsdfNode* transparent = new ccl::TransparentBsdfNode();
		transparent->set_color(ccl::make_float3(0.0, 0.0, 0.0));
		shader_graph->add(transparent);

		// connect to surface output
		ccl::ShaderNode* out = shader_graph->output();
		shader_graph->connect(transparent->output("BSDF"), out->input("Surface"));
	}

	return shader_graph;
}

// in this function we should crate the shader, add it to the shaders array and retun it index in this array
int sync_material(ccl::Scene* scene, const XSI::Material &xsi_material, const XSI::CTime &eval_time, std::vector<XSI::CStringArray> &aovs)
{
	ccl::ShaderGraph* shader_graph = material_to_graph(scene, xsi_material, eval_time, aovs);

	// create output shader
	ccl::Shader* shader = new ccl::Shader();

	shader->set_graph(shader_graph);
	shader->tag_update(scene);

	// add to the array
	scene->shaders.push_back(shader);

	return scene->shaders.size() - 1;
}

ccl::ShaderGraph* shaderball_shadernode_to_graph(ccl::Scene* scene, const XSI::Shader& xsi_shader, bool is_surface, const XSI::CTime& eval_time, bool &out_success)
{
	// convert shader node to Cycles one
	std::unordered_map<ULONG, ccl::ShaderNode*> nodes_map;
	ccl::ShaderGraph* shader_graph = new ccl::ShaderGraph();

	std::vector<XSI::CStringArray> aovs(2);
	aovs[0].Clear();
	aovs[1].Clear();
	ccl::ShaderNode* shader_node = xsi_node_to_cycles(scene, shader_graph, nodes_map, aovs, xsi_shader, eval_time);
	// we does not need aovs for shaderballs, because it's impossible to render these passes

	if (shader_node != NULL)
	{
		// connect it to the output port of the sahder graph (surface or volume)
		// use the first output port of the node
		ccl::vector<ccl::ShaderOutput*> shader_outputs = shader_node->outputs;
		size_t outputs_count = shader_outputs.size();
		if (outputs_count > 0)
		{
			ccl::ShaderOutput* shader_out = shader_node->outputs[0];

			ccl::ShaderNode* out = shader_graph->output();
			if (is_surface)
			{
				shader_graph->connect(shader_out, out->input("Surface"));
			}
			else
			{
				shader_graph->connect(shader_out, out->input("Volume"));
			}

			out_success = true;
		}
	}

	return shader_graph;
}

// call this function when we should previre shaderball rendertree node instead of whole material
int sync_shaderball_shadernode(ccl::Scene* scene, const XSI::Shader& xsi_shader, bool is_surface, const XSI::CTime& eval_time)
{
	// get parent material
	XSI::CRef parent = xsi_shader.GetRoot();
	ULONG parent_id = 0;
	XSI::siClassID parent_class = parent.GetClassID();
	if (parent_class == XSI::siMaterialID)
	{
		XSI::Material parent_material(parent);
		parent_id = parent_material.GetObjectID();
	}

	bool is_success = false;
	ccl::ShaderGraph* shader_graph = shaderball_shadernode_to_graph(scene, xsi_shader, is_surface, eval_time, is_success);

	if (is_success)
	{
		ccl::Shader* shader = new ccl::Shader();

		shader->set_graph(shader_graph);
		shader->tag_update(scene);
		scene->shaders.push_back(shader);

		return scene->shaders.size() - 1;
	}
	else
	{
		return -1;
	}
}

int sync_shaderball_texturenode(ccl::Scene* scene, const XSI::Texture& xsi_texture, const XSI::CTime& eval_time)
{
	XSI::Shader xsi_texture_shader(xsi_texture);

	return sync_shaderball_shadernode(scene, xsi_texture_shader, true, eval_time);
}

void sync_scene_materials(ccl::Scene* scene, UpdateContext* update_context)
{
	XSI::Scene xsi_scene = XSI::Application().GetActiveProject().GetActiveScene();
	XSI::CRefArray material_libs = xsi_scene.GetMaterialLibraries();
	XSI::CTime eval_time = update_context->get_time();

	std::vector<XSI::CStringArray> aovs(2);
	aovs[0].Clear();  // for colors
	aovs[1].Clear();  // for values

	for (LONG lib_index = 0; lib_index < material_libs.GetCount(); lib_index++)
	{
		XSI::MaterialLibrary lib = material_libs.GetItem(lib_index);
		XSI::CRefArray materials = lib.GetItems();
		for (LONG mat_index = 0; mat_index < materials.GetCount(); mat_index++)
		{
			XSI::Material xsi_material = materials.GetItem(mat_index);
			ULONG xsi_id = xsi_material.GetObjectID();
			XSI::CRefArray used_objects = xsi_material.GetUsedBy();
			if (used_objects.GetCount() > 0)
			{
				int shader_index = sync_material(scene, xsi_material, eval_time, aovs);
				if (shader_index >= 0)
				{
					update_context->add_material_index(xsi_id, shader_index, ShaderballType_Unknown);
				}
			}
		}
	}

	// add aov names to update context
	// it will be used later in pass sync
	update_context->add_aov_names(aovs[0], aovs[1]);
}

XSI::CStatus update_material(ccl::Scene* scene, const XSI::Material &xsi_material, size_t shader_index, const XSI::CTime &eval_time, std::vector<XSI::CStringArray> &aovs)
{
	ccl::ShaderGraph* shader_graph = material_to_graph(scene, xsi_material, eval_time, aovs);
	ccl::Shader* shader = scene->shaders[shader_index];

	shader->set_graph(shader_graph);
	shader->tag_update(scene);

	return XSI::CStatus::OK;
}

XSI::CStatus update_shaderball_shadernode(ccl::Scene* scene, ULONG xsi_id, ShaderballType shaderball_type, size_t shader_index, const XSI::CTime& eval_time)
{
	XSI::ProjectItem item = XSI::Application().GetObjectFromID(xsi_id);
	XSI::Shader xsi_shader(item);

	bool is_success = false;
	ccl::ShaderGraph* shader_graph = shaderball_shadernode_to_graph(scene, xsi_shader, !(shaderball_type == ShaderballType_VolumeShader), eval_time, is_success);
	if (is_success)
	{
		ccl::Shader* shader = scene->shaders[shader_index];
		shader->set_graph(shader_graph);
		shader->tag_update(scene);

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}