#include "scene/scene.h"
#include "scene/object.h"
#include "scene/hair.h"
#include "util/hash.h"

#include <xsi_x3dobject.h>
#include <xsi_hairprimitive.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_floatarray.h>
#include <xsi_nurbssurfacemesh.h>
#include <xsi_nurbssurface.h>

#include "../../update_context.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/strings.h"
#include "../cyc_scene.h"
#include "cyc_geometry.h"
#include "cyc_tangent_attribute.h"

void sync_surface_motion_deform(ccl::Mesh* surface, UpdateContext* update_context, const XSI::X3DObject& xsi_object, float u_sample_step, int u_samples, float v_sample_step, int v_samples)
{
	size_t motion_steps = update_context->get_motion_steps();
	surface->set_motion_steps(motion_steps);
	surface->set_use_motion_blur(true);

	size_t attribute_index = 0;
	ccl::Attribute* attr_m_positions = surface->attributes.add(ccl::ATTR_STD_MOTION_VERTEX_POSITION, ccl::ustring("std_motion_position"));
	ccl::Attribute* attr_m_normals = surface->attributes.add(ccl::ATTR_STD_MOTION_VERTEX_NORMAL, ccl::ustring("std_motion_normal"));
	ccl::float3* motion_positions = attr_m_positions->data_float3();
	ccl::float3* motion_normals = attr_m_normals->data_float3();
	MotionSettingsPosition motion_position = update_context->get_motion_position();
	for (size_t mi = 0; mi < motion_steps - 1; mi++)
	{
		size_t time_motion_step = calc_time_motion_step(mi, motion_steps, motion_position);

		float time = update_context->get_motion_time(time_motion_step);

		XSI::Primitive time_primitive(xsi_object.GetActivePrimitive(time));
		XSI::NurbsSurfaceMesh time_surface_geometry = time_primitive.GetGeometry(time);
		XSI::CNurbsSurfaceRefArray time_surfaces = time_surface_geometry.GetSurfaces();
		size_t time_surfaces_count = time_surfaces.GetCount();
		for (size_t i = 0; i < time_surfaces_count; i++) {
			XSI::NurbsSurface time_surface = time_surfaces.GetItem(i);

			for (size_t v = 0; v < v_samples; v++) {
				for (size_t u = 0; u < u_samples; u++) {
					XSI::MATH::CVector3 position;
					XSI::MATH::CVector3 u_tangent;
					XSI::MATH::CVector3 v_tangent;
					XSI::MATH::CVector3 normal;

					time_surface.EvaluateNormalizedPosition(u_sample_step * u, v_sample_step * v, position, u_tangent, v_tangent, normal);
					motion_positions[attribute_index] = vector3_to_float3(position);
					motion_normals[attribute_index] = vector3_to_float3(normal);

					attribute_index++;
				}
			}
		}
	}
}

void sync_surface_geom(ccl::Scene* scene, ccl::Mesh* mesh, UpdateContext* update_context, const XSI::CNurbsSurfaceRefArray& xsi_surfaces, float u_sample_step, int u_samples, float v_sample_step, int v_samples) {
	XSI::CTime eval_time = update_context->get_time();

	ULONG surfaces_count = xsi_surfaces.GetCount();
	size_t vertex_count = surfaces_count * u_samples * v_samples;
	mesh->reserve_mesh(
		vertex_count, // each surface contains the same number of vertices
		surfaces_count * (u_samples - 1) * (v_samples - 1) * 2  // and also the same number of polygons (triangles are x2)
	);

	ccl::Attribute* island_attr = NULL;
	if (mesh->need_attribute(scene, ccl::ATTR_STD_RANDOM_PER_ISLAND)) {
		island_attr = mesh->attributes.add(ccl::ATTR_STD_RANDOM_PER_ISLAND);
	}
	ccl::Attribute* uv_attr = NULL;
	if (mesh->need_attribute(scene, ccl::ATTR_STD_UV)) {
		uv_attr = mesh->attributes.add(ccl::ATTR_STD_UV);
	}

	ccl::Attribute* normal_attr = mesh->attributes.add(ccl::ATTR_STD_VERTEX_NORMAL);
	
	ccl::array<ccl::float3> mesh_vertices(vertex_count);
	size_t vertex_iterator = 0;
	for (size_t i = 0; i < surfaces_count; i++) {
		XSI::NurbsSurface surface = xsi_surfaces.GetItem(i);

		for (size_t v = 0; v < v_samples; v++) {
			for (size_t u = 0; u < u_samples; u++) {
				// get position and normal at the sample point
				XSI::MATH::CVector3 position;
				XSI::MATH::CVector3 u_tangent;
				XSI::MATH::CVector3 v_tangent;
				XSI::MATH::CVector3 normal;

				surface.EvaluateNormalizedPosition(u_sample_step * u, v_sample_step * v, position, u_tangent, v_tangent, normal);
				normal_attr->add(vector3_to_float3(normal));
				mesh_vertices[vertex_iterator] = vector3_to_float3(position);
				vertex_iterator++;
				// define polygon only if u and are not maximal values
				if (u + 1 < u_samples && v + 1 < v_samples) {
					// polygon is span to the vertices
					// u_count* (v + 1) + u│    │u_count * (v + 1) + u + 1
					// 	                  ─┼────┼─
					//                     │    │
					// 	                   │    │
					//   	              ─┼────┼─
					// 	   u_count * v + u │    │ u_count * v + u + 1
					// add vertex index shift for subsurfaces
					size_t v0 = u_samples * v + u + i * u_samples * v_samples;
					size_t v1 = u_samples * v + u + 1 + i * u_samples * v_samples;
					size_t v2 = u_samples * (v + 1) + u + i * u_samples * v_samples;
					size_t v3 = u_samples * (v + 1) + u + 1 + i * u_samples * v_samples;
					mesh->add_triangle(v0, v1, v3, 0, true);
					mesh->add_triangle(v0, v3, v2, 0, true);

					if (uv_attr != NULL) {
						uv_attr->add(ccl::make_float2(u * u_sample_step, v * v_sample_step));
						uv_attr->add(ccl::make_float2((u + 1) * u_sample_step, v * v_sample_step));
						uv_attr->add(ccl::make_float2((u + 1) * u_sample_step, (v + 1) * v_sample_step));

						uv_attr->add(ccl::make_float2(u * u_sample_step, v * v_sample_step));
						uv_attr->add(ccl::make_float2((u + 1) * u_sample_step, (v + 1) * v_sample_step));
						uv_attr->add(ccl::make_float2(u * u_sample_step, (v + 1) * v_sample_step));
					}

					if (island_attr != NULL) {
						island_attr->add(ccl::hash_uint_to_float(i));
						island_attr->add(ccl::hash_uint_to_float(i));
					}
				}
			}
		}
	}
	mesh->set_verts(mesh_vertices);

	// generated attribute
	ccl::Attribute* gen_attr = mesh->attributes.add(ccl::ATTR_STD_GENERATED, ccl::ustring("std_generated"));
	std::memcpy(gen_attr->data_float3(), mesh->get_verts().data(), sizeof(ccl::float3) * mesh->get_verts().size());

	if (uv_attr != NULL) {
		mikk_compute_tangents(mesh, ccl::Attribute::standard_name(ccl::ATTR_STD_UV), true);
	}
}

void sync_surface_geom_process(ccl::Scene* scene, ccl::Mesh* mesh, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, XSI::X3DObject& xsi_object, const XSI::Property& surface_property, bool motion_deform) {
	mesh->name = combine_geometry_name(xsi_object, xsi_primitive).GetAsciiString();

	LONG num_keys = 0;
	bool use_motion_blur = update_context->get_need_motion() && motion_deform;

	XSI::CTime eval_time = update_context->get_time();
	XSI::NurbsSurfaceMesh xsi_surface_geometry = xsi_primitive.GetGeometry(eval_time);
	XSI::CNurbsSurfaceRefArray xsi_surfaces = xsi_surface_geometry.GetSurfaces();

	int u_samples = surface_property.GetParameterValue("surface_u_samples", eval_time);
	int v_samples = surface_property.GetParameterValue("surface_v_samples", eval_time);
	float u_sample_step = 1.0f / (float)u_samples; u_samples = std::max(u_samples, 1) + 1;
	float v_sample_step = 1.0f / (float)v_samples; v_samples = std::max(v_samples, 1) + 1;

	sync_surface_geom(scene, mesh, update_context, xsi_surfaces, u_sample_step, u_samples, v_sample_step, v_samples);
	if (use_motion_blur)
	{
		sync_surface_motion_deform(mesh, update_context, xsi_object, u_sample_step, u_samples, v_sample_step, v_samples);
	}
	else
	{
		mesh->set_use_motion_blur(false);
	}
}

ccl::Mesh* sync_surface_object(ccl::Scene* scene, ccl::Object* surface_object, UpdateContext* update_context, XSI::X3DObject& xsi_object, const XSI::Property& surface_property)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	bool motion_deform = false;
	XSI::CString lightgroup = "";
	sync_geometry_object_parameters(scene, surface_object, xsi_object, lightgroup, motion_deform, "CyclesSurface", render_parameters, eval_time);

	update_context->add_lightgroup(lightgroup);

	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	ULONG xsi_surface_id = xsi_primitive.GetObjectID();
	if (update_context->is_geometry_exists(xsi_surface_id))
	{
		size_t geo_index = update_context->get_geometry_index(xsi_surface_id);
		ccl::Geometry* cyc_geo = scene->geometry[geo_index];
		if (cyc_geo->geometry_type == ccl::Geometry::Type::MESH)
		{
			return static_cast<ccl::Mesh*>(scene->geometry[geo_index]);
		}
	}

	ccl::Mesh* mesh = scene->create_node<ccl::Mesh>();

	XSI::Material xsi_material = xsi_object.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	size_t shader_index = 0;
	if (update_context->is_material_exists(xsi_material_id)){
		shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
	}

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);
	mesh->set_used_shaders(used_shaders);

	sync_surface_geom_process(scene, mesh, update_context, xsi_primitive, xsi_object, surface_property, motion_deform);

	update_context->add_geometry_index(xsi_surface_id, scene->geometry.size() - 1);

	return mesh;
}

XSI::CStatus update_surface(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	// as for curves, we always update geometry of the surface
	// because even if only change the property, we can change samples count and thus the geometry should be rebuild
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::Primitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));

	ULONG xsi_object_id = xsi_object.GetObjectID();

	if (xsi_prim.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesSurface", render_parameters, eval_time, false);
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			XSI::Property surface_prop = get_xsi_object_property(xsi_object, "CycleSurface");

			if (surface_prop.IsValid() && geometry->geometry_type == ccl::Geometry::Type::MESH)
			{
				ccl::Mesh* surface_geom = static_cast<ccl::Mesh*>(geometry);
				surface_geom->clear(true);

				sync_surface_geom_process(scene, surface_geom, update_context, xsi_prim, xsi_object, surface_prop, motion_deform);

				surface_geom->tag_update(scene, true);
			}
			else
			{
				return XSI::CStatus::Abort;
			}
		}
		else
		{
			return XSI::CStatus::Abort;
		}
	}
	else
	{
		return XSI::CStatus::Abort;
	}

	return XSI::CStatus::OK;
}