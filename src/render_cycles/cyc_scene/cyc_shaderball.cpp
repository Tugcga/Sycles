#include <cmath>

#include "scene/scene.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/attribute.h"
#include "scene/light.h"
#include "util/math_float3.h"

#include <xsi_x3dobject.h>

#include "../../render_base/type_enums.h"
#include "primitives_geometry.h"
#include "cyc_scene.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"

void sync_shaderball_hero(ccl::Scene* scene, const XSI::X3DObject &xsi_object, int shader_index, ShaderballType shaderball_type)
{
	ccl::Mesh* mesh = NULL;
	if (shaderball_type == ShaderballType_Texture)
	{// for texture use plain in xy plane
		mesh = scene->create_node<ccl::Mesh>();
		ccl::array<ccl::float3> vertex_coordinates(4);
		vertex_coordinates[0] = ccl::make_float3(-1.0, -1.0, 0.0);
		vertex_coordinates[1] = ccl::make_float3(1.0, -1.0, 0.0);
		vertex_coordinates[2] = ccl::make_float3(1.0, 1.0, 0.0);
		vertex_coordinates[3] = ccl::make_float3(-1.0, 1.0, 0.0);

		size_t num_triangles = 2;
		mesh->reserve_mesh(vertex_coordinates.size(), num_triangles);
		mesh->set_verts(vertex_coordinates);

		mesh->add_triangle(0, 1, 2, 0, false);
		mesh->add_triangle(0, 2, 3, 0, false);

		// add uv coordinates
		ccl::Attribute* uv_attr = mesh->attributes.add(ccl::ATTR_STD_UV, ccl::ustring("uv"));
		ccl::float2* uv_data = uv_attr->data_float2();
		uv_data[0] = ccl::make_float2(0.0, 0.0);
		uv_data[1] = ccl::make_float2(1.0, 0.0);
		uv_data[2] = ccl::make_float2(1.0, 1.0);
		uv_data[3] = ccl::make_float2(0.0, 0.0);
		uv_data[4] = ccl::make_float2(1.0, 1.0);
		uv_data[5] = ccl::make_float2(0.0, 1.0);
	}
	else
	{// for material or shader use xsi_object
		// use the cube
		// mesh = build_primitive(scene, cube_vertex_count, cube_vertices, cube_faces_count, cube_face_sizes, cube_face_indexes);
		mesh = build_primitive(scene, sphere_vertex_count, sphere_vertices, sphere_faces_count, sphere_face_sizes, sphere_face_indexes);
	}

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);

	mesh->set_used_shaders(used_shaders);
	ccl::Object* object = new ccl::Object();
	object->set_geometry(mesh);
	object->name = "shaderball";
	object->set_asset_name(ccl::ustring("shaderball"));
	ccl::Transform object_tfm = ccl::transform_identity();

	if (shaderball_type == ShaderballType_Material || shaderball_type == ShaderballType_SurfaceShader || shaderball_type == ShaderballType_VolumeShader)
	{
		object_tfm = object_tfm * ccl::transform_scale(6.0, 6.0, 6.0);
	}
	object->set_tfm(object_tfm);
	scene->objects.push_back(object);
}

void sync_one_light(ccl::Scene* scene, const XSI::MATH::CMatrix4 &xsi_matrix, ccl::float2 area_size, ccl::float4 color, bool visible_glossy)
{
	// create the shader
	ccl::Shader* shader = new ccl::Shader();
	shader->name = "Shaderball light shader";
	ccl::ShaderGraph* graph = new ccl::ShaderGraph();
	ccl::EmissionNode* node = new ccl::EmissionNode();
	node->set_color(ccl::make_float3(color.x, color.y, color.z));
	node->set_strength(color.w);
	graph->add(node);
	ccl::ShaderNode* out = graph->output();
	graph->connect(node->output("Emission"), out->input("Surface"));
	shader->set_graph(graph);
	shader->tag_update(scene);

	scene->shaders.push_back(shader);
	int shader_id = scene->shaders.size() - 1;

	// create the light
	ccl::Light* light = scene->create_node<ccl::Light>();
	light->set_shader(scene->shaders[shader_id]);
	light->set_is_enabled(true);

	light->set_light_type(ccl::LightType::LIGHT_AREA);
	light->set_axisu(ccl::make_float3(xsi_matrix.GetValue(0, 0), xsi_matrix.GetValue(0, 1), xsi_matrix.GetValue(0, 2)));
	light->set_axisv(ccl::make_float3(xsi_matrix.GetValue(1, 0), xsi_matrix.GetValue(1, 1), xsi_matrix.GetValue(1, 2)));
	light->set_size(1);
	light->set_is_portal(false);
	light->set_spread(M_PI);

	light->set_strength(ccl::one_float3());
	light->set_round(false);
	light->set_sizeu(area_size.x);
	light->set_sizev(area_size.y);
	light->set_cast_shadow(true);
	light->set_use_mis(true);
	light->set_max_bounces(1024);
	light->set_use_camera(false);
	light->set_use_diffuse(true);
	light->set_use_glossy(visible_glossy);
	light->set_use_transmission(true);
	light->set_use_scatter(true);
	light->set_is_shadow_catcher(false);

	light->set_dir(ccl::make_float3(-1 * xsi_matrix.GetValue(2, 0), -1 * xsi_matrix.GetValue(2, 1), -1 * xsi_matrix.GetValue(2, 2)));
	light->set_co(ccl::make_float3(xsi_matrix.GetValue(3, 0), xsi_matrix.GetValue(3, 1), xsi_matrix.GetValue(3, 2)));
}

void sync_shaderball_light(ccl::Scene* scene, ShaderballType shaderball_type)
{
	if (shaderball_type == ShaderballType_Material || shaderball_type == ShaderballType_SurfaceShader || shaderball_type == ShaderballType_VolumeShader)
	{
		// for materials use three are lights with hardcoded positions
		XSI::MATH::CVector3 light_01_position(3.02, 7.71, 11.39);  // left
		XSI::MATH::CVector3 light_02_position(3.25, 5.79, -12.79);  // right
		XSI::MATH::CVector3 light_03_position(15.63, 2.48, -0.11);  // center

		XSI::MATH::CMatrix4 light_01_matrix(0.91, 0.28, -0.29, 0.0, -0.39, 0.83, -0.39, 0.0, 0.13, 0.47, 0.87, 0.0, light_01_position.GetX(), light_01_position.GetY(), light_01_position.GetZ(), 1.0);
		XSI::MATH::CMatrix4 light_02_matrix(-0.41, 0.86, 0.28, 0.0, 0.89, 0.33, 0.3, 0.0, 0.16, 0.37, -0.91, 0.0, light_02_position.GetX(), light_02_position.GetY(), light_02_position.GetZ(), 1.0);
		XSI::MATH::CMatrix4 light_03_matrix(-0.31, 0.17, -0.93, 0.0, -0.1, 0.97, 0.21, 0.0, 0.94, 0.16, -0.27, 0.0, light_03_position.GetX(), light_03_position.GetY(), light_03_position.GetZ(), 1.0);

		ccl::float2 light_01_area_size = ccl::make_float2(5.8, 20.6);
		ccl::float2 light_02_area_size = ccl::make_float2(12.0, 4.13);
		ccl::float2 light_03_area_size = ccl::make_float2(6.5, 1.0);

		ccl::float4 light_01_color = ccl::make_float4(0.95, 1.0, 1.0, 1800);  // w - intensity for the sahder
		ccl::float4 light_02_color = ccl::make_float4(1.0, 1.0, 0.94, 800);
		ccl::float4 light_03_color = ccl::make_float4(1.0, 1.0, 1.0, 440);

		sync_one_light(scene, light_01_matrix, light_01_area_size, light_01_color, true);
		sync_one_light(scene, light_02_matrix, light_02_area_size, light_02_color, true);
		sync_one_light(scene, light_03_matrix, light_03_area_size, light_03_color, false);
	}
}

void sync_shaderball_camera(ccl::Scene* scene, UpdateContext* update_context, ShaderballType shaderball_type)
{
	ccl::Camera* camera = scene->camera;

	camera->set_full_width(update_context->get_full_width());
	camera->set_full_height(update_context->get_full_height());

	// for shaderball camera aspeac always equal to 1.0
	camera->viewplane.left = -1.0f;
	camera->viewplane.right = 1.0f;
	camera->viewplane.bottom = -1.0f;
	camera->viewplane.top = 1.0f;

	float fov_rad = DEG2RADF(60.0);  // set constant fov
	camera->set_camera_type(ccl::CAMERA_PERSPECTIVE);

	std::vector<float> xsi_camera_tfm(16);
	if (shaderball_type == ShaderballType_Texture)
	{
		XSI::MATH::CTransformation camera_tfm;
		camera_tfm.SetIdentity();
		camera_tfm.SetTranslationFromValues(0.0, 0.0, 1.0f / std::tanf(fov_rad / 2.0f));

		xsi_matrix_to_cycles_array(xsi_camera_tfm, camera_tfm.GetMatrix4(), true);
	}
	else
	{
		xsi_camera_tfm = { 
			0.0, 0.0, -1.0, 0.0,
			-0.34, 0.94, 0.0, 0.0,
			-0.94, -0.34, 0.0, 0.0,
			15.68, 5.89, 0.0, 1.0
		};
	}

	camera->set_matrix(tweak_camera_matrix(get_transform(xsi_camera_tfm), scene->camera->get_camera_type(), scene->camera->get_panorama_type()));

	// fov
	camera->set_fov(fov_rad);

	camera->update(scene);
}