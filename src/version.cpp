#include "version.h"

#define SYCLES_MAJOR_VERSION_NUM    2
#define SYCLES_MINOR_VERSION_NUM    2
#define SYCLES_PATCH_VERSION_NUM    0

unsigned int get_major_version()
{
	return SYCLES_MAJOR_VERSION_NUM;
}

unsigned int get_minor_version()
{
	return SYCLES_MINOR_VERSION_NUM;
}

unsigned int get_patch_version()
{
	return SYCLES_PATCH_VERSION_NUM;
}