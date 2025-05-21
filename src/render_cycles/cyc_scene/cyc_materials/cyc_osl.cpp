#include <xsi_arrayparameter.h>
#include <xsi_shader.h>
#include <xsi_shaderdef.h>
#include <xsi_shaderparamdefcontainer.h>
#include <xsi_shaderparamdef.h>
#include <xsi_color4f.h>

#include "scene/shader.h"
#include "scene/shader_nodes.h"
#include "scene/shader_graph.h"
#include "scene/scene.h"
#include "scene/osl.h"

#include "../../../utilities/xsi_shaders.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "cyc_materials.h"

ccl::ShaderNode* sync_osl_shader(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, const XSI::Shader &xsi_shader, std::unordered_map<ULONG, ccl::ShaderNode*> &nodes_map, std::vector<XSI::CStringArray> &aovs, const XSI::CTime &eval_time)
{
#ifdef WITH_OSL
	ccl::ShaderManager* manager = scene->shader_manager.get();
	if (manager->use_osl())
	{
		XSI::CParameterRefArray params = xsi_shader.GetParameters();
		XSI::CString path = get_string_parameter_value(params, "oslFilePath", eval_time);
		std::string filepath = path.GetAsciiString();

		ccl::OSLNode* node = ccl::OSLShaderManager::osl_node(shader_graph, scene, filepath, "");
		if (!node)
		{
			return NULL;
		}

		ULONG xsi_shader_id = xsi_shader.GetObjectID();
		nodes_map[xsi_shader_id] = node;
		node->name = ccl::ustring(xsi_shader.GetName().GetAsciiString());

		// Set all input ports
		XSI::ShaderDef shader_def = xsi_shader.GetShaderDef();
		XSI::ShaderParamDefContainer input_parameters = shader_def.GetInputParamDefs();
		XSI::CRefArray input_params_array = input_parameters.GetDefinitions();
		
		for (size_t input_index = 0; input_index < input_params_array.GetCount(); input_index++)
		{
			XSI::ShaderParamDef shader_param(input_params_array[input_index]);
			XSI::CString param_name = shader_param.GetName();
			XSI::ShaderParameter xsi_parameter = params.GetItem(param_name);
			ShaderParameterType xsi_parameter_type = get_shader_parameter_type(xsi_parameter);
			
			if (xsi_parameter_type == ShaderParameterType::ParameterType_Float)
			{
				sync_float_parameter(scene, shader_graph, node, xsi_parameter, nodes_map, aovs, std::string(param_name.GetAsciiString()), eval_time);
			}
			else if (xsi_parameter_type == ShaderParameterType::ParameterType_Color3 || xsi_parameter_type == ShaderParameterType::ParameterType_Color4)
			{
				sync_float3_parameter(scene, shader_graph, node, xsi_parameter, nodes_map, aovs, std::string(param_name.GetAsciiString()), eval_time);
			}
			else if (xsi_parameter_type == ShaderParameterType::ParameterType_Integer)
			{
				int int_value = get_int_parameter_value(params, param_name, eval_time);
				const ccl::SocketType& socket = node->input(param_name.GetAsciiString())->socket_type;
				node->set(socket, int_value);
			}
			else if (xsi_parameter_type == ShaderParameterType::ParameterType_String)
			{
				if (param_name != "nodeType" && param_name != "oslFilePath")
				{
					XSI::CString string_value = get_string_parameter_value(params, param_name, eval_time);
					const ccl::SocketType& socket = node->input(param_name.GetAsciiString())->socket_type;
					((ccl::Node*)node->input(param_name.GetAsciiString())->parent)->set(socket, string_value.GetAsciiString());
				}
			}
		}
		
		return node;
	}
#endif // WITH_OSL

	return NULL;
}
