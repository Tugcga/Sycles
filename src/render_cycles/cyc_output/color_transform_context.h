#pragma once
#include <xsi_arrayparameter.h>
#include <xsi_time.h>

#include "OpenColorIO/OpenColorIO.h"
namespace OCIO = OCIO_NAMESPACE;

class ColorTransformContext
{
public:
	ColorTransformContext();
	~ColorTransformContext();

	void update(const XSI::CParameterRefArray &render_parameters, const XSI::CTime &eval_time);
	void apply(size_t width, size_t height, size_t components, float* pixels);

	bool get_use_correction();

private:
	bool use_correction;  // if false, then in the transform task nothing to do
	bool use_ocio;  // if false, then use simple sRGB color correction

	// previous parameters
	bool prev_is_use_cm;
	int prev_cm_mode;
	int prev_ocio_display;
	int prev_ocio_view;
	int prev_ocio_look;
	float prev_cm_exposure;
	float prev_cm_gamma;

	OCIO::ConstProcessorRcPtr processor;
	OCIO::ConstCPUProcessorRcPtr cpu_processor;

	// these objects are constants and created at the constructor
	OCIO::ColorSpaceTransformRcPtr colorspace_transform;
	OCIO::DisplayViewTransformRcPtr dv_transform;
	OCIO::LookTransformRcPtr look_transform;
	OCIO::MatrixTransformRcPtr matrix_transform;
	OCIO::ExponentTransformRcPtr exp_transform;

	// use two different groups: with look and without it
	// bacuse we can select look - None, and in this case it should be ignored
	OCIO::GroupTransformRcPtr full_group_transform;
	OCIO::GroupTransformRcPtr without_look_group_transform;
};
