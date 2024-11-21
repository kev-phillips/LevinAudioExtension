// raudio.cpp
// Extension lib defines
#define LIB_NAME "levinplayer"
#define MODULE_NAME "player"

// include the Defold SDK
#include <dmsdk/sdk.h>
#undef PlaySound
#include "levinplayer_private.h" 

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
    int top = lua_gettop(L);

    const char *str = luaL_checkstring(L, 1);
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

static int ismusicplaying(lua_State *L)
{
    bool playing = false;
    playing = IsMusicStreamPlaying(music);
    lua_pushboolean(L, playing);
    return 1;
}

static int playmusic(lua_State *L)
{
    dmLogInfo("\n Frame count = %d", music.frameCount);
    PlayMusicStream(music);
    SetMasterVolume(1.0f);
    SetMusicVolume(music, 1.0f);
    SetMusicPitch(music, 1.0f);
    
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
    // CloseAudioDevice();
    InitAudioDevice();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeRAudio(dmExtension::Params* params)
{
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
    dmLogInfo("AppFinalizeRAudio");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeRAudio(dmExtension::Params* params)
{
    dmLogInfo("FinalizeRAudio");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateRAudio(dmExtension::Params* params)
{
    dmLogInfo("OnUpdateRAudio");
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
