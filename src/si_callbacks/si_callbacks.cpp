#include "si_locker.h"
#include "../utilities/logs.h"
#include "../render_base/render_engine_base.h"
#include "../render_cycles/render_engine_cyc.h"

RenderEngineCyc g_render;

RenderEngineCyc* render;
RenderEngineCyc* get_render_instance() { return render; }

SICALLBACK CyclesRender_Init(XSI::CRef &in_ctxt)
{
	XSI::Context ctxt(in_ctxt);
	XSI::Renderer renderer = ctxt.GetSource();

	XSI::CLongArray	process;
	process.Add(XSI::siRenderProcessRender);
	process.Add(XSI::siRenderProcessExportArchive);
	process.Add(XSI::siRenderProcessGenerateRenderMap);
	renderer.PutProcessTypes(process);

	XSI::CString options_name = "Cycles Render Options";
	renderer.AddProperty(XSI::siRenderPropertyOptions, "Cycles Render." + options_name);

	// setup output formats
	// ldr formats
	renderer.AddOutputImageFormat("PNG - Portable Network Graphics", "png");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger8);

	renderer.AddOutputImageFormat("BMP - Bitmap Picture", "bmp");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger4);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger2);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger1);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger4);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger2);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger1);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger4);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger2);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger1);

	renderer.AddOutputImageFormat("Truevision TGA", "tga");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger8);

	renderer.AddOutputImageFormat("SGI - Silicon Graphics Image", "sgi");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger8);

	renderer.AddOutputImageFormat("IFF - Interchange File Format", "iff");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);

	renderer.AddOutputImageFormat("JPEG - Joint Photographic Experts Group", "jpg");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthInteger8);

	renderer.AddOutputImageFormat("JPEG 2000", "jp2");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);

	renderer.AddOutputImageFormat("PPM - Portable PixMap", "ppm");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);

	// hdr formats
	renderer.AddOutputImageFormat("TIFF - Tagged Image File Format", "tiff");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthInteger32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelNormalVectorType, "XYZ", XSI::siImageBitDepthFloat32);

	renderer.AddOutputImageFormat("EXR - OpenEXR", "exr");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelNormalVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelDepthType, "D", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelGrayscaleType, "A", XSI::siImageBitDepthFloat32);

	renderer.AddOutputImageFormat("DPX - Digital Picture Exchange", "dpx");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelNormalVectorType, "XYZ", XSI::siImageBitDepthFloat32);

	renderer.AddOutputImageFormat("FITS - Flexible Image Transport System", "fits");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger16);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthInteger8);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelNormalVectorType, "XYZ", XSI::siImageBitDepthFloat32);

	renderer.AddOutputImageFormat("HDR - High Dynamic Range", "hdr");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelNormalVectorType, "XYZ", XSI::siImageBitDepthFloat32);

	renderer.AddOutputImageFormat("RLA - Run-Length Encoded Version A", "rla");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGBA", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelNormalVectorType, "XYZ", XSI::siImageBitDepthFloat32);

	renderer.AddOutputImageFormat("PFM - Portable Float Map Image", "pfm");
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelColorType, "RGB", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelVectorType, "XYZ", XSI::siImageBitDepthFloat32);
	renderer.AddOutputImageFormatSubType(XSI::siRenderChannelNormalVectorType, "XYZ", XSI::siImageBitDepthFloat32);

	// render channels
	renderer.AddDefaultChannel("Cycles Combined", XSI::siRenderChannelColorType);  // 3 for lightgroups, 4 for others
	renderer.AddDefaultChannel("Cycles Depth", XSI::siRenderChannelDepthType);  // 1
	renderer.AddDefaultChannel("Cycles Position", XSI::siRenderChannelVectorType);  // 3
	renderer.AddDefaultChannel("Cycles Normal", XSI::siRenderChannelNormalVectorType);  // 3
	renderer.AddDefaultChannel("Cycles UV", XSI::siRenderChannelVectorType);  // 3
	renderer.AddDefaultChannel("Cycles Object ID", XSI::siRenderChannelGrayscaleType);  // 1
	renderer.AddDefaultChannel("Cycles Material ID", XSI::siRenderChannelColorType);  // 1
	renderer.AddDefaultChannel("Cycles Diffuse Color", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Glossy Color", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Transmission Color", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Diffuse Indirect", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Glossy Indirect", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Transmission Indirect", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Diffuse Direct", XSI::siRenderChannelColorType);  // 
	renderer.AddDefaultChannel("Cycles Glossy Direct", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Transmission Direct", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Emission", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Background", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles AO", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Shadow Catcher", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Shadow", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Motion", XSI::siRenderChannelColorType);  // 4
	renderer.AddDefaultChannel("Cycles Motion Weight", XSI::siRenderChannelGrayscaleType);  // 1
	renderer.AddDefaultChannel("Cycles Mist", XSI::siRenderChannelGrayscaleType);  // 1
	renderer.AddDefaultChannel("Cycles Volume Direct", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Volume Indirect", XSI::siRenderChannelColorType);  // 3
	renderer.AddDefaultChannel("Cycles Sample Count", XSI::siRenderChannelGrayscaleType);  // 1
	renderer.AddDefaultChannel("Cycles AOV Color", XSI::siRenderChannelColorType);  // 4
	renderer.AddDefaultChannel("Cycles AOV Value", XSI::siRenderChannelGrayscaleType);  // 1

	render = &g_render;
	render->set_render_options_name(options_name);

	g_bAborted = false;
	::InitializeCriticalSection(&g_barrierAbort);
	g_hAbort = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	return XSI::CStatus::OK;
}

SICALLBACK CyclesRender_Term(XSI::CRef &in_ctxt)
{
	::DeleteObject(g_hAbort);
	::DeleteCriticalSection(&g_barrierAbort);

	g_hAbort = NULL;
	::ZeroMemory(&g_barrierAbort, sizeof(g_barrierAbort));

	return(XSI::CStatus::OK);
}

SICALLBACK CyclesRender_Cleanup(XSI::CRef &in_ctxt)
{
	render->clear();
	return(XSI::CStatus::OK);
}

SICALLBACK CyclesRender_Abort(XSI::CRef &in_ctxt)
{
	render->abort_render();
	set_abort(true);

	return(XSI::CStatus::OK);
}

SICALLBACK CyclesRender_Quality(XSI::CRef &in_ctxt)
{
	return XSI::CStatus::OK;
}

SICALLBACK CyclesRender_Query(XSI::CRef &in_ctxt)
{
	XSI::RendererContext ctxt(in_ctxt);
	const int type = ctxt.GetAttribute("QueryType");
	switch (type)
	{
	case XSI::siRenderQueryArchiveIsValid:
	{
		break;
	}
	case XSI::siRenderQueryWantDirtyList:
	{
		ctxt.PutAttribute("WantDirtyList", true);
		break;
	}
	case XSI::siRenderQueryDisplayBitDepths:
	{
		XSI::CLongArray bitDepths;
		bitDepths.Add(XSI::siImageBitDepthInteger8);
		bitDepths.Add(XSI::siImageBitDepthInteger16);

		XSI::CString softimage_version_string = XSI::Application().GetVersion();
		XSI::CStringArray softimage_version = softimage_version_string.Split(".");
		if (atoi(softimage_version[0].GetAsciiString()) >= 10)
		{
			bitDepths.Add(XSI::siImageBitDepthFloat32);
		}

		ctxt.PutAttribute("BitDepths", bitDepths);
		break;
	}
	default:;
#if XSISDK_VERSION > 11000
	case XSI::siRenderQueryHasPreMulAlphaOutput:
	{
		ctxt.PutAttribute("HasPreMulAlphaOutput", false);
		break;
	}
#endif      
	}

	return XSI::CStatus::OK;
}

SICALLBACK Cyc_OnObjectAdded_OnEvent(XSI::CRef& in_ctxt)
{
	render->on_object_add(in_ctxt);

	return XSI::CStatus::OK;
}


SICALLBACK Cyc_OnObjectRemoved_OnEvent(XSI::CRef& in_ctxt)
{
	render->on_object_remove(in_ctxt);

	return XSI::CStatus::OK;
}

XSI::CStatus begin_render_event(XSI::RendererContext& ctxt, XSI::CStringArray& output_paths)
{
	XSI::CStatus status;

	const XSI::siRenderingType render_type = (ctxt.GetSequenceLength() > 1) ? XSI::siRenderSequence : XSI::siRenderFramePreview;
	if (ctxt.GetSequenceIndex() == 0)
	{
		status = ctxt.TriggerEvent(XSI::siOnBeginSequence, render_type, ctxt.GetTime(), output_paths, XSI::siRenderFieldNone);
	}

	if (status != XSI::CStatus::OK)
	{
		return status;
	}
	status = ctxt.TriggerEvent(XSI::siOnBeginFrame, render_type, ctxt.GetTime(), output_paths, XSI::siRenderFieldNone);

	return status;
}

XSI::CStatus end_render_event(XSI::RendererContext& ctxt, XSI::CStringArray& output_paths, bool in_skipped)
{
	XSI::CStatus status;
	const XSI::siRenderingType render_type = (ctxt.GetSequenceLength() > 1) ? XSI::siRenderSequence : XSI::siRenderFramePreview;
	if (!in_skipped)
	{
		status = ctxt.TriggerEvent(XSI::siOnEndFrame, render_type, ctxt.GetTime(), output_paths, XSI::siRenderFieldNone);

		if (status != XSI::CStatus::OK)
		{
			return status;
		}
	}
	if (ctxt.GetSequenceIndex() + 1 == ctxt.GetSequenceLength())
	{
		status = ctxt.TriggerEvent(XSI::siOnEndSequence, render_type, ctxt.GetTime(), output_paths, XSI::siRenderFieldNone);
	}

	return status;
}

SICALLBACK CyclesRender_Process(XSI::CRef &in_ctxt)
{
	set_abort(false);
	XSI::CStatus status;

	XSI::RendererContext ctxt(in_ctxt);
	XSI::Renderer renderer = ctxt.GetSource();

	if (!render->is_ready_to_render())
	{
		return XSI::CStatus::Abort;
	}

	status = render->pre_render(ctxt);  // we return Abort when skip all output images
	if (status == XSI::CStatus::Abort)
	{
		return XSI::CStatus::OK;
	}

	if (status != XSI::CStatus::OK)
	{
		return XSI::CStatus::Abort;
	}

	LockRendererData locker = LockRendererData(renderer);
	if (locker.lock() != XSI::CStatus::OK)
	{
		return XSI::CStatus::Abort;
	}

	status = render->scene_process();

	if (locker.unlock() != XSI::CStatus::OK || status != XSI::CStatus::OK)
	{
		render->interrupt_update_scene();
		return  XSI::CStatus::Abort;
	}

	if (is_aborted())
	{
		return XSI::CStatus::Abort;
	}

	status = render->start_render();

	if (status == XSI::CStatus::OK)
	{
		status = render->post_render();
	}

	return status;
}

SICALLBACK CyclesRenderOptions_Define(XSI::CRef& in_ctxt)
{
	XSI::Context ctxt(in_ctxt);
	XSI::CustomProperty prop = ctxt.GetSource();

	return render->render_option_define(prop);
}

SICALLBACK CyclesRenderOptions_DefineLayout(XSI::CRef& in_ctxt)
{
	XSI::Context ctxt(in_ctxt);
	return render->render_option_define_layout(ctxt);
}

SICALLBACK CyclesRenderOptions_PPGEvent(const XSI::CRef& in_ctxt)
{
	XSI::PPGEventContext ctx(in_ctxt);
	return render->render_options_update(ctx);
}