#include "render_engine_cyc.h"

#include <xsi_parameter.h>
#include <xsi_customproperty.h>
#include <xsi_ppglayout.h>
#include <xsi_ppgitem.h>
#include <xsi_context.h>
#include <xsi_ppgeventcontext.h>

const XSI::siCapabilities block_mode = XSI::siReadOnly;  // siReadOnly or siNotInspectable

void build_layout(XSI::PPGLayout& layout)
{
	layout.Clear();
}

XSI::CStatus RenderEngineCyc::render_options_update(XSI::PPGEventContext& event_context) 
{
	XSI::PPGEventContext::PPGEvent event_id = event_context.GetEventID();
	bool is_refresh = false;
	if (event_id == XSI::PPGEventContext::siOnInit) 
	{
		XSI::CustomProperty cp_source = event_context.GetSource();
	}
	else if (event_id == XSI::PPGEventContext::siParameterChange) 
	{
		XSI::Parameter changed = event_context.GetSource();
		XSI::CustomProperty prop = changed.GetParent();
		XSI::CString param_name = changed.GetScriptName();
	}

	if (is_refresh)
	{
		event_context.PutAttribute("Refresh", true);
	}

	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineCyc::render_option_define_layout(XSI::Context& context)
{
	XSI::PPGLayout layout = context.GetSource();
	XSI::Parameter parameter = context.GetSource();
	XSI::CustomProperty property = parameter.GetParent();

	build_layout(layout);

	return XSI::CStatus::OK;
}

XSI::CStatus RenderEngineCyc::render_option_define(XSI::CustomProperty& property)
{
	const int caps = XSI::siPersistable | XSI::siAnimatable;
	XSI::Parameter param;

	return XSI::CStatus::OK;
}