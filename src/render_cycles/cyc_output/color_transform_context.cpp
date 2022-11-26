#include "color_transform_context.h"
#include "../../utilities/logs.h"
#include "../../input/input.h"
#include "../../utilities/math.h"

ColorTransformContext::ColorTransformContext()
{
	colorspace_transform = OCIO::ColorSpaceTransform::Create();
	colorspace_transform->setSrc(OCIO::ROLE_SCENE_LINEAR);
	colorspace_transform->setDst(OCIO::ROLE_SCENE_LINEAR);

	dv_transform = OCIO::DisplayViewTransform::Create();
	dv_transform->setSrc(OCIO::ROLE_SCENE_LINEAR);

	look_transform = OCIO::LookTransform::Create();
	look_transform->setSrc(OCIO::ROLE_SCENE_LINEAR);
	look_transform->setDst(OCIO::ROLE_SCENE_LINEAR);

	matrix_transform = OCIO::MatrixTransform::Create();
	exp_transform = OCIO::ExponentTransform::Create();

	full_group_transform = OCIO::GroupTransform::Create();
	without_look_group_transform = OCIO::GroupTransform::Create();

	// add all transforms to the group
	full_group_transform->appendTransform(colorspace_transform);
	full_group_transform->appendTransform(matrix_transform);
	full_group_transform->appendTransform(look_transform);
	full_group_transform->appendTransform(dv_transform);
	full_group_transform->appendTransform(exp_transform);

	without_look_group_transform->appendTransform(colorspace_transform);
	without_look_group_transform->appendTransform(matrix_transform);
	without_look_group_transform->appendTransform(dv_transform);
	without_look_group_transform->appendTransform(exp_transform);

	use_correction = false;
	use_ocio = false;

	prev_is_use_cm = false;
	prev_cm_mode = -1;
	prev_ocio_display = -1;
	prev_ocio_view = -1;
	prev_ocio_look = -1;
	prev_cm_exposure = 0.0f;
	prev_cm_gamma = 0.0f;
}

ColorTransformContext::~ColorTransformContext()
{

}

void ColorTransformContext::update(const XSI::CParameterRefArray& render_parameters, const XSI::CTime& eval_time)
{
	// read parameters
	bool is_use_cm = render_parameters.GetValue("cm_apply_to_ldr", eval_time);
	int cm_mode = render_parameters.GetValue("cm_mode", eval_time);
	int ocio_display = render_parameters.GetValue("cm_display_index", eval_time);
	int ocio_view = render_parameters.GetValue("cm_view_index", eval_time);
	int ocio_look = render_parameters.GetValue("cm_look_index", eval_time);
	float cm_exposure = render_parameters.GetValue("cm_exposure", eval_time);
	float cm_gamma = render_parameters.GetValue("cm_gamma", eval_time);

	// if these parameters are the same as in previous call, ignore update
	if (is_use_cm != prev_is_use_cm ||
		cm_mode != prev_cm_mode ||
		ocio_display != prev_ocio_display ||
		ocio_view != prev_ocio_view ||
		ocio_look != prev_ocio_look ||
		!equal_floats(cm_exposure, prev_cm_exposure) ||
		!equal_floats(cm_gamma, prev_cm_gamma))
	{
		if (is_use_cm)
		{
			use_correction = true;

			bool is_use_ocio = cm_mode == 1;
			if (is_use_ocio)
			{
				OCIOConfig ocio_config = get_ocio_config();

				if (ocio_config.is_init)
				{
					// check are selected values are correct
					if (ocio_display >= 0 && ocio_display < ocio_config.displays_count &&
						ocio_look >= 0 && ocio_look <= ocio_config.looks_count)
					{
						// get configuration object
						OCIO::ConstConfigRcPtr config = ocio_config.config;

						// set transform display
						const char* display = config->getDisplay(ocio_display);
						dv_transform->setDisplay(display);

						// set transform view
						int max_view_index = ocio_config.displays[ocio_display].views_count;
						const char* view = config->getView(display, ocio_view < max_view_index ? ocio_view : max_view_index - 1);
						dv_transform->setView(view);

						float gain = powf(2.0f, cm_exposure);
						const double matrix[16] = { gain, 0.0, 0.0, 0.0, 0.0, gain, 0.0, 0.0, 0.0, 0.0, gain, 0.0, 0.0, 0.0, 0.0, 1.0 };
						matrix_transform->setMatrix(matrix);

						float exponent = 1.0f / std::max(1e-6f, static_cast<float>(cm_gamma));
						const double value[4] = { exponent, exponent, exponent, 1.0 };
						exp_transform->setValue(value);

						if (ocio_look > 0)  // 0 is None
						{
							const char* look = config->getLookNameByIndex(ocio_look - 1);
							look_transform->setLooks(look);

							processor = config->getProcessor(full_group_transform);
							cpu_processor = processor->getDefaultCPUProcessor();
						}
						else
						{
							processor = config->getProcessor(without_look_group_transform);
							cpu_processor = processor->getDefaultCPUProcessor();
						}

						use_ocio = true;
					}
					else
					{
						use_ocio = false;
					}
				}
				else
				{
					use_ocio = false;
				}
			}
			else
			{
				use_ocio = false;
			}
		}
		else
		{
			use_correction = false;
		}

		// store parameters
		prev_is_use_cm = is_use_cm;
		prev_cm_mode = cm_mode;
		prev_ocio_display = ocio_display;
		prev_ocio_view = ocio_view;
		prev_ocio_look = ocio_look;
		prev_cm_exposure = cm_exposure;
		prev_cm_gamma = cm_gamma;
	}
}

bool ColorTransformContext::get_use_correction()
{
	return use_correction;
}

void ColorTransformContext::apply(size_t width, size_t height, size_t components, float* pixels)
{
	if (use_correction)
	{
		if (use_ocio)
		{
			OCIO::PackedImageDesc img_desc(pixels, width, height, components);
			cpu_processor->apply(img_desc);
		}
		else
		{
			size_t comps = std::min((int)components, 3);
			for (size_t p = 0; p < width * height; p++)
			{
				for (size_t c = 0; c < comps; c++)
				{
					pixels[components * p + c] = linear_to_srgb_float(pixels[components * p + c]);
				}
			}
		}
	}
}