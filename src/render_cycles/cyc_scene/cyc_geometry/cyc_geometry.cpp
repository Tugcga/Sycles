#include <xsi_property.h>
#include <xsi_arrayparameter.h>
#include <xsi_time.h>

#include "scene/scene.h"
#include "scene/object.h"
#include "scene/geometry.h"
#include "scene/mesh.h"
#include "scene/attribute.h"
#include "scene/hair.h"
#include "util/hash.h"

#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "../../update_context.h"

ccl::PathRayVisibility get_ray_visibility(const XSI::CParameterRefArray &property_params, const XSI::CTime &eval_time)
{
	ccl::PathRayVisibility visibility = ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= bool(property_params.GetValue("ray_visibility_camera", eval_time)) ? ccl::PATH_RAY_VISIBILITY_CAMERA : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= bool(property_params.GetValue("ray_visibility_diffuse", eval_time)) ? ccl::PATH_RAY_VISIBILITY_DIFFUSE : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= bool(property_params.GetValue("ray_visibility_glossy", eval_time)) ? ccl::PATH_RAY_VISIBILITY_GLOSSY : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= bool(property_params.GetValue("ray_visibility_transmission", eval_time)) ? ccl::PATH_RAY_VISIBILITY_TRANSMIT : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= bool(property_params.GetValue("ray_visibility_shadow", eval_time)) ? ccl::PATH_RAY_VISIBILITY_SHADOW : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= bool(property_params.GetValue("ray_visibility_volume_scatter", eval_time)) ? ccl::PATH_RAY_VISIBILITY_VOLUME_SCATTER : ccl::PATH_RAY_VISIBILITY_NONE;
	visibility |= bool(property_params.GetValue("ray_visibility_raycast", eval_time)) ? ccl::PATH_RAY_VISIBILITY_RAYCAST : ccl::PATH_RAY_VISIBILITY_NONE;

	XSI::Parameter is_holdout_param = property_params.GetItem("is_holdout");
	if (is_holdout_param.IsValid())
	{
		if (bool(is_holdout_param.GetValue(eval_time)))
		{
			visibility &= ~ccl::PATH_RAY_VISIBILITY_CAMERA;
		}
	}
	return visibility;
}

void override_curve_shape(ccl:: Scene* scene, ccl::Hair* hair, const XSI::CString &property_name, XSI::X3DObject& xsi_object, const XSI::CTime &eval_time) {
	XSI::Property xsi_property = get_xsi_object_property(xsi_object, property_name);
	if (xsi_property.IsValid()) {
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();
		int curve_override = xsi_params.GetValue("curve_override", eval_time);
		int curve_subdivisions = xsi_params.GetValue("curve_subdivisions", eval_time);

		if (curve_override == 1) {
			hair->curve_shape = ccl::CurveShapeType::CURVE_RIBBON;
		}
		else if (curve_override == 2) {
			hair->curve_shape = ccl::CurveShapeType::CURVE_THICK;
		}
		else if (curve_override == 3) {
			hair->curve_shape = ccl::CurveShapeType::CURVE_THICK_LINEAR;
		}
		else {
			hair->curve_shape = scene->params.hair_shape;
		}
	}
	else {
		hair->curve_shape = scene->params.hair_shape;
	}
}

void sync_geometry_object_parameters(ccl::Scene* scene, ccl::Object* object, XSI::X3DObject &xsi_object, XSI::CString &lightgroup, bool &out_motion_deform, const XSI::CString &property_name, const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time, bool full_update)
{
	// set unique pass id
	bool output_pass_assign_unique_pass_id = render_parameters.GetValue("output_pass_assign_unique_pass_id", eval_time);
	if (output_pass_assign_unique_pass_id)
	{
		object->set_pass_id(scene->objects.size());
	}
	else
	{
		object->set_pass_id(0);
	}

	XSI::Property xsi_property = get_xsi_object_property(xsi_object, property_name);
	bool use_property = xsi_property.IsValid();
	out_motion_deform = false;
	lightgroup = "";
	if (use_property)
	{
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();

		if (!output_pass_assign_unique_pass_id)
		{
			object->set_pass_id(xsi_params.GetValue("pass_id", eval_time));
		}

		out_motion_deform = xsi_params.GetValue("motion_blur_deformation", eval_time);

		object->set_visibility(get_ray_visibility(xsi_params, eval_time));

		object->set_is_shadow_catcher(xsi_params.GetValue("shadow_catcher", eval_time));
		object->set_use_holdout(xsi_params.GetValue("is_holdout", eval_time));
		object->set_shadow_terminator_shading_offset(xsi_params.GetValue("shadow_terminator", eval_time));
		object->set_shadow_terminator_geometry_offset(xsi_params.GetValue("shadow_terminator_geometry", eval_time));
		object->set_ao_distance(xsi_params.GetValue("ao_distance", eval_time));

		object->set_is_caustics_caster(xsi_params.GetValue("caustics_cast", eval_time));
		object->set_is_caustics_receiver(xsi_params.GetValue("caustics_receive", eval_time));

		lightgroup = xsi_params.GetValue("lightgroup", eval_time);
		object->set_lightgroup(ccl::ustring(lightgroup.GetAsciiString()));

		object->tag_visibility_modified();
		object->tag_is_shadow_catcher_modified();
		object->tag_use_holdout_modified();
		object->tag_shadow_terminator_shading_offset_modified();
		object->tag_shadow_terminator_geometry_offset_modified();
		object->tag_ao_distance_modified();
	}

	// next object settings
	if (full_update)
	{
		object->name = xsi_object.GetFullName().GetAsciiString();
		object->set_asset_name(OIIO::ustring(get_asset_name(xsi_object)));
		object->set_color(vector3_to_float3(get_object_color(xsi_object, eval_time)));
		object->set_alpha(1.0);

		XSI::CString to_hash = XSI::CString(object->name.c_str()) + "_" + XSI::CString(scene->objects.size());
		object->set_random_id(ccl::hash_uint2(ccl::hash_string(to_hash.GetAsciiString()), 0));

		object->tag_asset_name_modified();
		object->tag_color_modified();
		object->tag_alpha_modified();
		object->tag_random_id_modified();
	}

	object->tag_pass_id_modified();
	object->tag_update(scene);
}

void sync_vdb_object_parameters(ccl::Scene* scene, ccl::Object* object, XSI::X3DObject& xsi_object, XSI::CString& lightgroup, const XSI::CParameterRefArray& primitive_parameters, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time, bool full_update)
{
	// set unique pass id
	bool output_pass_assign_unique_pass_id = render_parameters.GetValue("output_pass_assign_unique_pass_id", eval_time);
	if (output_pass_assign_unique_pass_id)
	{
		object->set_pass_id(scene->objects.size());
	}
	else
	{
		object->set_pass_id(0);
	}

	if (!output_pass_assign_unique_pass_id)
	{
		XSI::Parameter pass_id_param = primitive_parameters.GetItem("pass_id");
		if (pass_id_param.IsValid())
		{
			object->set_pass_id(pass_id_param.GetValue(eval_time));
		}
	}

	XSI::Parameter ray_visibility_camera_param = primitive_parameters.GetItem("ray_visibility_camera");
	XSI::Parameter ray_visibility_diffuse_param = primitive_parameters.GetItem("ray_visibility_diffuse");
	XSI::Parameter ray_visibility_glossy_param = primitive_parameters.GetItem("ray_visibility_glossy");
	XSI::Parameter ray_visibility_transmission_param = primitive_parameters.GetItem("ray_visibility_transmission");
	XSI::Parameter ray_visibility_shadow_param = primitive_parameters.GetItem("ray_visibility_shadow");
	XSI::Parameter ray_visibility_volume_scatter_param = primitive_parameters.GetItem("ray_visibility_volume_scatter");
	if (ray_visibility_camera_param.IsValid() &&
		ray_visibility_diffuse_param.IsValid() &&
		ray_visibility_glossy_param.IsValid() &&
		ray_visibility_transmission_param.IsValid() &&
		ray_visibility_shadow_param.IsValid() &&
		ray_visibility_volume_scatter_param.IsValid())
	{
		object->set_visibility(get_ray_visibility(primitive_parameters, eval_time));
	}

	XSI::Parameter shadow_catcher_param = primitive_parameters.GetItem("shadow_catcher");
	if (shadow_catcher_param.IsValid())
	{
		object->set_is_shadow_catcher(shadow_catcher_param.GetValue(eval_time));
	}

	XSI::Parameter lightgroup_param = primitive_parameters.GetItem("lightgroup");
	if (lightgroup_param.IsValid())
	{
		lightgroup = lightgroup_param.GetValue(eval_time);
		object->set_lightgroup(ccl::ustring(lightgroup.GetAsciiString()));
	}
	
	object->tag_visibility_modified();
	object->tag_is_shadow_catcher_modified();

	// next object settings
	if (full_update)
	{
		object->name = xsi_object.GetFullName().GetAsciiString();
		object->set_asset_name(OIIO::ustring(get_asset_name(xsi_object)));
		object->set_color(vector3_to_float3(get_object_color(xsi_object, eval_time)));
		object->set_alpha(1.0);

		XSI::CString to_hash = XSI::CString(object->name.c_str()) + "_" + XSI::CString(scene->objects.size());
		object->set_random_id(ccl::hash_uint2(ccl::hash_string(to_hash.GetAsciiString()), 0));

		object->tag_asset_name_modified();
		object->tag_color_modified();
		object->tag_alpha_modified();
		object->tag_random_id_modified();
	}
	
	object->tag_pass_id_modified();
}

void store_positions(ccl::Geometry* geometry, UpdateContext* update_context, ULONG xsi_id) {
	ccl::Attribute* position_attribute = nullptr;
	if (geometry->geometry_type == ccl::Geometry::Type::MESH) {
		ccl::Mesh* mesh_geom = static_cast<ccl::Mesh*>(geometry);
		if (mesh_geom->get_subdivision_type() == ccl::Mesh::SubdivisionType::SUBDIVISION_NONE) {
			position_attribute = mesh_geom->attributes.find(ccl::ATTR_STD_POSITION);
		}
		else {
			// TODO: this function used to store attribute positions
			// and use it under update to restore displacement meshes
			// when we change material with displacement
			// but for subdivided polymeshes the system does not properly work
			// we try to restore data, but the render has artifacts
			// perhaps, no crashes
			// so, skip subdivided meshes
			// position_attribute = mesh_geom->subd_attributes.find(ccl::ATTR_STD_POSITION);
		}
	}
	else {
		position_attribute = geometry->attributes.find(ccl::ATTR_STD_POSITION);
	}
	
	if (position_attribute) {
		// get the number of motion steps
		size_t steps = geometry->get_motion_steps();
		ccl::array<ccl::packed_float3> positions(position_attribute->size * (steps == 0 ? 1 : steps));

		const ccl::packed_float3* data = position_attribute->data_for_write<ccl::packed_float3>();
		std::copy_n(data, position_attribute->size, positions.begin());
		if (steps > 0) {
			for (size_t s = 0; s < steps - 1; s++) {
				const ccl::packed_float3* motion_data = position_attribute->data_for_write<ccl::packed_float3>(s + 1);
				std::copy_n(motion_data, position_attribute->size, positions.begin() + (s + 1) * position_attribute->size);
			}
		}

		update_context->copy_positions(xsi_id, positions);
	}
}

XSI::CStatus reset_on_geometry(ccl::Scene* scene, UpdateContext* update_context, ULONG xsi_id, const ccl::array<ccl::packed_float3>* positions) {
	if (update_context->is_geometry_exists(xsi_id)) {
		size_t geometry_index = update_context->get_geometry_index(xsi_id);
		ccl::Geometry* geometry = scene->geometry[geometry_index];
		ccl::Attribute* attribute = nullptr;

		bool is_subdivide = false;

		if (geometry->geometry_type == ccl::Geometry::Type::MESH) {
			// for meshes we should restore either normal attributes or subdivided attributes
			ccl::Mesh* mesh = static_cast<ccl::Mesh*>(geometry);
			ccl::Mesh::SubdivisionType subdiv_type = mesh->get_subdivision_type();
			if (subdiv_type == ccl::Mesh::SubdivisionType::SUBDIVISION_NONE) {
				attribute = mesh->attributes.find(ccl::ATTR_STD_POSITION);
			}
			else {
				ccl::Mesh* mesh = static_cast<ccl::Mesh*>(geometry);
				attribute = mesh->subd_attributes.find(ccl::ATTR_STD_POSITION);
				is_subdivide = true;
			}
		}
		else {
			// for all other geometries - use always normal attributes
			attribute = geometry->attributes.find(ccl::ATTR_STD_POSITION);
		}

		if (attribute) {
			if (is_subdivide) {
				// for subdivide attribute we also should reset mesh vertex positions
				ccl::Mesh* mesh = static_cast<ccl::Mesh*>(geometry);
				mesh->resize_mesh(attribute->size, 0);
			}
			// and define position attribute
			size_t size = attribute->size;
			// copy non-motion positions
			std::copy_n(positions->data(), size, attribute->data_for_write<ccl::packed_float3>());
			if (positions->size() > size) {
				// array contains motion positions
				size_t steps = positions->size() / size;
				for (size_t s = 0; s < steps - 1; s++) {
					ccl::packed_float3* data = attribute->data_for_write<ccl::packed_float3>(s + 1);
					std::copy_n(positions->data() + size * (s + 1), size, data);
				}
			}

			geometry->tag_position_modified();
			return XSI::CStatus::OK;
		}
		else {
			return XSI::CStatus::Fail;
		}
	}
	else {
		return XSI::CStatus::Fail;
	}
}
