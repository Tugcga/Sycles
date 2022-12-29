#pragma once
#include "util/types.h"
#include "util/vector.h"
#include "scene/mesh.h"

#include "mikktspace.hh"

#include <xsi_longarray.h>

#include <string>

template<bool is_subd> struct MikkMeshWrapper 
{
    MikkMeshWrapper(ccl::ustring uv_name, const ccl::Mesh* mesh, ccl::float3* tangent, float* tangent_sign)
        : mesh(mesh), texface(NULL), orco(NULL), tangent(tangent), tangent_sign(tangent_sign)
    {
        const ccl::AttributeSet& attributes = is_subd ? mesh->subd_attributes : mesh->attributes;

        ccl::Attribute* attr_vN = attributes.find(ccl::ATTR_STD_VERTEX_NORMAL);
        vertex_normal = attr_vN->data_float3();

        ccl::Attribute* attr_uv = attributes.find(uv_name);
        if (attr_uv != NULL)
        {
            texface = attr_uv->data_float2();
        }
    }

    int GetNumFaces()
    {
        if constexpr (is_subd) 
        {
            return mesh->get_num_subd_faces();
        }
        else 
        {
            return mesh->num_triangles();
        }
    }

    int GetNumVerticesOfFace(const int face_num)
    {
        if constexpr (is_subd) 
        {
            return mesh->get_subd_num_corners()[face_num];
        }
        else 
        {
            return 3;
        }
    }

    int CornerIndex(const int face_num, const int vert_num)
    {
        if constexpr (is_subd) 
        {
            const Mesh::SubdFace& face = mesh->get_subd_face(face_num);
            return face.start_corner + vert_num;
        }
        else 
        {
            return face_num * 3 + vert_num;
        }
    }

    int VertexIndex(const int face_num, const int vert_num)
    {
        int corner = CornerIndex(face_num, vert_num);
        if constexpr (is_subd) 
        {
            return mesh->get_subd_face_corners()[corner];
        }
        else 
        {
            return mesh->get_triangles()[corner];
        }
    }

    mikk::float3 GetPosition(const int face_num, const int vert_num)
    {
        const ccl::float3 vP = mesh->get_verts()[VertexIndex(face_num, vert_num)];
        return mikk::float3(vP.x, vP.y, vP.z);
    }

    mikk::float3 GetTexCoord(const int face_num, const int vert_num)
    {
        if (texface != NULL) 
        {
            const int corner_index = CornerIndex(face_num, vert_num);
            ccl::float2 tfuv = texface[corner_index];
            return mikk::float3(tfuv.x, tfuv.y, 1.0f);
        }
        else if (orco != NULL) 
        {
            const int vertex_index = VertexIndex(face_num, vert_num);
            const ccl::float2 uv = map_to_sphere((orco[vertex_index] + orco_loc) * inv_orco_size);
            return mikk::float3(uv.x, uv.y, 1.0f);
        }
        else 
        {
            return mikk::float3(0.0f, 0.0f, 1.0f);
        }
    }

    mikk::float3 GetNormal(const int face_num, const int vert_num)
    {
        float3 vN;
        if (is_subd) 
        {
            const ccl::Mesh::SubdFace& face = mesh->get_subd_face(face_num);
            if (face.smooth) 
            {
                const int vertex_index = VertexIndex(face_num, vert_num);
                vN = vertex_normal[vertex_index];
            }
            else 
            {
                vN = face.normal(mesh);
            }
        }
        else 
        {
            if (mesh->get_smooth()[face_num]) 
            {
                const int vertex_index = VertexIndex(face_num, vert_num);
                vN = vertex_normal[vertex_index];
            }
            else
            {
                const Mesh::Triangle tri = mesh->get_triangle(face_num);
                vN = tri.compute_normal(&mesh->get_verts()[0]);
            }
        }
        return mikk::float3(vN.x, vN.y, vN.z);
    }

    void SetTangentSpace(const int face_num, const int vert_num, mikk::float3 T, bool orientation)
    {
        const int corner_index = CornerIndex(face_num, vert_num);
        tangent[corner_index] = ccl::make_float3(T.x, T.y, T.z);
        if (tangent_sign != NULL) 
        {
            tangent_sign[corner_index] = orientation ? 1.0f : -1.0f;
        }
    }

    const ccl::Mesh* mesh;
    int num_faces;

    ccl::float3* vertex_normal;
    ccl::float2* texface;
    ccl::float3* orco;
    ccl::float3 orco_loc, inv_orco_size;

    ccl::float3* tangent;
    float* tangent_sign;
};

void mikk_compute_tangents(ccl::Mesh* mesh, std::string uv_name, bool need_sign);