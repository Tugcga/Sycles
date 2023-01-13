#include "scene/volume.h"
#include "scene/scene.h"
#include "scene/object.h"

#include <xsi_x3dobject.h>
#include <xsi_parameter.h>
#include <xsi_arrayparameter.h>
#include <xsi_time.h>
#include <xsi_primitive.h>
#include <xsi_material.h>
#include <xsi_iceattribute.h>
#include <xsi_iceattributedataarray.h>
#include <xsi_iceattributedataarray2D.h>
#include <xsi_geometry.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include <unordered_map>
#include <unordered_set>
#include <string>

#include "../../update_context.h"
#include "cyc_geometry.h"
#include "../cyc_scene.h"
#include "../cyc_loaders/cyc_loaders.h"
#include "../../../utilities/xsi_properties.h"
#include "../../../utilities/logs.h"
#include "../../../utilities/arrays.h"
#include "../../../render_base/type_enums.h"

bool is_pointcloud_volume(const XSI::X3DObject &xsi_object, const XSI::CTime &eval_time)
{
	// we assume that pointcloud contains volume data
	// iff there are 4 attributes of the form: *, *_size, *_min, *_max, 
	// where all sequentional attributes are 3d-vectors

	XSI::Geometry xsi_geometry = xsi_object.GetActivePrimitive().GetGeometry();
	XSI::CRefArray attributes = xsi_geometry.GetICEAttributes();
	for (size_t i = 0; i < attributes.GetCount(); i++)
	{
		XSI::ICEAttribute xsi_attribute = attributes.GetItem(i);
		XSI::CString xsi_name = xsi_attribute.GetName();
		// try to find _size name
		XSI::CString name_end = xsi_name.GetSubString(xsi_name.Length() - 5, 5);
		if (name_end == XSI::CString("_size"))
		{
			// xsi_attribute has name *_size
			if (xsi_attribute.GetContextType() == XSI::siICENodeContextSingleton &&
				xsi_attribute.GetStructureType() == XSI::siICENodeStructureSingle &&
				xsi_attribute.GetDataType() == XSI::siICENodeDataVector3)
			{
				// size attribute correct
				// try to find the main part for the name (*)
				XSI::CString name_start = xsi_name.GetSubString(0, xsi_name.Length() - 5);
				if (name_start.Length() > 0)
				{
					XSI::ICEAttribute main_attribute = xsi_geometry.GetICEAttributeFromName(name_start);
					if (main_attribute.IsValid())
					{
						XSI::siICENodeContextType attr_context = main_attribute.GetContextType();
						XSI::siICENodeStructureType attr_structure = main_attribute.GetStructureType();
						XSI::siICENodeDataType attr_data = main_attribute.GetDataType();

						if(attr_context == XSI::siICENodeContextSingleton &&
							attr_structure == XSI::siICENodeStructureArray &&
							(attr_data == XSI::siICENodeDataFloat || attr_data == XSI::siICENodeDataVector3 || attr_data == XSI::siICENodeDataColor4))
						{
							// main attribute exist, try to find min and max
							XSI::ICEAttribute min_attribute = xsi_geometry.GetICEAttributeFromName(name_start + "_min");
							
							if (min_attribute.IsValid() &&
								min_attribute.GetContextType() == XSI::siICENodeContextSingleton &&
								min_attribute.GetStructureType() == XSI::siICENodeStructureSingle &&
								min_attribute.GetDataType() == XSI::siICENodeDataVector3)
							{
								// next check max
								XSI::ICEAttribute max_attribute = xsi_geometry.GetICEAttributeFromName(name_start + "_max");
								if (max_attribute.IsValid() &&
									max_attribute.GetContextType() == XSI::siICENodeContextSingleton &&
									max_attribute.GetStructureType() == XSI::siICENodeStructureSingle &&
									max_attribute.GetDataType() == XSI::siICENodeDataVector3)
								{
									// all attributes exists, return true
									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}

// key - main attribute name (*)
// value - type o the main data attribute (float, color or vector)
// the main method is the same as method for getting pointloud type (above)
// but insted retrun true/false, we write data to the map
std::unordered_map<std::string, VolumeAttributeType> build_volume_attributes_map(const XSI::Geometry &xsi_geometry)
{
	std::unordered_map<std::string, VolumeAttributeType> to_return;

	XSI::CRefArray attributes = xsi_geometry.GetICEAttributes();
	for (size_t i = 0; i < attributes.GetCount(); i++)
	{
		XSI::ICEAttribute xsi_attribute = attributes.GetItem(i);
		XSI::CString xsi_name = xsi_attribute.GetName();
		XSI::CString name_end = xsi_name.GetSubString(xsi_name.Length() - 5, 5);
		if (name_end == XSI::CString("_size"))
		{
			if (xsi_attribute.GetContextType() == XSI::siICENodeContextSingleton && xsi_attribute.GetStructureType() == XSI::siICENodeStructureSingle && xsi_attribute.GetDataType() == XSI::siICENodeDataVector3)
			{
				XSI::CString name_start = xsi_name.GetSubString(0, xsi_name.Length() - 5);
				if (name_start.Length() > 0)
				{
					XSI::ICEAttribute main_attribute = xsi_geometry.GetICEAttributeFromName(name_start);
					if (main_attribute.IsValid())
					{
						XSI::siICENodeContextType attr_context = main_attribute.GetContextType();
						XSI::siICENodeStructureType attr_structure = main_attribute.GetStructureType();
						XSI::siICENodeDataType attr_data = main_attribute.GetDataType();

						if (attr_context == XSI::siICENodeContextSingleton && attr_structure == XSI::siICENodeStructureArray && (attr_data == XSI::siICENodeDataFloat || attr_data == XSI::siICENodeDataVector3 || attr_data == XSI::siICENodeDataColor4))
						{
							XSI::ICEAttribute min_attribute = xsi_geometry.GetICEAttributeFromName(name_start + "_min");
							if (min_attribute.IsValid() && min_attribute.GetContextType() == XSI::siICENodeContextSingleton && min_attribute.GetStructureType() == XSI::siICENodeStructureSingle && min_attribute.GetDataType() == XSI::siICENodeDataVector3)
							{
								XSI::ICEAttribute max_attribute = xsi_geometry.GetICEAttributeFromName(name_start + "_max");
								if (max_attribute.IsValid() && max_attribute.GetContextType() == XSI::siICENodeContextSingleton && max_attribute.GetStructureType() == XSI::siICENodeStructureSingle && max_attribute.GetDataType() == XSI::siICENodeDataVector3)
								{
									to_return[std::string(name_start.GetAsciiString())] = attr_data == XSI::siICENodeDataColor4 ? VolumeAttributeType::VolumeAttributeType_Color : (attr_data == XSI::siICENodeDataVector3 ? VolumeAttributeType::VolumeAttributeType_Vector : VolumeAttributeType::VolumeAttributeType_Float);
								}
							}
						}
					}
				}
			}
		}
	}

	return to_return;
}

void sync_volume_parameters(ccl::Volume* volume, XSI::X3DObject& xsi_object, const XSI::CTime &eval_time)
{
	XSI::Property xsi_property;
	bool use_property = get_xsi_object_property(xsi_object, "CyclesVolume", xsi_property);
	float clipping = 0.001f;
	float step_size = 0.0f;
	int object_space = 0;

	if (use_property)
	{
		XSI::CParameterRefArray xsi_params = xsi_property.GetParameters();

		clipping = xsi_params.GetValue("volume_clipping");
		step_size = xsi_params.GetValue("volume_step_size");
		object_space = xsi_params.GetValue("volume_object_space");
	}

	volume->set_clipping(clipping);
	volume->set_step_size(step_size);
	volume->set_object_space(object_space == 0);
}

void sync_volume_attribute(ccl::Scene* scene, ccl::Volume* volume_geom, bool is_std_atribute, ccl::AttributeStandard std_attribute, const std::string &attribute_name, VolumeAttributeType attribute_data_type, const XSI::Primitive &xsi_primitive, const XSI::CTime &eval_time)
{
	ccl::Attribute* attribute = is_std_atribute ? 
		volume_geom->attributes.add(std_attribute) :
		volume_geom->attributes.add(ccl::ustring(attribute_name), attribute_data_type == VolumeAttributeType::VolumeAttributeType_Float ? ccl::TypeFloat : (attribute_data_type == VolumeAttributeType::VolumeAttributeType_Vector ? ccl::TypeVector : ccl::TypeColor), ccl::ATTR_ELEMENT_VOXEL);

	ICEVolumeLoader* ice_loader = new ICEVolumeLoader(attribute_data_type, xsi_primitive, attribute_name, eval_time);

	if (ice_loader->is_empty())
	{
		volume_geom->attributes.remove(attribute);
	}
	else
	{
		ccl::ImageParams volume_params;
		volume_params.frame = eval_time.GetTime();
		attribute->data_voxel() = scene->image_manager->add_image(ice_loader, volume_params, false);
	}
}

void sync_volume_geom_process(ccl::Scene* scene, ccl::Volume* volume_geom, UpdateContext* update_context, const XSI::Primitive &xsi_primitive, XSI::X3DObject &xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();

	sync_volume_parameters(volume_geom, xsi_object, eval_time);

	// we should get all valid combinations of ICE attributes
	// but exports only needed from this list
	XSI::Geometry xsi_geometry = xsi_primitive.GetGeometry(eval_time);
	std::unordered_map<std::string, VolumeAttributeType> volume_attributes_map = build_volume_attributes_map(xsi_geometry);
		
	// there are several built-in volume attribute names: density, color, flame, heat, temperature, velocity
	// so, at first we should export these names, and only then - other names
	std::unordered_set<std::string> exported_names;
	if (volume_geom->need_attribute(scene, ccl::AttributeStandard::ATTR_STD_VOLUME_DENSITY) && volume_attributes_map.contains("density") && volume_attributes_map["density"] == VolumeAttributeType::VolumeAttributeType_Float)
	{
		sync_volume_attribute(scene, volume_geom, true, ccl::AttributeStandard::ATTR_STD_VOLUME_DENSITY, "density", VolumeAttributeType::VolumeAttributeType_Float, xsi_primitive, eval_time);
		exported_names.insert("density");
	}

	if (volume_geom->need_attribute(scene, ccl::AttributeStandard::ATTR_STD_VOLUME_COLOR) && volume_attributes_map.contains("color") && volume_attributes_map["color"] == VolumeAttributeType::VolumeAttributeType_Color)
	{
		sync_volume_attribute(scene, volume_geom, true, ccl::AttributeStandard::ATTR_STD_VOLUME_COLOR, "color", VolumeAttributeType::VolumeAttributeType_Color, xsi_primitive, eval_time);
		exported_names.insert("color");
	}

	if (volume_geom->need_attribute(scene, ccl::AttributeStandard::ATTR_STD_VOLUME_FLAME) && volume_attributes_map.contains("flame") && volume_attributes_map["flame"] == VolumeAttributeType::VolumeAttributeType_Float)
	{
		sync_volume_attribute(scene, volume_geom, true, ccl::AttributeStandard::ATTR_STD_VOLUME_FLAME, "flame", VolumeAttributeType::VolumeAttributeType_Float, xsi_primitive, eval_time);
		exported_names.insert("flame");
	}

	if (volume_geom->need_attribute(scene, ccl::AttributeStandard::ATTR_STD_VOLUME_HEAT) && volume_attributes_map.contains("heat") && volume_attributes_map["heat"] == VolumeAttributeType::VolumeAttributeType_Float)
	{
		sync_volume_attribute(scene, volume_geom, true, ccl::AttributeStandard::ATTR_STD_VOLUME_HEAT, "heat", VolumeAttributeType::VolumeAttributeType_Float, xsi_primitive, eval_time);
		exported_names.insert("heat");
	}

	if (volume_geom->need_attribute(scene, ccl::AttributeStandard::ATTR_STD_VOLUME_TEMPERATURE) && volume_attributes_map.contains("temperature") && volume_attributes_map["temperature"] == VolumeAttributeType::VolumeAttributeType_Float)
	{
		sync_volume_attribute(scene, volume_geom, true, ccl::AttributeStandard::ATTR_STD_VOLUME_TEMPERATURE, "temperature", VolumeAttributeType::VolumeAttributeType_Float, xsi_primitive, eval_time);
		exported_names.insert("temperature");
	}

	if (volume_geom->need_attribute(scene, ccl::AttributeStandard::ATTR_STD_VOLUME_VELOCITY) && volume_attributes_map.contains("velocity") && volume_attributes_map["velocity"] == VolumeAttributeType::VolumeAttributeType_Vector)
	{
		sync_volume_attribute(scene, volume_geom, true, ccl::AttributeStandard::ATTR_STD_VOLUME_VELOCITY, "velocity", VolumeAttributeType::VolumeAttributeType_Vector, xsi_primitive, eval_time);
		exported_names.insert("velocity");
	}

	// next export all other required attributes
	for (auto const& [key, val] : volume_attributes_map)
	{
		if (!exported_names.contains(key) && volume_geom->need_attribute(scene, ccl::ustring(key.c_str())))
		{
			sync_volume_attribute(scene, volume_geom, false, ccl::AttributeStandard::ATTR_STD_NONE, key, val, xsi_primitive, eval_time);
			exported_names.insert(key);
		}
	}
}

ccl::Volume* sync_volume_object(ccl::Scene* scene, ccl::Object* object, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();

	bool motion_deform = false;
	XSI::CString lightgroup = "";
	sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time);
	update_context->add_lightgroup(lightgroup);

	XSI::Primitive xsi_primitive(xsi_object.GetActivePrimitive(eval_time));
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

	sync_volume_geom_process(scene, volume_geom, update_context, xsi_primitive, xsi_object);

	update_context->add_geometry_index(xsi_primitive_id, scene->geometry.size() - 1);

	return volume_geom;
}

XSI::CStatus update_volume(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::CParameterRefArray render_parameters = update_context->get_current_render_parameters();
	XSI::Primitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));

	ULONG xsi_object_id = xsi_object.GetObjectID();

	if (xsi_prim.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		bool motion_deform = false;
		XSI::CString lightgroup = "";
		std::vector<size_t> object_indexes = update_context->get_object_cycles_indexes(xsi_object_id);
		for (size_t i = 0; i < object_indexes.size(); i++)
		{
			size_t index = object_indexes[i];
			ccl::Object* object = scene->objects[index];

			sync_geometry_object_parameters(scene, object, xsi_object, lightgroup, motion_deform, "CyclesPointcloud", render_parameters, eval_time);
		}

		update_context->add_lightgroup(lightgroup);

		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::VOLUME)
			{
				ccl::Volume* volume_geom = static_cast<ccl::Volume*>(geometry);
				volume_geom->clear(true);

				sync_volume_geom_process(scene, volume_geom, update_context, xsi_prim, xsi_object);

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

XSI::CStatus update_volume_property(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject& xsi_object)
{
	XSI::CTime eval_time = update_context->get_time();
	XSI::Primitive xsi_prim(xsi_object.GetActivePrimitive(eval_time));

	ULONG xsi_object_id = xsi_object.GetObjectID();

	if (xsi_prim.IsValid() && update_context->is_object_exists(xsi_object_id))
	{
		ULONG xsi_id = xsi_prim.GetObjectID();
		if (update_context->is_geometry_exists(xsi_id))
		{
			size_t geo_index = update_context->get_geometry_index(xsi_id);
			ccl::Geometry* geometry = scene->geometry[geo_index];
			if (geometry->geometry_type == ccl::Geometry::Type::VOLUME)
			{
				ccl::Volume* volume_geom = static_cast<ccl::Volume*>(geometry);
				sync_volume_parameters(volume_geom, xsi_object, eval_time);
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