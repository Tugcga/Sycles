#include <tuple>

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
#include <xsi_triangle.h>
#include <xsi_trianglevertex.h>
#include <xsi_point.h>
#include <xsi_vector3.h>

#include "../../update_context.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/strings.h"
#include "../cyc_scene.h"
#include "cyc_geometry.h"
#include "cyc_tangent_attribute.h"

void sync_surface_motion_deform_parametric(ccl::Mesh* surface, UpdateContext* update_context, const XSI::X3DObject& xsi_object, float u_sample_step, int u_samples, float v_sample_step, int v_samples)
{
	size_t motion_steps = update_context->get_motion_steps();
	surface->set_motion_steps(motion_steps);
	surface->set_use_motion_blur(true);

	ccl::Attribute* attr_positions = surface->attributes.add(ccl::ATTR_STD_POSITION);
	ccl::Attribute* attr_normals = surface->attributes.add(ccl::ATTR_STD_VERTEX_NORMAL);

	attr_positions->add_motion(surface);
	attr_normals->add_motion(surface);

	MotionSettingsPosition motion_position = update_context->get_motion_position();
	for (size_t mi = 0; mi < motion_steps - 1; mi++)
	{
		ccl::packed_float3* position_ptr = attr_positions->data_for_write<ccl::packed_float3>(mi + 1);
		ccl::packed_normal* normal_ptr = attr_normals->data_for_write<ccl::packed_normal>(mi + 1);
		size_t attribute_index = 0;

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
					position_ptr[attribute_index] = vector3_to_float3(position);
					normal_ptr[attribute_index] = ccl::packed_normal(vector3_to_float3(normal));

					attribute_index++;
				}
			}
		}
	}
}

void sync_surface_geom_parametric(ccl::Scene* scene, ccl::Mesh* mesh, UpdateContext* update_context, const XSI::CNurbsSurfaceRefArray& xsi_surfaces, float u_sample_step, int u_samples, float v_sample_step, int v_samples) {
	XSI::CTime eval_time = update_context->get_time();

	ULONG surfaces_count = xsi_surfaces.GetCount();
	size_t vertex_count = surfaces_count * u_samples * v_samples;
	mesh->resize_mesh(
		vertex_count, // each surface contains the same number of vertices
		surfaces_count * (u_samples - 1) * (v_samples - 1) * 2  // and also the same number of polygons (triangles are x2)
	);

	ccl::Attribute* attr_position = mesh->attributes.add(ccl::ATTR_STD_POSITION);
	ccl::Attribute* normal_attr = mesh->attributes.add(ccl::ATTR_STD_VERTEX_NORMAL);

	ccl::packed_float3* position_ptr = attr_position->data_for_write<ccl::packed_float3>();
	ccl::packed_normal* normal_ptr = normal_attr->data_for_write<ccl::packed_normal>();

	int* mesh_triangles = mesh->get_triangles().data();
	bool* mesh_smooth = mesh->get_smooth().data();
	int* mesh_shader = mesh->get_shader().data();

	ccl::Attribute* island_attr = NULL;
	if (mesh->need_attribute(scene, ccl::ATTR_STD_RANDOM_PER_ISLAND)) {
		island_attr = mesh->attributes.add(ccl::ATTR_STD_RANDOM_PER_ISLAND);
	}
	ccl::Attribute* uv_attr = NULL;
	if (mesh->need_attribute(scene, ccl::ATTR_STD_UV)) {
		uv_attr = mesh->attributes.add(ccl::ATTR_STD_UV);
	}

	size_t vertex_iterator = 0;
	size_t quad_iterator = 0;
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
				normal_ptr[vertex_iterator] = ccl::packed_normal(vector3_to_float3(normal));
				position_ptr[vertex_iterator] = vector3_to_float3(position);
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
					mesh_triangles[6 * quad_iterator] = v0;
					mesh_triangles[6 * quad_iterator + 1] = v1;
					mesh_triangles[6 * quad_iterator + 2] = v3;
					mesh_triangles[6 * quad_iterator + 3] = v0;
					mesh_triangles[6 * quad_iterator + 4] = v3;
					mesh_triangles[6 * quad_iterator + 5] = v2;

					mesh_shader[2 * quad_iterator] = 0;
					mesh_shader[2 * quad_iterator + 1] = 0;

					mesh_smooth[2 * quad_iterator] = true;
					mesh_smooth[2 * quad_iterator + 1] = true;

					if (uv_attr != NULL) {
						ccl::float2* uv_ptr = uv_attr->data_for_write<ccl::float2>();
						uv_ptr[quad_iterator * 6] = ccl::make_float2(u * u_sample_step, v * v_sample_step);
						uv_ptr[quad_iterator * 6 + 1] = ccl::make_float2((u + 1) * u_sample_step, v * v_sample_step);
						uv_ptr[quad_iterator * 6 + 2] = ccl::make_float2((u + 1) * u_sample_step, (v + 1) * v_sample_step);

						uv_ptr[quad_iterator * 6 + 3] = ccl::make_float2(u * u_sample_step, v * v_sample_step);
						uv_ptr[quad_iterator * 6 + 4] = ccl::make_float2((u + 1) * u_sample_step, (v + 1) * v_sample_step);
						uv_ptr[quad_iterator * 6 + 5] = ccl::make_float2(u * u_sample_step, (v + 1) * v_sample_step);
					}

					if (island_attr != NULL) {
						float* island_ptr = island_attr->data_for_write<float>();
						float v = ccl::hash_uint_to_float(i);
						island_ptr[quad_iterator * 2] = v;
						island_ptr[quad_iterator * 2 + 1] = v;
					}

					quad_iterator++;
				}
			}
		}
	}

	// generated attribute
	ccl::Attribute* gen_attr = mesh->attributes.add(ccl::ATTR_STD_GENERATED, ccl::ustring("std_generated"));
	std::copy_n(mesh->get_position(), mesh->num_verts(), gen_attr->data_for_write<ccl::packed_float3>());
}

// construct map with required information about triangulation
// return tuple with three arrays for position, normal and uv attributes
std::tuple<std::vector<ccl::packed_float3>, std::vector<ccl::packed_normal>, std::vector<ccl::float2>> surface_to_triangulation(const XSI::NurbsSurfaceMesh& xsi_surface_geometry, bool fill_uvs) {
	// at first we should define the number of vertices
	size_t max_index = 0;
	XSI::CTriangleRefArray triangles = xsi_surface_geometry.GetTriangles();
	XSI::CLongArray indices = triangles.GetIndexArray();
	for (size_t i = 0; i < indices.GetCount(); i++) {
		LONG v = indices[i];
		if (v > max_index) {
			max_index = v;
		}
	}
	size_t vertex_count = max_index + 1;
	std::vector<ccl::packed_float3> positions(vertex_count);
	std::vector<ccl::packed_normal> normals(vertex_count);
	std::vector<ccl::float2> uvs(fill_uvs ? vertex_count : 0);
	
	LONG buffer_surface_index = 0;
	double buffer_square_distance = 0.0;
	double buffer_u = 0.0;
	double buffer_v = 0.0;
	XSI::MATH::CVector3 buffer_closed_position;

	for (size_t t_index = 0; t_index < triangles.GetCount(); t_index++) {
		XSI::Triangle triangle(triangles[t_index]);

		XSI::CLongArray triangle_indices = triangle.GetIndexArray();
		XSI::MATH::CVector3Array triangle_positions = triangle.GetPositionArray();
		XSI::MATH::CVector3Array triangle_normals = triangle.GetPolygonNodeNormalArray();

		for (size_t i = 0; i < triangle_indices.GetCount(); i++) {
			LONG index = triangle_indices[i];
			XSI::MATH::CVector3 position = triangle_positions[i];
			XSI::MATH::CVector3 normal = triangle_normals[i];

			positions[index] = ccl::packed_float3(vector3_to_float3(position));
			normals[index] = ccl::packed_normal(vector3_to_float3(normal));

			if (fill_uvs) {
				xsi_surface_geometry.GetClosestSurfacePosition(position, buffer_surface_index, buffer_square_distance, buffer_u, buffer_v, buffer_closed_position);
				uvs[index] = ccl::make_float2(buffer_u, buffer_v);
			}
		}
	}
	return std::make_tuple(positions, normals, uvs);
}

void sync_surface_motion_deform_approximation(ccl::Mesh* surface, UpdateContext* update_context, const XSI::X3DObject& xsi_object) {
	size_t motion_steps = update_context->get_motion_steps();
	surface->set_motion_steps(motion_steps);
	surface->set_use_motion_blur(true);

	ccl::Attribute* attr_positions = surface->attributes.add(ccl::ATTR_STD_POSITION);
	ccl::Attribute* attr_normals = surface->attributes.add(ccl::ATTR_STD_VERTEX_NORMAL);

	attr_positions->add_motion(surface);
	attr_normals->add_motion(surface);

	size_t positions_size = attr_positions->size;
	size_t normals_size = attr_normals->size;

	MotionSettingsPosition motion_position = update_context->get_motion_position();
	for (size_t mi = 0; mi < motion_steps - 1; mi++)
	{
		ccl::packed_float3* position_ptr = attr_positions->data_for_write<ccl::packed_float3>(mi + 1);
		ccl::packed_normal* normal_ptr = attr_normals->data_for_write<ccl::packed_normal>(mi + 1);

		size_t time_motion_step = calc_time_motion_step(mi, motion_steps, motion_position);
		float time = update_context->get_motion_time(time_motion_step);

		XSI::Primitive time_primitive(xsi_object.GetActivePrimitive(time));
		XSI::NurbsSurfaceMesh time_surface_geometry = time_primitive.GetGeometry(time);
		auto [positions, normals, uvs] = surface_to_triangulation(time_surface_geometry, false);

		if (positions.size() != positions_size || normals.size() != normals_size) {
			log_warning("Surface " + xsi_object.GetName() + " has invalid number of vertices at frame " + XSI::CString(time) + ". Render can contains artifacts.");
			if (positions.size() < positions_size) {
				// we should extend the array
				positions.resize(positions_size);
			}
			if (normals.size() < normals_size) {
				normals.resize(normals_size);
			}
		}

		std::copy_n(positions.data(), positions_size, position_ptr);
		std::copy_n(normals.data(), normals_size, normal_ptr);
	}
}

void sync_surface_geom_approximation(ccl::Scene* scene, ccl::Mesh* mesh, UpdateContext* update_context, const XSI::NurbsSurfaceMesh &xsi_surface_geometry) {
	// before we start create the mesh, we should geather triangulation information
	// vertex positions, normals, uvs, and also indices of each triangle
	bool need_uv = mesh->need_attribute(scene, ccl::ATTR_STD_UV);
	auto [positions, normals, uvs] = surface_to_triangulation(xsi_surface_geometry, need_uv);

	XSI::CTriangleRefArray triangles = xsi_surface_geometry.GetTriangles();
	size_t triangles_count = triangles.GetCount();
	mesh->resize_mesh(positions.size(), triangles_count);

	ccl::Attribute* attr_position = mesh->attributes.add(ccl::ATTR_STD_POSITION);
	ccl::Attribute* attr_normal = mesh->attributes.add(ccl::ATTR_STD_VERTEX_NORMAL);

	ccl::packed_float3* position_ptr = attr_position->data_for_write<ccl::packed_float3>();
	std::copy_n(positions.data(), positions.size(), position_ptr);
	ccl::packed_normal* normal_ptr = attr_normal->data_for_write<ccl::packed_normal>();
	std::copy_n(normals.data(), normals.size(), normal_ptr);

	// TODO: how to define island attribute?
	// if the surface constans from different patches, how it's possible to obtina oatch index for each triangle
	ccl::Attribute* attr_uv = NULL;
	if (need_uv) {
		attr_uv = mesh->attributes.add(ccl::ATTR_STD_UV);
	}

	int* mesh_triangles = mesh->get_triangles().data();
	bool* mesh_smooth = mesh->get_smooth().data();
	int* mesh_shader = mesh->get_shader().data();
	for (size_t triangle_index = 0; triangle_index < triangles_count; triangle_index++) {
		XSI::Triangle triangle(triangles[triangle_index]);
		XSI::CLongArray triangle_indices = triangle.GetIndexArray();
		mesh_triangles[3 * triangle_index] = triangle_indices[0];
		mesh_triangles[3 * triangle_index + 1] = triangle_indices[1];
		mesh_triangles[3 * triangle_index + 2] = triangle_indices[2];

		mesh_smooth[triangle_index] = true;
		mesh_shader[triangle_index] = 0;

		if (need_uv && attr_uv) {
			ccl::float2* uv_ptr = attr_uv->data_for_write<ccl::float2>();
			uv_ptr[3 * triangle_index] = uvs[triangle_indices[0]];
			uv_ptr[3 * triangle_index + 1] = uvs[triangle_indices[1]];
			uv_ptr[3 * triangle_index + 2] = uvs[triangle_indices[2]];
		}
	}
}

void sync_surface_geom_process(ccl::Scene* scene, ccl::Mesh* mesh, UpdateContext* update_context, const XSI::Primitive& xsi_primitive, XSI::X3DObject& xsi_object, const XSI::Property& surface_property, bool motion_deform) {
	mesh->name = combine_geometry_name(xsi_object, xsi_primitive).GetAsciiString();

	bool use_motion_blur = update_context->get_need_motion() && motion_deform;

	XSI::CTime eval_time = update_context->get_time();
	XSI::NurbsSurfaceMesh xsi_surface_geometry = xsi_primitive.GetGeometry(eval_time);

	int trianglulation_type = surface_property.GetParameterValue("surface_triangulation_type", eval_time);
	if (trianglulation_type == 0) {
		// parametric mode
		int u_samples = surface_property.GetParameterValue("surface_u_samples", eval_time);
		int v_samples = surface_property.GetParameterValue("surface_v_samples", eval_time);
		float u_sample_step = 1.0f / (float)u_samples; u_samples = std::max(u_samples, 1) + 1;
		float v_sample_step = 1.0f / (float)v_samples; v_samples = std::max(v_samples, 1) + 1;

		XSI::CNurbsSurfaceRefArray xsi_surfaces = xsi_surface_geometry.GetSurfaces();
		sync_surface_geom_parametric(scene, mesh, update_context, xsi_surfaces, u_sample_step, u_samples, v_sample_step, v_samples);
		if (use_motion_blur) {
			sync_surface_motion_deform_parametric(mesh, update_context, xsi_object, u_sample_step, u_samples, v_sample_step, v_samples);
		}
		else {
			mesh->set_use_motion_blur(false);
		}
	}
	else {
		// geometry approximation mode
		sync_surface_geom_approximation(scene, mesh, update_context, xsi_surface_geometry);
		if (use_motion_blur) {
			sync_surface_motion_deform_approximation(mesh, update_context, xsi_object);
		}
		else {
			mesh->set_use_motion_blur(false);
		}
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

	if (scene->shaders[shader_index]->has_displacement) {
		store_positions(mesh, update_context, xsi_object.GetObjectID());
	}

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