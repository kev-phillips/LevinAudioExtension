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

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"reverse", Reverse},
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
    //InitAudioDevice();
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
