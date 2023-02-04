#pragma once
#include <xsi_ref.h>
#include <xsi_shaderparameter.h>
#include <xsi_shader.h>
#include <xsi_fcurve.h>
#include <xsi_imageclip2.h>

enum ShaderParameterType
{
	ParameterType_Unknown,
	ParameterType_Float,
	ParameterType_Integer,
	ParameterType_Boolean,
	ParameterType_String,
	ParameterType_Color3,
	ParameterType_Color4,
	ParameterType_Vector3
};

struct GradientPoint
{
	float pos;
	XSI::MATH::CColor4f color;
	float mid;
};

// return type of the shader parameter
ShaderParameterType get_shader_parameter_type(XSI::ShaderParameter& parameter);

// return name of hte output shader node parameter, connected to the given input parameter of the other node
// xsi_shader is a node where we search parameter name
// input_name is a name from another node
XSI::CString get_output_parameter_connected_to_input_parameter(const XSI::Shader& xsi_shader, const XSI::CString& input_name);

// return shader parameter of the material root node, which has root_parameter_name name and connected to first-level node in the tree
// if there is no port with the name, return invalid parameter
XSI::ShaderParameter get_root_shader_parameter(const XSI::CRefArray& first_level_shaders, const XSI::CString& root_parameter_name, bool check_substring);

// return true if input shader node is compound
bool is_shader_compound(const XSI::Shader& shader);

// return shader node (may be inside compound), connected to the parameter
// parameter is a right port, shader node at the left
// *node*------>*parameter*
// if ignore_converters is true, then we should goes through color_to_sceler and scalar_to_color
XSI::Shader get_input_node(const XSI::ShaderParameter& parameter, bool ignore_converters = false);

// return parameter, connected to the input parameter of the shader node
// if it connect to the primitive node, then return the last parameter before this connection (if return_output is false)
// if it connect to compound input, then return this input (if return_output is false, empty parameter is true)
// go deeper through passthrough nodes
// if return_output = true then return parameter of the final node
// if false, then parameter before last connection
XSI::ShaderParameter get_source_parameter(const XSI::ShaderParameter &parameter, bool return_output = false);

float get_float_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
int get_int_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
bool get_bool_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::CString get_string_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::MATH::CColor4f get_color_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::MATH::CVector3 get_vector_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::FCurve get_fcurve_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);
XSI::ImageClip2 get_clip_parameter_value(const XSI::CParameterRefArray& all_parameters, const XSI::CString& parameter_name, const XSI::CTime& eval_time);