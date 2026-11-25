#pragma once

// Recordings in a .levinaudio file (LevinPlayer 1.3): Ogg Vorbis audio that is
// decoded once when the song loads, split into left and right, and lent to the
// module's placeholder samples as slices (views, nothing copied per slice).
//
// No Defold or raylib dependencies: needs jar_xm's declarations and
// stb_vorbis_decode_memory, so it can be unit tested on its own.

#include <stdint.h>
#include <stdlib.h>
#include "levinaudio.h"

// (C linkage, as stb_vorbis.c gives its functions when raudio.cpp compiles it as C++)
#ifdef __cplusplus
extern "C" {
#endif
int stb_vorbis_decode_memory(const unsigned char *mem, int len, int *channels, int *sample_rate, short **output);
#ifdef __cplusplus
}
#endif

// Just what's needed from jar_xm (its header defines functions outright, so it can't be
// included a second time; the implementation lives in raudio.cpp).
typedef struct jar_xm_context_s jar_xm_context_t;
int jar_xm_attach_sample(jar_xm_context_t* ctx, uint16_t instrument, uint16_t sample, const int16_t* left, const int16_t* right, uint32_t frames, int loop, uint32_t loop_start, uint32_t loop_length);

typedef struct LevinRecordings
{
    int16_t *left[LEVINAUDIO_MAX_RECORDINGS];
    int16_t *right[LEVINAUDIO_MAX_RECORDINGS];   // 0 for a mono recording
    uint32_t frames[LEVINAUDIO_MAX_RECORDINGS];
    int sample_rate[LEVINAUDIO_MAX_RECORDINGS];
} LevinRecordings;

static inline uint32_t levinrec_u32(const unsigned char *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static inline void levinrecordings_free(LevinRecordings *r)
{
    for (int i = 0; i < LEVINAUDIO_MAX_RECORDINGS; ++i)
    {
        free(r->left[i]); free(r->right[i]);
        r->left[i] = r->right[i] = 0; r->frames[i] = 0; r->sample_rate[i] = 0;
    }
}

// Decodes the recordings the slices use and attaches every slice to its sample.
// Returns the number of slices attached (bad entries are skipped). Free with
// levinrecordings_free() after the module is unloaded, not before.
static inline int levinrecordings_attach(LevinRecordings *r, const LevinAudioFile *file, jar_xm_context_t *ctx)
{
    if (!ctx || !file->slices || !file->slice_count) return 0;
    int attached = 0;
    for (uint16_t k = 0; k < file->slice_count; ++k)
    {
        const unsigned char *e = file->slices + 20u * k;
        uint8_t ins = e[0], smp = e[1], rec = e[2], loop = e[3];
        uint32_t start = levinrec_u32(e + 4), frames = levinrec_u32(e + 8), ls = levinrec_u32(e + 12), ll = levinrec_u32(e + 16);
        if (rec >= LEVINAUDIO_MAX_RECORDINGS || !file->recording[rec]) continue;
        if (!r->left[rec])
        {
            int channels = 0, rate = 0; short *pcm = 0;
            int n = stb_vorbis_decode_memory(file->recording[rec], (int)file->recording_size[rec], &channels, &rate, &pcm);
            if (n <= 0 || !pcm || channels < 1) { free(pcm); continue; }
            r->left[rec] = (int16_t *)malloc((size_t)n * sizeof(int16_t));
            r->right[rec] = channels > 1 ? (int16_t *)malloc((size_t)n * sizeof(int16_t)) : 0;
            if (!r->left[rec] || (channels > 1 && !r->right[rec])) { free(pcm); free(r->left[rec]); free(r->right[rec]); r->left[rec] = r->right[rec] = 0; continue; }
            for (int i = 0; i < n; ++i)
            {
                r->left[rec][i] = pcm[i * channels];
                if (channels > 1) r->right[rec][i] = pcm[i * channels + 1];
            }
            free(pcm);
            r->frames[rec] = (uint32_t)n;
            r->sample_rate[rec] = rate;
        }
        if (start >= r->frames[rec] || frames == 0) continue;
        if (frames > r->frames[rec] - start) frames = r->frames[rec] - start;
        if (jar_xm_attach_sample(ctx, ins, smp, r->left[rec] + start, r->right[rec] ? r->right[rec] + start : 0, frames, loop, ls, ll)) attached++;
    }
    return attached;
}
