#pragma once
#include "scene/scene.h"
#include "scene/mesh.h"

#include <xsi_geometryaccessor.h>
#include <xsi_longarray.h>
#include <xsi_polygonface.h>
#include <xsi_polygonmesh.h>
#include <xsi_vertex.h>
#include <xsi_geometry.h>

void sync_mesh_attribute_vertex_color(ccl::Scene* scene, ccl::Mesh* mesh, ccl::AttributeSet& attributes, const XSI::CGeometryAccessor& xsi_geo_acc, bool use_triangles, const XSI::CLongArray& triangle_nodes, const XSI::CPolygonFaceRefArray& faces);
void sync_mesh_attribute_random_per_island(ccl::Scene* scene, ccl::Mesh* mesh, ccl::AttributeSet& attributes, bool use_triangles, size_t nodes_count, size_t triangles_count, const XSI::CLongArray& triangle_nodes, const XSI::PolygonMesh& xsi_polymesh, const XSI::CPolygonFaceRefArray& faces);
void sync_mesh_attribute_pointness(ccl::Scene* scene, ccl::Mesh* mesh, bool use_triangles, size_t vertex_count, size_t nodes_count, const XSI::CVertexRefArray& vertices, const XSI::CFloatArray& node_normals, const XSI::PolygonMesh& xsi_polymesh);
void sync_mesh_uvs(ccl::Mesh* mesh, bool use_triangles, size_t triangles_count, size_t nodes_count, const XSI::CRefArray& uv_refs, const XSI::CPolygonFaceRefArray& faces, const XSI::CLongArray& triangle_nodes);
void sync_ice_attributes(ccl::Scene* scene, ccl::Mesh* mesh, const XSI::Geometry& xsi_geometry, bool use_triangles, ULONG vertex_count, ULONG nodes_count, const std::vector<LONG>& nodes_to_vertex);