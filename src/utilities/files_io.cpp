#include <string>
#include <windows.h>
#include <filesystem>

#include <xsi_application.h>
#include <xsi_project.h>

#include "../input/input.h"

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