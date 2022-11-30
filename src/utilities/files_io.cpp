#include <string>
#include <windows.h>

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