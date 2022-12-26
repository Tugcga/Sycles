#include <xsi_model.h>
#include <xsi_light.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>

#include "scene/scene.h"
#include "scene/light.h"

#include "../update_context.h"
#include "cyc_scene.h"
#include "../../utilities/logs.h"
#include "../../utilities/xsi_properties.h"

// find all children, not only at first level
XSI::CRefArray get_model_children(const XSI::X3DObject &xsi_object)
{
	XSI::CRefArray children;
	XSI::CStringArray str_families_subobject;
	children = xsi_object.FindChildren2("", "", str_families_subobject);

	return children;
}

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

XSI::MATH::CTransformation calc_instance_object_tfm(const XSI::KinematicState& master_root, const XSI::KinematicState& master_object, const XSI::KinematicState& instance_root, const XSI::CTime& eval_time)
{
	return calc_instance_object_tfm(master_root.GetTransform(eval_time), master_object.GetTransform(eval_time), instance_root.GetTransform(eval_time));
}

// this function has three optional arguments: override_instance_tfm, master_ids and override_root_id
// these arguments used when the instance contains nested instance
// in this case we process this nested instance, but pass as these arguments data from the top level
// so, override_instance_tfm is either identical or transfrom of the top level instance object
// master_ids either empty or the path of id to the nested subinstance
// override_root_id is id of the top level instance
void sync_instance_model(ccl::Scene* scene, UpdateContext* update_context, const XSI::Model &instance_model, const XSI::MATH::CTransformation& override_instance_tfm, std::vector<ULONG> master_ids, ULONG override_root_id)
{
	bool use_override = master_ids.size() > 0;

	XSI::Model xsi_master = instance_model.GetInstanceMaster();
	XSI::CRefArray children = get_model_children(xsi_master);
	XSI::CTime eval_time = update_context->get_time();

	for (size_t i = 0; i < children.GetCount(); i++)
	{
		XSI::X3DObject xsi_object(children[i]);
		ULONG xsi_id = xsi_object.GetObjectID();
		XSI::CString xsi_object_type = xsi_object.GetType();

		if (is_render_visible(xsi_object, eval_time))
		{
			if (xsi_object_type == "polymsh")
			{

			}
			else if (xsi_object_type == "light")
			{
				// create copy of the light
				XSI::Light xsi_light(xsi_object);

				// in this method we set master object transform
				sync_xsi_light(scene, xsi_light, update_context);
				// so, we should change it
				XSI::MATH::CTransformation instance_object_tfm = calc_instance_object_tfm(xsi_master.GetKinematics().GetGlobal(), xsi_object.GetKinematics().GetGlobal(),  instance_model.GetKinematics().GetGlobal(), eval_time);
				size_t light_index = scene->lights.size() - 1;
				ccl::Light* light = scene->lights[light_index];
				sync_light_tfm(light, tweak_xsi_light_transform(instance_object_tfm, xsi_light, eval_time).GetMatrix4());

				// add data to update context about indices of masters and cycles objects
				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master.GetObjectID());
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_light_instance_data(use_override ? override_root_id : instance_model.GetObjectID(), light_index, m_ids);
				//update_context->add_light_instance_data(instance_model.GetObjectID(), light_index, xsi_master.GetObjectID(), xsi_object.GetObjectID());
			}
			else if (xsi_object_type == "cyclesPoint" || xsi_object_type == "cyclesSun" || xsi_object_type == "cyclesSpot" || xsi_object_type == "cyclesArea")  // does not consider background, because it should be unique
			{
				sync_custom_light(scene, xsi_object, update_context);
				// also set other transform
				XSI::MATH::CTransformation instance_object_tfm = 
					calc_instance_object_tfm(
						xsi_master.GetKinematics().GetGlobal().GetTransform(eval_time), 
						xsi_object.GetKinematics().GetGlobal().GetTransform(eval_time), 
						use_override ? override_instance_tfm : instance_model.GetKinematics().GetGlobal().GetTransform(eval_time));
				
				size_t light_index = scene->lights.size() - 1;
				ccl::Light* light = scene->lights[light_index];
				sync_light_tfm(light, instance_object_tfm.GetMatrix4());

				std::vector<ULONG> m_ids(master_ids);
				m_ids.push_back(xsi_master.GetObjectID());
				m_ids.push_back(xsi_object.GetObjectID());
				update_context->add_light_instance_data(use_override ? override_root_id : instance_model.GetObjectID(), light_index, m_ids);
			}
			else if (xsi_object_type == "#model")
			{
				XSI::Model xsi_model(xsi_object);
				XSI::siModelKind model_kind = xsi_model.GetModelKind();
				if (model_kind == XSI::siModelKind_Instance)
				{// this is instance inside the instance
					// xsi_object transform is global transform inside master
					// we should convert it to global transform inside the instance
					XSI::MATH::CTransformation override_tfm = 
						calc_instance_object_tfm(
							xsi_master.GetKinematics().GetGlobal().GetTransform(eval_time), 
							xsi_object.GetKinematics().GetGlobal().GetTransform(eval_time), 
							use_override ? override_instance_tfm : instance_model.GetKinematics().GetGlobal().GetTransform(eval_time));

					std::vector<ULONG> m_ids(master_ids);
					m_ids.push_back(xsi_master.GetObjectID());
					m_ids.push_back(xsi_object.GetObjectID());
					// and pass it to the sync method as real global transform fo the instanciated root object
					sync_instance_model(scene, update_context, xsi_model, override_tfm, m_ids, use_override ? override_root_id : instance_model.GetObjectID());

					// we should write to the update context the data about nested instance
					update_context->add_light_nested_instance_data(xsi_model.GetObjectID(), use_override ? override_root_id : instance_model.GetObjectID());
				}
			}
			else
			{
				log_message("unknown object in instance master " + xsi_object.GetName() + " type: " + xsi_object_type);
			}
		}
	}
}

void update_instance_light_transform(ccl::Scene* scene, UpdateContext* update_context, ULONG xsi_instance_id, const XSI::KinematicState &xsi_instance_root, const XSI::CTime &eval_time)
{
	std::unordered_map<size_t, std::vector<ULONG>> object_index_map = update_context->get_light_from_instance_data(xsi_instance_id);
	for (const auto& [light_index, xsi_ids] : object_index_map)
	{
		ccl::Light* light = scene->lights[light_index];

		if (xsi_ids.size() >= 2)
		{
			XSI::MATH::CTransformation xsi_tfm;
			XSI::CString master_object_type;
			XSI::X3DObject master_object;

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
		if (update_context->is_light_nested_to_host_instances_contains_id(xsi_instance_id))
		{
			std::vector<ULONG> nested_to_host_instances_ids = update_context->get_light_nested_to_host_instances_ids(xsi_instance_id);
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
	else
	{
		log_message("no " + xsi_model.GetName());
	}

	// TODO: check other similar maps to recognize other types of geometry
	
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

	// TODO: check other similar maps to recognize other types of geometry

	return XSI::CStatus::Abort;
}