#include <xsi_model.h>
#include <xsi_light.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include "scene/scene.h"
#include "scene/light.h"

#include "../update_context.h"
#include "cyc_scene.h"
#include "../../utilities/logs.h"
#include "../../utilities/math.h"
#include "../../utilities/xsi_properties.h"
#include "cyc_geometry/cyc_geometry.h"

// instance transfom defines by 
// 1. master model transform A
// 2. master object transform B
// 3. root instance transform C
// the result objcet instance transform is B * A^{-1} * C
XSI::MATH::CTransformation calc_instance_object_tfm(const XSI::MATH::CTransformation &master_root_tfm, const XSI::MATH::CTransformation& master_object_tfm, const XSI::MATH::CTransformation& instance_root_tfm)
{
	XSI::MATH::CTransformation master_root_tfm_inverse;
	master_root_tfm_inverse.Invert(master_root_tfm);

	XSI::MATH::CTransformation to_return;

	to_return.Mul(master_object_tfm, master_root_tfm_inverse);
	to_return.MulInPlace(instance_root_tfm);

	return to_return;
}

std::vector<XSI::MATH::CTransformation> calc_instance_object_tfm(const XSI::KinematicState& master_root, const XSI::KinematicState& master_object, const std::vector<XSI::MATH::CTransformation> &instance_root_tfm_array, bool need_motion, const std::vector<float>& motion_times, const XSI::CTime& eval_time)
{
	if (need_motion)
	{
		std::vector<XSI::MATH::CTransformation> to_return(motion_times.size(), XSI::MATH::CTransformation());
		for (size_t i = 0; i < motion_times.size(); i++)
		{
			float time = motion_times[i];
			to_return[i] = calc_instance_object_tfm(master_root.GetTransform(time), master_object.GetTransform(time), instance_root_tfm_array[i]);
		}

		return to_return;
	}
	else
	{
		return { calc_instance_object_tfm(master_root.GetTransform(eval_time), master_object.GetTransform(eval_time), instance_root_tfm_array[0]) };
	};
}

XSI::MATH::CTransformation calc_finall_instance_tfm(XSI::CString& master_object_type, XSI::X3DObject& master_object, const std::vector<ULONG> &xsi_ids, const XSI::KinematicState& xsi_instance_root, const XSI::CTime &eval_time)
{
	XSI::MATH::CTransformation xsi_tfm;

	for (size_t i = 0; i < xsi_ids.size() / 2; i++)
	{
		ULONG master_id = xsi_ids[xsi_ids.size() - 1 - 2 * i - 1];
		ULONG object_id = xsi_ids[xsi_ids.size() - 1 - 2 * i];

		XSI::ProjectItem xsi_master_root_item = XSI::Application().GetObjectFromID(master_id);
		XSI::ProjectItem xsi_master_object_item = XSI::Application().GetObjectFromID(object_id);

		XSI::X3DObject xsi_master_root(xsi_master_root_item);
		XSI::X3DObject xsi_master_object(xsi_master_object_item);

		if (i == 0)
		{
			master_object_type = xsi_master_object.GetType();  // actual object at the end of the array
			master_object = xsi_master_object;
		}

		XSI::MATH::CTransformation xsi_master_root_tfm = xsi_master_root.GetKinematics().GetGlobal().GetTransform(eval_time);
		XSI::MATH::CTransformation xsi_master_object_tfm = xsi_master_object.GetKinematics().GetGlobal().GetTransform(eval_time);

		xsi_master_root_tfm.InvertInPlace();
		xsi_tfm.MulInPlace(xsi_master_object_tfm);
		xsi_tfm.MulInPlace(xsi_master_root_tfm);
	}

	xsi_tfm.MulInPlace(xsi_instance_root.GetTransform(eval_time));

	return xsi_tfm;
}

void update_instance_light_transform(ccl::Scene* scene, UpdateContext* update_context, ULONG xsi_instance_id, const XSI::KinematicState &xsi_instance_root, const XSI::CTime &eval_time)
{
	std::unordered_map<size_t, std::vector<ULONG>> object_index_map = update_context->get_light_from_instance_data(xsi_instance_id);
	for (const auto& [light_index, xsi_ids] : object_index_map)
	{
		ccl::Light* light = scene->lights[light_index];

		if (xsi_ids.size() >= 2)
		{
			XSI::CString master_object_type;
			XSI::X3DObject master_object;
			XSI::MATH::CTransformation xsi_tfm = calc_finall_instance_tfm(master_object_type, master_object, xsi_ids, xsi_instance_root, eval_time);
			
			if (master_object_type == "light")
			{
				XSI::Light xsi_light(master_object);
				sync_light_tfm(light, tweak_xsi_light_transform(xsi_tfm, xsi_light, eval_time).GetMatrix4());

				light->tag_update(scene);
			}
			else if (master_object_type == "cyclesPoint" || master_object_type == "cyclesSun" || master_object_type == "cyclesSpot" || master_object_type == "cyclesArea")
			{
				sync_light_tfm(light, xsi_tfm.GetMatrix4());
				light->tag_update(scene);
			}
		}
	}
}

void update_instance_geometry_transform(ccl::Scene* scene, UpdateContext* update_context, ULONG xsi_instance_id, const XSI::KinematicState& xsi_instance_root, const XSI::CTime& eval_time)
{
	std::unordered_map<size_t, std::vector<ULONG>> object_index_map = update_context->get_geometry_from_instance_data(xsi_instance_id);
	for (const auto& [object_index, xsi_ids] : object_index_map)
	{
		ccl::Object* object = scene->objects[object_index];

		if (xsi_ids.size() >= 2)
		{
			XSI::CString master_object_type;
			XSI::X3DObject master_object;
			XSI::MATH::CTransformation xsi_tfm = calc_finall_instance_tfm(master_object_type, master_object, xsi_ids, xsi_instance_root, eval_time);

			if (master_object_type == "polymsh" || master_object_type == "hair")
			{
				ccl::Transform tfm = xsi_matrix_to_transform(xsi_tfm.GetMatrix4());
				object->set_tfm(tfm);
				object->tag_tfm_modified();

				object->tag_update(scene);
			}
		}
	}
}

// called when we change transform of the instance object
XSI::CStatus update_instance_transform(ccl::Scene* scene, UpdateContext *update_context, const XSI::Model &xsi_model)
{
	// we should update transforms of all subobjects
	ULONG xsi_instance_id = xsi_model.GetObjectID();
	XSI::KinematicState xsi_instance_root = xsi_model.GetKinematics().GetGlobal();
	XSI::CTime eval_time = update_context->get_time();

	if (update_context->is_light_from_instance_data_contains_id(xsi_instance_id))
	{// this isntance contains light sources
		update_instance_light_transform(scene, update_context, xsi_instance_id, xsi_instance_root, eval_time);
		// also update all instances which is relative to this instance (in the case when there are nested instances)
		if (update_context->is_nested_to_host_instances_contains_id(xsi_instance_id))
		{
			std::vector<ULONG> nested_to_host_instances_ids = update_context->get_nested_to_host_instances_ids(xsi_instance_id);
			for (size_t i = 0; i < nested_to_host_instances_ids.size(); i++)
			{
				ULONG host_id = nested_to_host_instances_ids[i];
				XSI::ProjectItem host_item = XSI::Application().GetObjectFromID(host_id);
				XSI::Model host_model(host_item);
				XSI::KinematicState host_kine = host_model.GetKinematics().GetGlobal();

				if (update_context->is_light_from_instance_data_contains_id(host_id))
				{
					update_instance_light_transform(scene, update_context, host_id, host_kine, eval_time);
				}
			}
		}

		return XSI::CStatus::OK;
	}
	else if (update_context->is_geometry_from_instance_data_contains_id(xsi_instance_id))
	{
		update_instance_geometry_transform(scene, update_context, xsi_instance_id, xsi_instance_root, eval_time);
		if (update_context->is_nested_to_host_instances_contains_id(xsi_instance_id))
		{
			std::vector<ULONG> nested_to_host_instances_ids = update_context->get_nested_to_host_instances_ids(xsi_instance_id);
			for (size_t i = 0; i < nested_to_host_instances_ids.size(); i++)
			{
				ULONG host_id = nested_to_host_instances_ids[i];
				XSI::ProjectItem host_item = XSI::Application().GetObjectFromID(host_id);
				XSI::Model host_model(host_item);
				XSI::KinematicState host_kine = host_model.GetKinematics().GetGlobal();

				if (update_context->is_geometry_from_instance_data_contains_id(host_id))
				{
					update_instance_geometry_transform(scene, update_context, host_id, host_kine, eval_time);
				}
			}
		}

		return XSI::CStatus::OK;
	}
	{
		log_message("no " + xsi_model.GetName());
	}

	return XSI::CStatus::Abort;
}

// change transform of the scene object
// we should check, may be this object is a part of some instance
XSI::CStatus update_instance_transform_from_master_object(ccl::Scene* scene, UpdateContext* update_context, XSI::X3DObject &xsi_object)
{
	ULONG xsi_master_object_id = xsi_object.GetObjectID();
	XSI::CTime eval_time = update_context->get_time();

	if (update_context->is_light_id_to_instance_contains_id(xsi_master_object_id))
	{
		std::vector instance_ids = update_context->get_light_id_to_instance_ids(xsi_master_object_id);
		for (size_t i = 0; i < instance_ids.size(); i++)
		{
			ULONG xsi_instance_id = instance_ids[i];
			XSI::ProjectItem xsi_instance_item = XSI::Application().GetObjectFromID(xsi_instance_id);
			XSI::Model xsi_instance_model(xsi_instance_item);
			XSI::KinematicState xsi_instance_kine = xsi_instance_model.GetKinematics().GetGlobal();
			
			update_instance_light_transform(scene, update_context, xsi_instance_id, xsi_instance_kine, eval_time);
		}

		return XSI::CStatus::OK;
	}
	else if (update_context->is_geometry_id_to_instance_contains_id(xsi_master_object_id))
	{
		std::vector instance_ids = update_context->get_geometry_id_to_instance_ids(xsi_master_object_id);
		for (size_t i = 0; i < instance_ids.size(); i++)
		{
			ULONG xsi_instance_id = instance_ids[i];
			XSI::ProjectItem xsi_instance_item = XSI::Application().GetObjectFromID(xsi_instance_id);
			XSI::Model xsi_instance_model(xsi_instance_item);
			XSI::KinematicState xsi_instance_kine = xsi_instance_model.GetKinematics().GetGlobal();

			update_instance_geometry_transform(scene, update_context, xsi_instance_id, xsi_instance_kine, eval_time);
		}

		return XSI::CStatus::OK;
	}

	return XSI::CStatus::Abort;
}