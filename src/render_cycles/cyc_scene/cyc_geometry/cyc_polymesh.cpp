#include "scene/mesh.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/hair.h"
#include "util/color.h"
#include "util/disjoint_set.h"
#include "util/hash.h"

#include <xsi_x3dobject.h>
#include <xsi_primitive.h>
#include <xsi_geometry.h>
#include <xsi_geometryaccessor.h>
#include <xsi_polygonmesh.h>
#include <xsi_vertex.h>
#include <xsi_polygonnode.h>
#include <xsi_material.h>
#include <xsi_polygonface.h>
#include <xsi_edge.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include <vector>

#include "../../update_context.h"
#include "cyc_geometry.h"
#include "cyc_polymesh_attributes.h"
#include "../cyc_scene.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/math.h"
#include "../../../utilities/strings.h"
#include "../../../render_base/type_enums.h"
#include "../primitives_geometry.h"

ccl::Mesh* build_primitive(ccl::Scene* scene, int vertex_count, float* vertices, int faces_count, int* face_sizes, int* face_indexes, bool smooth)
{
	ccl::Mesh* mesh = scene->create_node<ccl::Mesh>();

	ccl::array<ccl::float3> vertex_coordinates;
	for (size_t i = 0; i < vertex_count * 3; i += 3)
	{
		vertex_coordinates.push_back_slow(ccl::make_float3(vertices[i + 0], vertices[i + 1], vertices[i + 2]));
	}

	size_t num_triangles = 0;
	for (size_t i = 0; i < faces_count; i++)
	{
		num_triangles += face_sizes[i] - 2;
	}
	mesh->resize_mesh(vertex_coordinates.size(), num_triangles);
	ccl::Attribute* attr_position = mesh->attributes.add(ccl::ATTR_STD_POSITION);
	attr_position->resize(vertex_coordinates.size());
	ccl::packed_float3* verts = attr_position->data_for_write<ccl::packed_float3>();
	for (size_t i = 0; i < vertex_coordinates.size(); i++) {
		verts[i] = vertex_coordinates[i];
	}

	// create triangles
	int index_offset = 0;

	int* data_triangles = mesh->get_triangles().data();
	bool* data_smooth = mesh->get_smooth().data();
	int* data_shader = mesh->get_shader().data();

	size_t counter = 0;

	for (size_t i = 0; i < faces_count; i++)  // iterate over polygons
	{
		for (int j = 0; j < face_sizes[i] - 2; j++)  // for each polygon by n-2
		{
			int v0 = face_indexes[index_offset];
			int v1 = face_indexes[index_offset + j + 1];
			int v2 = face_indexes[index_offset + j + 2];
			data_triangles[3 * counter] = v0;
			data_triangles[3 * counter + 1] = v1;
			data_triangles[3 * counter + 2] = v2;
			data_smooth[counter] = smooth;
			data_shader[counter] = 0;

			counter += 1;
		}

		index_offset += face_sizes[i];
	}

	return mesh;
}

ccl::Mesh* build_primitive(ccl::Scene* scene, XSI::siICEShapeType shape_type)
{
	if (shape_type == XSI::siICEShapeDisc)
	{
		return build_primitive(scene, disc_vertex_count, disc_vertices, disc_faces_count, disc_face_sizes, disc_face_indexes);
	}
	else if (shape_type == XSI::siICEShapeRectangle)
	{
		return build_primitive(scene, plane_vertex_count, plane_vertices, plane_faces_count, plane_face_sizes, plane_face_indexes);
	}
	else if (shape_type == XSI::siICEShapeBox)
	{
		return build_primitive(scene, cube_vertex_count, cube_vertices, cube_faces_count, cube_face_sizes, cube_face_indexes);
	}
	else if (shape_type == XSI::siICEShapeCylinder)
	{
		return build_primitive(scene, cylinder_vertex_count, cylinder_vertices, cylinder_faces_count, cylinder_face_sizes, cylinder_face_indexes);
	}
	else if (shape_type == XSI::siICEShapeCone)
	{
		return build_primitive(scene, cone_vertex_count, cone_vertices, cone_faces_count, cone_face_sizes, cone_face_indexes);
	}
	else
	{// sphere for all other shapes
		return build_primitive(scene, sphere_vertex_count, sphere_vertices, sphere_faces_count, sphere_face_sizes, sphere_face_indexes, true);
	}
}

// get from SItoA
void get_geo_accessor_normals(const XSI::CGeometryAccessor &in_geo_acc, LONG in_normal_indices_size, XSI::CFloatArray &out_node_normals)
{
	XSI::CRefArray user_normals_refs = in_geo_acc.GetUserNormals();
	if (user_normals_refs.GetCount() <= 0)
	{
		in_geo_acc.GetNodeNormals(out_node_normals);
	}
	else
	{
		// there are user normals available... we simply take the first user normals in the ref array
		XSI::ClusterProperty cluster_prop(user_normals_refs[0]);
		// get the cluster property element array
		XSI::CClusterPropertyElementArray cluster_prop_elements = cluster_prop.GetElements();

		const LONG cluster_element_count = cluster_prop_elements.GetCount();
		if (cluster_element_count <= in_normal_indices_size)
		{
			cluster_prop.GetValues(out_node_normals);
		}
		else
		{
			// we do not have a matching count, so we need to get the user normals "on foot", 
			// because clusterProp.GetValues(nodeNormals) would crash Softimage
			// resize the array of floats
			out_node_normals.Resize(in_normal_indices_size * 3);
			// get them
			XSI::CDoubleArray tmp;
			float* nrm = (float*)out_node_normals.GetArray();
			for (LONG i = 0; i < in_normal_indices_size; i++, nrm += 3)
			{
				tmp = cluster_prop_elements.GetItem(i);
				nrm[0] = float(tmp[0]);
				nrm[1] = float(tmp[1]);
				nrm[2] = float(tmp[2]);
			}
		}
	}
}

void sync_polymesh_motion_deform(ccl::Mesh* mesh, UpdateContext* update_context, const XSI::X3DObject &xsi_object, SubdivideMode subdiv_mode, bool geo_use_angle, float geo_angle)
{
	// TODO: implement proper motion deform
	/*
	size_t motion_steps = update_context->get_motion_steps();
	
	// check we can add motion blur
	// the number of vertices should be the same in all steps
	bool meshes_correct = true;
	size_t original_vertices = mesh->num_verts();

	for (size_t i = 0; i < motion_steps; i++)
	{
		float time = update_context->get_motion_time(i);
		XSI::PolygonMesh xsi_time_mesh = xsi_object.GetActivePrimitive(time).GetGeometry(time, XSI::siConstructionModeSecondaryShape);
		XSI::CGeometryAccessor xsi_time_acc = xsi_time_mesh.GetGeometryAccessor(XSI::siConstructionModeSecondaryShape, XSI::siCatmullClark, 0, false, geo_use_angle, geo_angle);
		size_t time_vertices = subdiv_mode == SubdivideMode_CatmulClark ? xsi_time_acc.GetVertexCount() : xsi_time_acc.GetNodeCount();

		if (time_vertices != original_vertices)
		{
			meshes_correct = false;
			log_warning("Mesh object " + XSI::CString(mesh->name.c_str()) + " has invalid number of vertices at frame " + XSI::CString(time) + ". Disabling motion blur for it.");
			break;
		}
	}

	if (meshes_correct)
	{
		mesh->set_motion_steps(motion_steps);

		ccl::vector<ccl::float3> positions_buffer;
		ccl::vector<ccl::float3> normals_buffer;
		positions_buffer.resize(original_vertices);
		normals_buffer.resize(original_vertices);

		// create motion attributes
		ccl::AttributeSet& attributes = subdiv_mode != SubdivideMode_None ? mesh->subd_attributes : mesh->attributes;

		ccl::Attribute* attr_m_positions = attributes.add(ccl::ATTR_STD_MOTION_VERTEX_POSITION, ccl::ustring("std_motion_vertex_position"));
		ccl::Attribute* attr_m_normals = attributes.add(ccl::ATTR_STD_MOTION_VERTEX_NORMAL, ccl::ustring("std_motion_vertex_normal"));

		// the number of steps is equal to toatl steps - 1
		// does not set the step for center
		MotionSettingsPosition motion_position = update_context->get_motion_position();
		for (size_t mi = 0; mi < motion_steps - 1; mi++)
		{
			size_t time_motion_step = calc_time_motion_step(mi, motion_steps, motion_position);

			float time = update_context->get_motion_time(time_motion_step);

			XSI::PolygonMesh xsi_time_mesh = xsi_object.GetActivePrimitive(time).GetGeometry(time, XSI::siConstructionModeSecondaryShape);
			XSI::CGeometryAccessor xsi_time_acc = xsi_time_mesh.GetGeometryAccessor(XSI::siConstructionModeSecondaryShape, XSI::siCatmullClark, 0, false, geo_use_angle, geo_angle);
			
			XSI::CVertexRefArray vertices = xsi_time_mesh.GetVertices();
			XSI::CPolygonNodeRefArray nodes = xsi_time_mesh.GetNodes();

			size_t vertex_count = xsi_time_acc.GetVertexCount();
			size_t nodes_count = xsi_time_acc.GetNodeCount();
			for (size_t v_index = 0; v_index < vertex_count; v_index++)
			{
				XSI::Vertex vertex = vertices[v_index];
				XSI::MATH::CVector3 vertex_position = vertex.GetPosition();
				bool is_valid = true;
				XSI::MATH::CVector3 vertex_normal = vertex.GetNormal(is_valid);
				ccl::float3 position = vector3_to_float3(vertex_position);
				ccl::float3 normal = vector3_to_float3(vertex_normal);
				if (subdiv_mode == SubdivideMode_CatmulClark)
				{
					positions_buffer[vertex.GetIndex()] = position;
					normals_buffer[vertex.GetIndex()] = normal;
				}
				else
				{
					XSI::CPolygonNodeRefArray vertex_nodes = vertex.GetNodes();
					size_t vertex_nodes_count = vertex_nodes.GetCount();
					for (size_t node_index = 0; node_index < vertex_nodes_count; node_index++)
					{
						XSI::PolygonNode node(vertex_nodes[node_index]);
						positions_buffer[node.GetIndex()] = position;
						// we will set normals later
					}
				}
			}

			// for trianglular mesh and linear subdivision get normals from nodes
			if (subdiv_mode != SubdivideMode_CatmulClark)
			{
				XSI::CFloatArray node_normals;
				get_geo_accessor_normals(xsi_time_acc, nodes_count, node_normals);
				for (size_t ni = 0; ni < original_vertices; ni++)
				{
					normals_buffer[ni] = ccl::make_float3(node_normals[3 * ni], node_normals[3 * ni + 1], node_normals[3 * ni + 2]);
				}
			}

			memcpy(attr_m_positions->data_float3() + mi * original_vertices, &positions_buffer[0], sizeof(float3) * original_vertices);
			memcpy(attr_m_normals->data_float3() + mi * original_vertices, &normals_buffer[0], sizeof(float3)* original_vertices);
		}

		mesh->set_use_motion_blur(true);
		mesh->tag_motion_steps_modified();
		mesh->tag_use_motion_blur_modified();

		// clear buffers
		positions_buffer.clear();
		positions_buffer.shrink_to_fit();
		normals_buffer.clear();
		normals_buffer.shrink_to_fit();
	}
	else
	{
		mesh->set_use_motion_blur(false);
	}*/
}

std::vector<LONG> build_node_to_vertex_map(const XSI::CGeometryAccessor& geometry, size_t nodes_count) {
	XSI::CLongArray triangle_nodes;
	XSI::CLongArray triangle_vertices;
	geometry.GetTriangleNodeIndices(triangle_nodes);
	geometry.GetTriangleVertexIndices(triangle_vertices);

	LONG triangles_count = geometry.GetTriangleCount();
	LONG tri_indices_count = triangles_count * 3;

	std::vector<LONG> xsi_node_to_vertex(nodes_count, 0);
	LONG samples_count = triangle_nodes.GetCount();

	LONG* raw_tri_nodes = (LONG*)triangle_nodes.GetArray();
	LONG* raw_tri_verts = (LONG*)triangle_vertices.GetArray();

	for (LONG i = 0; i < samples_count; i++) {
		xsi_node_to_vertex[raw_tri_nodes[i]] = raw_tri_verts[i];
	}

	return xsi_node_to_vertex;
}

void sync_triangle_mesh(ccl::Scene* scene, ccl::Mesh* mesh, const XSI::CGeometryAccessor &xsi_geo_acc, const XSI::PolygonMesh &xsi_polymesh)
{
	XSI::CLongArray xsi_polygon_material_indices;
	xsi_geo_acc.GetPolygonMaterialIndices(xsi_polygon_material_indices);

	// read geometry data
	XSI::CLongArray triangle_nodes;
	XSI::CDoubleArray vertex_positions;
	XSI::CLongArray polygon_materials;  // index in the large material list (with repetitions) for each polygon
	XSI::CLongArray triangle_polygons;  // polygon index for each triangle
	LONG triangles_count = xsi_geo_acc.GetTriangleCount();
	LONG nodes_count = xsi_geo_acc.GetNodeCount();
	ULONG vertex_count = xsi_geo_acc.GetVertexCount();
	xsi_geo_acc.GetTriangleNodeIndices(triangle_nodes);
	xsi_geo_acc.GetVertexPositions(vertex_positions);
	xsi_geo_acc.GetPolygonMaterialIndices(polygon_materials);
	xsi_geo_acc.GetPolygonTriangleIndices(triangle_polygons);

	// vertex positions are positions of vertices, but we need nodes
	// so, we should construct a map from vertex index to node index
	// and then iterate throw nodes and use corresponding vertice indices

	// use simple array as map
	// index - node index, value - corresponding vertex index
	std::vector<LONG> xsi_node_to_vertex = build_node_to_vertex_map(xsi_geo_acc, nodes_count);

	// for triagle mesh vertices are xsi nodes
	mesh->resize_mesh(nodes_count, triangles_count);

	// form vertices array
	ccl::array<ccl::packed_float3> mesh_vertices(nodes_count);
	for (LONG i = 0; i < nodes_count; i++) {
		LONG v_index = xsi_node_to_vertex[i];
		mesh_vertices[i] = ccl::make_float3(vertex_positions[3*v_index], vertex_positions[3 * v_index + 1], vertex_positions[3 * v_index + 2]);
	}

	// set mesh vertices
	ccl::Attribute* attr_position = mesh->attributes.add(ccl::ATTR_STD_POSITION);
	attr_position->resize(mesh_vertices.size());
	ccl::packed_float3* verts = attr_position->data_for_write<ccl::packed_float3>();
	std::copy_n(mesh_vertices.data(), mesh_vertices.size(), attr_position->data_for_write<ccl::packed_float3>());
	mesh->tag_position_modified();

	// next triangles
	int* mesh_triangles = mesh->get_triangles().data();
	bool* mesh_smooth = mesh->get_smooth().data();
	int* mesh_shader = mesh->get_shader().data();
	for (size_t i = 0; i < triangles_count; i++) {
		LONG n0 = triangle_nodes[3 * i];
		LONG n1 = triangle_nodes[3 * i + 1];
		LONG n2 = triangle_nodes[3 * i + 2];

		LONG material_index = polygon_materials[triangle_polygons[i]];

		mesh_triangles[3 * i] = n0;
		mesh_triangles[3 * i + 1] = n1;
		mesh_triangles[3 * i + 2] = n2;

		mesh_smooth[i] = true;
		mesh_shader[i] = material_index;
	}

	// normals
	XSI::CFloatArray node_normals;
	xsi_geo_acc.GetNodeNormals(node_normals);
	get_geo_accessor_normals(xsi_geo_acc, nodes_count, node_normals);

	ccl::AttributeSet& attributes = mesh->attributes;
	ccl::Attribute* attr_n = attributes.add(ccl::ATTR_STD_VERTEX_NORMAL, ccl::ustring("std_normal"));
	ccl::packed_normal* normal_data = attr_n->data_for_write<ccl::packed_normal>();
	for (size_t node_index = 0; node_index < nodes_count; node_index++)
	{
		*normal_data = ccl::make_float3(node_normals[3 * node_index], node_normals[3 * node_index + 1], node_normals[3 * node_index + 2]);
		normal_data++;
	}

	// generated attribute
	ccl::Attribute* gen_attr = attributes.add(ccl::ATTR_STD_GENERATED, ccl::ustring("std_generated"));
	std::copy_n(mesh->get_position(), mesh->num_verts(), gen_attr->data_for_write<ccl::packed_float3>());

	// use common method for export attrbutes
	XSI::CPolygonFaceRefArray faces;
	sync_mesh_attribute_vertex_color(scene, mesh, attributes, xsi_geo_acc, SubdivideMode_None, triangle_nodes, faces);
	sync_mesh_attribute_random_per_island(scene, mesh, attributes, SubdivideMode_None, nodes_count, triangles_count, triangle_nodes, xsi_polymesh, faces);
	if (mesh->need_attribute(scene, ccl::ATTR_STD_POINTINESS)) {
		// use slow vertices structs
		XSI::CVertexRefArray xsi_vertices = xsi_polymesh.GetVertices();
		sync_mesh_attribute_pointness(scene, mesh, SubdivideMode_None, vertex_count, nodes_count, xsi_vertices, node_normals, xsi_polymesh);
	}
	
	// uvs
	XSI::CRefArray uv_refs = xsi_geo_acc.GetUVs();
	// export first uv as default uv attribute
	sync_mesh_uvs(mesh, SubdivideMode_None, triangles_count, nodes_count, uv_refs, faces, triangle_nodes);
	// we does no need to create custom tangents, because Cycles make it himself
	sync_ice_attributes(scene, mesh, xsi_polymesh, SubdivideMode_None, vertex_count, nodes_count, xsi_node_to_vertex);
}

void sync_subdivide_mesh(ccl::Scene* scene, ccl::Mesh* mesh, const XSI::CGeometryAccessor& xsi_geo_acc, const XSI::PolygonMesh& xsi_polymesh, SubdivideMode subdiv_mode, ULONG subdiv_level, SubdivideSpace subdiv_space, float subdiv_size, const XSI::MATH::CMatrix4& xsi_matrix)
{
	XSI::CLongArray xsi_polygon_material_indices;
	xsi_geo_acc.GetPolygonMaterialIndices(xsi_polygon_material_indices);

	XSI::CDoubleArray vertex_positions;
	ULONG vertex_count = xsi_geo_acc.GetVertexCount();
	ULONG nodes_count = xsi_geo_acc.GetNodeCount();
	xsi_geo_acc.GetVertexPositions(vertex_positions);

	XSI::CVertexRefArray xsi_vertices = xsi_polymesh.GetVertices();
	XSI::CPolygonFaceRefArray xsi_faces = xsi_polymesh.GetPolygons();
	XSI::CLongArray polygon_sizes;
	xsi_geo_acc.GetPolygonVerticesCount(polygon_sizes);
	XSI::CLongArray node_indices;
	XSI::CFloatArray node_normals;  // define array here, but fill it later, if we need this
	std::vector<LONG> xsi_node_to_vertex = build_node_to_vertex_map(xsi_geo_acc, nodes_count);
	
	size_t polygons_count = polygon_sizes.GetCount();

	// it define the size of attributes.ATTR_STD_POSITION to be the first argument
	mesh->resize_mesh(subdiv_mode == SubdivideMode_CatmulClark ? vertex_count : nodes_count, 0);
	int num_corners = 0;
	for (size_t i = 0; i < polygon_sizes.GetCount(); i++) {
		num_corners += polygon_sizes[i];
	}
	mesh->resize_subd_faces(polygons_count, num_corners);

	ccl::array<ccl::packed_float3> mesh_vertices(subdiv_mode == SubdivideMode_CatmulClark ? vertex_count : nodes_count);
	ccl::array<ccl::packed_normal> mesh_normals(subdiv_mode == SubdivideMode_CatmulClark ? vertex_count : nodes_count);

	// these arrays have the length polygons_count
	int* subd_start_corner = mesh->get_subd_start_corner().data();
	int* subd_num_corners = mesh->get_subd_num_corners().data();
	int* subd_ptex_offset = mesh->get_subd_ptex_offset().data();
	int* subd_shader = mesh->get_subd_shader().data();
	bool* subd_smooth = mesh->get_subd_smooth().data();

	// and this array - num_corners
	int* subd_face_corners = mesh->get_subd_face_corners().data();

	if (subdiv_mode == SubdivideMode_CatmulClark) {
		// for Catmul-Clark subdivision we use mesh vertices
		for (size_t v_index = 0; v_index < vertex_count; v_index++) {
			XSI::Vertex vertex = xsi_vertices[v_index];
			XSI::MATH::CVector3 vertex_position = vertex.GetPosition();
			ccl::float3 position = ccl::make_float3(vertex_position.GetX(), vertex_position.GetY(), vertex_position.GetZ());
			bool is_valid = true;
			XSI::MATH::CVector3 normal = vertex.GetNormal(is_valid);
			mesh_vertices[v_index] = position;
			mesh_normals[v_index] = ccl::packed_normal(vector3_to_float3(normal));
		}

		// assign mesh faces
		size_t faces_count = xsi_faces.GetCount();
		size_t corner_counter = 0;
		for (size_t face_index = 0; face_index < faces_count; face_index++)
		{
			subd_start_corner[0] = corner_counter; subd_start_corner++;
			
			XSI::PolygonFace face(xsi_faces[face_index]);
			XSI::CVertexRefArray face_vertices = face.GetVertices();
			size_t face_vertex_count = face_vertices.GetCount();

			subd_num_corners[0] = face_vertex_count; subd_num_corners++;
			subd_ptex_offset[0] = face_index; subd_ptex_offset++;
			subd_shader[0] = xsi_polygon_material_indices[face_index]; subd_shader++;
			subd_smooth[0] = true; subd_smooth++;
			for (size_t v = 0; v < face_vertex_count; v++)
			{
				XSI::Vertex vert(face_vertices[v]);
				subd_face_corners[0] = vert.GetIndex(); subd_face_corners++;
				corner_counter++;
			}
		}
	} 
	else {
		// for linear subdivision we should use nodes instead of vertices
		// here we should geather from geometry as node positions and node normals
		for (LONG i = 0; i < nodes_count; i++)
		{
			LONG v_index = xsi_node_to_vertex[i];
			mesh_vertices[i] = ccl::make_float3(vertex_positions[3 * v_index], vertex_positions[3 * v_index + 1], vertex_positions[3 * v_index + 2]);
		}
		xsi_geo_acc.GetNodeNormals(node_normals);
		xsi_geo_acc.GetNodeIndices(node_indices);
		get_geo_accessor_normals(xsi_geo_acc, nodes_count, node_normals);
		LONG node_iterator = 0;
		for (LONG i = 0; i < polygons_count; i++) {
			LONG poly_size = polygon_sizes[i];
			subd_start_corner[0] = node_iterator; subd_start_corner++;
			subd_num_corners[0] = poly_size; subd_num_corners++;
			subd_ptex_offset[0] = i; subd_ptex_offset++;
			subd_shader[0] = xsi_polygon_material_indices[i]; subd_shader++;
			subd_smooth[0] = false; subd_smooth++;
			
			for (LONG j = 0; j < poly_size; j++) {
				LONG node_index = node_indices[node_iterator];
				// define node normal
				mesh_normals[node_iterator] = ccl::packed_normal(ccl::make_float3(node_normals[3 * node_index], node_normals[3 * node_index + 1], node_normals[3 * node_index + 2]));
				subd_face_corners[0] = node_index; subd_face_corners++;
				node_iterator++;
			}
		}
	}
	ccl::AttributeSet& attributes = mesh->subd_attributes;

	// set vertex positions
	ccl::Attribute* subd_attr_pos = attributes.add(ccl::ATTR_STD_POSITION);
	subd_attr_pos->resize(mesh_vertices.size());
	std::copy_n(mesh_vertices.data(), mesh_vertices.size(), subd_attr_pos->data_for_write<ccl::packed_float3>());

	// normals
	ccl::Attribute* subd_attr_n = attributes.add(ccl::ATTR_STD_VERTEX_NORMAL, ccl::ustring("std_normal"));
	std::copy_n(mesh_normals.data(), mesh_normals.size(), subd_attr_n->data_for_write<ccl::packed_normal>());

	ccl::Attribute* gen_subd_attr = attributes.add(ccl::ATTR_STD_GENERATED, ccl::ustring("std_generated"));
	std::copy_n(&mesh_vertices[0], mesh->num_verts(), gen_subd_attr->data_for_write<ccl::packed_float3>());

	if (subdiv_mode == SubdivideMode_CatmulClark) {
		// creases
		size_t num_creases = 0;
		XSI::CEdgeRefArray xsi_edges_array = xsi_polymesh.GetEdges();
		size_t xsi_edges_array_count = xsi_edges_array.GetCount();
		std::vector<double> creases_values(xsi_edges_array_count, 0.0);
		std::vector<int> creases_vertices(2 * xsi_edges_array_count, -1);

		for (size_t e_index = 0; e_index < xsi_edges_array_count; e_index++)
		{
			XSI::Edge edge(xsi_edges_array[e_index]);
			double crease_value = edge.GetCrease();
			if (crease_value > 0)
			{
				XSI::CVertexRefArray edge_vertices = edge.GetVertices();
				if (edge_vertices.GetCount() == 2)
				{
					num_creases++;
					creases_values[e_index] = crease_value;

					XSI::Vertex v0(edge_vertices[0]);
					XSI::Vertex v1(edge_vertices[1]);
					creases_vertices[2 * e_index] = v0.GetIndex();
					creases_vertices[2 * e_index + 1] = v1.GetIndex();
				}
			}
		}

		mesh->reserve_subd_creases(num_creases);
		if (num_creases > 0)
		{
			// set values if we need it
			for (size_t e_index = 0; e_index < xsi_edges_array_count; e_index++)
			{
				size_t v0 = creases_vertices[2 * e_index];
				size_t v1 = creases_vertices[2 * e_index + 1];
				double crease_value = creases_values[e_index];
				if (v0 >= 0 && v1 >= 0 && crease_value > 0.0)
				{
					mesh->add_edge_crease(v0, v1, crease_value);
				}
			}
		}
		creases_values.clear();
		creases_values.shrink_to_fit();
		creases_vertices.clear();
		creases_vertices.shrink_to_fit();

		// next for the vertices
		for (size_t v_index = 0; v_index < vertex_count; v_index++)
		{
			XSI::Vertex vertex = xsi_vertices[v_index];
			double vertex_crease = vertex.GetCrease();
			if (vertex_crease > 0.0)
			{
				mesh->add_vertex_crease(v_index, vertex_crease);
			}
		}
	}
	
	XSI::CLongArray triangle_nodes;  // these arrays does not actualy used for subdivided mesh
	LONG triangles_count = xsi_geo_acc.GetTriangleCount();
	sync_mesh_attribute_vertex_color(scene, mesh, attributes, xsi_geo_acc, subdiv_mode, triangle_nodes, xsi_faces);
	sync_mesh_attribute_random_per_island(scene, mesh, attributes, subdiv_mode, nodes_count, triangles_count, triangle_nodes, xsi_polymesh, xsi_faces);
	sync_mesh_attribute_pointness(scene, mesh, subdiv_mode, vertex_count, nodes_count, xsi_vertices, node_normals, xsi_polymesh);

	// uvs
	XSI::CRefArray uv_refs = xsi_geo_acc.GetUVs();
	// export first uv as default uv attribute
	sync_mesh_uvs(mesh, subdiv_mode, triangles_count, nodes_count, uv_refs, xsi_faces, triangle_nodes);
	sync_ice_attributes(scene, mesh, xsi_polymesh, subdiv_mode, vertex_count, nodes_count, xsi_node_to_vertex);
	
	// set subdivision
	if (subdiv_space == SubdivideSpace::SubdivideMode_Pixel) {
		// pixels
		mesh->set_subd_dicing_rate(subdiv_size);
		mesh->set_subd_adaptive_space(ccl::Mesh::SUBDIVISION_ADAPTIVE_SPACE_PIXEL);
	}
	else { //SubdivideSpace::SubdivideMode_Object
		// object, edge size
		mesh->set_subd_dicing_rate(subdiv_size);
		mesh->set_subd_adaptive_space(ccl::Mesh::SUBDIVISION_ADAPTIVE_SPACE_OBJECT);
	}
	
	mesh->set_subdivision_type(subdiv_mode == SubdivideMode_Linear ? ccl::Mesh::SUBDIVISION_LINEAR : ccl::Mesh::SUBDIVISION_CATMULL_CLARK);
	mesh->set_subd_max_level(subdiv_level);
	ccl::Transform tfm = xsi_matrix_to_transform(xsi_matrix);
	mesh->set_subd_objecttoworld(tfm);
}

void sync_mesh_subdiv_property(XSI::X3DObject& xsi_object, int &io_level, SubdivideMode &io_mode, SubdivideSpace &io_space, float &io_pixel_size, float &io_edge_length, int &io_smooth_boundary, int &io_smooth_uv, const XSI::CTime &eval_time)
{
	XSI::Property xsi_property = get_xsi_object_property(xsi_object, "CyclesMesh");
	bool use_property = xsi_property.IsValid();
	if (use_property)
	{
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();

		int level = xsi_params.GetValue("subdiv_max_level", eval_time);
		int mode = xsi_params.GetValue("subdiv_type", eval_time);
		int space = xsi_params.GetValue("subdiv_space", eval_time);

		if (mode != 0) {
			io_level = level;
			io_mode = mode == 1 ? SubdivideMode_Linear : SubdivideMode_CatmulClark;
			io_space = space == 0 ? SubdivideSpace::SubdivideMode_Pixel : SubdivideSpace::SubdivideMode_Object;

			if (io_level <= 0)
			{
				io_mode = SubdivideMode_None;
			}
		}

		io_smooth_boundary = xsi_params.GetValue("subdiv_boundary_smooth", eval_time);
		io_smooth_uv = xsi_params.GetValue("subdiv_uv_smooth", eval_time);
		io_pixel_size = xsi_params.GetValue("subdiv_pixel_size", eval_time);
		io_edge_length = xsi_params.GetValue("subdiv_edge_length", eval_time);
	}
}

void sync_polymesh_process(ccl::Scene* scene, ccl::Mesh* mesh_geom, UpdateContext* update_context, XSI::X3DObject &xsi_object, const XSI::Primitive &xsi_primitive, bool motion_deform, const XSI::CTime &eval_time)
{
	// geometry is new, create it
	XSI::PolygonMesh xsi_polymesh = xsi_primitive.GetGeometry(eval_time, XSI::siConstructionModeSecondaryShape);
	mesh_geom->name = combine_geometry_name(xsi_object, xsi_polymesh).GetAsciiString();

	// get geometry property
	XSI::Property geo_property = get_xsi_object_property(xsi_object, "geomapprox");
	bool is_geo_prop = geo_property.IsValid();
	int geo_subdivs = 0;
	float geo_angle = 60.0;
	bool geo_use_angle = true;
	if (is_geo_prop)
	{
		geo_subdivs = geo_property.GetParameterValue("gapproxmordrsl", eval_time);
		geo_angle = geo_property.GetParameterValue("gapproxmoan", eval_time);
		geo_use_angle = geo_property.GetParameterValue("gapproxmoad", eval_time);
	}

	// constuct geometry accessor
	XSI::CGeometryAccessor xsi_geo_acc = xsi_polymesh.GetGeometryAccessor(XSI::siConstructionModeSecondaryShape, XSI::siCatmullClark, 0, false, geo_use_angle, geo_angle);

	// set used shaders
	ccl::array<ccl::Node*> used_shaders;
	XSI::CRefArray xsi_geo_materials = xsi_geo_acc.GetMaterials();
	for (size_t i = 0; i < xsi_geo_materials.GetCount(); i++)
	{
		XSI::Material xsi_material = xsi_geo_materials[i];
		ULONG xsi_material_id = xsi_material.GetObjectID();
		size_t shader_index = 0;
		if (update_context->is_material_exists(xsi_material_id))
		{
			shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
		}

		used_shaders.push_back_slow(scene->shaders[shader_index]);
	}
	mesh_geom->set_used_shaders(used_shaders);

	float subdiv_pixel_size = 1.0f;
	float subdiv_edge_size = 0.01f;
	SubdivideMode subdiv_mode = geo_subdivs == 0 ? SubdivideMode_None : SubdivideMode_CatmulClark;
	SubdivideSpace subdiv_space = SubdivideSpace::SubdivideMode_Pixel;
	int subdiv_boundary_smooth = 0;  // make it synchronised with default values in mesh properties
	int subdiv_uv_smooth = 4;
	sync_mesh_subdiv_property(xsi_object, geo_subdivs, subdiv_mode, subdiv_space, subdiv_pixel_size, subdiv_edge_size, subdiv_boundary_smooth, subdiv_uv_smooth, eval_time);

	geo_subdivs = std::max(0, geo_subdivs);
	if (subdiv_space == SubdivideSpace::SubdivideMode_Pixel) {
		subdiv_pixel_size = std::max(0.5f, subdiv_pixel_size);
	}
	if (subdiv_space == SubdivideSpace::SubdivideMode_Object) {
		subdiv_edge_size = std::max(0.001f, subdiv_edge_size);
	}

	if (subdiv_mode == SubdivideMode_None)
	{// non subdivided mesh
		// so, we should create triangles
		sync_triangle_mesh(scene, mesh_geom, xsi_geo_acc, xsi_polymesh);
	}
	else
	{// create subdivide mesh
		sync_subdivide_mesh(scene, mesh_geom, xsi_geo_acc, xsi_polymesh, subdiv_mode, geo_subdivs, subdiv_space, subdiv_space == SubdivideSpace::SubdivideMode_Pixel ? subdiv_pixel_size : subdiv_edge_size, xsi_object.GetKinematics().GetGlobal().GetTransform(eval_time).GetMatrix4());
		mesh_geom->set_subdivision_boundary_interpolation(
			subdiv_boundary_smooth == 0 ? ccl::Mesh::SUBDIVISION_BOUNDARY_EDGE_AND_CORNER : 
										  ccl::Mesh::SUBDIVISION_BOUNDARY_EDGE_ONLY);

		mesh_geom->set_subdivision_fvar_interpolation(
			 subdiv_uv_smooth == 0 ? ccl::Mesh::SUBDIVISION_FVAR_LINEAR_ALL :
			(subdiv_uv_smooth == 1 ? ccl::Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_ONLY :
			(subdiv_uv_smooth == 2 ? ccl::Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS1 :
			(subdiv_uv_smooth == 3 ? ccl::Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS2 :
			(subdiv_uv_smooth == 4 ? ccl::Mesh::SUBDIVISION_FVAR_LINEAR_BOUNDARIES : 
									 ccl::Mesh::SUBDIVISION_FVAR_LINEAR_NONE)))));
	}

	mesh_geom->set_use_motion_blur(false);
	if (update_context->get_need_motion() && motion_deform)
	{
		sync_polymesh_motion_deform(mesh_geom, update_context, xsi_object, subdiv_mode, geo_use_angle, geo_angle);
	}
}

ccl::Mesh* sync_polymesh_object(ccl::Scene* scene, ccl::Object* mesh_object, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	bool motion_deform = false;
	XSI::CString lightgroup = "";
	sync_geometry_object_parameters(scene, mesh_object, xsi_object, lightgroup, motion_deform, "CyclesMesh", render_parameters, eval_time);

	update_context->add_lightgroup(lightgroup);

	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	ULONG xsi_polymesh_id = xsi_primitive.GetObjectID();
	
	if (update_context->is_geometry_exists(xsi_polymesh_id))
	{
		size_t geometry_index = update_context->get_geometry_index(xsi_polymesh_id);
		ccl::Geometry* geometry = scene->geometry[geometry_index];

		if (geometry->geometry_type == ccl::Geometry::Type::MESH)
		{
			ccl::Mesh* mesh_geom = static_cast<ccl::Mesh*>(geometry);
			return mesh_geom;
		}
	}

	// create output mesh
	ccl::Mesh* mesh_geom = scene->create_node<ccl::Mesh>();

	sync_polymesh_process(scene, mesh_geom, update_context, xsi_object, xsi_primitive, motion_deform, eval_time);

	update_context->add_geometry_index(xsi_polymesh_id, scene->geometry.size() - 1);

	return mesh_geom;
}

XSI::CStatus update_polymesh(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	ULONG xsi_object_id = xsi_object.GetObjectID();
	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	ULONG xsi_polymesh_id = xsi_primitive.GetObjectID();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	if (xsi_primitive.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		// update object properties for all instances
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesMesh", render_parameters, eval_time, false);  // false - dose not reassign color and name of the object
		}
		update_context->add_lightgroup(lightgroup);

		if (update_context->is_geometry_exists(xsi_polymesh_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_polymesh_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::MESH)
			{
				ccl::Mesh* mesh_geom = static_cast<ccl::Mesh*>(geometry);
				mesh_geom->clear(true);

				sync_polymesh_process(scene, mesh_geom, update_context, xsi_object, xsi_primitive, motion_deform, eval_time);
				mesh_geom->tag_update(scene, true);
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