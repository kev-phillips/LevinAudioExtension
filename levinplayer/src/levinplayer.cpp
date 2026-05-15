// raudio.cpp
// Extension lib defines
#define LIB_NAME "levinplayer"
#define MODULE_NAME "levinplayer"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#undef PlaySound
#include "levinplayer_private.h" 

#define XM_NOTE_OFF 97
#define XM_NOTE_IS_VALID(n) ((n) > 0 && (n) < XM_NOTE_OFF)
#define FADE_CHANNEL_CAPACITY 64

typedef struct FadeState {
    bool active;
    float start;
    float target;
    float duration;
    float elapsed;
    bool stopWhenDone;
} FadeState;

static FadeState music_fade = { 0 };
static FadeState channel_fades[FADE_CHANNEL_CAPACITY] = { 0 };
static float current_music_volume = 1.0f;

static float MaxFloat(float a, float b)
{
    return a > b ? a : b;
}

static float Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float LerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

static void ClearFadeState()
{
    memset(&music_fade, 0, sizeof(music_fade));
    memset(channel_fades, 0, sizeof(channel_fades));
}

static void ApplyMusicVolume(float volume)
{
    current_music_volume = MaxFloat(0.0f, volume);
    if (IsMusicReady(music))
    {
        SetMusicVolume(music, current_music_volume);
    }
}

static void UpdateFade(FadeState *fade, float dt, float *out_value, bool *out_done)
{
    if (!fade->active)
    {
        *out_done = false;
        return;
    }

    fade->elapsed += MaxFloat(0.0f, dt);
    float t = fade->duration <= 0.0f ? 1.0f : Clamp01(fade->elapsed / fade->duration);
    *out_value = LerpFloat(fade->start, fade->target, t);
    *out_done = t >= 1.0f;
    if (*out_done)
    {
        fade->active = false;
    }
}

static void UpdateFadeState(float dt)
{
    if (!IsMusicReady(music))
    {
        ClearFadeState();
        return;
    }

    float value = 0.0f;
    bool done = false;
    UpdateFade(&music_fade, dt, &value, &done);
    if (music_fade.active || done)
    {
        ApplyMusicVolume(value);
        if (done && music_fade.stopWhenDone)
        {
            StopMusicStream(music);
        }
    }

    for (int i = 0; i < FADE_CHANNEL_CAPACITY; ++i)
    {
        UpdateFade(&channel_fades[i], dt, &value, &done);
        if (channel_fades[i].active || done)
        {
            SetMusicChannelVolume(music, i + 1, value);
        }
    }
}

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
    ClearFadeState();

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
    ClearFadeState();

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
    bool playing = IsMusicReady(music) && IsMusicStreamPlaying(music);
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
    ClearFadeState();
    return 0;
}

static int playmusic(lua_State *L)
{
    if (!IsMusicReady(music))
    {
        dmLogWarning("play_music: no music loaded.");
        return 0;
    }

    PlayMusicStream(music);    
    return 0;
}

static int stopmusic(lua_State *L)
{
    if (!IsMusicReady(music))
    {
        dmLogWarning("stop_music: no music loaded.");
        return 0;
    }

    StopMusicStream(music);
    ClearFadeState();
    return 0;
}

static int pausemusic(lua_State *L)
{
    if (!IsMusicReady(music))
    {
        dmLogWarning("pause_music: no music loaded.");
        return 0;
    }

    PauseMusicStream(music);
    return 0;
}

static int resumemusic(lua_State *L)
{
    if (!IsMusicReady(music))
    {
        dmLogWarning("resume_music: no music loaded.");
        return 0;
    }

    ResumeMusicStream(music);
    return 0;
}

static int musicvolume(lua_State *L)
{
    double volume = luaL_checknumber(L, 1);
    music_fade.active = false;
    ApplyMusicVolume((float)volume);
    return 0;
}

static int musicpitch(lua_State *L)
{
    double pitch = luaL_checknumber(L, 1);
    SetMusicPitch(music, pitch);
    return 0;
}

static int setchannelmuted(lua_State *L)
{
    int channel = luaL_checkinteger(L, 1);
    bool muted = lua_toboolean(L, 2);
    if (!IsMusicReady(music))
    {
        dmLogWarning("set_channel_muted: no music loaded.");
        return 0;
    }

    SetMusicChannelMuted(music, channel, muted);
    return 0;
}

static int ischannelmuted(lua_State *L)
{
    int channel = luaL_checkinteger(L, 1);
    bool muted = IsMusicReady(music) && IsMusicChannelMuted(music, channel);
    lua_pushboolean(L, muted);
    return 1;
}

static int setchannelvolume(lua_State *L)
{
    int channel = luaL_checkinteger(L, 1);
    double volume = luaL_checknumber(L, 2);
    if (!IsMusicReady(music))
    {
        dmLogWarning("set_channel_volume: no music loaded.");
        return 0;
    }

    if (channel >= 1 && channel <= FADE_CHANNEL_CAPACITY)
    {
        channel_fades[channel - 1].active = false;
    }
    SetMusicChannelVolume(music, channel, (float)volume);
    return 0;
}

static int getchannelvolume(lua_State *L)
{
    int channel = luaL_checkinteger(L, 1);
    double volume = IsMusicReady(music) ? GetMusicChannelVolume(music, channel) : 1.0;
    lua_pushnumber(L, volume);
    return 1;
}

static int musiclength(lua_State *L)
{    
    double length = IsMusicReady(music) ? GetMusicTimeLength(music) : 0.0;
    lua_pushnumber(L, length);
    return 1;
}

static int musicposition(lua_State *L)
{
    double position = IsMusicReady(music) ? GetMusicTimePlayed(music) : 0.0;
    lua_pushnumber(L, position);
    return 1;
}

static int seekmusic(lua_State *L)
{
    double position = luaL_checknumber(L, 1);
    if (!IsMusicReady(music))
    {
        dmLogWarning("seek_music: no music loaded.");
        return 0;
    }

    if (position < 0.0) position = 0.0;
    double length = GetMusicTimeLength(music);
    if (length > 0.0 && position > length) position = length;

    SeekMusicStream(music, (float)position);
    return 0;
}

static int fademusicvolume(lua_State *L)
{
    double target = luaL_checknumber(L, 1);
    double duration = luaL_optnumber(L, 2, 0.0);
    bool stop_when_done = lua_toboolean(L, 3);

    if (!IsMusicReady(music))
    {
        dmLogWarning("fade_music_volume: no music loaded.");
        return 0;
    }

    music_fade.active = true;
    music_fade.start = current_music_volume;
    music_fade.target = MaxFloat(0.0f, (float)target);
    music_fade.duration = MaxFloat(0.0f, (float)duration);
    music_fade.elapsed = 0.0f;
    music_fade.stopWhenDone = stop_when_done;
    UpdateFadeState(0.0f);
    return 0;
}

static int stopmusicfade(lua_State *L)
{
    music_fade.active = false;
    return 0;
}

static int ismusicfading(lua_State *L)
{
    lua_pushboolean(L, music_fade.active);
    return 1;
}

static int fadechannelvolume(lua_State *L)
{
    int channel = luaL_checkinteger(L, 1);
    double target = luaL_checknumber(L, 2);
    double duration = luaL_optnumber(L, 3, 0.0);

    if (!IsMusicReady(music))
    {
        dmLogWarning("fade_channel_volume: no music loaded.");
        return 0;
    }

    if (channel < 1 || channel > FADE_CHANNEL_CAPACITY)
    {
        dmLogWarning("fade_channel_volume: channel %d is outside supported range 1-%d.", channel, FADE_CHANNEL_CAPACITY);
        return 0;
    }

    FadeState *fade = &channel_fades[channel - 1];
    fade->active = true;
    fade->start = GetMusicChannelVolume(music, channel);
    fade->target = MaxFloat(0.0f, (float)target);
    fade->duration = MaxFloat(0.0f, (float)duration);
    fade->elapsed = 0.0f;
    fade->stopWhenDone = false;
    UpdateFadeState(0.0f);
    return 0;
}

static int stopchannelfade(lua_State *L)
{
    int channel = luaL_checkinteger(L, 1);
    if (channel >= 1 && channel <= FADE_CHANNEL_CAPACITY)
    {
        channel_fades[channel - 1].active = false;
    }
    return 0;
}

static int ischannelfading(lua_State *L)
{
    int channel = luaL_checkinteger(L, 1);
    bool active = channel >= 1 && channel <= FADE_CHANNEL_CAPACITY && channel_fades[channel - 1].active;
    lua_pushboolean(L, active);
    return 1;
}

static int updatefades(lua_State *L)
{
    double dt = luaL_optnumber(L, 1, 0.0);
    UpdateFadeState((float)dt);
    return 0;
}

static int musicbuffersize(lua_State *L)
{
    int size = luaL_checkinteger(L, 1);
    if (size < 256)
    {
        dmLogWarning("music_buffer_size: clamping %d to 256 frames.", size);
        size = 256;
    }
    SetAudioStreamBufferSizeDefault(size);
    return 0;
}

static const char *XmNoteName(uint8_t note, char *buffer, size_t buffer_size)
{
    if (note == 0) return "...";
    if (note == XM_NOTE_OFF) return "===";
    if (!XM_NOTE_IS_VALID(note)) return "???";

    static const char *names[] = {
        "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
    };
    uint8_t note_index = note - 1;
    snprintf(buffer, buffer_size, "%s%d", names[note_index % 12], note_index / 12 + 1);
    return buffer;
}

static const char *XmEffectName(uint8_t effect)
{
    switch (effect)
    {
        case 0: return "0";
        case 1: return "1";
        case 2: return "2";
        case 3: return "3";
        case 4: return "4";
        case 5: return "5";
        case 6: return "6";
        case 7: return "7";
        case 8: return "8";
        case 9: return "9";
        case 0xA: return "A";
        case 0xB: return "B";
        case 0xC: return "C";
        case 0xD: return "D";
        case 0xE: return "E";
        case 0xF: return "F";
        case 16: return "G";
        case 17: return "H";
        case 21: return "L";
        case 25: return "P";
        case 27: return "R";
        case 29: return "T";
        case 33: return "X";
        default: return "?";
    }
}

static void PushIntegerField(lua_State *L, const char *key, lua_Integer value)
{
    lua_pushinteger(L, value);
    lua_setfield(L, -2, key);
}

static void PushNumberField(lua_State *L, const char *key, lua_Number value)
{
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
}

static void PushStringField(lua_State *L, const char *key, const char *value)
{
    lua_pushstring(L, value);
    lua_setfield(L, -2, key);
}

static int trackerstate(lua_State *L)
{
    MusicTrackerChannel channels[64];
    MusicTrackerState state;
    if (!IsMusicReady(music) || !GetMusicTrackerState(music, &state, channels, 64))
    {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    PushIntegerField(L, "order", state.order);
    PushIntegerField(L, "pattern", state.pattern);
    PushIntegerField(L, "row", state.row);
    PushIntegerField(L, "tick", state.tick);
    PushIntegerField(L, "bpm", state.bpm);
    PushIntegerField(L, "tempo", state.tempo);
    PushIntegerField(L, "channels_count", state.channelsCount);
    PushIntegerField(L, "module_length", state.moduleLength);
    PushIntegerField(L, "patterns_count", state.patternsCount);
    PushNumberField(L, "samples", state.samples);
    PushNumberField(L, "time", state.time);

    lua_newtable(L);
    for (int i = 0; i < state.channelsReturned; ++i)
    {
        MusicTrackerChannel *channel = channels + i;

        lua_newtable(L);
        PushIntegerField(L, "index", channel->index);
        PushNumberField(L, "volume", channel->volume);
        PushNumberField(L, "actual_volume", channel->actualVolume);
        PushNumberField(L, "panning", channel->panning);
        PushNumberField(L, "latest_trigger", channel->latestTrigger);
        lua_pushboolean(L, IsMusicChannelMuted(music, channel->index));
        lua_setfield(L, -2, "muted");
        PushNumberField(L, "channel_volume", GetMusicChannelVolume(music, channel->index));

        char note_name[4];
        char text[32];
        const char *name = XmNoteName((uint8_t)channel->note, note_name, sizeof(note_name));
        const char *effect = XmEffectName((uint8_t)channel->effectType);
        snprintf(text, sizeof(text), "%s %02X %02X %s%02X",
            name,
            channel->instrument,
            channel->volumeColumn,
            effect,
            channel->effectParam);

        PushIntegerField(L, "note", channel->note);
        PushStringField(L, "note_name", name);
        PushIntegerField(L, "instrument", channel->instrument);
        PushIntegerField(L, "volume_column", channel->volumeColumn);
        PushIntegerField(L, "effect_type", channel->effectType);
        PushIntegerField(L, "effect_param", channel->effectParam);
        PushIntegerField(L, "row_note", channel->rowNote);
        PushIntegerField(L, "row_instrument", channel->rowInstrument);
        PushIntegerField(L, "row_volume_column", channel->rowVolumeColumn);
        PushIntegerField(L, "row_effect_type", channel->rowEffectType);
        PushIntegerField(L, "row_effect_param", channel->rowEffectParam);
        PushStringField(L, "effect", effect);
        PushStringField(L, "text", text);

        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "channels");

    return 1;
}

static int audiospectrum(lua_State *L)
{
    MusicAudioSpectrum spectrum;
    if (!GetMusicAudioSpectrum(music, &spectrum))
    {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    PushNumberField(L, "rms", spectrum.rms);
    PushNumberField(L, "peak", spectrum.peak);
    PushNumberField(L, "left_rms", spectrum.leftRms);
    PushNumberField(L, "right_rms", spectrum.rightRms);
    PushNumberField(L, "left_peak", spectrum.leftPeak);
    PushNumberField(L, "right_peak", spectrum.rightPeak);

    lua_newtable(L);
    for (int i = 0; i < 16; ++i)
    {
        lua_pushnumber(L, spectrum.bands[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "bands");

    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"is_music_playing", ismusicplaying},
    {"music_length", musiclength},
    {"music_position", musicposition},
    {"music_buffer_size", musicbuffersize},
    {"tracker_state", trackerstate},
    {"audio_spectrum", audiospectrum},
    {"music_pitch", musicpitch},
    {"music_volume", musicvolume},
    {"set_channel_muted", setchannelmuted},
    {"is_channel_muted", ischannelmuted},
    {"set_channel_volume", setchannelvolume},
    {"get_channel_volume", getchannelvolume},
    {"fade_music_volume", fademusicvolume},
    {"stop_music_fade", stopmusicfade},
    {"is_music_fading", ismusicfading},
    {"fade_channel_volume", fadechannelvolume},
    {"stop_channel_fade", stopchannelfade},
    {"is_channel_fading", ischannelfading},
    {"update_fades", updatefades},
    {"play_music", playmusic},
    {"stop_music", stopmusic},
    {"pause_music", pausemusic},
    {"resume_music", resumemusic},
    {"seek_music", seekmusic},
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
    SetAudioStreamBufferSizeDefault(8192);
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
    ClearFadeState();
    CloseAudioDevice();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateRAudio(dmExtension::Params* params)
{
    if (IsMusicReady(music) && IsMusicStreamPlaying(music))
    {
        UpdateMusicStream(music);
    }
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
            break;
    }
}

// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// RAudio is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(LevinPlayer, LIB_NAME, AppInitializeRAudio, AppFinalizeRAudio, InitializeRAudio, OnUpdateRAudio, OnEventRAudio, FinalizeRAudio)
