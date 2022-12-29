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
#include "cyc_tangent_attribute.h"
#include "../cyc_scene.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/math.h"
#include "../../../utilities/strings.h"
#include "../../../render_base/type_enums.h"

ccl::Mesh* build_primitive(ccl::Scene* scene, int vertex_count, float* vertices, int faces_count, int* face_sizes, int* face_indexes)
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
	mesh->reserve_mesh(vertex_coordinates.size(), num_triangles);
	mesh->set_verts(vertex_coordinates);

	// create triangles
	int index_offset = 0;

	for (size_t i = 0; i < faces_count; i++)  // iterate over polygons
	{
		for (int j = 0; j < face_sizes[i] - 2; j++)  // for each polygon by n-2
		{
			int v0 = face_indexes[index_offset];
			int v1 = face_indexes[index_offset + j + 1];
			int v2 = face_indexes[index_offset + j + 2];
			mesh->add_triangle(v0, v1, v2, 0, false);
		}

		index_offset += face_sizes[i];
	}

	return mesh;
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
	std::vector<LONG> xsi_node_to_vertex(nodes_count);
	XSI::CVertexRefArray xsi_vertices = xsi_polymesh.GetVertices();
	for (LONG i = 0; i < vertex_count; i++)
	{
		XSI::Vertex v = xsi_vertices[i];
		LONG v_index = v.GetIndex();
		XSI::CPolygonNodeRefArray v_nodes = v.GetNodes();
		LONG v_nodes_count = v_nodes.GetCount();
		for (LONG j = 0; j < v_nodes_count; j++)
		{
			XSI::PolygonNode v_node = v_nodes[j];
			LONG node_index = v_node.GetIndex();
			xsi_node_to_vertex[node_index] = v_index;
		}
	}

	// for triagle mesh vertices are xsi nodes
	mesh->reserve_mesh(nodes_count, triangles_count);

	// form vertices array
	ccl::array<ccl::float3> mesh_vertices(nodes_count);
	for (LONG i = 0; i < nodes_count; i++)
	{
		LONG v_index = xsi_node_to_vertex[i];
		mesh_vertices[i] = ccl::make_float3(vertex_positions[3*v_index], vertex_positions[3 * v_index + 1], vertex_positions[3 * v_index + 2]);
	}

	// set mesh vertices
	mesh->set_verts(mesh_vertices);

	// next triangles
	for (size_t i = 0; i < triangles_count; i++)
	{
		// get triangle nodes
		LONG n0 = triangle_nodes[3 * i];
		LONG n1 = triangle_nodes[3 * i + 1];
		LONG n2 = triangle_nodes[3 * i + 2];

		LONG material_index = polygon_materials[triangle_polygons[i]];

		// add triangle
		mesh->add_triangle(n0, n1, n2, material_index, true);
	}

	// normals
	XSI::CFloatArray node_normals;
	xsi_geo_acc.GetNodeNormals(node_normals);
	get_geo_accessor_normals(xsi_geo_acc, nodes_count, node_normals);

	ccl::AttributeSet& attributes = mesh->attributes;
	ccl::Attribute* attr_n = attributes.add(ccl::ATTR_STD_VERTEX_NORMAL, ccl::ustring("std_normal"));
	ccl::float3* normal_data = attr_n->data_float3();
	for (size_t node_index = 0; node_index < nodes_count; node_index++)
	{
		*normal_data = ccl::make_float3(node_normals[3 * node_index], node_normals[3 * node_index + 1], node_normals[3 * node_index + 2]);
		normal_data++;
	}

	// generated attribute
	ccl::Attribute* gen_attr = attributes.add(ccl::ATTR_STD_GENERATED, ccl::ustring("std_generated"));
	gen_attr->flags |= ccl::ATTR_SUBDIVIDED;
	std::memcpy(gen_attr->data_float3(), mesh->get_verts().data(), sizeof(ccl::float3) * mesh->get_verts().size());

	// use common method for export attrbutes
	XSI::CPolygonFaceRefArray faces;
	sync_mesh_attribute_vertex_color(scene, mesh, attributes, xsi_geo_acc, true, triangle_nodes, faces);
	sync_mesh_attribute_random_per_island(scene, mesh, attributes, true, nodes_count, triangles_count, triangle_nodes, xsi_polymesh, faces);
	sync_mesh_attribute_pointness(scene, mesh, true, vertex_count, nodes_count, xsi_vertices, node_normals, xsi_polymesh);
	
	// uvs
	XSI::CRefArray uv_refs = xsi_geo_acc.GetUVs();
	// export first uv as default uv attribute
	sync_mesh_uvs(mesh, true, triangles_count, nodes_count, uv_refs, faces, triangle_nodes);
	// export tangent for each uv
	for (size_t i = 0; i < uv_refs.GetCount(); i++)
	{
		XSI::ClusterProperty uv_prop(uv_refs[i]);
		mikk_compute_tangents(mesh, uv_prop.GetName().GetAsciiString(), true);
	}
	sync_ice_attributes(scene, mesh, xsi_polymesh, true, vertex_count, nodes_count, xsi_node_to_vertex);

	// motion deform
}

void sync_subdivide_mesh(ccl::Scene* scene, ccl::Mesh* mesh, const XSI::CGeometryAccessor& xsi_geo_acc, const XSI::PolygonMesh& xsi_polymesh, SubdivideMode subdiv_mode, ULONG subdiv_level, float subdiv_dicing_rate, const XSI::MATH::CMatrix4 &xsi_matrix)
{
	XSI::CLongArray xsi_polygon_material_indices;
	xsi_geo_acc.GetPolygonMaterialIndices(xsi_polygon_material_indices);

	XSI::CDoubleArray vertex_positions;
	XSI::CLongArray polygon_materials;
	ULONG vertex_count = xsi_geo_acc.GetVertexCount();
	xsi_geo_acc.GetVertexPositions(vertex_positions);
	xsi_geo_acc.GetPolygonMaterialIndices(polygon_materials);

	XSI::CVertexRefArray xsi_vertices = xsi_polymesh.GetVertices();
	XSI::CPolygonFaceRefArray xsi_faces = xsi_polymesh.GetPolygons();
	XSI::CLongArray polygon_sizes;
	xsi_geo_acc.GetPolygonVerticesCount(polygon_sizes);
	size_t polygons_count = polygon_sizes.GetCount();

	mesh->reserve_mesh(vertex_count, 0);
	int num_corners = 0;
	int num_ngons = 0;
	for (size_t i = 0; i < polygon_sizes.GetCount(); i++)
	{
		num_corners += polygon_sizes[i];
		num_ngons += polygon_sizes[i] == 4 ? 0 : 1;
	}
	mesh->reserve_subd_faces(polygons_count, num_ngons, num_corners);

	ccl::array<ccl::float3> mesh_vertices(vertex_count);
	ccl::array<ccl::float3> mesh_normals(vertex_count);
	for (size_t v_index = 0; v_index < vertex_count; v_index++)
	{
		XSI::Vertex vertex = xsi_vertices[v_index];
		XSI::MATH::CVector3 vertex_position = vertex.GetPosition();
		ccl::float3 position = ccl::make_float3(vertex_position.GetX(), vertex_position.GetY(), vertex_position.GetZ());
		bool is_valid = true;
		XSI::MATH::CVector3 normal = vertex.GetNormal(is_valid);
		mesh_vertices[v_index] = position;
		mesh_normals[v_index] = vector3_to_float3(normal);
	}
	mesh->set_verts(mesh_vertices);

	// normals
	ccl::AttributeSet& attributes = mesh->subd_attributes;
	ccl::Attribute* attr_n = attributes.add(ccl::ATTR_STD_VERTEX_NORMAL, ccl::ustring("std_normal"));
	std::memcpy(attr_n->data_float3(), mesh_normals.data(), sizeof(ccl::float3) * mesh_normals.size());


	ccl::Attribute* gen_attr = attributes.add(ccl::ATTR_STD_GENERATED, ccl::ustring("std_generated"));
	gen_attr->flags |= ccl::ATTR_SUBDIVIDED;
	std::memcpy(gen_attr->data_float3(), mesh->get_verts().data(), sizeof(ccl::float3) * mesh->get_verts().size());

	// faces
	ccl::vector<int> vi;
	size_t faces_count = xsi_faces.GetCount();
	for (size_t face_index = 0; face_index < faces_count; face_index++)
	{
		XSI::PolygonFace face(xsi_faces[face_index]);
		XSI::CVertexRefArray face_vertices = face.GetVertices();
		size_t face_vertex_count = face_vertices.GetCount();
		vi.resize(face_vertex_count);
		for (size_t v = 0; v < face_vertex_count; v++)
		{
			XSI::Vertex vert(face_vertices[v]);
			vi[v] = vert.GetIndex();
		}
		mesh->add_subd_face(&vi[0], face_vertex_count, xsi_polygon_material_indices[face_index], false);
	}

	//creases
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

	XSI::CLongArray triangle_nodes;  // these arrays does not actualy used for subdivided mesh
	XSI::CFloatArray node_normals;
	LONG nodes_count = xsi_geo_acc.GetNodeCount();
	LONG triangles_count = xsi_geo_acc.GetTriangleCount();
	sync_mesh_attribute_vertex_color(scene, mesh, attributes, xsi_geo_acc, false, triangle_nodes, xsi_faces);
	sync_mesh_attribute_random_per_island(scene, mesh, attributes, false, nodes_count, triangles_count, triangle_nodes, xsi_polymesh, xsi_faces);
	sync_mesh_attribute_pointness(scene, mesh, false, vertex_count, nodes_count, xsi_vertices, node_normals, xsi_polymesh);

	// uvs
	XSI::CRefArray uv_refs = xsi_geo_acc.GetUVs();
	// export first uv as default uv attribute
	sync_mesh_uvs(mesh, false, triangles_count, nodes_count, uv_refs, xsi_faces, triangle_nodes);
	// export tangent for each uv
	for (size_t i = 0; i < uv_refs.GetCount(); i++)
	{
		XSI::ClusterProperty uv_prop(uv_refs[i]);
		mikk_compute_tangents(mesh, uv_prop.GetName().GetAsciiString(), true);
	}

	std::vector<LONG> xsi_node_to_vertex;
	sync_ice_attributes(scene, mesh, xsi_polymesh, false, vertex_count, nodes_count, xsi_node_to_vertex);

	// set subdivision
	mesh->set_subd_dicing_rate(subdiv_dicing_rate);
	mesh->set_subd_max_level(subdiv_level);
	ccl::Transform tfm = xsi_matrix_to_transform(xsi_matrix);
	mesh->set_subd_objecttoworld(tfm);

	// without open subdiv the mode always is Linear
	mesh->set_subdivision_type(subdiv_mode == SubdivideMode_Linear ? ccl::Mesh::SUBDIVISION_LINEAR : ccl::Mesh::SUBDIVISION_CATMULL_CLARK);
}

void sync_mesh_subdiv_property(XSI::X3DObject& xsi_object, int &io_level, SubdivideMode &io_mode, float &io_dicing_rate, const XSI::CTime &eval_time)
{
	XSI::Property xsi_property;
	bool use_property = get_xsi_object_property(xsi_object, "CyclesMesh", xsi_property);
	if (use_property)
	{
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();

		int level = xsi_params.GetValue("subdiv_max_level", eval_time);
		float dicing_rate = xsi_params.GetValue("subdiv_dicing_rate", eval_time);
		int mode = xsi_params.GetValue("subdiv_type", eval_time);

		if (mode != 0)
		{
			io_dicing_rate = dicing_rate;
			io_level = level;
			io_mode = mode == 1 ? SubdivideMode_Linear : SubdivideMode_CatmulClark;

			if (io_level <= 0)
			{
				io_mode = SubdivideMode_None;
			}
		}
	}
}

void sync_polymesh_process(ccl::Scene* scene, ccl::Mesh* mesh_geom, UpdateContext* update_context, XSI::X3DObject &xsi_object, const XSI::Primitive &xsi_primitive, const XSI::CTime &eval_time)
{
	// geometry is new, create it
	XSI::PolygonMesh xsi_polymesh = xsi_primitive.GetGeometry(eval_time, XSI::siConstructionModeSecondaryShape);

	// get geometry property
	XSI::Property geo_property;
	bool is_geo_prop = get_xsi_object_property(xsi_object, "geomapprox", geo_property);
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

	float subdiv_dicing_rate = 1.0f;
	SubdivideMode subdiv_mode = geo_subdivs == 0 ? SubdivideMode_None : SubdivideMode_CatmulClark;
	sync_mesh_subdiv_property(xsi_object, geo_subdivs, subdiv_mode, subdiv_dicing_rate, eval_time);

	geo_subdivs = std::max(0, geo_subdivs);
	subdiv_dicing_rate = std::max(0.1f, subdiv_dicing_rate);

	if (subdiv_mode == SubdivideMode_None)
	{// non subdivided mesh
		// so, we should create triangles
		sync_triangle_mesh(scene, mesh_geom, xsi_geo_acc, xsi_polymesh);
	}
	else
	{// create subdivide mesh
		sync_subdivide_mesh(scene, mesh_geom, xsi_geo_acc, xsi_polymesh, subdiv_mode, geo_subdivs, subdiv_dicing_rate, xsi_object.GetKinematics().GetGlobal().GetTransform(eval_time).GetMatrix4());
	}
}

ccl::Mesh* sync_polymesh_object(ccl::Scene* scene, ccl::Object* mesh_object, UpdateContext* update_context, XSI::X3DObject& xsi_object, const XSI::CParameterRefArray& render_parameters)
{
	XSI::CTime eval_time = update_context->get_time();

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
			ccl::Mesh* mesh_geo = static_cast<ccl::Mesh*>(geometry);
			return mesh_geo;
		}
	}

	// create output mesh
	ccl::Mesh* mesh_geom = scene->create_node<ccl::Mesh>();

	sync_polymesh_process(scene, mesh_geom, update_context, xsi_object, xsi_primitive, eval_time);

	// transform
	sync_transform(mesh_object, update_context, xsi_object.GetKinematics().GetGlobal());

	update_context->add_geometry_index(xsi_polymesh_id, scene->geometry.size() - 1);

	return mesh_geom;
}

XSI::CStatus update_polymesh(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject &xsi_object, const XSI::CParameterRefArray& render_parameters)
{
	XSI::CTime eval_time = update_context->get_time();
	ULONG xsi_object_id = xsi_object.GetObjectID();
	XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive(eval_time);
	ULONG xsi_polymesh_id = xsi_primitive.GetObjectID();

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

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesHairs", render_parameters, eval_time);
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

				sync_polymesh_process(scene, mesh_geom, update_context, xsi_object, xsi_primitive, eval_time);
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