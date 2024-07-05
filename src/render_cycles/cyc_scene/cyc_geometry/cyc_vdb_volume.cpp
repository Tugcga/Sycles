#include <xsi_x3dobject.h>
#include <xsi_material.h>

#include "scene/scene.h"
#include "scene/object.h"

#include "../cyc_loaders/cyc_loaders.h"
#include "../../../render_cycles/update_context.h"
#include "cyc_geometry.h"
#include "../../../render_cycles/cyc_primitives/vdb_primitive.h"
#include "../../../utilities/math.h"
#include "../../../utilities/strings.h"
#include "../../../utilities/arrays.h"

void add_vdb_to_volume(ccl::Scene* scene, ccl::Volume* volume_geom, const XSI::CTime& eval_time, const XSI::X3DObject& xsi_object, const VDBData& vdb_data, ULONG index, int frame, const XSI::CString& file_path,
	bool is_std, ccl::AttributeStandard std, const ccl::ustring& attr_name, bool is_vector)
{
	ccl::Attribute* attr = NULL;
	if (is_std)
	{
		attr = volume_geom->attributes.add(std);
	}
	else
	{
		attr = volume_geom->attributes.add(attr_name, is_vector ? ccl::TypeDesc::TypeVector : ccl::TypeDesc::TypeFloat, ccl::ATTR_ELEMENT_VOXEL);
	}

	XSIVDBLoader* vdb_loader = new XSIVDBLoader(vdb_data.grids[index], xsi_object.GetObjectID(), file_path, index, vdb_data.grid_names[index].GetAsciiString());
	ccl::ImageParams volume_params;
	volume_params.frame = frame;

	attr->data_voxel() = scene->image_manager->add_image(vdb_loader, volume_params, false);
}

bool is_vector_type(openvdb::GridBase::ConstPtr grid)
{
	if (grid->isType<openvdb::Vec3fGrid>() ||
		grid->isType<openvdb::Vec3IGrid>() ||
		grid->isType<openvdb::Vec3dGrid>())
	{
		return true;
	}
	else
	{
		return false;
	}
}

void sync_vdb_volume_geom_process(ccl::Scene* scene, ccl::Volume* volume_geom, UpdateContext* update_context, XSI::X3DObject& xsi_object, const VDBData& vdb_data, const XSI::CString &file_path)
{
	volume_geom->name = combine_geometry_name(xsi_object, "vdb").GetAsciiString();
	bool force_load_all_grids = true;

	XSI::CTime eval_time = update_context->get_time();

	sync_volume_parameters(volume_geom, xsi_object, eval_time);
	// read velocity scale
	XSI::CustomPrimitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));
	XSI::CParameterRefArray& xsi_prim_params = xsi_prim.GetParameters();
	float velocity_scale = xsi_prim_params.GetValue("velocty_scale");
	// and setup it
	volume_geom->set_velocity_scale(velocity_scale);

	// read velocity grid names
	XSI::CString velocity_name = xsi_prim_params.GetValue("velocity_name");
	XSI::CString velocity_x_name = xsi_prim_params.GetValue("velocity_x_name");
	XSI::CString velocity_y_name = xsi_prim_params.GetValue("velocity_y_name");
	XSI::CString velocity_z_name = xsi_prim_params.GetValue("velocity_z_name");

	int frame = get_frame(eval_time);
	std::vector<XSI::CString> loaded_grid_names(0);

	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_DENSITY))
	{
		int index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_DENSITY));
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_DENSITY, ccl::ustring(""), false);
			loaded_grid_names.push_back(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_DENSITY));
		}
	}

	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_COLOR))
	{
		int index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_COLOR));
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_COLOR, ccl::ustring(""), false);
			loaded_grid_names.push_back(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_COLOR));
		}
	}

	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_FLAME))
	{
		int index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_FLAME));
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_FLAME, ccl::ustring(""), false);
			loaded_grid_names.push_back(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_FLAME));
		}
	}

	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_HEAT))
	{
		int index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_HEAT));
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_HEAT, ccl::ustring(""), false);
			loaded_grid_names.push_back(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_HEAT));
		}
	}

	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_TEMPERATURE))
	{
		int index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_TEMPERATURE));
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_TEMPERATURE, ccl::ustring(""), false);
			loaded_grid_names.push_back(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_TEMPERATURE));
		}
	}

	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_VELOCITY))
	{
		int index = vdb_data.get_grid_index(velocity_name);
		XSI::CString loaded_name = velocity_name;
		if (index == -1)
		{// no grid with the custom velocity name
			// try to find grid with the default velocity name
			index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY));
			loaded_name = ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY);
		}

		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_VELOCITY, ccl::ustring(""), false);
			loaded_grid_names.push_back(loaded_name);
		}
	}

	// also for different velocity components
	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_VELOCITY_X))
	{
		int index = vdb_data.get_grid_index(velocity_x_name);
		XSI::CString loaded_name = velocity_x_name;
		if (index == -1)
		{
			index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY_X));
			loaded_name = ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY_X);
		}
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_VELOCITY_X, ccl::ustring(""), false);
			loaded_grid_names.push_back(loaded_name);
		}
	}
	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_VELOCITY_Y))
	{
		int index = vdb_data.get_grid_index(velocity_y_name);
		XSI::CString loaded_name = velocity_y_name;
		if (index == -1)
		{
			index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY_Y));
			loaded_name = ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY_Y);
		}
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_VELOCITY_Y, ccl::ustring(""), false);
			loaded_grid_names.push_back(loaded_name);
		}
	}
	if (force_load_all_grids || volume_geom->need_attribute(scene, ccl::ATTR_STD_VOLUME_VELOCITY_Z))
	{
		int index = vdb_data.get_grid_index(velocity_z_name);
		XSI::CString loaded_name = velocity_z_name;
		if (index == -1)
		{
			index = vdb_data.get_grid_index(ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY_Z));
			loaded_name = ccl::Attribute::standard_name(ccl::ATTR_STD_VOLUME_VELOCITY_Z);
		}
		if (index >= 0)
		{
			add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, index, frame, file_path, true, ccl::ATTR_STD_VOLUME_VELOCITY_Z, ccl::ustring(""), false);
			loaded_grid_names.push_back(loaded_name);
		}
	}

	// next we should check what grid shader needs and load it
	for (ULONG i = 0; i < vdb_data.grids_count; i++)
	{
		XSI::CString attr_name = vdb_data.grid_names[i];
		if (!is_contains(loaded_grid_names, attr_name))
		{
			ccl::ustring a_name = ccl::ustring(attr_name.GetAsciiString());
			if (force_load_all_grids || volume_geom->need_attribute(scene, a_name))
			{
				add_vdb_to_volume(scene, volume_geom, eval_time, xsi_object, vdb_data, i, frame, file_path, false, ccl::ATTR_STD_NONE, a_name, is_vector_type(vdb_data.grids[i]));
				loaded_grid_names.push_back(attr_name);
			}
		}
	}
}

ccl::Volume* sync_vdb_volume_object(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::X3DObject& xsi_object, const VDBData& vdb_data)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	XSI::CustomPrimitive xsi_primitive(xsi_object.GetActivePrimitive(eval_time));
	XSI::CParameterRefArray primitive_parameters = xsi_primitive.GetParameters();

	XSI::CString lightgroup = "";
	sync_vdb_object_parameters(scene, object, xsi_object, lightgroup, primitive_parameters, render_parameters, eval_time);
	update_context->add_lightgroup(lightgroup);

	ULONG xsi_primitive_id = xsi_primitive.GetObjectID();
	if (update_context->is_geometry_exists(xsi_primitive_id))
	{
		size_t geo_index = update_context->get_geometry_index(xsi_primitive_id);
		ccl::Geometry* cyc_geo = scene->geometry[geo_index];
		if (cyc_geo->geometry_type == ccl::Geometry::Type::VOLUME)
		{
			return static_cast<ccl::Volume*>(scene->geometry[geo_index]);
		}
	}

	ccl::Volume* volume_geom = scene->create_node<ccl::Volume>();

	XSI::Material xsi_material = xsi_object.GetMaterial();
	ULONG xsi_material_id = xsi_material.GetObjectID();
	size_t shader_index = 0;
	if (update_context->is_material_exists(xsi_material_id))
	{
		shader_index = update_context->get_xsi_material_cycles_index(xsi_material_id);
	}

	ccl::array<ccl::Node*> used_shaders;
	used_shaders.push_back_slow(scene->shaders[shader_index]);
	volume_geom->set_used_shaders(used_shaders);

	XSI::CParameterRefArray& prim_params = xsi_primitive.GetParameters();
	XSI::CString file_path = vdbprimitive_inputs_to_path(prim_params, eval_time);

	sync_vdb_volume_geom_process(scene, volume_geom, update_context, xsi_object, vdb_data, file_path);

	update_context->add_geometry_index(xsi_primitive_id, scene->geometry.size() - 1);

	return volume_geom;
}

XSI::CStatus update_vdb(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::CustomPrimitive xsi_primitive(xsi_object.GetActivePrimitive(eval_time));
	XSI::CParameterRefArray primitive_parameters = xsi_primitive.GetParameters();

	ULONG xsi_object_id = xsi_object.GetObjectID();

	if (xsi_primitive.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_vdb_object_parameters(scene, object, xsi_object, lightgroup, primitive_parameters, render_parameters, eval_time, false);
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_primitive.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::VOLUME)
			{
				ccl::Volume* volume_geom = static_cast<ccl::Volume*>(geometry);
				volume_geom->clear(true);

				XSI::CParameterRefArray& prim_params = xsi_primitive.GetParameters();
				XSI::CString file_path = vdbprimitive_inputs_to_path(prim_params, eval_time);
				VDBData vdb_data = get_vdb_data(xsi_primitive);

				sync_vdb_volume_geom_process(scene, volume_geom, update_context, xsi_object, vdb_data, file_path);

				volume_geom->tag_update(scene, true);
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