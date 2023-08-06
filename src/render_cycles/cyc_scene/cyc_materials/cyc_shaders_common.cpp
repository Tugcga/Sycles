#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include <xsi_shader.h>
#include <xsi_color4f.h>

#include <vector>

#include "../../../render_base/type_enums.h"
#include "../../../utilities/strings.h"
#include "../../../utilities/xsi_shaders.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "names_converter.h"

ShadernodeType get_shadernode_type(const XSI::Shader &xsi_shader, XSI::CString &out_type)
{
	XSI::CString prog_id = xsi_shader.GetProgID();
	std::vector<size_t> pos = get_symbol_positions(prog_id, '.');

	if (pos.size() >= 2)
	{
		// get part to the first point
		XSI::CString app_name = prog_id.GetSubString(0, pos[0]);
		if (app_name == "CyclesShadersPlugin")
		{
			XSI::CString shader_name = prog_id.GetSubString(pos[0] + 1, pos[1] - pos[0] - 1);
			if (shader_name.Length() > 6)
			{
				out_type = shader_name.GetSubString(6, shader_name.Length());
				if (out_type == "OutputColorAOV" || out_type == "OutputValueAOV")
				{
					return ShadernodeType_CyclesAOV;
				}
				else
				{
					return ShadernodeType_Cycles;
				}
			}
			else
			{
				return ShadernodeType_Unknown;
			}
		}
		else if (app_name == "CyclesOSL")
		{
			out_type = "osl";
			return ShadernodeType_OSL;
		}
		else if (app_name == "Softimage")
		{
			out_type = prog_id.GetSubString(pos[0] + 1, pos[1] - pos[0] - 1);
			return ShadernodeType_NativeXSI;
		}
		else if (app_name == "GLTFShadersPlugin")
		{
			out_type = prog_id.GetSubString(pos[0] + 1, pos[1] - pos[0] - 1);
			return ShadernodeType_GLTF;
		}
		else
		{
			return ShadernodeType_Unknown;
		}
	}
	else
	{
		return ShadernodeType_Unknown;
	}
}

void make_aovs_reconnections(ccl::ShaderGraph* shader_graph, ccl::ShaderNode* aov_node, ccl::ShaderNode* next_node, const XSI::Shader& xsi_aov_node, const std::string& next_input_name, const XSI::CTime& eval_time)
{
	// there are no output connection node for aov node in Cycles, but in XSI we have this port
	// so, make reconnection with output port of the source for converted node
	/*
		┌───────┐   ┌─────┐    ┌─────────┐
		│   ?   ├───┤ aov │  ┌─┤next node│
		└───────┤   └─────┘  │ └─────────┘
				│            │
				└────────────┘
	*/
	bool is_color = xsi_aov_node.GetProgID() == "CyclesShadersPlugin.CyclesOutputColorAOV.1.0";
	ccl::ShaderInput* aov_input = aov_node->input(is_color ? "Color" : "Value");
	// get connection
	ccl::ShaderOutput* something_output = aov_input->link;
	if (something_output)
	{
		// there is connection, use it
		shader_graph->connect(something_output, next_node->input(next_input_name.c_str()));
	}
	else
	{
		// nothing connected to aov node
		// but user assume that value from the node affects to the shader
		// so, create constant node (with value from aov) and connect it as to aov and next node
		if (is_color)
		{
			XSI::MATH::CColor4f xsi_aov_color = get_color_parameter_value(xsi_aov_node.GetParameters(), "Color", eval_time);
			ccl::ColorNode* new_color = shader_graph->create_node<ccl::ColorNode>();
			shader_graph->add(new_color);
			new_color->set_value(color4_to_float3(xsi_aov_color));
			// make connections
			shader_graph->connect(new_color->output("Color"), aov_input);
			shader_graph->connect(new_color->output("Color"), next_node->input(next_input_name.c_str()));
		}
		else
		{
			float xsi_aov_value = get_float_parameter_value(xsi_aov_node.GetParameters(), "Value", eval_time);
			ccl::ValueNode* new_value = shader_graph->create_node<ccl::ValueNode>();
			shader_graph->add(new_value);
			new_value->set_value(xsi_aov_value);
			shader_graph->connect(new_value->output("Value"), aov_input);
			shader_graph->connect(new_value->output("Value"), next_node->input(next_input_name.c_str()));
		}
	}
}

bool make_nodes_connection(ccl::ShaderGraph* shader_graph, ccl::ShaderNode* output_node, ccl::ShaderNode* input_node, const XSI::Shader &xsi_output_shader, const XSI::CString &xsi_output_name, const std::string &input_name, const XSI::CTime &eval_time)
{
	// obtain output xsi shader node type, again
	XSI::CString type_name;
	ShadernodeType output_node_type = get_shadernode_type(xsi_output_shader, type_name);
	if (output_node_type == ShadernodeType::ShadernodeType_Cycles || output_node_type == ShadernodeType::ShadernodeType_NativeXSI || output_node_type == ShadernodeType::ShadernodeType_GLTF)
	{
		std::string connect_name = convert_port_name(xsi_output_shader.GetProgID(), xsi_output_name);
		if (connect_name.length() > 0)
		{
			shader_graph->connect(output_node->output(connect_name.c_str()), input_node->input(input_name.c_str()));
			return true;
		}
	}
	else if(output_node_type == ShadernodeType::ShadernodeType_CyclesAOV)
	{
		make_aovs_reconnections(shader_graph, output_node, input_node, xsi_output_shader, input_name, eval_time);
		return true;
	}
	else if (output_node_type == ShadernodeType::ShadernodeType_OSL)
	{
		// osl parser create nodes with output parameter names with prefix out
		// so, we should remove first three letters to obtain actual output port name
		if (xsi_output_name.Length() > 3)
		{
			shader_graph->connect(output_node->output(xsi_output_name.GetSubString(3).GetAsciiString()), input_node->input(input_name.c_str()));
			return true;
		}
	}

	return false;
}