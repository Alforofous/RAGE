#include "RAGE.hpp"

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#elif __linux__
#include <unistd.h>
#endif

static std::string getExecutablePath()
{
	char buffer[1024];
#ifdef _WIN32
	GetModuleFileName(NULL, buffer, sizeof(buffer));
#elif __APPLE__
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) == 0)
	{
		char real_path[PATH_MAX];
		if (realpath(buffer, real_path) != NULL)
		{
			return std::string(real_path);
		}
	}
#elif __linux__
	readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif
	return std::string(buffer);
}

std::string getExecutableDir()
{
	std::string path = getExecutablePath();
	std::filesystem::path dirPath(path);
	return dirPath.parent_path();
}