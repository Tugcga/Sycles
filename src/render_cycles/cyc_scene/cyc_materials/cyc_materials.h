#pragma once
#include "scene/shader.h"
#include "scene/shader_graph.h"

#include <xsi_shader.h>
#include <xsi_time.h>
#include <xsi_shaderparameter.h>

#include <unordered_map>

void sync_float3_parameter(XSI::ShaderParameter& xsi_parameter,
	const std::string& cycles_name,
	ccl::ShaderNode* cycles_node,
	ccl::ShaderGraph* shader_graph,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	const XSI::CTime& eval_time,
	std::vector<XSI::CStringArray>& aovs);
void sync_float_parameter(XSI::ShaderParameter& xsi_parameter,
	const std::string& cycles_name,
	ccl::ShaderNode* cycles_node,
	ccl::ShaderGraph* shader_graph,
	std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map,
	const XSI::CTime& eval_time,
	std::vector<XSI::CStringArray>& aovs);
ccl::ShaderNode* sync_cycles_shader(const XSI::Shader& xsi_shader, const XSI::CString& shader_type, const XSI::CParameterRefArray& xsi_parameters, const XSI::CTime& eval_time, ccl::ShaderGraph* shader_graph, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, std::vector<XSI::CStringArray>& aovs);
ccl::ShaderNode* xsi_node_to_cycles(const XSI::Shader& xsi_shader, ccl::ShaderGraph* shader_graph, std::unordered_map<ULONG, ccl::ShaderNode*>& nodes_map, const XSI::CTime& eval_time, std::vector<XSI::CStringArray>& aovs);
