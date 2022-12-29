#include "cyc_tangent_attribute.h"

// copy from Blender mesh.cpp
void mikk_compute_tangents(ccl::Mesh* mesh, std::string uv_name, bool need_sign)
{
	// create tangent attributes
	const bool is_subd = mesh->get_num_subd_faces();
	ccl::AttributeSet& attributes = is_subd ? mesh->subd_attributes : mesh->attributes;
	ccl::Attribute* attr;
	ccl::ustring name = ccl::ustring((std::string(uv_name) + ".tangent").c_str());
	attr = attributes.add(ccl::ATTR_STD_UV_TANGENT, name);
	
	ccl::float3* tangent = attr->data_float3();
	// create bitangent sign attribute
	float* tangent_sign = NULL;

	if (need_sign)
	{
		ccl::Attribute* attr_sign;
		ccl::ustring name_sign = ccl::ustring((uv_name + ".tangent_sign").c_str());
		attr_sign = attributes.add(ccl::ATTR_STD_UV_TANGENT_SIGN, name_sign);

		tangent_sign = attr_sign->data_float();
	}

	// setup userdata
	if (is_subd) 
	{
		MikkMeshWrapper<true> userdata(ccl::ustring(uv_name), mesh, tangent, tangent_sign);
		// compute tangents
		mikk::Mikktspace(userdata).genTangSpace();
	}
	else 
	{
		MikkMeshWrapper<false> userdata(ccl::ustring(uv_name), mesh, tangent, tangent_sign);
		// compute tangents
		mikk::Mikktspace(userdata).genTangSpace();
	}
}