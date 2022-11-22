#include <xsi_application.h>

XSI::CString remove_digits(const XSI::CString& orignal_str)
{
	XSI::CString to_return(orignal_str);
	to_return.TrimRight("0123456789");
	return to_return;
}

XSI::CString replace_symbols(const XSI::CString& input_string, const XSI::CString& splitter, const XSI::CString& replacer)
{
	XSI::CStringArray parts = input_string.Split(splitter);
	XSI::CString to_return = parts[0];
	for (ULONG i = 1; i < parts.GetCount(); i++)
	{
		to_return += replacer + parts[i];
	}

	return to_return;
}