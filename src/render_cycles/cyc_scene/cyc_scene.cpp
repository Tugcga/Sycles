#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/background.h"
#include "scene/camera.h"

#include <xsi_renderercontext.h>
#include <xsi_primitive.h>
#include <xsi_camera.h>
#include <xsi_kinematics.h>

#include "../../utilities/logs.h"

#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))
#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))

static inline ccl::Transform get_transform(std::vector<float>& array)
{
	ccl::ProjectionTransform projection;
	projection.x = ccl::make_float4(array[0], array[1], array[2], array[3]);
	projection.y = ccl::make_float4(array[4], array[5], array[6], array[7]);
	projection.z = ccl::make_float4(array[8], array[9], array[10], array[11]);
	projection.w = ccl::make_float4(array[12], array[13], array[14], array[15]);
	projection = projection_transpose(projection);
	return projection_to_transform(projection);
}

void sync_scene(ccl::Scene *scene, XSI::RendererContext& xsi_render_context)
{
	// for test purpose only we create simple scene with one plane and one cube with sky light
	// from actual xsi scene we get only camera position
	// for meshes use default surface shader (it has index 0)

	// add plane
	ccl::Mesh* plane_mesh = scene->create_node<ccl::Mesh>();

	ccl::array<ccl::Node*> plane_used_shaders;
	plane_used_shaders.push_back_slow(scene->shaders[0]);
	plane_mesh->set_used_shaders(plane_used_shaders);

	ccl::Object* plane_object = new ccl::Object();
	plane_object->set_geometry(plane_mesh);
	plane_object->name = "plane";
	plane_object->set_is_shadow_catcher(true);

	ccl::Transform plane_tfm = ccl::transform_identity();
	plane_object->set_tfm(plane_tfm);
	scene->objects.push_back(plane_object);

	plane_mesh->reserve_mesh(4, 2);  // on plane 4 vertices, 2 triangles
	float plane_radius = 5.0;
	ccl::array<ccl::float3> vertices(4);
	vertices[0] = ccl::make_float3(plane_radius, 0, plane_radius);
	vertices[1] = ccl::make_float3(-plane_radius, 0, plane_radius);
	vertices[2] = ccl::make_float3(-plane_radius, 0, -plane_radius);
	vertices[3] = ccl::make_float3(plane_radius, 0, -plane_radius);
	plane_mesh->set_verts(vertices);

	// triangles
	plane_mesh->add_triangle(0, 1, 2, 0, false);
	plane_mesh->add_triangle(0, 2, 3, 0, false);

	// uvs
	ccl::Attribute* uv_attr = plane_mesh->attributes.add(ccl::ATTR_STD_UV, ccl::ustring("std_uv"));
	ccl::float2* default_uv = uv_attr->data_float2();
	float scale = 0.75f;
	default_uv[0] = ccl::make_float2(scale, scale);
	default_uv[1] = ccl::make_float2(0.0, scale);
	default_uv[2] = ccl::make_float2(0.0, 0.0);
	default_uv[3] = ccl::make_float2(scale, scale);
	default_uv[4] = ccl::make_float2(0.0, 0.0);
	default_uv[5] = ccl::make_float2(scale, 0.0);

	// add cube
	ccl::Geometry* cube_geom = scene->create_node<ccl::Mesh>();

	ccl::array<ccl::Node*> cube_used_shaders;
	cube_used_shaders.push_back_slow(scene->shaders[0]);
	cube_geom->set_used_shaders(cube_used_shaders);

	ccl::Object* cube_object = new ccl::Object();
	cube_object->set_geometry(cube_geom);
	cube_object->name = "cube";
	ccl::Transform cube_tfm = ccl::transform_identity();
	cube_tfm = cube_tfm * ccl::transform_scale(1.0f, 1.0f, 1.0f) * ccl::transform_euler(ccl::make_float3(0, 0, 0)) * ccl::transform_translate(ccl::make_float3(0, 2, 0));
	cube_object->set_tfm(cube_tfm);
	scene->objects.push_back(cube_object);

	ccl::Mesh* cube_mesh = static_cast<ccl::Mesh*>(cube_geom);

	static float p_array[24] = { -1, -1, -1, 1, -1, -1, -1, 1, -1, 1, 1, -1,
							   -1, -1, 1,  1, -1, 1,  -1, 1, 1,  1, 1, 1 };
	int p_array_length = 24;
	static int nverts_array_length = 6;
	static int nverts_array[6] = { 4, 4, 4, 4, 4, 4 };
	static int verts_array[24]{ 0, 2, 3, 1, 0, 1, 5, 4, 0, 4, 6, 2,
							  1, 3, 7, 5, 2, 6, 7, 3, 4, 5, 7, 6 };
	int vertsArrayLength = 24;

	ccl::array<ccl::float3> P;
	ccl::vector<float> UV;
	ccl::vector<int> verts, nverts;

	size_t copyNum = 1;
	float cube_size = 2.0;

	for (size_t c = 0; c < copyNum; c++)
	{
		for (size_t i = 0; i < p_array_length; i += 3)
		{
			P.push_back_slow(ccl::make_float3(
				cube_size * p_array[i + 0], cube_size * p_array[i + 1], cube_size * p_array[i + 2]));
		}
		for (int i = 0; i < vertsArrayLength; i++)
		{
			verts.push_back(verts_array[i]);
		}
		for (int i = 0; i < nverts_array_length; i++)
		{
			nverts.push_back(nverts_array[i]);
		}
	}

	size_t num_triangles = 0;
	for (size_t i = 0; i < nverts.size(); i++)
	{
		num_triangles += nverts[i] - 2;
	}
	cube_mesh->reserve_mesh(P.size(), num_triangles);
	cube_mesh->set_verts(P);

	// create triangles
	int index_offset = 0;

	for (size_t i = 0; i < nverts.size(); i++)  // iterate over polygons
	{
		for (int j = 0; j < nverts[i] - 2; j++)  // for each polygon by n-2
		{
			int v0 = verts[index_offset];
			int v1 = verts[index_offset + j + 1];
			int v2 = verts[index_offset + j + 2];
			cube_mesh->add_triangle(v0, v1, v2, 0, false);
		}

		index_offset += nverts[i];
	}

	// background
	ccl::Shader* bg_shader = scene->default_background;
	ccl::ShaderGraph* bg_graph = new ccl::ShaderGraph();
	ccl::BackgroundNode* bg_node = bg_graph->create_node<ccl::BackgroundNode>();
	bg_node->input("Color")->set(ccl::make_float3(1.0, 1.0, 1.0));
	bg_node->input("Strength")->set(0.25);
	ccl::ShaderNode* bg_out = bg_graph->output();
	bg_graph->add(bg_node);
	bg_graph->connect(bg_node->output("Background"), bg_out->input("Surface"));
	ccl::SkyTextureNode* sky_node = bg_graph->create_node<ccl::SkyTextureNode>();
	sky_node->tex_mapping.rotation = ccl::make_float3(-0.5 * XSI::MATH::PI, 0, 0);
	sky_node->set_sun_rotation(DEG2RADF(45.0));
	sky_node->set_sun_elevation(DEG2RADF(36.0));
	bg_graph->add(sky_node);
	bg_graph->connect(sky_node->output("Color"), bg_node->input("Color"));
	bg_shader->set_graph(bg_graph);
	bg_shader->tag_update(scene);

	scene->background->set_transparent(true);

	// camera
	XSI::Primitive camera_prim(xsi_render_context.GetAttribute("Camera"));
	XSI::X3DObject camera_obj = camera_prim.GetOwners()[0];
	XSI::MATH::CMatrix4 camera_tfm_matrix = camera_obj.GetKinematics().GetGlobal().GetTransform().GetMatrix4();
	std::vector<float> xsi_camera_tfm(16);
	xsi_camera_tfm[0] = camera_tfm_matrix.GetValue(0, 0);
	xsi_camera_tfm[1] = camera_tfm_matrix.GetValue(0, 1);
	xsi_camera_tfm[2] = camera_tfm_matrix.GetValue(0, 2);
	xsi_camera_tfm[3] = camera_tfm_matrix.GetValue(0, 3);

	xsi_camera_tfm[4] = camera_tfm_matrix.GetValue(1, 0);
	xsi_camera_tfm[5] = camera_tfm_matrix.GetValue(1, 1);
	xsi_camera_tfm[6] = camera_tfm_matrix.GetValue(1, 2);
	xsi_camera_tfm[7] = camera_tfm_matrix.GetValue(1, 3);

	xsi_camera_tfm[8] = -1 * camera_tfm_matrix.GetValue(2, 0);
	xsi_camera_tfm[9] = -1 * camera_tfm_matrix.GetValue(2, 1);
	xsi_camera_tfm[10] = -1 * camera_tfm_matrix.GetValue(2, 2);
	xsi_camera_tfm[11] = -1 * camera_tfm_matrix.GetValue(2, 3);

	xsi_camera_tfm[12] = camera_tfm_matrix.GetValue(3, 0);
	xsi_camera_tfm[13] = camera_tfm_matrix.GetValue(3, 1);
	xsi_camera_tfm[14] = camera_tfm_matrix.GetValue(3, 2);
	xsi_camera_tfm[15] = camera_tfm_matrix.GetValue(3, 3);

	// get camera fov
	XSI::Camera xsi_camera(camera_obj);
	float camera_aspect = float(xsi_camera.GetParameterValue("aspect"));
	bool is_horizontal = camera_obj.GetParameterValue("fovtype") == 1;
	float fov_grad = float(camera_obj.GetParameterValue("fov"));
	if ((is_horizontal || camera_aspect <= 1) && (!is_horizontal || camera_aspect >= 1))
	{
		if(camera_aspect <= 1)
		{
			fov_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_grad) / 2.0) * camera_aspect));
		}
		else
		{
			fov_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_grad) / 2.0) / camera_aspect));
		}
	}
	float fov_rad = DEG2RADF(fov_grad);

	scene->camera->set_full_width(xsi_render_context.GetAttribute("ImageWidth"));
	scene->camera->set_full_height(xsi_render_context.GetAttribute("ImageHeight"));
	scene->camera->compute_auto_viewplane();
	scene->camera->set_camera_type(ccl::CAMERA_PERSPECTIVE);
	scene->camera->set_matrix(get_transform(xsi_camera_tfm));
	scene->camera->set_fov(fov_rad);
}