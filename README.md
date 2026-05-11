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
levinplayer.music_buffer_size(4096)
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
- `levinplayer.unload_music()`

`tracker_state()` returns `nil` when no XM module is loaded. For XM playback it returns song position plus a `channels` table containing note, instrument, effect, volume, panning, and latest trigger data for each channel.

## Notes

This extension intentionally focuses on tracker formats. Defold already handles common streamed audio formats such as Ogg Vorbis and WAV.

Known tested platforms: Windows, macOS, and Android.

The default music stream buffer is set to 4096 frames to reduce underruns when Defold's main update loop has short hitches. You can tune this per project with `levinplayer.music_buffer_size(frames)` before loading music.
