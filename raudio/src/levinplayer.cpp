// raudio.cpp
// Extension lib defines
#define LIB_NAME "levinplayer"
#define MODULE_NAME "player"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <stdlib.h>
#include <string.h>
#undef PlaySound
#include "levinplayer_private.h" 

static void FreeResourceMusicData()
{
    if (resource_music_data)
    {
        free(resource_music_data);
        resource_music_data = 0;
        resource_music_size = 0;
    }
}

static const char *GetFileExtension(const char *filePath)
{
    const char *dot = strrchr(filePath, '.');
    if (!dot || dot == filePath) return "";
    return dot;
}

static void patch_path()
{
    #if defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_IOS) // #ifndef DM_PLATFORM_ANDROID
    char *bundlePath = new char[strlen(path) + strlen(asset_path) + 1];
    strcpy(bundlePath, path);
    strcat(bundlePath, asset_path);
    path = bundlePath;
    dmLogInfo("Patched: %s", path);
    #endif
}

static int Reverse(lua_State* L)
{
    // The number of expected items to be on the Lua stack
    // once this struct goes out of scope
    DM_LUA_STACK_CHECK(L, 1);

    // Check and get parameter string from stack
    char* str = (char*)luaL_checkstring(L, 1);

    // Reverse the string
    int len = strlen(str);
    for(int i = 0; i < len / 2; i++) {
        const char a = str[i];
        const char b = str[len - i - 1];
        str[i] = b;
        str[len - i - 1] = a;
    }

    // Put the reverse string on the stack
    lua_pushstring(L, str);

    // Return 1 item
    return 1;
}

static int buildpath(lua_State *L)
{
    const char *build_path = luaL_checkstring(L, 1);
    if (build_path != NULL && build_path[0] != '\0')
    {
        path = build_path;
        dmLogInfo("Music Base Path for Defold Editor: %s", path);
        return 0;
    }
    else
    {
        dmLogError("build_path cannot be empty. Please provide a full path of your project folder when building on Defold Editor.");
        return 0;
    }
}

static int mastervolume(lua_State *L)
{
    double volume = luaL_checknumber(L, 1);
    SetMasterVolume(volume);
    return 0;
}


static int loadmusic(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    if (IsMusicReady(music))
    {
        StopMusicStream(music);
        UnloadMusicStream(music);
    }
    FreeResourceMusicData();

    char *bundlePath = new char[strlen(path) + strlen(str) + 1];
    strcpy(bundlePath, path);
    strcat(bundlePath, str);

    #if defined(DM_PLATFORM_HTML5)
        std::regex pattern(".*(?=\/)[/]");
        std::string result = std::regex_replace(bundlePath, pattern, "");
        char *cstr = new char[result.length() + 1];
        strcpy(cstr, result.c_str());
        bundlePath = cstr;
        dmLogInfo("File for HTML: %s", bundlePath);
    #endif

    music_count++;
    music = LoadMusicStream(bundlePath);

    if (music.isLoaded)
    {
        dmLogInfo("\nMusic locked and loaded.\n");
    }
    else
    {
        dmLogInfo("\nMusic not loaded\n");
    }
            
    return 0;
}

static int loadmusic_resource(lua_State *L)
{
    const char *resource_path = luaL_checkstring(L, 1);
    if (resource_path == NULL || resource_path[0] == '\0')
    {
        dmLogError("load_music_resource: resource path cannot be empty.");
        return 0;
    }

    if (resource_factory == 0)
    {
        dmLogError("load_music_resource: resource factory not available.");
        return 0;
    }

    void *raw_data = 0;
    uint32_t raw_size = 0;
    dmResource::Result result = dmResource::GetRaw(resource_factory, resource_path, &raw_data, &raw_size);
    if (result != dmResource::RESULT_OK || raw_data == 0 || raw_size == 0)
    {
        dmLogError("load_music_resource: failed to load '%s' (result=%d).", resource_path, result);
        return 0;
    }

    if (IsMusicReady(music))
    {
        StopMusicStream(music);
        UnloadMusicStream(music);
    }
    FreeResourceMusicData();

    resource_music_data = malloc(raw_size);
    if (!resource_music_data)
    {
        dmLogError("load_music_resource: out of memory for '%s'.", resource_path);
        return 0;
    }

    memcpy(resource_music_data, raw_data, raw_size);
    resource_music_size = raw_size;
    // dmResource::GetRaw returns resource data managed by Defold; do not release raw_data here.
    const char *ext = GetFileExtension(resource_path);
    music = LoadMusicStreamFromMemory(ext, (const unsigned char *)resource_music_data, (int)resource_music_size);

    if (!IsMusicReady(music))
    {
        dmLogError("load_music_resource: could not load music data for '%s'.", resource_path);
    }

    return 0;
}

static int ismusicplaying(lua_State *L)
{
    bool playing = false;
    playing = IsMusicStreamPlaying(music);
    lua_pushboolean(L, playing);
    return 1;
}

static int unloadmusic(lua_State *L)
{
    if (IsMusicReady(music))
    {
        StopMusicStream(music);
        UnloadMusicStream(music);
    }
    FreeResourceMusicData();
    return 0;
}

static int playmusic(lua_State *L)
{
    dmLogInfo("\n Frame count = %d", music.frameCount);
    PlayMusicStream(music);    
    return 0;
}

static int musicvolume(lua_State *L)
{
    double volume = luaL_checknumber(L, 1);
    SetMusicVolume(music, volume);
    return 0;
}

static int musicpitch(lua_State *L)
{
    double pitch = luaL_checknumber(L, 1);
    SetMusicPitch(music, pitch);
    return 0;
}

static int musiclength(lua_State *L)
{    
    double length = GetMusicTimeLength(music);
   lua_pushnumber(L, length);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"is_music_playing", ismusicplaying},
    {"music_length", musiclength},
    {"music_pitch", musicpitch},
    {"music_volume", musicvolume},
    {"play_music", playmusic},
    {"load_music", loadmusic},
    {"load_music_resource", loadmusic_resource},
    {"unload_music", unloadmusic},
    {"build_path", buildpath},
    {"master_volume", mastervolume},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeRAudio(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializeLevinPlayer");
    InitAudioDevice();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeRAudio(dmExtension::Params* params)
{
    resource_factory = params->m_ResourceFactory;
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    path = levinplayer_init();
    patch_path();
    dmLogInfo("Music Base Path: %s", path);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeRAudio(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeRAudio(dmExtension::Params* params)
{
    if (IsMusicReady(music))
    {
        StopMusicStream(music);
        UnloadMusicStream(music);
    }
    FreeResourceMusicData();
    CloseAudioDevice();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateRAudio(dmExtension::Params* params)
{
    UpdateMusicStream(music);
    return dmExtension::RESULT_OK;
}

static void OnEventRAudio(dmExtension::Params* params, const dmExtension::Event* event)
{
    switch(event->m_Event)
    {
        case dmExtension::EVENT_ID_ACTIVATEAPP:
            dmLogInfo("OnEventRAudio - EVENT_ID_ACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_DEACTIVATEAPP:
            dmLogInfo("OnEventRAudio - EVENT_ID_DEACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_ICONIFYAPP:
            dmLogInfo("OnEventRAudio - EVENT_ID_ICONIFYAPP");
            break;
        case dmExtension::EVENT_ID_DEICONIFYAPP:
            dmLogInfo("OnEventRAudio - EVENT_ID_DEICONIFYAPP");
            break;
        default:
            dmLogWarning("OnEventRAudio - Unknown event id");
            break;
    }
}

// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// RAudio is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(LevinPlayer, LIB_NAME, AppInitializeRAudio, AppFinalizeRAudio, InitializeRAudio, OnUpdateRAudio, OnEventRAudio, FinalizeRAudio)
