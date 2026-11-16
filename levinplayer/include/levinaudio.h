#pragma once

// .levinaudio container reader.
//
// A .levinaudio file wraps a normal XM or MOD module together with extra
// metadata (named channel groups, start volumes, cues, tags) and, optionally,
// editor project data. The player only needs the module and the metadata;
// anything else is skipped, so newer files keep loading in older players.
//
// Layout (all integers little-endian):
//
//   offset 0   "LVNA"            magic
//   offset 4   uint16 version    currently 1
//   offset 6   uint16 flags      reserved, 0
//   offset 8   chunks...         until end of file
//
//   chunk:     char id[4], uint32 size, size bytes of data (no padding)
//
//   "META"  UTF-8 JSON metadata                      (optional, at most one)
//   "XM  "  a complete .xm module                    (exactly one module chunk)
//   "MOD "  a complete .mod module
//   other   ignored (e.g. "EDIT" holds editor project data)
//
// This header has no Defold or raylib dependencies so it can be unit tested
// on its own.

#include <stdint.h>
#include <string.h>

#define LEVINAUDIO_VERSION 1

typedef struct LevinAudioFile
{
    const unsigned char *module;      // points into the source buffer
    uint32_t module_size;
    const char *module_ext;           // ".xm" or ".mod"
    const char *meta;                 // points into the source buffer, NOT null-terminated
    uint32_t meta_size;               // 0 when there is no META chunk
    uint16_t version;
} LevinAudioFile;

static inline uint32_t levinaudio_read_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// True when the buffer starts with the .levinaudio magic.
static inline int levinaudio_is_container(const unsigned char *data, uint32_t size)
{
    return data != 0 && size >= 8 && memcmp(data, "LVNA", 4) == 0;
}

// Parses a .levinaudio buffer. Returns 1 on success and fills `out` with
// pointers into `data`, so `data` must outlive any use of `out`.
// Returns 0 for anything malformed: bad magic, unsupported version, a chunk
// running past the end, no module, or more than one module/META chunk.
static inline int levinaudio_parse(const unsigned char *data, uint32_t size, LevinAudioFile *out)
{
    if (out == 0 || !levinaudio_is_container(data, size)) return 0;
    memset(out, 0, sizeof(*out));

    out->version = (uint16_t)(data[4] | (data[5] << 8));
    if (out->version == 0 || out->version > LEVINAUDIO_VERSION) return 0;

    uint32_t pos = 8;
    int modules = 0;
    int metas = 0;
    while (pos < size)
    {
        if (size - pos < 8) return 0;                       // truncated chunk header
        const unsigned char *id = data + pos;
        uint32_t chunk_size = levinaudio_read_u32(data + pos + 4);
        pos += 8;
        if (chunk_size > size - pos) return 0;              // chunk runs past the end
        const unsigned char *body = data + pos;

        if (memcmp(id, "META", 4) == 0)
        {
            if (++metas > 1) return 0;
            out->meta = (const char *)body;
            out->meta_size = chunk_size;
        }
        else if (memcmp(id, "XM  ", 4) == 0 || memcmp(id, "MOD ", 4) == 0)
        {
            if (++modules > 1 || chunk_size == 0) return 0;
            out->module = body;
            out->module_size = chunk_size;
            out->module_ext = (id[0] == 'X') ? ".xm" : ".mod";
        }
        // Unknown chunks (EDIT, future additions) are skipped.

        pos += chunk_size;
    }

    return modules == 1;
}
