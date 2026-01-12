#pragma once

#include <dmsdk/sdk.h>
#include "raudio.h"
#include <stdlib.h>

#if defined(DM_PLATFORM_HTML5)
#include <regex>
#endif

//Paths
static const char *path;
static const char *asset_path = "/assets/";

extern const char *levinplayer_init();

static dmResource::HFactory resource_factory = 0;
static void *resource_music_data = 0;
static uint32_t resource_music_size = 0;

static Music music;
static int music_count = 0;
static int key = 0;
