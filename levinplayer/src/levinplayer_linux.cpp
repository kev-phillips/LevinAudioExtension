#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_LINUX)

// Linux: the folder holding the running executable, like the Windows version.

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *levinplayer_init()
{
	char *bundlePath = (char *)malloc(PATH_MAX);
	ssize_t length = readlink("/proc/self/exe", bundlePath, PATH_MAX - 1);
	if (length > 0)
	{
		bundlePath[length] = 0;
		// Remove the last path component
		char *slash = strrchr(bundlePath, '/');
		if (slash)
		{
			*slash = 0;
		}
	}
	else
	{
		bundlePath[0] = 0;
	}
	return bundlePath;
}

#endif
