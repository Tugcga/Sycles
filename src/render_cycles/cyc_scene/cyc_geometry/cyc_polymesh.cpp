#include "scene/mesh.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/hair.h"

#include <xsi_x3dobject.h>

#include "../../update_context.h"

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

void sync_polymesh_object(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object)
{

}