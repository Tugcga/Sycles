#pragma once
#include <xsi_property.h>
#include <xsi_arrayparameter.h>
#include <xsi_time.h>
#include <xsi_x3dobject.h>

#include "scene/mesh.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/volume.h"
#include "scene/pointcloud.h"

#include "../../update_context.h"
#include "../../../render_base/type_enums.h"

// cyc_geometry
ccl::uint get_ray_visibility(const XSI::CParameterRefArray& property_params, const XSI::CTime& eval_time);
// common object parameters for hair and meshes
void sync_geometry_object_parameters(ccl::Scene* scene, ccl::Object* object, XSI::X3DObject& xsi_object, XSI::CString& lightgroup, bool& out_motion_deform, const XSI::CString& property_name, const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time);

// cyc_polymesh
ccl::Mesh* build_primitive(ccl::Scene* scene, int vertex_count, float* vertices, int faces_count, int* face_sizes, int* face_indexes, bool smooth = false);
ccl::Mesh* build_primitive(ccl::Scene* scene, XSI::siICEShapeType shape_type);
ccl::Mesh* sync_polymesh_object(ccl::Scene* scene, ccl::Object* mesh_object, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_polymesh(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_hair
ccl::Hair* sync_hair_object(ccl::Scene* scene, ccl::Object* hair_object, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_hair(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_hair_property(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_pointcloud
PointcloudType get_pointcloud_type(XSI::X3DObject& xsi_object, const XSI::CTime& eval_time);
bool is_pointcloud_instances(const XSI::X3DObject& xsi_object, const XSI::CTime& eval_time);
bool is_valid_shape(XSI::siICEShapeType shape_type);
XSI::MATH::CTransformation build_point_transform(const XSI::MATH::CVector3f& position, const XSI::MATH::CRotationf& rotation, float size, const XSI::MATH::CVector3f& scale, bool is_scale_define);
std::vector<std::vector<XSI::MATH::CTransformation>> build_time_points_transforms(const XSI::X3DObject& xsi_object, const std::vector<float>& motion_times);
void sync_point_primitive_shape(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::siICEShapeType shape_type, const XSI::MATH::CColor4f& color, const std::vector<XSI::MATH::CTransformation>& point_tfms, XSI::X3DObject& xsi_pointcloud, const XSI::CTime& eval_time);

// cycs_strands
bool is_pointcloud_strands(const XSI::X3DObject& xsi_object);
ccl::Hair* sync_strands_object(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_strands(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_points
bool is_pointcloud_points(XSI::X3DObject& xsi_object, const XSI::CTime& eval_time);
ccl::PointCloud* sync_points_object(ccl::Scene* scene, ccl::Object* points_object, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_points(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_points_property(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);

// cyc_volume
bool is_pointcloud_volume(const XSI::X3DObject& xsi_object, const XSI::CTime& eval_time);
ccl::Volume* sync_volume_object(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_volume(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);
XSI::CStatus update_volume_property(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object);