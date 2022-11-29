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

XSI::CString get_common_substring(const XSI::CStringArray& strings_array)
{
	XSI::CString common_substring;
	bool is_set_common_substring = false;
	size_t common_substring_count = 0;

	for (size_t i = 0; i < strings_array.GetCount(); i++)
	{
		XSI::CString str = strings_array[i];
		ULONG slash_pos = str.ReverseFindString("\\");
		XSI::CString str_part = str.GetSubString(slash_pos + 1, str.Length());
		if (!is_set_common_substring)
		{
			is_set_common_substring = true;
			ULONG dot_pos = str_part.ReverseFindString(".");

			common_substring = str_part.GetSubString(0, dot_pos);
		}
		else
		{// find equal symbols in the new substring and common substring
			size_t index = 0;
			bool is_finish = false;
			while (!is_finish)
			{
				if (str_part.GetAt(index) == common_substring.GetAt(index))
				{
					index++;
				}
				else
				{
					is_finish = true;
				}
				if (index + 1 > str_part.Length() || index + 1 > common_substring.Length())
				{
					is_finish = true;
				}
			}
			common_substring = common_substring.GetSubString(0, index);
		}
		common_substring_count++;
	}

	if (common_substring_count == 1 && common_substring.Length() > 0)
	{
		ULONG dot_position = common_substring.ReverseFindString(".");
		if (dot_position > 2)
		{
			common_substring = common_substring.GetSubString(0, dot_position + 1);
		}
	}

	return common_substring;
}

XSI::CString add_aov_name_to_path(const XSI::CString& file_path, const XSI::CString& aov_name)
{
	// find last slash (after the folder with the image)
	ULONG slash_pos = file_path.ReverseFindString("\\");

	// find the second dot
	ULONG first_dot_pos = file_path.ReverseFindString(".");
	if (first_dot_pos == UINT_MAX)
	{// no dots in the path, this is not ok
		return file_path + "." + aov_name;
	}

	XSI::CString path_substring = file_path.GetSubString(0, first_dot_pos);
	// find dot in this substring
	ULONG second_dot_pos = path_substring.ReverseFindString(".");
	if (second_dot_pos == UINT_MAX || second_dot_pos < slash_pos)
	{// the path contains only one dot - before image extension (or other dots inside directory path)
		return path_substring + "." + aov_name + file_path.GetSubString(first_dot_pos);
	}

	return file_path.GetSubString(0, second_dot_pos) + "." + aov_name + file_path.GetSubString(second_dot_pos);
}