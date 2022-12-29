#include "scene/scene.h"
#include "scene/mesh.h"
#include "util/color.h"
#include "util/disjoint_set.h"
#include "util/hash.h"

#include <xsi_geometryaccessor.h>
#include <xsi_floatarray.h>
#include <xsi_polygonface.h>
#include <xsi_polygonnode.h>
#include <xsi_clusterproperty.h>
#include <xsi_edge.h>
#include <xsi_polygonmesh.h>
#include <xsi_vertex.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_iceattributedataarray2D.h>

#include "../../../utilities/math.h"
#include "../../../utilities/logs.h"

void sync_mesh_attribute_vertex_color(ccl::Scene* scene, ccl::Mesh* mesh, ccl::AttributeSet& attributes, const XSI::CGeometryAccessor& xsi_geo_acc, bool use_triangles, const XSI::CLongArray& triangle_nodes, const XSI::CPolygonFaceRefArray& faces)
{
	XSI::CRefArray vertex_colors_array = xsi_geo_acc.GetVertexColors();
	size_t vertex_colors_array_count = vertex_colors_array.GetCount();

	for (size_t i = 0; i < vertex_colors_array_count; i++)
	{
		XSI::ClusterProperty vertex_color_prop(vertex_colors_array[i]);
		XSI::CFloatArray values;
		vertex_color_prop.GetValues(values);
		ccl::ustring vc_name = ccl::ustring((vertex_color_prop.GetName()).GetAsciiString());
		if (mesh->need_attribute(scene, vc_name))
		{
			ccl::Attribute* vc_attr = attributes.add(vc_name, ccl::TypeRGBA, ccl::ATTR_ELEMENT_CORNER_BYTE);
			vc_attr->flags |= ccl::ATTR_SUBDIVIDED;
			ccl::uchar4* cdata = vc_attr->data_uchar4();
			if (use_triangles)
			{
				size_t triangles_count = triangle_nodes.GetCount() / 3;
				for (size_t t_index = 0; t_index < triangles_count; t_index++)
				{
					size_t v0 = triangle_nodes[3 * t_index];
					size_t v1 = triangle_nodes[3 * t_index + 1];
					size_t v2 = triangle_nodes[3 * t_index + 2];
					ccl::float4 c0 = ccl::make_float4(values[4 * v0], values[4 * v0 + 1], values[4 * v0 + 2], values[4 * v0 + 3]);
					ccl::float4 c1 = ccl::make_float4(values[4 * v1], values[4 * v1 + 1], values[4 * v1 + 2], values[4 * v1 + 3]);
					ccl::float4 c2 = ccl::make_float4(values[4 * v2], values[4 * v2 + 1], values[4 * v2 + 2], values[4 * v2 + 3]);
					cdata[0] = ccl::color_float4_to_uchar4(c0);
					cdata[1] = ccl::color_float4_to_uchar4(c1);
					cdata[2] = ccl::color_float4_to_uchar4(c2);

					cdata += 3;
				}
			}
			else
			{
				size_t faces_count = faces.GetCount();
				for (size_t face_index = 0; face_index < faces_count; face_index++)
				{
					XSI::PolygonFace face(faces[face_index]);
					XSI::CPolygonNodeRefArray face_nodes = face.GetNodes();
					size_t face_nodes_count = face_nodes.GetCount();
					for (size_t node_index = 0; node_index < face_nodes_count; node_index++)
					{
						XSI::PolygonNode node(face_nodes[node_index]);
						size_t n = node.GetIndex();
						cdata[0] = ccl::color_float4_to_uchar4(ccl::make_float4(values[4 * n], values[4 * n + 1], values[4 * n + 2], values[4 * n + 3]));
						cdata++;
					}
				}
			}
		}
	}
}

void sync_mesh_attribute_random_per_island(ccl::Scene* scene, ccl::Mesh* mesh, ccl::AttributeSet& attributes, bool use_triangles, size_t nodes_count, size_t triangles_count, const XSI::CLongArray& triangle_nodes, const XSI::PolygonMesh& xsi_polymesh, const XSI::CPolygonFaceRefArray& faces)
{
	if (mesh->need_attribute(scene, ccl::ATTR_STD_RANDOM_PER_ISLAND))
	{
		ccl::DisjointSet vertices_sets(nodes_count);
		// fill data about incident edges
		XSI::CEdgeRefArray edges = xsi_polymesh.GetEdges();
		size_t edges_count = edges.GetCount();
		for (size_t edge_inex = 0; edge_inex < edges_count; edge_inex++)
		{
			XSI::Edge edge = edges[edge_inex];
			XSI::CPolygonNodeRefArray edge_nodes = edge.GetNodes();
			size_t edge_nodes_count = edge_nodes.GetCount();
			XSI::PolygonNode start_node = edge_nodes[0];
			for (size_t e_node = 1; e_node < edge_nodes_count; e_node++)
			{
				XSI::PolygonNode n = edge_nodes[e_node];
				vertices_sets.join(start_node.GetIndex(), n.GetIndex());
			}
		}

		ccl::Attribute* island_attribute = attributes.add(ccl::ATTR_STD_RANDOM_PER_ISLAND);
		float* island_data = island_attribute->data_float();
		// fill attribute for every triangle
		if (!use_triangles)
		{// for subdivided mesh
			size_t face_count = faces.GetCount();
			for (size_t face_index = 0; face_index < face_count; face_index++)
			{
				XSI::PolygonFace face(faces[face_index]);
				XSI::CPolygonNodeRefArray face_nodes = face.GetNodes();
				XSI::PolygonNode node(face_nodes[0]);
				float value = ccl::hash_uint_to_float(vertices_sets.find(node.GetIndex()));
				island_data[face_index] = value;
			}
		}
		else
		{// for triangle mesh
			for (size_t t_index = 0; t_index < triangles_count; t_index++)
			{
				island_data[t_index] = ccl::hash_uint_to_float(vertices_sets.find(triangle_nodes[3 * t_index]));
			}
		}
	}
}

//base on blender_mesh.cpp
class VertexAverageComparator
{
public:
	VertexAverageComparator(const ccl::array<ccl::float3>& verts) : verts_(verts) { }

	bool operator()(const int& vert_idx_a, const int& vert_idx_b)
	{
		const ccl::float3& vert_a = verts_[vert_idx_a];
		const ccl::float3& vert_b = verts_[vert_idx_b];
		if (vert_a == vert_b)
		{
			// Special case for doubles, so we ensure ordering
			return vert_idx_a > vert_idx_b;
		}
		const float x1 = vert_a.x + vert_a.y + vert_a.z;
		const float x2 = vert_b.x + vert_b.y + vert_b.z;
		return x1 < x2;
	}

protected:
	const ccl::array<ccl::float3>& verts_;
};

class EdgeMap
{
public:
	EdgeMap() { }

	void clear()
	{
		edges_.clear();
	}

	void insert(int v0, int v1)
	{
		get_sorted_verts(v0, v1);
		edges_.insert(std::pair<int, int>(v0, v1));
	}

	bool exists(int v0, int v1)
	{
		get_sorted_verts(v0, v1);
		return edges_.find(std::pair<int, int>(v0, v1)) != edges_.end();
	}

protected:
	void get_sorted_verts(int& v0, int& v1)
	{
		if (v0 > v1)
		{
			std::swap(v0, v1);
		}
	}

	std::set<std::pair<int, int>> edges_;
};

void sync_mesh_attribute_pointness(ccl::Scene* scene, ccl::Mesh* mesh, bool use_triangles, size_t vertex_count, size_t nodes_count, const XSI::CVertexRefArray& vertices, const XSI::CFloatArray& node_normals, const XSI::PolygonMesh& xsi_polymesh)
{
	if (!mesh->need_attribute(scene, ccl::ATTR_STD_POINTINESS))
	{
		return;
	}

	const size_t num_verts = !use_triangles ? vertex_count : nodes_count;
	if (num_verts == 0)
	{
		return;
	}

	// STEP 1: Find out duplicated vertices and point duplicates to a single
	//         original vertex.
	//
	ccl::vector<size_t> sorted_vert_indeices(num_verts);
	for (size_t vert_index = 0; vert_index < num_verts; ++vert_index)
	{
		sorted_vert_indeices[vert_index] = vert_index;
	}

	VertexAverageComparator compare(mesh->get_verts());
	sort(sorted_vert_indeices.begin(), sorted_vert_indeices.end(), compare);
	// This array stores index of the original vertex for the given vertex
	// index.
	//	
	ccl::vector<size_t> vert_orig_index(num_verts);
	for (size_t sorted_vert_index = 0; sorted_vert_index < num_verts; ++sorted_vert_index)
	{
		const size_t vert_index = sorted_vert_indeices[sorted_vert_index];
		const ccl::float3& vert_co = mesh->get_verts()[vert_index];
		bool found = false;
		for (size_t other_sorted_vert_index = sorted_vert_index + 1; other_sorted_vert_index < num_verts; ++other_sorted_vert_index)
		{
			const size_t other_vert_index = sorted_vert_indeices[other_sorted_vert_index];
			const ccl::float3& other_vert_co = mesh->get_verts()[other_vert_index];
			// We are too far away now, we wouldn't have duplicate
			if ((other_vert_co.x + other_vert_co.y + other_vert_co.z) - (vert_co.x + vert_co.y + vert_co.z) > 3 * FLT_EPSILON)
			{
				break;
			}
			// Found duplicate.
			if (len_squared(other_vert_co - vert_co) < FLT_EPSILON)
			{
				found = true;
				vert_orig_index[vert_index] = other_vert_index;
				break;
			}
		}
		if (!found)
		{
			vert_orig_index[vert_index] = vert_index;
		}
	}
	// Make sure we always points to the very first orig vertex
	for (size_t vert_index = 0; vert_index < num_verts; ++vert_index)
	{
		size_t orig_index = vert_orig_index[vert_index];
		while (orig_index != vert_orig_index[orig_index])
		{
			orig_index = vert_orig_index[orig_index];
		}
		vert_orig_index[vert_index] = orig_index;
	}
	sorted_vert_indeices.free_memory();
	// STEP 2: Calculate vertex normals taking into account their possible
	//         duplicates which gets "welded" together.
	//
	ccl::vector<ccl::float3> vert_normal(num_verts, ccl::zero_float3());

	// First we accumulate all vertex normals in the original index.
	for (size_t vert_index = 0; vert_index < num_verts; ++vert_index)
	{
		if (!use_triangles)
		{
			XSI::Vertex vertex(vertices[vert_index]);
			const size_t orig_index = vert_orig_index[vert_index];
			bool is_valid = true;
			vert_normal[orig_index] += vector3_to_float3(vertex.GetNormal(is_valid));
		}
		else
		{
			const ccl::float3 normal = ccl::make_float3(node_normals[3 * vert_index], node_normals[3 * vert_index + 1], node_normals[3 * vert_index + 2]);
			const size_t orig_index = vert_orig_index[vert_index];
			vert_normal[orig_index] += normal;
		}
	}
	// Then we normalize the accumulated result and flush it to all duplicates
	// as well.
	//
	for (size_t vert_index = 0; vert_index < num_verts; ++vert_index)
	{
		const size_t orig_index = vert_orig_index[vert_index];
		vert_normal[vert_index] = normalize(vert_normal[orig_index]);
	}

	// STEP 3: Calculate pointiness using single ring neighborhood.
	ccl::vector<size_t> counter(num_verts, 0);
	std::vector<float> raw_data(num_verts, 0.0f);
	std::vector<ccl::float3> edge_accum(num_verts, ccl::zero_float3());

	XSI::CEdgeRefArray edges = xsi_polymesh.GetEdges();
	size_t edges_count = edges.GetCount();
	EdgeMap visited_edges;
	int edge_index = 0;
	memset(&counter[0], 0, sizeof(size_t) * counter.size());
	for (size_t edge_index = 0; edge_index < edges_count; edge_index++)
	{
		XSI::Edge e(edges[edge_index]);
		XSI::CVertexRefArray edge_vertices = e.GetVertices();
		XSI::Vertex vert_0(edge_vertices[0]);
		XSI::Vertex vert_1(edge_vertices[1]);

		size_t v0 = 0;
		size_t v1 = 0;
		if (!use_triangles)
		{
			v0 = vert_orig_index[vert_0.GetIndex()];
			v1 = vert_orig_index[vert_1.GetIndex()];
		}
		else
		{
			XSI::CPolygonNodeRefArray vert_nodes = vert_0.GetNodes();
			XSI::PolygonNode n_0(vert_nodes[0]);
			v0 = vert_orig_index[n_0.GetIndex()];

			vert_nodes = vert_1.GetNodes();
			XSI::PolygonNode n_1(vert_nodes[0]);
			v1 = vert_orig_index[n_1.GetIndex()];
		}
		if (visited_edges.exists(v0, v1))
		{
			continue;
		}
		visited_edges.insert(v0, v1);

		ccl::float3 co0 = mesh->get_verts()[v0];
		ccl::float3 co1 = mesh->get_verts()[v1];

		ccl::float3 edge = normalize(co1 - co0);
		edge_accum[v0] += edge;
		edge_accum[v1] += -edge;
		++counter[v0];
		++counter[v1];
	}

	for (size_t vert_index = 0; vert_index < num_verts; ++vert_index)
	{
		const int orig_index = vert_orig_index[vert_index];
		if (orig_index != vert_index)
		{
			// Skip duplicates, they'll be overwritten later on
			continue;
		}
		if (counter[vert_index] > 0)
		{
			const ccl::float3 normal = vert_normal[vert_index];
			const float angle = ccl::safe_acosf(dot(normal, edge_accum[vert_index] / counter[vert_index]));
			raw_data[vert_index] = angle * M_1_PI_F;
		}
		else
		{
			raw_data[vert_index] = 0.0f;
		}
	}
	// STEP 3: Blur vertices to approximate 2 ring neighborhood. 
	ccl::AttributeSet& attributes = (!use_triangles) ? mesh->subd_attributes : mesh->attributes;
	ccl::Attribute* attr = attributes.add(ccl::ATTR_STD_POINTINESS);
	attr->flags |= ccl::ATTR_SUBDIVIDED;
	float* data = attr->data_float();
	memcpy(data, &raw_data[0], sizeof(float) * raw_data.size());
	memset(&counter[0], 0, sizeof(size_t) * counter.size());
	edge_index = 0;
	visited_edges.clear();
	for (size_t edge_index = 0; edge_index < edges_count; edge_index++)
	{
		XSI::Edge e(edges[edge_index]);
		XSI::CVertexRefArray edge_vertices = e.GetVertices();
		XSI::Vertex vert_0(edge_vertices[0]);
		XSI::Vertex vert_1(edge_vertices[1]);

		size_t v0 = 0;
		size_t v1 = 0;
		if (!use_triangles)
		{
			v0 = vert_orig_index[vert_0.GetIndex()];
			v1 = vert_orig_index[vert_1.GetIndex()];
		}
		else
		{
			XSI::CPolygonNodeRefArray vert_nodes = vert_0.GetNodes();
			XSI::PolygonNode n_0(vert_nodes[0]);
			v0 = vert_orig_index[n_0.GetIndex()];

			vert_nodes = vert_1.GetNodes();
			XSI::PolygonNode n_1(vert_nodes[0]);
			v1 = vert_orig_index[n_1.GetIndex()];
		}

		if (visited_edges.exists(v0, v1))
		{
			continue;
		}
		visited_edges.insert(v0, v1);
		data[v0] += raw_data[v1];
		data[v1] += raw_data[v0];
		++counter[v0];
		++counter[v1];
	}
	for (int vert_index = 0; vert_index < num_verts; ++vert_index)
	{
		data[vert_index] /= counter[vert_index] + 1;
	}
	// STEP 4: Copy attribute to the duplicated vertices
	for (int vert_index = 0; vert_index < num_verts; ++vert_index)
	{
		const size_t orig_index = vert_orig_index[vert_index];
		data[vert_index] = data[orig_index];
	}
}

void sync_mesh_uvs(ccl::Mesh* mesh, bool use_triangles, size_t triangles_count, size_t nodes_count, const XSI::CRefArray &uv_refs, const XSI::CPolygonFaceRefArray& faces, const XSI::CLongArray& triangle_nodes)
{
	LONG uv_count = uv_refs.GetCount();
	ULONG one_uv_length = !use_triangles ? nodes_count : (triangles_count * 3);

	ccl::ustring uv_name = ccl::ustring("std_uv");
	ccl::Attribute* uv_attr;
	if (!use_triangles)
	{
		uv_attr = mesh->subd_attributes.add(ccl::ATTR_STD_UV, uv_name);
	}
	else
	{
		uv_attr = mesh->attributes.add(ccl::ATTR_STD_UV, uv_name);
	}

	if (!use_triangles)
	{
		uv_attr->flags |= ccl::ATTR_SUBDIVIDED;
	}

	ccl::float2* default_uv = uv_attr->data_float2();
	for (size_t uv_index = 0; uv_index < uv_count; uv_index++)
	{
		XSI::ClusterProperty uv_prop(uv_refs[uv_index]);
		XSI::CFloatArray uv_values;
		uv_prop.GetValues(uv_values);

		ccl::Attribute* uv_attribute;
		ccl::ustring uv_name = ccl::ustring(uv_prop.GetName().GetAsciiString());

		if (!use_triangles)
		{
			uv_attribute = mesh->subd_attributes.add(ccl::ATTR_STD_UV, uv_name);
		}
		else
		{
			uv_attribute = mesh->attributes.add(ccl::ATTR_STD_UV, uv_name);
		}
		if (!use_triangles)
		{
			uv_attribute->flags |= ccl::ATTR_SUBDIVIDED;
		}

		ccl::float2* uv_attribute_data = uv_attribute->data_float2();
		if (!use_triangles)
		{
			size_t faces_count = faces.GetCount();
			for (size_t face_index = 0; face_index < faces_count; face_index++)
			{
				XSI::PolygonFace face(faces[face_index]);
				XSI::CPolygonNodeRefArray face_nodes = face.GetNodes();
				size_t face_nodes_count = face_nodes.GetCount();
				for (size_t node_index = 0; node_index < face_nodes_count; node_index++)
				{
					XSI::PolygonNode node(face_nodes[node_index]);
					ccl::float2 uv_value = ccl::make_float2(uv_values[node.GetIndex() * 3], uv_values[node.GetIndex() * 3 + 1]);
					uv_attribute_data[0] = ccl::make_float2(uv_value.x, uv_value.y);
					uv_attribute_data++;

					if (uv_index == 0)
					{
						default_uv[0] = ccl::make_float2(uv_value.x, uv_value.y);
						default_uv++;
					}
				}
			}
		}
		else
		{// for plane mesh use triangles (and use triangle_to_node map to find valid node index)
			for (size_t t_index = 0; t_index < triangles_count; t_index++)
			{
				size_t p1 = triangle_nodes[3 * t_index];
				size_t p2 = triangle_nodes[3 * t_index + 1];
				size_t p3 = triangle_nodes[3 * t_index + 2];

				ccl::float2 uv_value_0 = ccl::make_float2(uv_values[p1 * 3], uv_values[p1 * 3 + 1]);
				ccl::float2 uv_value_1 = ccl::make_float2(uv_values[p2 * 3], uv_values[p2 * 3 + 1]);
				ccl::float2 uv_value_2 = ccl::make_float2(uv_values[p3 * 3], uv_values[p3 * 3 + 1]);

				uv_attribute_data[0] = ccl::make_float2(uv_value_0.x, uv_value_0.y);
				uv_attribute_data[1] = ccl::make_float2(uv_value_1.x, uv_value_1.y);
				uv_attribute_data[2] = ccl::make_float2(uv_value_2.x, uv_value_2.y);
				uv_attribute_data += 3;

				if (uv_index == 0)
				{
					default_uv[0] = ccl::make_float2(uv_value_0.x, uv_value_0.y);
					default_uv[1] = ccl::make_float2(uv_value_1.x, uv_value_1.y);
					default_uv[2] = ccl::make_float2(uv_value_2.x, uv_value_2.y);
					default_uv += 3;
				}
			}
		}
	}
}

void sync_ice_attributes(ccl::Scene* scene, ccl::Mesh* mesh, const XSI::Geometry &xsi_geometry, bool use_triangles, ULONG vertex_count, ULONG nodes_count, const std::vector<LONG>& nodes_to_vertex)
{
	XSI::CRefArray xsi_ice_attributes = xsi_geometry.GetICEAttributes();

	for (LONG i = 0; i < xsi_ice_attributes.GetCount(); i++)
	{
		XSI::ICEAttribute xsi_attribute(xsi_ice_attributes[i]);
		XSI::CString xsi_attr_name = xsi_attribute.GetName();
		ccl::ustring attr_name = ccl::ustring(xsi_attr_name.GetAsciiString());

		if (mesh->need_attribute(scene, attr_name))
		{
			// get the context, type and structure
			XSI::siICENodeContextType attr_context = xsi_attribute.GetContextType();
			// siICENodeContextComponent0D - One element per vertex or point
			// siICENodeContextSingleton - Only one element; for example, the transformation matrix of a geometry.
			XSI::siICENodeDataType attr_data_type = xsi_attribute.GetDataType();
			XSI::siICENodeStructureType attr_structure = xsi_attribute.GetStructureType();
			// we can export either per-point single values or object single value

			if ((attr_context == XSI::siICENodeContextComponent0D || attr_context == XSI::siICENodeContextSingleton) && attr_structure == XSI::siICENodeStructureSingle)
			{
				bool is_array = attr_context == XSI::siICENodeContextComponent0D;
				if (attr_data_type == XSI::siICENodeDataVector3)
				{
					XSI::CICEAttributeDataArrayVector3f attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = !use_triangles ? mesh->subd_attributes.add(attr_name, ccl::TypeVector, ccl::ATTR_ELEMENT_VERTEX) : mesh->attributes.add(attr_name, ccl::TypeVector, ccl::ATTR_ELEMENT_VERTEX);
					cycles_attribute->flags |= ccl::ATTR_SUBDIVIDED;

					ccl::float3* cyc_attr_data = cycles_attribute->data_float3();
					if (use_triangles)
					{
						for (size_t node_index = 0; node_index < nodes_count; node_index++)
						{
							*cyc_attr_data = vector3_to_float3(attr_data[is_array ? nodes_to_vertex[node_index] : 0]);
							cyc_attr_data++;
						}
					}
					else
					{
						for (size_t v_index = 0; v_index < vertex_count; v_index++)
						{
							*cyc_attr_data = vector3_to_float3(attr_data[is_array ? v_index : 0]);
							cyc_attr_data++;
						}
					}
				}
				else if (attr_data_type == XSI::siICENodeDataVector2)
				{
					XSI::CICEAttributeDataArrayVector2f attr_data;
					xsi_attribute.GetDataArray(attr_data);
					
					ccl::Attribute* cycles_attribute = !use_triangles ? mesh->subd_attributes.add(attr_name, ccl::TypeFloat2, ccl::ATTR_ELEMENT_VERTEX) : mesh->attributes.add(attr_name, ccl::TypeFloat2, ccl::ATTR_ELEMENT_VERTEX);
					cycles_attribute->flags |= ccl::ATTR_SUBDIVIDED;

					ccl::float2* cyc_attr_data = cycles_attribute->data_float2();

					if (use_triangles)
					{
						for (size_t node_index = 0; node_index < nodes_count; node_index++)
						{
							*cyc_attr_data = vector2_to_float2(attr_data[is_array ? nodes_to_vertex[node_index] : 0]);
							cyc_attr_data++;
						}
					}
					else
					{
						for (size_t v_index = 0; v_index < vertex_count; v_index++)
						{
							*cyc_attr_data = vector2_to_float2(attr_data[is_array ? v_index : 0]);
							cyc_attr_data++;
						}
					}
				}
				else if (attr_data_type == XSI::siICENodeDataColor4)
				{
					XSI::CICEAttributeDataArrayColor4f attr_data;
					xsi_attribute.GetDataArray(attr_data);

					ccl::Attribute* cycles_attribute = !use_triangles ? mesh->subd_attributes.add(attr_name, ccl::TypeColor, ccl::ATTR_ELEMENT_VERTEX) : mesh->attributes.add(attr_name, ccl::TypeColor, ccl::ATTR_ELEMENT_VERTEX);
					cycles_attribute->flags |= ccl::ATTR_SUBDIVIDED;

					ccl::float4* cyc_attr_data = cycles_attribute->data_float4();

					if (use_triangles)
					{
						for (size_t node_index = 0; node_index < nodes_count; node_index++)
						{
							*cyc_attr_data = color4_to_float4(attr_data[is_array ? nodes_to_vertex[node_index] : 0]);
							cyc_attr_data++;
						}
					}
					else
					{
						for (size_t v_index = 0; v_index < vertex_count; v_index++)
						{
							*cyc_attr_data = color4_to_float4(attr_data[is_array ? v_index : 0]);
							cyc_attr_data++;
						}
					}
				}
				else if (attr_data_type == XSI::siICENodeDataBool || attr_data_type == XSI::siICENodeDataFloat || attr_data_type == XSI::siICENodeDataLong)
				{
					ccl::Attribute* cycles_attribute = !use_triangles ? mesh->subd_attributes.add(attr_name, ccl::TypeFloat, ccl::ATTR_ELEMENT_VERTEX) : mesh->attributes.add(attr_name, ccl::TypeFloat, ccl::ATTR_ELEMENT_VERTEX);
					cycles_attribute->flags |= ccl::ATTR_SUBDIVIDED;

					float* cyc_attr_data = cycles_attribute->data_float();

					if (attr_data_type == XSI::siICENodeDataBool)
					{
						XSI::CICEAttributeDataArrayBool attr_data;
						xsi_attribute.GetDataArray(attr_data);

						if (use_triangles)
						{
							for (size_t node_index = 0; node_index < nodes_count; node_index++)
							{
								*cyc_attr_data = attr_data[is_array ? nodes_to_vertex[node_index] : 0] ? 1.0f : 0.0f;
								cyc_attr_data++;
							}
						}
						else
						{
							for (size_t v_index = 0; v_index < vertex_count; v_index++)
							{
								*cyc_attr_data = attr_data[is_array ? v_index : 0] ? 1.0f : 0.0f;
								cyc_attr_data++;
							}
						}

					}
					else if (attr_data_type == XSI::siICENodeDataFloat)
					{
						XSI::CICEAttributeDataArrayFloat attr_data;
						xsi_attribute.GetDataArray(attr_data);

						if (use_triangles)
						{
							for (size_t node_index = 0; node_index < nodes_count; node_index++)
							{
								*cyc_attr_data = attr_data[is_array ? nodes_to_vertex[node_index] : 0];
								cyc_attr_data++;
							}
						}
						else
						{
							for (size_t v_index = 0; v_index < vertex_count; v_index++)
							{
								*cyc_attr_data = attr_data[is_array ? v_index : 0];
								cyc_attr_data++;
							}
						}
					}
					else if (attr_data_type == XSI::siICENodeDataLong)
					{
						XSI::CICEAttributeDataArrayLong attr_data;
						xsi_attribute.GetDataArray(attr_data);

						if (use_triangles)
						{
							for (size_t node_index = 0; node_index < nodes_count; node_index++)
							{
								*cyc_attr_data = (float)attr_data[is_array ? nodes_to_vertex[node_index] : 0];
								cyc_attr_data++;
							}
						}
						else
						{
							for (size_t v_index = 0; v_index < vertex_count; v_index++)
							{
								*cyc_attr_data = (float)attr_data[is_array ? v_index : 0];
								cyc_attr_data++;
							}
						}
					}
				}
				
			}
		}
	}
}