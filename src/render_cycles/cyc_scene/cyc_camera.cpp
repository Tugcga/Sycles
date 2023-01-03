#include <xsi_kinematics.h>
#include <xsi_camera.h>
#include <xsi_status.h>

#include "scene/scene.h"
#include "scene/camera.h"


#include "../../render_cycles/update_context.h"
#include "../../utilities/math.h"
#include "../../utilities/logs.h"
#include "../../utilities/xsi_properties.h"

std::vector<float> extract_camera_tfm(const XSI::Camera &xsi_camera, const XSI::CTime &eval_time)
{
	XSI::MATH::CMatrix4 camera_tfm_matrix = xsi_camera.GetKinematics().GetGlobal().GetTransform(eval_time).GetMatrix4();
	std::vector<float> xsi_camera_tfm(16);
	xsi_matrix_to_cycles_array(xsi_camera_tfm, camera_tfm_matrix, true);

	return xsi_camera_tfm;
}

void sync_camera_motion(ccl::Scene* scene, UpdateContext* update_context, float fov_prev, float fov_next)
{
	ccl::Camera* camera = scene->camera;
	if (update_context->get_need_motion())
	{
		ccl::array<ccl::Transform> motions;
		motions.resize(update_context->get_motion_steps(), camera->get_matrix());

		// setup camera motion properties
		MotionSettingsPosition motion_position = update_context->get_motion_position();
		float shutter_time = update_context->get_motion_shutter_time();
		camera->set_motion_position(motion_position == MotionPosition_Start ? ccl::MOTION_POSITION_START : (motion_position == MotionPosition_Center ? ccl::MOTION_POSITION_CENTER : ccl::MOTION_POSITION_END));
		camera->set_shuttertime(shutter_time);
		camera->set_rolling_shutter_type(update_context->get_motion_rolling() ? ccl::Camera::RollingShutterType::ROLLING_SHUTTER_TOP : ccl::Camera::RollingShutterType::ROLLING_SHUTTER_NONE);
		camera->set_rolling_shutter_duration(update_context->get_motion_rolling_duration());

		XSI::Camera xsi_camera = update_context->get_camera();
		XSI::KinematicState camera_kinematic = xsi_camera.GetKinematics().GetGlobal();
		std::vector<float> time_array(16, 0.0f);
		for (size_t i = 0; i < update_context->get_motion_steps(); i++)
		{
			float current_time = update_context->get_motion_time(i);
			xsi_matrix_to_cycles_array(time_array, camera_kinematic.GetTransform(current_time).GetMatrix4(), true);
			ccl::Transform time_tfm = tweak_camera_matrix(get_transform(time_array), scene->camera->get_camera_type(), scene->camera->get_panorama_type());
			motions[i] = time_tfm;
		}
		time_array.clear();
		time_array.shrink_to_fit();

		scene->camera->set_motion(motions);

		scene->camera->set_use_perspective_motion(false);
		if (scene->camera->get_camera_type() == ccl::CAMERA_PERSPECTIVE)
		{
			// set previous and next fov
			scene->camera->set_fov_pre(fov_prev);
			scene->camera->set_fov_post(fov_next);
			if (scene->camera->get_fov_pre() != scene->camera->get_fov() || scene->camera->get_fov_post() != scene->camera->get_fov())
			{
				scene->camera->set_use_perspective_motion(true);
			}
		}
	}
	else
	{
		ccl::array<ccl::Transform> motions;
		motions.resize(1, scene->camera->get_matrix());
		scene->camera->set_motion(motions);

		scene->camera->set_use_perspective_motion(false);
		scene->camera->set_fov_pre(scene->camera->get_fov());
		scene->camera->set_fov_post(scene->camera->get_fov());
	}
}

XSI::CStatus sync_camera(ccl::Scene* scene, UpdateContext* update_context)
{
	XSI::Camera xsi_camera = update_context->get_camera();
	XSI::CTime eval_time = update_context->get_time();

	ccl::Camera* camera = scene->camera;

	camera->set_full_width(update_context->get_full_width());
	camera->set_full_height(update_context->get_full_height());

	// get camera custom property
	XSI::Property camera_property;
	bool is_camera_extension = get_xsi_object_property(xsi_camera, "CyclesCamera", camera_property);
	 
	float camera_aspect = float(xsi_camera.GetParameterValue("aspect", eval_time));
	float fov_grad = float(xsi_camera.GetParameterValue("fov", eval_time));
	float fov_prev_grad = float(xsi_camera.GetParameterValue("fov", update_context->get_motion_fisrt_time()));
	float fov_next_grad = float(xsi_camera.GetParameterValue("fov", update_context->get_motion_last_time()));
	float near_clip = xsi_camera.GetParameterValue("near", eval_time);
	float far_clip = xsi_camera.GetParameterValue("far", eval_time);
	bool is_ortho = xsi_camera.GetParameterValue("proj") == 0;

	CameraType camera_type = CameraType_General;
	if (is_camera_extension)
	{
		int camera_typ_value = camera_property.GetParameterValue("camera_type", eval_time);
		camera_type = camera_typ_value == 0 ? CameraType_General : CameraType_Panoramic;
	}

	if (camera_type == CameraType_General)
	{
		// check is camera orthographic
		if (is_ortho)
		{// orthographic
			float ortho_aspect = float(xsi_camera.GetParameterValue("orthoheight", eval_time)) * camera_aspect / 2.0f;
			camera->viewplane.left = -1.0f * ortho_aspect;
			camera->viewplane.right = ortho_aspect;
			camera->viewplane.bottom = -1.0f * ortho_aspect / camera_aspect;
			camera->viewplane.top = ortho_aspect / camera_aspect;
		}
		else
		{// perspective
			// setup viewplane
			if (camera_aspect >= 1.0f)
			{
				camera->viewplane.left = -1.0f * camera_aspect;
				camera->viewplane.right = camera_aspect;
				camera->viewplane.bottom = -1.0f;
				camera->viewplane.top = 1.0f;
			}
			else
			{
				camera->viewplane.left = -1.0f;
				camera->viewplane.right = 1.0f;
				camera->viewplane.bottom = -1.0f / camera_aspect;
				camera->viewplane.top = 1.0f / camera_aspect;
			}

			bool is_horizontal = xsi_camera.GetParameterValue("fovtype", eval_time) == 1;
			if ((is_horizontal || camera_aspect <= 1.0f) && (!is_horizontal || camera_aspect >= 1.0f))
			{
				if (camera_aspect <= 1)
				{
					fov_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_grad) / 2.0) * camera_aspect));
					fov_prev_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_prev_grad) / 2.0) * camera_aspect));
					fov_next_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_next_grad) / 2.0) * camera_aspect));
				}
				else
				{
					fov_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_grad) / 2.0) / camera_aspect));
					fov_prev_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_prev_grad) / 2.0) / camera_aspect));
					fov_next_grad = RAD2DEGF(2 * atan(tan(DEG2RADF(fov_next_grad) / 2.0) / camera_aspect));
				}
			}
		}
	}
	else
	{// setup panoramic camera
		ccl::BoundBox2D cam_box;
		camera->viewplane = cam_box;
		int panorama_type = camera_property.GetParameterValue("panorama_type", eval_time);
		camera->set_panorama_type(panorama_type == 0 ? ccl::PANORAMA_EQUIRECTANGULAR : (panorama_type == 1 ? ccl::PANORAMA_FISHEYE_EQUIDISTANT : (panorama_type == 2 ? ccl::PANORAMA_FISHEYE_EQUISOLID : (panorama_type == 3 ? ccl::PANORAMA_MIRRORBALL : ccl::PANORAMA_FISHEYE_LENS_POLYNOMIAL))));
		camera->set_fisheye_fov(DEG2RADF((float)camera_property.GetParameterValue("fisheye_fov", eval_time)));
		camera->set_fisheye_lens(camera_property.GetParameterValue("fisheye_lens", eval_time));
		camera->set_latitude_min(DEG2RADF((float)camera_property.GetParameterValue("equ_latitude_min", eval_time)));
		camera->set_latitude_max(DEG2RADF((float)camera_property.GetParameterValue("equ_latitude_max", eval_time)));
		camera->set_longitude_min(DEG2RADF((float)camera_property.GetParameterValue("equ_longitude_min", eval_time)));
		camera->set_longitude_max(DEG2RADF((float)camera_property.GetParameterValue("equ_longitude_max", eval_time)));
		camera->set_fisheye_polynomial_k0(DEG2RADF((float)camera_property.GetParameterValue("polynomial_k0", eval_time)));
		camera->set_fisheye_polynomial_k1(DEG2RADF((float)camera_property.GetParameterValue("polynomial_k1", eval_time)));
		camera->set_fisheye_polynomial_k2(DEG2RADF((float)camera_property.GetParameterValue("polynomial_k2", eval_time)));
		camera->set_fisheye_polynomial_k3(DEG2RADF((float)camera_property.GetParameterValue("polynomial_k3", eval_time)));
		camera->set_fisheye_polynomial_k4(DEG2RADF((float)camera_property.GetParameterValue("polynomial_k4", eval_time)));
	}
	
	float fov_rad = DEG2RADF(fov_grad);
	float fov_prev_rad = DEG2RADF(fov_prev_grad);
	float fov_next_rad = DEG2RADF(fov_next_grad);

	// clipping distances, for perspective/orthographic only
	if (camera_type == CameraType_General)
	{
		camera->set_nearclip(near_clip);
		camera->set_farclip(far_clip);
	}

	// camera type
	if (camera_type == CameraType_General)
	{
		camera->set_camera_type(is_ortho ? ccl::CAMERA_ORTHOGRAPHIC : ccl::CAMERA_PERSPECTIVE);
	}
	else
	{
		camera->set_camera_type(ccl::CAMERA_PANORAMA);
	}

	// dof
	if (is_camera_extension)
	{
		camera->set_aperture_ratio(camera_property.GetParameterValue("aperture_ratio", eval_time));
		XSI::X3DObject camera_interest = xsi_camera.GetInterest();
		XSI::MATH::CVector3 interest_pos = camera_interest.GetKinematics().GetGlobal().GetTransform().GetTranslation();
		XSI::MATH::CVector3 camera_pos = xsi_camera.GetKinematics().GetGlobal().GetTransform(eval_time).GetTranslation();
		float focal_distance = interest_pos.SubInPlace(camera_pos).GetLength();
		camera->set_focaldistance(focal_distance);
		camera->set_aperturesize(camera_property.GetParameterValue("aperture_size", eval_time));
		camera->set_blades((int)camera_property.GetParameterValue("blades", eval_time));
		camera->set_bladesrotation(DEG2RADF((float)camera_property.GetParameterValue("blades_rotation", eval_time)));

		camera->set_sensorwidth(camera_property.GetParameterValue("sensor_width", eval_time));
		camera->set_sensorheight(camera_property.GetParameterValue("sensor_height", eval_time));
	}

	// transforms
	std::vector<float> camera_tfm = extract_camera_tfm(xsi_camera, eval_time);
	camera->set_matrix(tweak_camera_matrix(get_transform(camera_tfm), scene->camera->get_camera_type(), scene->camera->get_panorama_type()));

	// fov
	camera->set_fov(fov_rad);

	// set curent camera as dicing camera
	*scene->dicing_camera = *camera;

	sync_camera_motion(scene, update_context, fov_prev_rad, fov_next_rad);

	camera->tag_full_width_modified();
	camera->tag_full_height_modified();
	camera->tag_matrix_modified();
	camera->tag_fov_modified();
	camera->tag_camera_type_modified();
	camera->tag_panorama_type_modified();
	camera->tag_sensorwidth_modified();

	camera->update(scene);

	return XSI::CStatus::OK;
}