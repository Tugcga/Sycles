#include <string>
#include <windows.h>
#include <filesystem>
#include <map>
#include <cstdio>

#include <xsi_application.h>
#include <xsi_project.h>
#include <xsi_utils.h>

#include "../input/input.h"
#include "logs.h"
#include "strings.h"
#include "math.h"
#include "io_exr.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool create_dir(const std::string& file_path)
{
	const size_t lastSlash = file_path.find_last_of("/\\");
	std::string folder_path = file_path.substr(0, lastSlash);
	std::string file_name = file_path.substr(lastSlash + 1, file_path.length());
	if (file_name.length() > 0 && file_name[0] == ':')//unsupported file start
	{
		return false;
	}
	while (CreateDirectory(folder_path.c_str(), NULL) == FALSE)
	{
		if (ERROR_ALREADY_EXISTS == GetLastError())
		{
			return true;
		}
		TCHAR sTemp[MAX_PATH];
		int k = folder_path.length();
		strcpy(sTemp, folder_path.c_str());

		while (CreateDirectory(sTemp, NULL) != TRUE)
		{
			while (sTemp[--k] != '\\')
			{
				if (k <= 1)
				{
					return false;
				}
				sTemp[k] = NULL;
			}
		}
	}
	return true;
}

XSI::CString create_temp_path()
{
	UUID uuid;
	UuidCreate(&uuid);
	char* uuid_str;
	UuidToStringA(&uuid, (RPC_CSTR*)&uuid_str);
	XSI::CString temp_path = get_project_path() + "\\sycles_temp_" + XSI::CString(uuid_str);
	RpcStringFreeA((RPC_CSTR*)&uuid_str);

	std::string temp_path_str = temp_path.GetAsciiString();

	if (!std::filesystem::is_directory(temp_path_str) || !std::filesystem::exists(temp_path_str))
	{
		std::filesystem::create_directory(temp_path_str);
	}

	return temp_path;
}

void remove_temp_path(const XSI::CString &temp_path)
{
	if (temp_path.Length() > 0)
	{
		std::filesystem::remove_all(temp_path.GetAsciiString());
	}
}

std::map<int, XSI::CString> sync_image_tiles(const XSI::CString& image_path)
{
	std::map<int, XSI::CString> to_return;

	ULONG start = image_path.ReverseFindString("1001");
	if (start != UINT_MAX)
	{
		std::filesystem::path input_path(image_path.GetAsciiString());
		std::string input_string = input_path.generic_string();
		// get directory
		std::filesystem::path directory_path = input_path.parent_path();

		std::string start_string = input_string.substr(0, start);
		std::string end_string = input_string.substr(start + 4);
		for (const auto& directory_file : std::filesystem::directory_iterator(directory_path))
		{
			std::filesystem::path file_path = directory_file.path();
			std::string file_string = file_path.generic_string();

			size_t find_start_pos = file_string.find(start_string);
			if (find_start_pos != std::string::npos)
			{
				size_t find_end_pos = file_string.rfind(end_string);

				if (find_end_pos != std::string::npos)
				{
					if (find_start_pos == 0 && find_end_pos > start && (find_end_pos - start) == 4)
					{
						std::string udim_string = file_string.substr(start, 4);
						int udim_int;
						int is_convert = sscanf(udim_string.c_str(), "%d", &udim_int);
						if (is_convert == 1)  // success to convert
						{
							to_return[udim_int] = XSI::CString(replace_all_substrings(file_string, "/", "\\").c_str());
						}
					}
				}
			}
		}

	}

	return to_return;
}

std::vector<float> load_image(const XSI::CString &file_path, ULONG&out_width, ULONG&out_height, ULONG &out_channels, bool& out_sucess)
{
	int width = 0;
	int height = 0;
	int channels = 0;

	// get extension of the image
	ULONG point_pos = file_path.ReverseFindString(".");
	if (point_pos != UINT_MAX)
	{
		XSI::CString ext = file_path.GetSubString(point_pos + 1);
		ext.Lower();

		// is extension supported by stb_image?
		if (ext == "jpeg" || ext == "jpg" || ext == "png" || ext ==  "bmp" || ext == "hdr" || ext == "psd" ||
			ext == "tga" || ext == "gif" || ext == "pic" || ext == "pgm" || ext == "ppm")
		{
			stbi_ldr_to_hdr_gamma(1.0f);

			float* pixels_data = stbi_loadf(file_path.GetAsciiString(), &width, &height, &channels, 0);
			if (width == 0 || height == 0 || channels == 0)
			{
				out_sucess = false;
			}
			else
			{
				out_width = width;
				out_height = height;
				out_channels = channels;
				out_sucess = true;
			}

			if (out_sucess)
			{
				// flip pixels
				return flip_pixels(pixels_data, out_width, out_height, out_channels);
			}
		}
		else if (ext == "exr")
		{
			// use tiny exr to load pixels of the exr image
			std::vector<float> exr_pixels;
			int width = 0;
			int height = 0;
			int channels = 0;
			bool is_success = load_input_exr(file_path.GetAsciiString(), exr_pixels, width, height, channels);
			if (is_success)
			{
				out_sucess = true;
				out_width = width;
				out_height = height;
				out_channels = channels;

				return flip_pixels(&exr_pixels[0], out_width, out_height, out_channels);
			}
			else
			{
				out_sucess = false;
			}
		}
		else
		{
			// unsupported image load format
			out_sucess = false;
		}
	}

	return std::vector<float>(0);
}

// return true if extension corresponds to the ldr image format,
// flase if it hdr
bool is_ext_ldr(std::string ext)
{
	if (ext == "png" || ext == "bmp" || ext == "tga" || ext == "jpg" || ext == "ppm")
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool is_output_extension_supported(const XSI::CString& extension)
{
	if (extension == "pfm" || extension == "ppm" || extension == "exr" || extension == "png" || extension == "bmp" || extension == "tga" || extension == "jpg" || extension == "hdr")
	{
		return true;
	}
	else
	{
		return false;
	}
}