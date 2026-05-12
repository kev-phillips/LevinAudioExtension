# LevinPlayer

Defold native extension for playing XM/MOD tracker music with lightweight tracker-state feedback for music-driven visuals and gameplay.

The reusable extension lives in `levinplayer/`. The rest of this repository is an example Defold project.

## Installation

For a release dependency, package the `levinplayer/` folder at the root of a zip file and add that zip URL to your Defold project's dependencies.

If your game loads tracker files with `levinplayer.load_music_resource()`, add the folder containing those files to `game.project`:

```ini
[project]
custom_resources = res/common/assets
```

## Lua API

The extension registers a global Lua module named `levinplayer`.

```lua
levinplayer.master_volume(1.0)
levinplayer.music_buffer_size(8192)
levinplayer.load_music_resource("/res/common/assets/song.xm")
levinplayer.play_music()
levinplayer.music_volume(1.0)
levinplayer.music_pitch(1.0)
```

Available functions:

- `levinplayer.master_volume(volume)`
- `levinplayer.music_buffer_size(frames)`
- `levinplayer.load_music(path)`
- `levinplayer.load_music_resource(resource_path)`
- `levinplayer.play_music()`
- `levinplayer.music_volume(volume)`
- `levinplayer.music_pitch(pitch)`
- `levinplayer.music_length()`
- `levinplayer.is_music_playing()`
- `levinplayer.tracker_state()`
- `levinplayer.audio_spectrum()`
- `levinplayer.unload_music()`

`tracker_state()` returns `nil` when no supported tracker module is loaded. For XM and MOD playback it returns song position plus a `channels` table containing note, instrument, effect, volume, panning, and trigger data for each tracker channel. MOD files report the channel count from the module, so classic ProTracker files usually expose four channels.

Channel `note`, `instrument`, and `volume_column` values are latched playback-state values: they hold the last audible tracker event until the channel receives a new note or note-off. Raw pattern-cell values for the current row are exposed separately as `row_note`, `row_instrument`, `row_volume_column`, `row_effect_type`, and `row_effect_param`.

`audio_spectrum()` returns lightweight mixed-output spectral feedback for visualizers. It currently exposes 16 smoothed frequency bands plus `rms` and `peak`, derived from the generated PCM stream before it is submitted to Defold audio. This is intended for shader/UI feedback such as logo bars, beat-reactive effects, or future FFT-style visual development.

## Notes

This extension intentionally focuses on tracker formats. Defold already handles common streamed audio formats such as Ogg Vorbis and WAV.

Known tested platforms: Windows, macOS, and Android.

The default music stream buffer is set to 8192 frames to reduce underruns when Defold's main update loop has short hitches. You can tune this per project with `levinplayer.music_buffer_size(frames)` before loading music.
