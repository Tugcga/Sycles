#include <xsi_fcurve.h>

#include "scene/osl.h"
#include "kernel/osl/services.h"

#include "cyc_materialx.h"
#include "cyc_materialx_attributes.h"
#include "cyc_materials.h"
#include "../../../utilities/strings.h"
#include "../../../utilities/arrays.h"
#include "../../../utilities/xsi_shaders.h"
#include "../../../utilities/files_io.h"

#include "MaterialXCore/Document.h"
#include "MaterialXFormat/XmlIo.h"
#include "MaterialXRenderGlsl/GlslMaterial.h"
#include "MaterialXRender/ShaderRenderer.h"

MaterialX::NodePtr get_or_create_node(
	UpdateContext* update_context, 
	ULONG xsi_id, 
	MaterialX::DocumentPtr& mx_doc, 
	const std::string& node_type,
	const std::string& node_name,
	const std::string& node_output,
	bool& is_create) {

	if (update_context->has_mx_node(xsi_id)) {
		is_create = false;
		return update_context->get_mx_node(xsi_id);
	}

	MaterialX::NodePtr new_node = mx_doc->addNode(node_type, node_name, node_output);
	is_create = true;
	update_context->add_mx_node(xsi_id, new_node);

	return new_node;
}

// convert xsi data types to string
std::string parameter_type_to_string(const XSI::ShaderParameter& xsi_parameter, UpdateContext* update_context) {
	// get parent node
	XSI::Shader xsi_node = xsi_parameter.GetParent();
	if (xsi_node.IsValid()) {
		XSI::CString xsi_shader_prog_id = xsi_node.GetProgID();

		std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>, std::vector<std::tuple<std::string, std::string>>> node_data = update_context->get_mx_data(xsi_shader_prog_id.Split(".")[1].GetAsciiString());

		std::string param_name = xsi_parameter.GetName().GetAsciiString();
		XSI::ShaderParamDef xsi_def = xsi_parameter.GetDefinition();
		bool is_input = xsi_def.IsInput();
		bool is_output = xsi_def.IsOutput();

		// inputs are first, otputs are last
		std::vector<std::tuple<std::string, std::string>> ports_array = is_input ? std::get<1>(node_data) : std::get<2>(node_data);
		for (size_t i = 0; i < ports_array.size(); i++) {
			std::tuple<std::string, std::string> one_parameter = ports_array[i];
			std::string name = std::get<0>(one_parameter);
			std::string type = std::get<1>(one_parameter);
			if (name == param_name) {
				return type;
			}
		}
	}

	return "";
}

std::string colorspace_to_string(const XSI::CString& xsi_value) {
	if (xsi_value == "Automatic") {
		return "auto";
	}
	else if (xsi_value == "Linear") {
		return "lin_rec709";
	}
	else if (xsi_value == "sRGB") {
		return "srgb_texture";
	}
	else {
		return "user";
	}
}

std::string multioutput_name() {
	return "multioutput";
}

size_t get_shader_outputs_count(UpdateContext *update_context, const XSI::Shader& xsi_shader, std::string& out_last_output) {
	size_t to_return = 0;

	XSI::CParameterRefArray shader_parameters = xsi_shader.GetParameters();
	ULONG params_count = shader_parameters.GetCount();
	size_t outputs_count = 0;
	for (ULONG i = 0; i < params_count; i++) {
		XSI::ShaderParameter param = shader_parameters[i];
		XSI::ShaderParamDef param_def = param.GetDefinition();

		if (param_def.IsOutput()) {
			std::string data_type_string = parameter_type_to_string(param, update_context);

			if (data_type_string != "") {
				to_return++;
				out_last_output = data_type_string;
			}
		}
	}

	return to_return;
}

ccl::InterpolationType string_to_interpolation(const XSI::CString &type_str) {
	if (type_str == "closest") {
		return ccl::InterpolationType::INTERPOLATION_CLOSEST;
	}
	else if (type_str == "linear") {
		return ccl::InterpolationType::INTERPOLATION_LINEAR;
	}
	else {  // cubic
		return ccl::InterpolationType::INTERPOLATION_CUBIC;
	}
}

ccl::ExtensionType string_to_extension(const XSI::CString &type_str) {
	if (type_str == "constant") {
		return ccl::ExtensionType::EXTENSION_EXTEND;
	}
	else if (type_str == "clamp") {
		return ccl::ExtensionType::EXTENSION_CLIP;
	}
	else if (type_str == "mirror") {
		return ccl::ExtensionType::EXTENSION_MIRROR;
	}
	else {  // periodic
		return ccl::ExtensionType::EXTENSION_REPEAT;
	}
}

ccl::ImageParams image_parameters(const XSI::Shader & xsi_node, const XSI::CTime &eval_time) {
	ccl::ImageParams params;
	params.animated = false;
	params.interpolation = ccl::InterpolationType::INTERPOLATION_CUBIC;
	params.extension = ccl::ExtensionType::EXTENSION_REPEAT;
	params.alpha_type = ccl::ImageAlphaType::IMAGE_ALPHA_AUTO;
	params.colorspace = ccl::u_colorspace_scene_linear_srgb; // ccl::u_colorspace_data;

	if (!xsi_node.IsValid()) {
		return params;
	}

	XSI::CParameterRefArray all_parameters = xsi_node.GetParameters();

	// try find several typical parameters
	// if found - define image property
	XSI::CValue filtertype = xsi_node.GetParameterValue("filtertype");  // "closest,linear,cubic"
	if (!filtertype.IsEmpty() && filtertype.m_t == XSI::siString) {
		XSI::CString filtertype_value = get_string_parameter_value(all_parameters, "filtertype", eval_time);
		params.interpolation = string_to_interpolation(filtertype_value);
	}

	// all other image nodes
	XSI::CValue uaddressmode = xsi_node.GetParameterValue("uaddressmode");
	XSI::CValue vaddressmode = xsi_node.GetParameterValue("vaddressmode");
	if (!uaddressmode.IsEmpty() && !vaddressmode.IsEmpty() && uaddressmode.m_t == XSI::siString && vaddressmode.m_t == XSI::siString) {
		XSI::CString uaddressmode_str = get_string_parameter_value(all_parameters, "uaddressmode", eval_time);
		XSI::CString vaddressmode_str = get_string_parameter_value(all_parameters, "vaddressmode", eval_time);
		if (uaddressmode_str != vaddressmode_str) {
			log_warning("MaterialX node " + xsi_node.GetFullName() + " constains as uaddressmode and vaddressmode properties, but it has different values. Cycles supports only one value for both directions, use U-axis.");
		}
		params.extension = string_to_extension(uaddressmode_str);
	}

	if (uaddressmode.IsEmpty() || vaddressmode.IsEmpty()) {
		// ND_UsdUVTexture, ND_UsdUVTexture_23 use
		XSI::CValue wrap_s = xsi_node.GetParameterValue("wrapS");
		XSI::CValue wrap_t = xsi_node.GetParameterValue("wrapT");
		if (!wrap_s.IsEmpty() && !wrap_t.IsEmpty() && wrap_s.m_t == XSI::siString && wrap_t.m_t == XSI::siString) {
			XSI::CString uwrap_s_str = get_string_parameter_value(all_parameters, "wrapS", eval_time);
			XSI::CString wrap_t_str = get_string_parameter_value(all_parameters, "wrapT", eval_time);
			if (uwrap_s_str != wrap_t_str) {
				log_warning("MaterialX node " + xsi_node.GetFullName() + " constains as wrapS and wrapT properties, but it has different values. Cycles supports only one value for both directions, use U-axis.");
			}
			params.extension = string_to_extension(uwrap_s_str);
		}
	}

	return params;
}

bool add_input_value_to_node(ccl::Scene* scene, UpdateContext* update_context, MaterialX::NodePtr& node, const XSI::ShaderParameter& xsi_parameter, const XSI::siShaderParameterDataType xsi_type) {
	std::string type_string = parameter_type_to_string(xsi_parameter, update_context);
	std::string name = xsi_parameter.GetName().GetAsciiString();

	XSI::ShaderParameter finall_xsi_parameter = get_source_parameter(xsi_parameter);
	XSI::CValue xsi_value = finall_xsi_parameter.GetValue(update_context->get_time());

	if (xsi_type == XSI::siShaderDataTypeBoolean) {
		node->setInputValue(name, (bool)xsi_value);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeInteger) {
		node->setInputValue(name, (int)xsi_value);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeScalar) {
		node->setInputValue(name, (float)xsi_value);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeVector2) {
		MaterialX::Vector2 mx_vector = MaterialX::Vector2(xsi_parameter.GetParameterValue("x"), xsi_parameter.GetParameterValue("y"));
		node->setInputValue(name, mx_vector);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeVector3) {
		MaterialX::Vector3 mx_vector = MaterialX::Vector3(xsi_parameter.GetParameterValue("x"), xsi_parameter.GetParameterValue("y"), xsi_parameter.GetParameterValue("z"));
		node->setInputValue(name, mx_vector);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeVector4) {
		MaterialX::Vector4 mx_vector = MaterialX::Vector4(xsi_parameter.GetParameterValue("x"),
			xsi_parameter.GetParameterValue("y"),
			xsi_parameter.GetParameterValue("z"),
			xsi_parameter.GetParameterValue("w"));
		node->setInputValue(name, mx_vector);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeQuaternion) {
		XSI::MATH::CQuaternionf value_quaternion(xsi_value);
		// write as custom quaternion data
		std::string data_string = std::to_string(value_quaternion.GetX()) + "," + std::to_string(value_quaternion.GetY()) + "," + std::to_string(value_quaternion.GetZ()) + "," + std::to_string(value_quaternion.GetW());
		node->setInputValue(name, data_string, "quaternion");
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeMatrix33) {
		MaterialX::Matrix33 mx_matrix = MaterialX::Matrix33(xsi_parameter.GetParameterValue("_00"), xsi_parameter.GetParameterValue("_01"), xsi_parameter.GetParameterValue("_02"),
			xsi_parameter.GetParameterValue("_10"), xsi_parameter.GetParameterValue("_11"), xsi_parameter.GetParameterValue("_12"),
			xsi_parameter.GetParameterValue("_20"), xsi_parameter.GetParameterValue("_21"), xsi_parameter.GetParameterValue("_22"));
		node->setInputValue(name, mx_matrix);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeMatrix44) {
		MaterialX::Matrix44 mx_matrix = MaterialX::Matrix44(xsi_parameter.GetParameterValue("_00"), xsi_parameter.GetParameterValue("_01"), xsi_parameter.GetParameterValue("_02"), xsi_parameter.GetParameterValue("_03"),
			xsi_parameter.GetParameterValue("_10"), xsi_parameter.GetParameterValue("_11"), xsi_parameter.GetParameterValue("_12"), xsi_parameter.GetParameterValue("_13"),
			xsi_parameter.GetParameterValue("_20"), xsi_parameter.GetParameterValue("_21"), xsi_parameter.GetParameterValue("_22"), xsi_parameter.GetParameterValue("_23"),
			xsi_parameter.GetParameterValue("_30"), xsi_parameter.GetParameterValue("_31"), xsi_parameter.GetParameterValue("_32"), xsi_parameter.GetParameterValue("_33"));
		node->setInputValue(name, mx_matrix);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeColor3) {
		XSI::MATH::CColor4f value_color(xsi_value);
		MaterialX::Color3 mx_color = MaterialX::Color3(value_color.GetR(), value_color.GetG(), value_color.GetB());
		node->setInputValue(name, mx_color);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeColor4) {
		XSI::MATH::CColor4f value_color(xsi_value);
		MaterialX::Color4 mx_color = MaterialX::Color4(value_color.GetR(), value_color.GetG(), value_color.GetB(), value_color.GetA());
		node->setInputValue(name, mx_color);
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeString) {
		XSI::CString value_sting(xsi_value);
		node->setInputValue(name, std::string(value_sting.GetAsciiString()));
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeProfileCurve) {
		// write data in the format ltx, lty: t, v: rtx, rty;...
		std::string data_string = "";
		XSI::FCurve curve(xsi_value);
		XSI::CFCurveKeyRefArray curve_keys = curve.GetKeys();
		for (size_t i = 0; i < curve_keys.GetCount(); i++) {
			XSI::FCurveKey key(curve_keys[i]);
			data_string += std::to_string(key.GetLeftTanX()) + "," + std::to_string(key.GetLeftTanY()) + ":" +
				std::to_string(key.GetTime().GetTime()) + "," + std::to_string((float)key.GetValue()) + ":" +  // suppose that value is a float
				std::to_string(key.GetRightTanX()) + "," + std::to_string(key.GetRightTanY());

			if (i < curve_keys.GetCount() - 1) {
				data_string += ";";
			}
		}

		node->setInputValue(name, data_string, "fcurve");
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeGradient) {
		XSI::CParameterRefArray gradient_parameters = xsi_parameter.GetParameters();
		XSI::Parameter markers_parameter = gradient_parameters.GetItem("markers");
		XSI::CParameterRefArray markers = markers_parameter.GetParameters();
		ULONG markers_count = markers.GetCount();

		// store keys as pos, r, g, b, a
		std::vector<std::tuple<float, float, float, float, float>> markers_array;
		for (size_t i = 0; i < markers_count; i++) {
			XSI::ShaderParameter p = markers[i];
			float pos = p.GetParameterValue("pos");
			float mid = p.GetParameterValue("mid");
			float r = p.GetParameterValue("red");
			float g = p.GetParameterValue("green");
			float b = p.GetParameterValue("blue");
			float a = p.GetParameterValue("alpha");
			if (pos > -1) {
				markers_array.push_back(std::make_tuple(pos, r, g, b, a));
			}
		}

		// sort markers array by position
		std::sort(markers_array.begin(), markers_array.end());

		// output as string in the form: pos: r, g, b, a; pos: r, g, b, a
		std::string data_string = "";
		for (size_t i = 0; i < markers_array.size(); i++) {
			auto m = markers_array[i];

			data_string += std::to_string(std::get<0>(m)) + ":" + std::to_string(std::get<1>(m)) + "," + std::to_string(std::get<2>(m)) + "," + std::to_string(std::get<3>(m)) + "," + std::to_string(std::get<4>(m));
			if (i < markers_array.size() - 1) {
				data_string += ";";
			}
		}

		node->setInputValue(name, data_string, "gradient");
		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeImage) {
		XSI::ShaderParameter image_parameter = get_source_parameter(xsi_parameter, true);
		// we create node/graph input in any case
		// if there are no valid connected clip, then the input is empty
		MaterialX::InputPtr input;
		input = node->addInput(name, "filename");

		if (image_parameter.IsValid()) {
			XSI::CRef image_source = image_parameter.GetSource();
			if (image_source.IsValid()) {
				XSI::ImageClip2 image_clip(image_source);

				if (image_clip.IsValid()) {
					XSI::CString xsi_image_path = replace_symbols(image_clip.GetFileName(), "\\", "/");
					XSI::CValue color_profile = image_clip.GetParameterValue("RenderColorProfile");

					input->setValueString(xsi_image_path.GetAsciiString());
					input->setColorSpace(colorspace_to_string(color_profile));

					update_context->add_mx_image(xsi_image_path.GetAsciiString(), image_parameters(xsi_parameter.GetParent(), update_context->get_time()));
				}
			}
			else {
				input->setValueString("");
			}
		}
		// also for invalid image parameter
		if (!image_parameter.IsValid()) {
			input->setValueString("");
		}

		return true;
	}
	// does not support the following properties
	else if (xsi_type == XSI::siShaderDataTypeProperty) {}
	else if (xsi_type == XSI::siShaderDataTypeLightProfile) {}
	else if (xsi_type == XSI::siShaderDataTypeReference) {}
	else if (xsi_type == XSI::siShaderDataTypeCustom) {}
	else if (xsi_type == XSI::siShaderDataTypeStructure) {}
	else if (xsi_type == XSI::siShaderDataTypeArray) {}

	return false;
}

bool is_input_connected(const XSI::Shader& node, const XSI::ShaderParameter& parameter) {
	XSI::ShaderParameter param_source = get_source_parameter(parameter, true);
	XSI::Shader param_node = param_source.GetParent();
	if (param_node.IsValid()) {
		if (param_node.GetObjectID() == node.GetObjectID()) {
			return false;
		}
		else {
			return true;
		}
	}
	else {
		return false;
	}
}

MaterialX::NodePtr shader_to_node(ccl::Scene* scene, UpdateContext* update_context, MaterialX::DocumentPtr mx_doc, const XSI::Shader& xsi_node);

void propagate_connection(ccl::Scene* scene, UpdateContext* update_context, const XSI::ShaderParameter& xsi_parameter, MaterialX::DocumentPtr& mx_doc, MaterialX::InputPtr mx_input) {
	XSI::Shader parameter_parent = xsi_parameter.GetParent();
	
	if (parameter_parent.IsValid() && is_input_connected(parameter_parent, xsi_parameter)) {
		XSI::ShaderParameter input_source = get_source_parameter(xsi_parameter, true);

		if (input_source.IsValid()) {
			XSI::Shader source_shader = input_source.GetParent();
			if (!is_shader_compound(source_shader)) {
				MaterialX::NodePtr source_node = shader_to_node(scene, update_context, mx_doc, source_shader);  // <--- here is recursion
				// it can return null, if the node is unknown
				if (source_node) {
					mx_input->setConnectedNode(source_node);

					std::string source_last_output_name = "";
					size_t source_outputs = get_shader_outputs_count(update_context, source_shader, source_last_output_name);
					if (source_outputs > 1) {
						std::string out_name = input_source.GetName().GetAsciiString();
						mx_input->setOutputString(out_name);
					}
				}
			}
			// if we finish at compound, then there are no connections to the port, and value already was properly defined
		}
	}
}

MaterialX::NodePtr shader_to_node(ccl::Scene* scene, UpdateContext* update_context, MaterialX::DocumentPtr mx_doc, const XSI::Shader &xsi_node) {
	bool is_new = false;
	XSI::CString node_type;
	ShadernodeType type = get_shadernode_type(xsi_node, node_type);
	if (type == ShadernodeType::ShadernodeType_MaterialX) {
		// convert obtained node type (which is name with type, someting like image_float or image_color3) to mx type (image)
		auto [mx_node_type, inputs, outputs] = update_context->get_mx_data(("ND_" + node_type).GetAsciiString());
		if (mx_node_type.size() == 0) {
			return nullptr;
		}

		XSI::CString xsi_name = replace_letter(replace_letter(replace_letter(xsi_node.GetUniqueName(), '<', '_'), '>', '_'), ',', '_');
		std::string node_output_type = "";
		size_t outputs_count = get_shader_outputs_count(update_context, xsi_node, node_output_type);
		if (outputs_count > 1) {
			node_output_type = multioutput_name();
		}

		MaterialX::NodePtr mx_node = get_or_create_node(update_context, xsi_node.GetObjectID(), mx_doc, mx_node_type, xsi_name.GetAsciiString(), node_output_type, is_new);
		if (is_new) {
			XSI::CParameterRefArray shader_parameters = xsi_node.GetParameters();
			for (size_t i = 0; i < shader_parameters.GetCount(); i++) {
				XSI::ShaderParameter param = shader_parameters[i];
				XSI::ShaderParamDef param_def = param.GetDefinition();

				XSI::siShaderParameterDataType xsi_type = param_def.GetDataType();

				bool is_input = param_def.IsInput();
				bool is_output = param_def.IsOutput();
				std::string parameter_name = param.GetName().GetAsciiString();

				if (is_input) {
					bool is_add = add_input_value_to_node(scene, update_context, mx_node, param, xsi_type);
					if (!is_add) {
						std::string parameter_type = parameter_type_to_string(param, update_context);
						// non-empty paramter connected to something, then add the input port
						if (parameter_type.size() > 0 && is_input_connected(xsi_node, param)) {
							// add simple input by name and type, without setting value
							mx_node->addInput(parameter_name, parameter_type);
							is_add = true;
						}
					}

					if (is_add) {
						propagate_connection(scene, update_context, param, mx_doc, mx_node->getInput(parameter_name));
					}
				}
			}
		}
		return mx_node;
	}
	else {
		log_warning("Unsupported type of the shader node " + xsi_node.GetName() + ", skip it in MaterialX shader.");
	}

	return nullptr;
}

void add_attributes_to_node(ccl::OSLNode* node, const std::string & shader_code) {
	std::vector<std::string> str_literals = extract_string_literals(shader_code);
	// try to add each literal to the force attributes list
	// but consider only supported attributes
	for (size_t i = 0; i < str_literals.size(); i++) {
		std::string literal = str_literals[i];
		if (is_contains(mx_to_cyc_attributes, literal)) {
			// split literal by : and add only the second part
			size_t colon_pos = literal.find(':');
			if (colon_pos != std::string::npos) {
				ccl::ustring attr_name = ccl::ustring(literal.substr(colon_pos + 1));
				node->force_attributes.push_back_slow(ccl::ustring(attr_name));
			}
		}
	}
}

// return true if all exported correcly, false, if we should fallback to general material export
bool sync_materialx_material(ccl::Scene* scene, ccl::ShaderGraph* shader_graph, UpdateContext* update_context, const XSI::Shader& root_node, const XSI::CString& material_name) {
	// we should create MateriaX doc
	// then convert it to osl
	// and create osl node in the graph
	MaterialX::DocumentPtr mx_doc = MaterialX::createDocument();
	update_context->clear_mx_nodes();

	MaterialX::NodePtr mx_root_node = shader_to_node(scene, update_context, mx_doc, root_node);
	if (mx_root_node == nullptr) {
		return false;
	}

	// at first get context, then (if it the first call, it load std library)
	MaterialX::GenContext gen_context(update_context->get_osl_generator());
	// and only then use std library
	mx_doc->setDataLibrary(update_context->get_std_lib());

	MaterialX::TypedElementPtr element(mx_root_node);
	MaterialX::GlslMaterialPtr material = MaterialX::GlslMaterial::create();
	material->setDocument(mx_doc);
	material->setElement(element);
	material->setMaterialNode(mx_root_node);

	try {
		MaterialX::ShaderPtr shader = createShader(element->getNamePath(), gen_context, element);
		const std::string& shader_code = shader->getSourceCode(MaterialX::Stage::PIXEL);
		XSI::CString cache_path = create_texture_cache_path();
		std::string osl_file_path = (cache_path + material_name).GetAsciiString() + std::string(".osl");
		std::string oso_file_path = (cache_path + material_name).GetAsciiString() + std::string(".oso");
		write_text_file(shader_code, osl_file_path);

		// and also try to delete oso-file, because the Cycles use it and does not update when osl is changed
		remove_file(oso_file_path);
		
		// and now we should crete osl shader node inside the graph
		ccl::ShaderManager* manager = scene->shader_manager.get();
		if (manager->use_osl()) {
			// for simplicity we save osl-code into file and pass this file to the manager
			ccl::OSLNode* node = ccl::OSLShaderManager::osl_node(shader_graph, scene, osl_file_path, "", "");

			// register all textures in all devices for osl rendering
			for (auto const& [image_path, image_params] : update_context->get_mx_images()) {
				ccl::ImageHandle handle = scene->image_manager->add_image(image_path, image_params);
				ccl::OSLManager::foreach_osl_device(scene->device, [&](ccl::Device* sub, ccl::OSLGlobals*) {
					if (auto* ss = scene->osl_manager->get_shading_system(sub)) {
						auto* services = static_cast<ccl::OSLRenderServices*>(ss->renderer());
						ccl::OSLUStringHash image_hash = ccl::OSLUStringHash(image_path);
						if (services->textures.find(image_hash) == services->textures.end()) {
							services->textures.erase(image_hash);
						}
						services->textures.insert(image_hash, ccl::OSLTextureHandle(handle));
					}
				});
			}
			
			// next we should extract all required attributes
			add_attributes_to_node(node, shader_code);

			if (node) {
				shader_graph->connect(node->output("out"), shader_graph->output()->input("Surface"));
				return true;
			}
			else {
				return false;
			}
		}
	}
	catch (MaterialX::ExceptionRenderError& e) {
		for (const std::string& error : e.errorLog()) {
			log_warning(("Fail to generate OSL shader for the element " + element->getName()).c_str() + XSI::CString(". Error: ") + error.c_str());
		}
	}
	catch (std::exception& e) {
		log_warning(("Fail to generate shader for the element " + element->getName()).c_str() + XSI::CString(". Error: ") + std::string(e.what()).c_str());
	}

	return false;
}