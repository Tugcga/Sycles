#include "scene/scene.h"
#include "scene/object.h"
#include "scene/light.h"

#include <unordered_map>

#include <xsi_ref.h>
#include <xsi_x3dobject.h>
#include <xsi_model.h>
#include <xsi_group.h>
#include <xsi_application.h>
#include <xsi_project.h>
#include <xsi_scene.h>

#include "../../utilities/xsi_properties.h"
#include "../update_context.h"
#include "../../utilities/arrays.h"
#include "../../utilities/maps.h"

enum SET_TYPE
{
	RECEIVER_LIGHT,
	BLOCKER_SHADOW
};

enum OBJECT_TYPE
{
	LIGHT,
	OBJECT
};

void search_group_members(XSI::X3DObject& root,
	const std::unordered_map<std::string, std::vector<ULONG>>& light_group_to_ids,
	const std::unordered_map<std::string, std::vector<ULONG>>& shadow_group_to_ids,
	std::unordered_map<ULONG, std::vector<ULONG>>& light_id_to_lights,
	std::unordered_map<ULONG, std::vector<ULONG>>& shadow_id_to_lights)
{
	if (root.GetType() == "#model")
	{
		XSI::Model root_model(root);

		XSI::CRefArray root_groups = root_model.GetGroups();
		LONG groups_count = root_groups.GetCount();
		for (size_t i = 0; i < groups_count; i++)
		{
			XSI::Group group(root_groups[i]);
			std::string group_name = group.GetFullName().GetAsciiString();

			if (light_group_to_ids.contains(group_name)) { extend_map_by_group_members(light_id_to_lights, group.GetMembers(), light_group_to_ids.at(group_name)); }
			if (shadow_group_to_ids.contains(group_name)) { extend_map_by_group_members(shadow_id_to_lights, group.GetMembers(), shadow_group_to_ids.at(group_name)); }
		}
	}

	XSI::CRefArray children = root.GetChildren();
	LONG children_count = children.GetCount();
	for (size_t i = 0; i < children_count; i++)
	{
		XSI::X3DObject child(children[i]);
		if (child.IsValid())
		{
			search_group_members(child, light_group_to_ids, shadow_group_to_ids, light_id_to_lights, shadow_id_to_lights);
		}
	}
}

void geather_group_names_from_objects(const std::vector<ULONG>& xsi_ids, std::unordered_map<std::string, std::vector<ULONG>>& light_groups, std::unordered_map<std::string, std::vector<ULONG>>& shadow_groups)
{
	for (size_t i = 0; i < xsi_ids.size(); i++)
	{
		ULONG xsi_id = xsi_ids[i];
		XSI::X3DObject xsi_object(XSI::Application().GetObjectFromID(xsi_id));
		if (xsi_object.IsValid())
		{
			XSI::Property ll_prop;
			bool is_prop_exists = get_xsi_object_property(xsi_object, "CyclesLightLinking", ll_prop);
			if (is_prop_exists)
			{
				std::string light_group_name = ((XSI::CString)ll_prop.GetParameterValue("light_group_name")).GetAsciiString();
				std::string shadow_group_name = ((XSI::CString)ll_prop.GetParameterValue("shadow_group_name")).GetAsciiString();

				if (light_group_name.size() > 0) { extend_map(light_groups, light_group_name, xsi_id); }
				if (shadow_group_name.size() > 0) { extend_map(shadow_groups, shadow_group_name, xsi_id); }
			}
		}
	}
}

void define_objects_light_or_shadow_set(ccl::Scene* scene, const std::vector<size_t>& cycles_indices, ccl::uint value, SET_TYPE type)
{
	for (size_t j = 0; j < cycles_indices.size(); j++)
	{
		size_t cyc_index = cycles_indices[j];
		ccl::Object* cyc_object = scene->objects[cyc_index];
		if (type == SET_TYPE::RECEIVER_LIGHT)
		{
			cyc_object->set_receiver_light_set(value);
		}
		else if (type == SET_TYPE::BLOCKER_SHADOW)
		{
			cyc_object->set_blocker_shadow_set(value);
		}

		cyc_object->tag_update(scene);
	}
}

std::vector<std::vector<ULONG>> geather_sets(
	ccl::Scene* scene,
	UpdateContext* update_context,
	const std::vector<ULONG>& xsi_object_ids,
	const std::unordered_map<ULONG, std::vector<ULONG>>& id_to_lights,
	SET_TYPE type)
{
	std::vector<std::vector<ULONG>> sets;
	for (size_t i = 0; i < xsi_object_ids.size(); i++)
	{
		ULONG xsi_id = xsi_object_ids[i];  // this is xsi id
		std::vector<size_t> cycles_indices = update_context->get_object_cycles_indexes(xsi_id);  // this is array of corresponding objects on the render
		if (id_to_lights.contains(xsi_id))
		{
			// extract corresponding list of lights
			std::vector<ULONG> lights = id_to_lights.at(xsi_id);
			// sort this array, this will allow simple method to compare two arrays (this with already stored in light_sets)
			std::sort(lights.begin(), lights.end());
			// find index of lights array in light_sets
			int set_index = get_index_in_array(sets, lights);
			if (set_index != -1)
			{
				// this array already stored in the list
				// set this index
				define_objects_light_or_shadow_set(scene, cycles_indices, set_index + 1, type);
			}
			else
			{
				// there are no set with current lights
				// save this set as a new set
				if (sets.size() < 63)
				{// limit the number of sets to 64: the first one is empty, and others are < 63
					sets.push_back(lights);
					// instead sets.size() - 1 we use sets.size(), because the first non-default set shold be index 1
					define_objects_light_or_shadow_set(scene, cycles_indices, sets.size(), type);
				}
			}
		}
		else
		{
			// object has not defined light linking
			// set it default value = 0
			define_objects_light_or_shadow_set(scene, cycles_indices, 0, type);
		}
	}

	return sets;
}

void define_member_sets(ccl::Scene* scene, const std::vector<std::vector<ULONG>>& sets, ULONG xsi_id, const std::vector<size_t>& cyc_indices, SET_TYPE set_type)
{
	// if yes, then update the mask
	uint64_t mask = 0;
	uint64_t unit = 1;
	for (size_t i = 0; i < sets.size(); i++)
	{
		std::vector<ULONG> set = sets[i];
		if (is_sorted_array_contains_value(set, xsi_id))
		{
			mask = mask | (unit << (i + 1));
		}
	}
	mask = ~mask;  // negate the mask

	for (size_t i = 0; i < cyc_indices.size(); i++)
	{
		size_t cyc_index = cyc_indices[i];
		ccl::Object* object = scene->objects[cyc_index];
		if (set_type == SET_TYPE::RECEIVER_LIGHT) { object->set_light_set_membership(mask); }
		else if (set_type == SET_TYPE::BLOCKER_SHADOW) { object->set_shadow_set_membership(mask); }
		object->tag_update(scene);
	}
}

// we call this method in post_scene stage, when the scene is updated/exported and update context contains data about scene cyncronisation
void sync_light_linking(ccl::Scene* scene, UpdateContext* update_context)
{
	// enumerate all exported objects and check is it contains light linking property or not
	// geather names of all groups, used for light linking
	// store two maps with names as keys and array of ids as value: for light and for shadow
	std::unordered_map<std::string, std::vector<ULONG>> light_group_to_ids;  // key is a group name, value is an array of xsi ids which select this group in the property
	std::unordered_map<std::string, std::vector<ULONG>> shadow_group_to_ids;
	std::vector<ULONG> xsi_light_ids = update_context->get_xsi_light_ids();
	std::vector<ULONG> xsi_object_ids = update_context->get_xsi_object_ids();
	geather_group_names_from_objects(xsi_light_ids, light_group_to_ids, shadow_group_to_ids);
	geather_group_names_from_objects(xsi_object_ids, light_group_to_ids, shadow_group_to_ids);

	// enumerate all groups in the scene
	// for each group check is it name in light or shadow map
	// if no, skip it, this group does not used
	// if the name is a key, then for each object in this group form an array of lights, which uses this group
	// we consider only objects which are keys in object_xsi_to_cyc in update context
	std::unordered_map<ULONG, std::vector<ULONG>> light_id_to_lights;  // for light groups
	std::unordered_map<ULONG, std::vector<ULONG>> shadow_id_to_lights;  // for shadow groups
	XSI::Model root = XSI::Application().GetActiveProject().GetActiveScene().GetRoot();
	search_group_members(root, light_group_to_ids, shadow_group_to_ids, light_id_to_lights, shadow_id_to_lights);

	// next we shold iterate throw scene objects and define receiver_light_set and blocker_shadow_set indices
	// in the same time we should store what light sets corresponds to these indices
	// potential sets are values in light_id_to_lights and shadow_id_to_lights maps
	std::vector<std::vector<ULONG>> light_sets = geather_sets(scene, update_context, xsi_object_ids, light_id_to_lights, SET_TYPE::RECEIVER_LIGHT);
	std::vector<std::vector<ULONG>> shadow_sets = geather_sets(scene, update_context, xsi_object_ids, shadow_id_to_lights, SET_TYPE::BLOCKER_SHADOW);

	// next eterate lights of the scene
	// and for each light define the mask for all sets
	for (size_t i = 0; i < xsi_light_ids.size(); i++)
	{
		ULONG light_id = xsi_light_ids[i];
		std::vector<size_t> light_indices = update_context->get_xsi_light_cycles_indexes(light_id);
		// iterate throw sets and check is light_id in this set
		define_member_sets(scene, light_sets, light_id, light_indices, SET_TYPE::RECEIVER_LIGHT);
		define_member_sets(scene, shadow_sets, light_id, light_indices, SET_TYPE::BLOCKER_SHADOW);
	}

	// the same for objects
	for (size_t i = 0; i < xsi_object_ids.size(); i++)
	{
		ULONG object_id = xsi_object_ids[i];
		std::vector<size_t> object_indices = update_context->get_object_cycles_indexes(object_id);
		define_member_sets(scene, light_sets, object_id, object_indices, SET_TYPE::RECEIVER_LIGHT);
		define_member_sets(scene, shadow_sets, object_id, object_indices, SET_TYPE::BLOCKER_SHADOW);
	}
}