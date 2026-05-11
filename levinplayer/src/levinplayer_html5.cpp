#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_HTML5)

#include <emscripten.h>
#include <stdlib.h>

const char *levinplayer_init()
{
	//Not using the path for HTML5
	const char *path = "";
	return path;
}

#endif