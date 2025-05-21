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

bool is_contains(const std::vector<XSI::CString>& array, const XSI::CString& value)
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

// we assume that arrays a and b are sorted
// so we can check their values from begint to end
bool is_arrays_the_same(const std::vector<ULONG>& a, const std::vector<ULONG>& b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.size(); i++)
	{
		if (a[i] != b[i])
		{
			return false;
		}
	}

	return true;
}

int get_index_in_array(const std::vector<std::vector<ULONG>>& collection, const std::vector<ULONG>& array)
{
	for (size_t i = 0; i < collection.size(); i++)
	{
		if (is_arrays_the_same(collection[i], array))
		{
			return i;
		}
	}

	return -1;
}

float get_maximum(const std::vector<float>& array)
{
	float to_return = array[0];
	for (size_t i = 0; i < array.size(); i++)
	{
		float v = array[i];
		if (to_return < v)
		{
			to_return = v;
		}
	}

	return to_return;
}

// return index of the value in the array
// return -1 if there are no such element
int binary_search(const std::vector<ULONG>& array, ULONG value)
{
	int i = 0, j = array.size() - 1;
	while (i <= j) {
		int m = i + (j - i) / 2;
		if (array[m] < value)
		{
			i = m + 1;
		}
		else if (array[m] > value)
		{
			j = m - 1;
		}
		else
		{
			return m;
		}
	}
	return -1;
}

bool is_sorted_array_contains_value(const std::vector<ULONG>& array, ULONG value)
{
	// use binary search
	return binary_search(array, value) != -1;
}

void copy_filtered(const std::vector<float>& src, std::vector<float>& dst, float limit_value) {
	size_t dst_iterator = 0;
	for (size_t i = 0; i < src.size(); i++) {
		float v = src[i];
		if (v > limit_value) {
			dst[dst_iterator] = v;
			dst_iterator++;

			if (dst_iterator >= dst.size()) {
				break;
			}
		}
	}
}