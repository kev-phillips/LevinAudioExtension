#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_ANDROID)

#include <stdlib.h>
#include <string.h>

const char *levinplayer_init()
{
    const char *bundlePath = "/assets/";
    char *path = (char *)malloc(strlen(bundlePath) + 1);
    strcpy(path, bundlePath);
    return path;
}

#endif
