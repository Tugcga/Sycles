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
#include "cyc_materialx.h"
#include "names_converter.h"

int create_default_shader(ccl::Scene* scene)
{
	ccl::Shader* shader = scene->create_node<ccl::Shader>();
	shader->name = "default_shader";
	std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
	ccl::GlossyBsdfNode* glossy_node = shader_graph->create_node<ccl::GlossyBsdfNode>();
	glossy_node->set_roughness(0.25f);
	glossy_node->set_color(ccl::make_float3(1.0f, 1.0f, 1.0f));

	ccl::ShaderNode* out = shader_graph->output();
	shader_graph->connect(glossy_node->output("BSDF"), out->input("Surface"));
	shader->set_graph(std::move(shader_graph));
	shader->tag_update(scene);

	return scene->shaders.size() - 1;
}

int create_emission_checker(ccl::Scene* scene, float checker_scale)
{
	ccl::Shader* shader = scene->create_node<ccl::Shader>();
	shader->name = "emission_checker";
	std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
	ccl::EmissionNode* emission_node = shader_graph->create_node<ccl::EmissionNode>();
	emission_node->set_strength(1.0);

	ccl::CheckerTextureNode* checker_node = shader_graph->create_node<ccl::CheckerTextureNode>();
	checker_node->set_scale(checker_scale);
	checker_node->set_color1(ccl::make_float3(0.2, 0.2, 0.2));
	checker_node->set_color2(ccl::make_float3(0.8, 0.8, 0.8));

	ccl::TextureCoordinateNode* uv_node = shader_graph->create_node<ccl::TextureCoordinateNode>();

	ccl::ShaderNode* out = shader_graph->output();
	shader_graph->connect(emission_node->output("Emission"), out->input("Surface"));
	shader_graph->connect(checker_node->output("Color"), emission_node->input("Color"));
	shader_graph->connect(uv_node->output("UV"), checker_node->input("Vector"));
	shader->set_graph(std::move(shader_graph));
	shader->tag_update(scene);

	return scene->shaders.size() - 1;
}

// we call this method for every node from render tree to convert it to the Cycles node in the sahder graph
// here we does not add the node to the graph
ccl::ShaderNode* xsi_node_to_cycles(
	ccl::Scene* scene,  // we need scene for osl shader manager
	ccl::ShaderGraph* shader_graph,
	const XSI::Shader& xsi_shader,
	UpdateContext* update_context)
{
	ULONG xsi_id = xsi_shader.GetObjectID();
	if (update_context->is_nodes_map_contains(xsi_id)) {
		return update_context->get_from_nodes_map(xsi_id);
	}
	else
	{
		XSI::CString out_type;
		ShadernodeType shadernode_type = get_shadernode_type(xsi_shader, out_type);

		if (shadernode_type == ShadernodeType::ShadernodeType_Cycles || shadernode_type == ShadernodeType::ShadernodeType_CyclesAOV)
		{
			XSI::CParameterRefArray params = xsi_shader.GetParameters();
			return sync_cycles_shader(scene, xsi_shader, out_type, params, shader_graph, update_context);
		}
		else if (shadernode_type == ShadernodeType::ShadernodeType_OSL)
		{
			return sync_osl_shader(scene, shader_graph, xsi_shader, update_context);
		}
		else if (shadernode_type == ShadernodeType::ShadernodeType_NativeXSI)
		{
			return sync_xsi_shader(scene, shader_graph, xsi_shader, out_type, update_context);
		}
		else if (shadernode_type == ShadernodeType::ShadernodeType_GLTF)
		{
			return sync_gltf_shader(scene, shader_graph, xsi_shader, out_type, update_context);
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
	const XSI::ShaderParameter& xsi_parameter,
	const std::string& cycles_name,
	UpdateContext* update_context)
{
	// check is this final parameter is connected to something
	XSI::CRef source = xsi_parameter.GetSource();
	XSI::CTime eval_time = update_context->get_time();
	if (source.IsValid() && source.GetClassID() == XSI::siShaderParameterID)
	{// there is connection with shader parameter 
		XSI::ShaderParameter xsi_source_parameter(source);
		XSI::Shader xsi_source_node(xsi_source_parameter.GetParent());

		ccl::ShaderNode* source_node = xsi_node_to_cycles(scene, shader_graph, xsi_source_node, update_context);
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
	const std::string& cycles_name,
	UpdateContext* update_context) {
	XSI::CTime eval_time = update_context->get_time();
	XSI::ShaderParameter xsi_finall_parameter = get_source_parameter(xsi_parameter);

	bool is_connect = sync_shader_parameter_connection(scene, shader_graph, cycles_node, xsi_finall_parameter, cycles_name, update_context);

	ShaderParameterType parameter_type = get_shader_parameter_type(xsi_finall_parameter);
	XSI::Shader xsi_finall_parameter_shader = xsi_finall_parameter.GetParent();

	ccl::ShaderInput* input = cycles_node->input(cycles_name.c_str());

	if (parameter_type == ShaderParameterType::ParameterType_Float) {
		float float_value = get_float_parameter_value(xsi_finall_parameter_shader.GetParameters(), xsi_finall_parameter.GetName(), eval_time);
		input->set(float_value);
	}
}

void sync_int_parameter(ccl::Scene* scene,
	ccl::ShaderGraph* shader_graph,
	ccl::ShaderNode* cycles_node,
	XSI::ShaderParameter& xsi_parameter,
	const std::string& cycles_name,
	UpdateContext* update_context) {
	// in fact do the same as for float, but if obtained type if float, then clamp it
	// or use direct value
	XSI::CTime eval_time = update_context->get_time();
	XSI::ShaderParameter xsi_finall_parameter = get_source_parameter(xsi_parameter);
	
	bool is_connect = sync_shader_parameter_connection(scene, shader_graph, cycles_node, xsi_finall_parameter, cycles_name, update_context);

	ShaderParameterType parameter_type = get_shader_parameter_type(xsi_finall_parameter);
	XSI::Shader xsi_finall_parameter_shader = xsi_finall_parameter.GetParent();

	ccl::ShaderInput* input = cycles_node->input(cycles_name.c_str());

	// may be when obtain finall parameter we go thorw converter from float to int
	if (parameter_type == ShaderParameterType::ParameterType_Float) {
		float float_value = get_float_parameter_value(xsi_finall_parameter_shader.GetParameters(), xsi_finall_parameter.GetName(), eval_time);
		int int_value = int(float_value);

		// WARNING: for now only on parameter in Cycles can be integer: Thin Wall
		input->set(int_value);
	}
	else if (parameter_type == ShaderParameterType::ParameterType_Integer) {
		int int_value = get_int_parameter_value(xsi_finall_parameter_shader.GetParameters(), xsi_finall_parameter.GetName(), eval_time);
		input->set(int_value);
	}
	else if (parameter_type == ShaderParameterType::ParameterType_Boolean) {
		bool bool_value = get_bool_parameter_value(xsi_finall_parameter_shader.GetParameters(), xsi_finall_parameter.GetName(), eval_time);
		input->set(bool_value ? 1 : 0);
	}
}

void sync_float3_parameter(ccl::Scene* scene,
	ccl::ShaderGraph* shader_graph,
	ccl::ShaderNode* cycles_node,
	XSI::ShaderParameter& xsi_parameter,
	const std::string& cycles_name,
	UpdateContext* update_context) {
	// return final float3 value of the parameter
	XSI::ShaderParameter xsi_finall_parameter = get_source_parameter(xsi_parameter);
	XSI::CTime eval_time = update_context->get_time();
	
	bool is_connect = sync_shader_parameter_connection(scene, shader_graph, cycles_node, xsi_finall_parameter, cycles_name, update_context);

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
	XSI::CString &xsi_node_port_name,
	XSI::Shader &xsi_shader,
	const XSI::ShaderParameter& material_port,
	UpdateContext* update_context) {
	XSI::ShaderParameter material_port_source = get_source_parameter(material_port, true);  // return output parameter
	if (material_port_source.IsValid())
	{
		XSI::Shader xsi_node(material_port_source.GetParent());
		if (xsi_node.IsValid())
		{
			ccl::ShaderNode* to_return = xsi_node_to_cycles(scene, shader_graph, xsi_node, update_context);
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

//  return true if there is valid MaterialX node, conencted to material port of the root material
XSI::Shader has_materialx(const XSI::Material& xsi_material, bool& out_correct) {
	XSI::CParameterRefArray xsi_material_parameters = xsi_material.GetParameters();
	XSI::ShaderParameter root_material = xsi_material_parameters.GetItem("material");
	XSI::ShaderParameter root_material_connection = get_source_parameter(root_material, true);
	
	XSI::Shader first_node(root_material_connection.GetParent());
	if (!first_node.IsValid()) {
		out_correct = false;
		return first_node;
	}

	XSI::CString node_type;
	ShadernodeType type = get_shadernode_type(first_node, node_type);
	if (type == ShadernodeType::ShadernodeType_MaterialX) {
		if (node_type == "surfacematerial" || node_type == "lama_surface") {
			out_correct = true;
		}
		else {
			out_correct = false;
		}
		
		return first_node;
	}
	else {
		out_correct = false;
		return first_node;
	}
}

void material_to_graph(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Material& xsi_material, UpdateContext* update_context)
{
	bool is_correct = false;
	XSI::Shader material_node = has_materialx(xsi_material, is_correct);
	bool make_materialx = false;
	if (is_correct) {
		// material_node is a valid root materialX node, so we can try to export it as osl
		// but first we should check that osl is enabled
		if (scene->params.shadingsystem == ccl::ShadingSystem::SHADINGSYSTEM_OSL) {
			XSI::CString store_name = xsi_material.GetLibrary().GetName() + "_" + replace_letter(replace_letter(replace_letter(replace_letter(xsi_material.GetUniqueName(), '<', '_'), '>', '_'), ',', '_'), ' ', '_');
			make_materialx = sync_materialx_material(scene, shader_graph, update_context, material_node, store_name);
		}
		else {
			log_warning("It looks like the material " + xsi_material.GetName() + " has MaterialX nodes, but the shading system is SVM. Ignore it.");
		}
	}

	if (!make_materialx) {
		// start export new material, so, we should clear map from xsi nodes id to cycles nodes
		update_context->clear_nodes_map();

		XSI::CString xsi_node_surface_port_name = "";
		XSI::Shader xsi_shader_surface;
		XSI::CParameterRefArray xsi_material_parameters = xsi_material.GetParameters();
		ccl::ShaderNode* surface_node = sync_material_port(scene, shader_graph, xsi_node_surface_port_name, xsi_shader_surface, xsi_material_parameters.GetItem("surface"), update_context);

		XSI::CString xsi_node_volume_port_name = "";
		XSI::Shader xsi_shader_volume;
		ccl::ShaderNode* volume_node = sync_material_port(scene, shader_graph, xsi_node_volume_port_name, xsi_shader_volume, xsi_material_parameters.GetItem("volume"), update_context);

		XSI::CString xsi_node_displace_port_name = "";
		XSI::Shader xsi_shader_displacement;
		ccl::ShaderNode* displacement_node = sync_material_port(scene, shader_graph, xsi_node_displace_port_name, xsi_shader_displacement, xsi_material_parameters.GetItem("normal"), update_context);

		bool is_empty_surface = true;
		XSI::CTime eval_time = update_context->get_time();
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
			ccl::TransparentBsdfNode* transparent = shader_graph->create_node<ccl::TransparentBsdfNode>();
			transparent->set_color(ccl::make_float3(0.0, 0.0, 0.0));

			// connect to surface output
			ccl::ShaderNode* out = shader_graph->output();
			shader_graph->connect(transparent->output("BSDF"), out->input("Surface"));
		}
	}
}

// in this function we should crate the shader, add it to the shaders array and retun it index in this array
int sync_material(ccl::Scene* scene, const XSI::Material &xsi_material, UpdateContext* update_context)
{
	std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
	material_to_graph(scene, shader_graph.get(), xsi_material, update_context);

	// create output shader
	ccl::Shader* shader = scene->create_node<ccl::Shader>();

	shader->set_graph(std::move(shader_graph));
	shader->tag_update(scene);

	return scene->shaders.size() - 1;
}

void shaderball_shadernode_to_graph(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Shader& xsi_shader, bool is_surface, UpdateContext* update_context, bool &out_success)
{
	// convert shader node to Cycles one
	update_context->clear_nodes_map();
	// also clear aovs, because in shaderball rendering it does not used
	update_context->clear_aovs();

	ccl::ShaderNode* shader_node = xsi_node_to_cycles(scene, shader_graph, xsi_shader, update_context);
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
}

// call this function when we should previre shaderball rendertree node instead of whole material
int sync_shaderball_shadernode(ccl::Scene* scene, const XSI::Shader& xsi_shader, bool is_surface, UpdateContext* update_context)
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
	std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
	shaderball_shadernode_to_graph(scene, shader_graph.get(), xsi_shader, is_surface, update_context, is_success);

	if (is_success)
	{
		ccl::Shader* shader = scene->create_node<ccl::Shader>();

		shader->set_graph(std::move(shader_graph));
		shader->tag_update(scene);

		return scene->shaders.size() - 1;
	}
	else
	{
		return -1;
	}
}

int sync_shaderball_texturenode(ccl::Scene* scene, const XSI::Texture& xsi_texture, UpdateContext* update_context)
{
	XSI::Shader xsi_texture_shader(xsi_texture);

	return sync_shaderball_shadernode(scene, xsi_texture_shader, true, update_context);
}

void sync_material_process(ccl::Scene* scene, UpdateContext* update_context, const XSI::Material& xsi_material, bool ignore_empty) {
	ULONG xsi_id = xsi_material.GetObjectID();

	update_context->add_sync_profiler_time_start(SyncType::Material, xsi_id, xsi_material.GetFullName());
	XSI::CRefArray used_objects = xsi_material.GetUsedBy();
	if (ignore_empty || used_objects.GetCount() > 0)
	{
		int shader_index = sync_material(scene, xsi_material, update_context);
		if (shader_index >= 0)
		{
			update_context->add_material_index(xsi_id,
				shader_index,
				scene->shaders[shader_index]->has_displacement,
				ShaderballType_Unknown);
		}
	}
	update_context->add_sync_profiler_time_finish(SyncType::Material, xsi_id);
}

void sync_object_materials(ccl::Scene* scene, UpdateContext* update_context, const XSI::CRef& object_ref) {
	XSI::siClassID object_class = object_ref.GetClassID();
	if (object_class == XSI::siX3DObjectID || object_class == XSI::siLightID) {
		XSI::X3DObject xsi_object(object_ref);
		XSI::CRefArray materials = xsi_object.GetMaterials();
		LONG materials_count = materials.GetCount();
		for (size_t i = 0; i < materials_count; i++) {
			XSI::Material xsi_material(materials[i]);
			if (xsi_material.IsValid() && !update_context->is_material_exists(xsi_material.GetObjectID())) {
				sync_material_process(scene, update_context, xsi_material, false);
			}
		}
	}
}

void sync_scene_materials(ccl::Scene* scene, UpdateContext* update_context)
{
	XSI::Scene xsi_scene = XSI::Application().GetActiveProject().GetActiveScene();
	XSI::CRefArray material_libs = xsi_scene.GetMaterialLibraries();
	XSI::CTime eval_time = update_context->get_time();

	// before we start export all materials - clear previously exported aovs
	update_context->clear_aovs();

	for (LONG lib_index = 0; lib_index < material_libs.GetCount(); lib_index++)
	{
		XSI::MaterialLibrary lib = material_libs.GetItem(lib_index);
		XSI::CRefArray materials = lib.GetItems();
		for (LONG mat_index = 0; mat_index < materials.GetCount(); mat_index++)
		{
			XSI::Material xsi_material = materials.GetItem(mat_index);

			sync_material_process(scene, update_context, xsi_material, false);
		}
	}

	// after export we does not need to add aovs to update context, becasue each node add it

	// add aov names to update context
	// it will be used later in pass sync
	// update_context->add_aov_names(aovs[0], aovs[1]);
}

XSI::CStatus update_material(ccl::Scene* scene, const XSI::Material &xsi_material, size_t shader_index, UpdateContext* update_context)
{
	std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
	material_to_graph(scene, shader_graph.get(), xsi_material, update_context);
	ccl::Shader* shader = scene->shaders[shader_index];

	// TODO: there is a bug
	// when change background shader node intensity to zero with connected sky texture node, and then back to normal value,
	// then the sun intensity is disabled (even if it enebled in the node)
	shader->set_graph(std::move(shader_graph));
	shader->tag_modified();
	shader->tag_update(scene);

	return XSI::CStatus::OK;
}

XSI::CStatus update_shaderball_shadernode(ccl::Scene* scene, ULONG xsi_id, ShaderballType shaderball_type, size_t shader_index, UpdateContext* update_context)
{
	XSI::ProjectItem item = XSI::Application().GetObjectFromID(xsi_id);
	XSI::Shader xsi_shader(item);

	bool is_success = false;
	std::unique_ptr<ccl::ShaderGraph> shader_graph = std::make_unique<ccl::ShaderGraph>();
	shaderball_shadernode_to_graph(scene, shader_graph.get(), xsi_shader, !(shaderball_type == ShaderballType_VolumeShader), update_context, is_success);
	if (is_success)
	{
		ccl::Shader* shader = scene->shaders[shader_index];
		shader->set_graph(std::move(shader_graph));
		shader->tag_update(scene);

		return XSI::CStatus::OK;
	}
	else
	{
		return XSI::CStatus::Abort;
	}
}

bool get_material_id_from_name(const XSI::CString& material_identificator, ULONG &io_id) {
	XSI::Scene xsi_scene = XSI::Application().GetActiveProject().GetActiveScene();
	XSI::CRefArray material_libs = xsi_scene.GetMaterialLibraries();

	for (LONG lib_index = 0; lib_index < material_libs.GetCount(); lib_index++) {
		XSI::MaterialLibrary lib = material_libs.GetItem(lib_index);
		XSI::CString lib_name = lib.GetName();
		XSI::CRefArray materials = lib.GetItems();
		for (LONG mat_index = 0; mat_index < materials.GetCount(); mat_index++)
		{
			XSI::Material xsi_material = materials.GetItem(mat_index);
			XSI::CString mat_name = xsi_material.GetName();

			if (lib_name + "." + mat_name == material_identificator) {
				io_id = xsi_material.GetObjectID();
				return true;
			}
		}
	}

	return false;
}

// return OK. if material exported, Abort if it fails
// we call this function from export curve process, when material is alredy checked to be missed
XSI::CStatus sync_missed_material(ccl::Scene* scene, UpdateContext* update_context, int material_id) {
	XSI::ProjectItem xsi_item = XSI::Application().GetObjectFromID(material_id);
	if (!xsi_item.IsValid()) {
		return XSI::CStatus::Abort;
	}

	XSI::CString xsi_item_type = xsi_item.GetType();

	if (xsi_item_type != "material") {
		return XSI::CStatus::Abort;
	}

	XSI::Material xsi_material(xsi_item);
	XSI::CTime eval_time = update_context->get_time();
	
	sync_material_process(scene, update_context, xsi_material, true);
	// does not need to ad aovs, it add at export methods
	// update_context->add_aov_names(aovs[0], aovs[1]);

	return XSI::CStatus::OK;
}