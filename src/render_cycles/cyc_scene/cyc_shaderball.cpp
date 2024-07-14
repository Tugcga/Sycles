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
#include "cyc_geometry/cyc_geometry.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"

void sync_shaderball_hero(ccl::Scene* scene, const XSI::X3DObject &xsi_object, int shader_index, ShaderballType shaderball_type)
{
	// WARNING: if shaderball_type is Shader, then default hero object can be plane for some shader nodes
	// for textures we force to use plane

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
		mesh = scene->create_node<ccl::Mesh>();
		XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive();
		XSI::PolygonMesh xsi_polymesh = xsi_primitive.GetGeometry();
		XSI::CGeometryAccessor xsi_geo_acc = xsi_polymesh.GetGeometryAccessor();
		sync_triangle_mesh(scene, mesh, xsi_geo_acc, xsi_polymesh);
	}

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);

	mesh->set_used_shaders(used_shaders);
	ccl::Object* object = new ccl::Object();
	object->set_geometry(mesh);
	object->name = "shaderball";
	object->set_asset_name(ccl::ustring("shaderball"));
	ccl::Transform object_tfm = ccl::transform_identity();

	object->set_tfm(object_tfm);
	scene->objects.push_back(object);
}

void sync_shaderball_background_object(ccl::Scene* scene, UpdateContext* update_context, const XSI::X3DObject& xsi_object, ShaderballType shaderball_type)
{
	if (shaderball_type != ShaderballType::ShaderballType_Texture && shaderball_type != ShaderballType::ShaderballType_Unknown)
	{
		// add background object only for non-txture shaderball
		// at first get all materials
		XSI::Primitive xsi_primitive = xsi_object.GetActivePrimitive();
		XSI::PolygonMesh xsi_polymesh = xsi_primitive.GetGeometry();
		XSI::CGeometryAccessor xsi_geo_acc = xsi_polymesh.GetGeometryAccessor();

		XSI::CRefArray xsi_geo_materials = xsi_geo_acc.GetMaterials();

		XSI::CTime eval_time = update_context->get_time();
		std::vector<XSI::CStringArray> aovs(2);
		aovs[0].Clear();
		aovs[1].Clear();

		ccl::array<ccl::Node*> used_shaders;

		for (size_t i = 0; i < xsi_geo_materials.GetCount(); i++)
		{
			XSI::Material xsi_material = xsi_geo_materials[i];
			ULONG xsi_material_id = xsi_material.GetObjectID();
			if (!update_context->is_material_exists(xsi_material_id))
			{
				// sync this material
				int shader_index = sync_material(scene, xsi_material, eval_time, aovs);
				if (shader_index >= 0)
				{
					// here we assume that background object use material without displacement
					// in any case it never updates, because it's impossible to change material of the background object in the shaderball
					update_context->add_material_index(xsi_material_id, shader_index, false, ShaderballType_Unknown);
					used_shaders.push_back_slow(scene->shaders[shader_index]);
				}
			}
		}

		// next create the mesh
		ccl::Mesh* mesh = scene->create_node<ccl::Mesh>();

		// does not use more general method sync_polymesh_process, because it done not necessary job
		sync_triangle_mesh(scene, mesh, xsi_geo_acc, xsi_polymesh);
		mesh->set_used_shaders(used_shaders);

		ccl::Object* object = new ccl::Object();
		object->set_geometry(mesh);
		object->name = "shaderball_background";
		object->set_asset_name(ccl::ustring("shaderball"));
		ccl::Transform object_tfm = ccl::transform_identity();

		object->set_tfm(object_tfm);
		scene->objects.push_back(object);
	}
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
	light->set_tfm(xsi_matrix_to_transform(xsi_matrix));
	light->set_size(1);
	light->set_is_portal(false);
	light->set_spread(M_PI);

	light->set_strength(ccl::one_float3());
	light->set_ellipse(false);
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
}

void sync_shaderball_light(ccl::Scene* scene, ShaderballType shaderball_type)
{
	if (shaderball_type == ShaderballType_Material || shaderball_type == ShaderballType_SurfaceShader || shaderball_type == ShaderballType_VolumeShader)
	{
		// for materials use three are lights with hardcoded positions
		XSI::MATH::CVector3 light_01_position(4.681608649, 2.37621171135, 1.09074195703);  // left
		XSI::MATH::CVector3 light_02_position(-0.900347601501, 1.80086294527, -4.24738890937);  // right
		XSI::MATH::CVector3 light_03_position(3.80472730759, 4.11287476589, -3.82204509725);  // center

		XSI::MATH::CMatrix4 light_01_matrix(0.285109869921, 0.138573015478, -0.948424947719, 0.0, -0.293566369241, 0.954565501039, 0.0512200261283, 0.0, 0.91243144889, 0.263822333413, 0.312836422861, 0.0, light_01_position.GetX(), light_01_position.GetY(), light_01_position.GetZ(), 1.0);
		XSI::MATH::CMatrix4 light_02_matrix(-0.973217037833, -0.177618927199, 0.145945585655, 0.0, -0.144414184675, 0.966315765653, 0.213022027769, 0.0, -0.178866264388, 0.186240054099, - 0.966085659615, 0.0, light_02_position.GetX(), light_02_position.GetY(), light_02_position.GetZ(), 1.0);
		XSI::MATH::CMatrix4 light_03_matrix(-0.389445327417, 0.7982058482, 0.459564751534, 0.0, 0.729921350278, -0.0368398548379, 0.68253765281, 0.0, 0.561735844825, 0.601257223653, -0.568280381189, 0.0, light_03_position.GetX(), light_03_position.GetY(), light_03_position.GetZ(), 1.0);

		ccl::float2 light_01_area_size = ccl::make_float2(1.25, 7.0);
		ccl::float2 light_02_area_size = ccl::make_float2(1.0, 7.0);
		ccl::float2 light_03_area_size = ccl::make_float2(3.0, 4.0);

		ccl::float4 light_01_color = ccl::make_float4(1.0, 1.0, 1.0, 71.5);  // w - intensity for the sahder
		ccl::float4 light_02_color = ccl::make_float4(1.0, 1.0, 1.0, 70);
		ccl::float4 light_03_color = ccl::make_float4(1.0, 1.0, 1.0, 140);

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
		fov_rad = DEG2RADF(56.0);
		
		XSI::MATH::CTransformation camera_tfm;
		camera_tfm.SetIdentity();
		camera_tfm.SetTranslationFromValues(5.044413951743427, 5.510346057834283, -5.108205571845123);
		camera_tfm.SetRotationFromXYZAnglesValues(-0.6065696235148601, 2.3624776818468876, -1.4689623295538468e-08);

		xsi_matrix_to_cycles_array(xsi_camera_tfm, camera_tfm.GetMatrix4(), true);
	}

	camera->set_matrix(tweak_camera_matrix(get_transform(xsi_camera_tfm), scene->camera->get_camera_type(), scene->camera->get_panorama_type()));

	// fov
	camera->set_fov(fov_rad);

	camera->update(scene);
}