#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include "../../../utilities/logs.h"

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
