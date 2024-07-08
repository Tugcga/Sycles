#include <xsi_application.h>
#include <xsi_time.h>
#include <xsi_arrayparameter.h>
#include <xsi_utils.h>
#include <xsi_polygonmesh.h>
#include <xsi_hairprimitive.h>
#include <xsi_primitive.h>
#include <xsi_x3dobject.h>

#include <vector>
#include <string>
#include <iomanip>

#include "util/array.h"
#include "OpenImageIO/ustring.h"

#include "../input/input.h"

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
		if (str.Length() > 0)
		{
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
	}

	if (common_substring_count == 0)
	{
		return "";
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

std::vector<size_t> get_symbol_positions(const XSI::CString &string, char symbol)
{
	std::vector<size_t> to_return;

	for (ULONG i = 0; i < string.Length(); i++)
	{
		if (string.GetAt(i) == symbol)
		{
			to_return.push_back(i);
		}
	}

	return to_return;
}

bool is_vector_contains(ccl::array<int> &array, int value)
{
	for (size_t i = 0; i < array.size(); i++)
	{
		if (array[i] == value)
		{
			return true;
		}
	}

	return false;
}

int char_to_digit(char s)
{
	if (s == '0') { return 0; }
	else if (s == '1') { return 1; }
	else if (s == '2') { return 2; }
	else if (s == '3') { return 3; }
	else if (s == '4') { return 4; }
	else if (s == '5') { return 5; }
	else if (s == '6') { return 6; }
	else if (s == '7') { return 7; }
	else if (s == '8') { return 8; }
	else if (s == '9') { return 9; }
	return 0;
}

ccl::array<int> string_to_array(const XSI::CString &string)
{
	ccl::array<int> array;
	int buffer = 0;
	for (size_t i = 0; i < string.Length(); i++)
	{
		char s = string.GetAt(i);
		if (s == ' ')
		{// finish the string, release the buffer
			if (buffer > 1000 && buffer < 10000)
			{
				if (!is_vector_contains(array, buffer))
				{
					array.push_back_slow(buffer);
				}
			}
			buffer = 0;
		}
		else
		{
			buffer = buffer * 10 + char_to_digit(s);
		}
	}
	// also release the buffer at the end
	if (buffer > 1000 && buffer < 10000)
	{
		if (!is_vector_contains(array, buffer))
		{
			array.push_back_slow(buffer);
		}
	}
	return array;
}

bool is_digit(char s)
{
	if (s == '1' || s == '2' || s == '3' || s == '4' || s == '5' || s == '6' || s == '7' || s == '8' || s == '9' || s == '0')
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool is_number(const XSI::CString &str)
{
	for (ULONG i = 0; i < str.Length(); i++)
	{
		if (!is_digit(str.GetAt(i)))
		{
			return false;
		}
	}
	return true;
}

std::string build_source_image_path(const XSI::CString &path, const XSI::CString &source_type, bool is_cyclic, int sequence_start, int sequence_frames, int sequence_offset, const XSI::CTime &eval_time, bool allow_tile, bool &change_to_udims)
{
	sequence_frames = std::max(1, sequence_frames);
	if (source_type == XSI::CString("single_image"))
	{
		return path.GetAsciiString();
	}
	else if (source_type == XSI::CString("tiled"))
	{
		if (allow_tile)
		{
			// find the first appearence of the 1001 from the right side, and change it to <UDIM>
			size_t i = path.Length() - 4;
			while (i >= 0)
			{
				if (path.GetSubString(i, 4) == XSI::CString("1001"))
				{
					change_to_udims = true;
					return std::string((path.GetSubString(0, i) + "<UDIM>" + path.GetSubString(i + 4)).GetAsciiString());
				}
				else
				{
					i = i - 1;
				}
			}
			return path.GetAsciiString();
		}
		else
		{
			return path.GetAsciiString();
		}
	}
	else
	{
		double d_frame = eval_time.GetTime();
		int int_frame = static_cast<int>(d_frame + (d_frame >= 0 ? 0.5 : -0.5));
		int f = int_frame - sequence_start;
		if (is_cyclic)
		{
			while (f < 0)
			{
				f = f + sequence_frames;
			}
			while (f >= sequence_frames)
			{
				f = f - sequence_frames;
			}
		}
		else
		{
			if (f < 0)
			{
				f = 0;
			}
			if (f >= sequence_frames)
			{
				f = sequence_frames - 1;
			}
		}
		f = f + sequence_offset + 1;

		// next we should find image with ending and change it by f-value
		std::vector<size_t> point_positions = get_symbol_positions(path, '.');
		if (point_positions.size() > 0)
		{
			XSI::CString general_path = path.GetSubString(0, point_positions[point_positions.size() - 1]);
			XSI::CString ext = path.GetSubString(point_positions[point_positions.size() - 1] + 1);  // extension without point
			// in general_path we shold find last '_'
			std::vector<size_t> line_positions = get_symbol_positions(general_path, '_');
			//get the maximum from point positions and line positions
			int limit_pos = -1;
			if (point_positions.size() == 1)
			{// no point in the path, find throw the lines
				if (line_positions.size() > 0)
				{
					limit_pos = line_positions[line_positions.size() - 1];
				}
			}
			else
			{// there are point in the path
				if (line_positions.size() == 0)
				{
					limit_pos = point_positions[point_positions.size() - 2];
				}
				else
				{
					limit_pos = std::max(line_positions[line_positions.size() - 1], point_positions[point_positions.size() - 2]);
				}
			}

			if (limit_pos != -1)
			{
				XSI::CString number_part = general_path.GetSubString(limit_pos + 1);
				if (is_number(number_part))
				{
					return (general_path.GetSubString(0, limit_pos + 1) + XSI::CString(f) + "." + ext).GetAsciiString();
				}
				else
				{// the part is not a number
					return path.GetAsciiString();
				}
			}
			else
			{// no . or _ before extension, so, can't recognize the frame number
				return path.GetAsciiString();
			}
		}
		else
		{// no point in the path, so, no extension
			return path.GetAsciiString();
		}
	}
}

std::string replace_all_substrings(const std::string& input_string, const std::string &what_part, const std::string& replace_part)
{
	std::string to_return(input_string);

	size_t pos = 0;
	while (pos += replace_part.length())
	{
		pos = to_return.find(what_part, pos);
		if (pos == std::string::npos) {
			break;
		}

		to_return.replace(pos, what_part.length(), replace_part);
	}

	return to_return;
}

bool is_ends_with(const OIIO::ustring& input_string, const XSI::CString& end_fragment)
{
	size_t input_size = input_string.size();
	size_t ending_size = end_fragment.Length();
	if (input_size < ending_size)
	{
		return false;
	}

	
	for (size_t i = 0; i < ending_size; i++)
	{
		char v = end_fragment[ending_size - i - 1];
		if (input_string[input_size - 1 - i] != v)
		{
			return false;
		}
	}

	return true;
}

bool is_start_from(const OIIO::ustring& str, const OIIO::ustring& prefix)
{
	if (str.size() < prefix.size())
	{
		return false;
	}

	for (size_t i = 0; i < prefix.size(); i++)
	{
		if (str[i] != prefix[i])
		{
			return false;
		}
	}

	return true;
}

// get from OpenVDB for Softimage sources (VDB_GridIO.cpp:37)
inline std::string parse_file_name(const std::string& in_filename, int frame)
{
	std::string retVal;
	std::string filename = in_filename;

	size_t resPos = filename.find("$F");
	if (resPos == std::string::npos)
	{
		retVal.append(filename);

	}
	else
	{
		std::ostringstream oss;

		if (filename.size() - 2 > resPos && filename[resPos + 2] >= 48 && filename[resPos + 2] <= 57) // check for digits 0...9
		{
			int numWidth = filename[resPos + 2] - 48;
			oss << std::setfill('0') << std::setw(numWidth) << frame;
			filename.erase(resPos, 3);
		}
		else
		{
			filename.erase(resPos, 2);
			oss << frame;
		}

		retVal.append(filename);
		retVal.append(oss.str());
	}

	return retVal;
};

XSI::CString vdbprimitive_inputs_to_path(const XSI::CParameterRefArray& params, const XSI::CTime& eval_time)
{
	XSI::CString full_path = XSI::CUtils::ResolveTokenString(XSI::CString(params.GetValue("folder", eval_time)), eval_time, false);
	if (full_path[full_path.Length() - 1] != '//')
	{
		full_path += XSI::CUtils::Slash();
	}

	std::string file_name = parse_file_name(std::string(XSI::CString(params.GetValue("file", eval_time)).GetAsciiString()), (int)(params.GetValue("frame", eval_time)) + (int)(params.GetValue("offset", eval_time)));
	full_path += file_name.c_str();
	full_path += ".vdb";

	if (full_path.Length() > 0 && !XSI::CUtils::IsAbsolutePath(full_path))
	{
		
		XSI::CString project_path = get_project_path();
		full_path = XSI::CUtils::ResolvePath(XSI::CUtils::BuildPath(project_path, full_path));
	}

	return full_path;
}

XSI::CString combine_names(const XSI::CString &prefix, const XSI::CString &postfix)
{
	return prefix + "." + postfix;
}

XSI::CString combine_geometry_name(const XSI::X3DObject &xsi_object, const XSI::PolygonMesh &xsi_polymesh)
{
	return combine_names(xsi_object.GetFullName(), xsi_polymesh.GetName());
}

XSI::CString combine_geometry_name(const XSI::X3DObject& xsi_object, const XSI::HairPrimitive& xsi_hair)
{
	return combine_names(xsi_object.GetFullName(), xsi_hair.GetName());
}

XSI::CString combine_geometry_name(const XSI::X3DObject& xsi_object, const XSI::Primitive& xsi_primitive)
{
	return combine_names(xsi_object.GetFullName(), xsi_primitive.GetName());
}

XSI::CString combine_geometry_name(const XSI::X3DObject& xsi_object, const XSI::CString &name)
{
	return combine_names(xsi_object.GetFullName(), name);
}