#pragma once
#include "si_callbacks.h"

class LockRendererData
{
protected:
	XSI::Renderer& renderer_;
	bool locked_;

public:
	LockRendererData(XSI::Renderer& renderer) : renderer_(renderer), locked_(false)
	{

	}

	XSI::CStatus lock()
	{
		if (!locked_)
		{
			const XSI::CStatus res = renderer_.LockSceneData();
			if (res == XSI::CStatus::OK)
			{
				locked_ = true;
			}
			return res;
		}
		return XSI::CStatus::OK;
	}

	XSI::CStatus unlock()
	{
		if (locked_)
		{
			const XSI::CStatus res = renderer_.UnlockSceneData();
			if (res == XSI::CStatus::OK)
			{
				locked_ = false;
			}
			return res;
		}
		return XSI::CStatus::OK;
	}

	~LockRendererData()
	{
		unlock(); // ensure unlocked happens when this object goes out of scope
	}
};

//! Abort handling.
static bool				g_bAborted;
HANDLE					g_hAbort;
CRITICAL_SECTION		g_barrierAbort;

void set_abort(bool in_bAbort)
{
	::EnterCriticalSection(&g_barrierAbort);
	g_bAborted = in_bAbort;
	if (in_bAbort)
	{
		::SetEvent(g_hAbort);
	}
	else
	{
		::ResetEvent(g_hAbort);
	}
	::LeaveCriticalSection(&g_barrierAbort);
}

bool is_aborted()
{
	::EnterCriticalSection(&g_barrierAbort);
	const bool b_abort = g_bAborted;
	::LeaveCriticalSection(&g_barrierAbort);

	return(b_abort);
}