#pragma once
#include <xsi_property.h>
#include <xsi_arrayparameter.h>
#include <xsi_time.h>

#include "scene/mesh.h"
#include "scene/scene.h"
#include "scene/object.h"

// cyc_geometry
ccl::uint get_ray_visibility(const XSI::CParameterRefArray& property_params, const XSI::CTime& eval_time);
// common object parameters for hair and meshes
void sync_geometry_object_parameters(ccl::Scene* scene, ccl::Object* object, XSI::X3DObject& xsi_object, XSI::CString& lightgroup, bool& out_motion_deform, const XSI::CString& property_name, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time);

// cyc_polymesh
ccl::Mesh* build_primitive(ccl::Scene* scene, int vertex_count, float* vertices, int faces_count, int* face_sizes, int* face_indexes);
ccl::Mesh* sync_polymesh_object(ccl::Scene* scene, ccl::Object* mesh_object, UpdateContext* update_context, XSI::X3DObject& xsi_object, const XSI::CParameterRefArray& render_parameters);
XSI::CStatus update_polymesh(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object, const XSI::CParameterRefArray& render_parameters);

// cyc_hair
ccl::Hair* sync_hair_object(ccl::Scene* scene, ccl::Object* hair_object, UpdateContext* update_context, XSI::X3DObject& xsi_object, const XSI::CParameterRefArray& render_parameters);
XSI::CStatus update_hair(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object, const XSI::CParameterRefArray& render_parameters);