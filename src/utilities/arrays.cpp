#include <xsi_x3dobject.h>

#include <vector>
#include <string>

bool is_contains(const std::vector<std::string>& array, const std::string &value)
{
	for (LONG i = 0; i < array.size(); i++)
	{
		if (array[i] == value)
		{
			return true;
		}
	}
	return false;
}

bool is_contains(const std::vector<ULONG> &array, const ULONG value)
{
	for (LONG i = 0; i < array.size(); i++)
	{
		if (array[i] == value)
		{
			return true;
		}
	}
	return false;
}

bool is_contains(const XSI::CRefArray &array, const XSI::CRef &object)
{
	for (LONG i = 0; i < array.GetCount(); i++)
	{
		if (object == array[i])
		{
			return true;
		}
	}

	return false;
}

int get_index_in_array(const std::vector<ULONG> &array, const ULONG value)
{
	for (LONG i = 0; i < array.size(); i++)
	{
		if (array[i] == value)
		{
			return i;
		}
	}
	return -1;
}

int get_index_in_array(const std::vector<std::string>& array, const std::string& value)
{
	for (LONG i = 0; i < array.size(); i++)
	{
		if (array[i] == value)
		{
			return i;
		}
	}
	return -1;
}