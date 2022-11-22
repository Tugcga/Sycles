#pragma once
#include <xsi_application.h>

// remove digits from the given string
XSI::CString remove_digits(const XSI::CString& orignal_str);

// replace each splitter symbol to replacer in he input string
XSI::CString replace_symbols(const XSI::CString& input_string, const XSI::CString& splitter, const XSI::CString& replacer);