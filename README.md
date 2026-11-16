# LevinPlayer

Defold native extension for playing XM/MOD tracker music with lightweight tracker-state feedback for music-driven visuals and gameplay.

The reusable extension lives in `levinplayer/`. The rest of this repository is an example Defold project.

## Why Bother?

Defold already plays common audio formats such as WAV and Ogg Vorbis, and those are still the right choice for many sound effects, voice lines, and pre-rendered music tracks. This extension is for cases where tracker music is a better fit.

XM and MOD files can be much smaller than rendered audio because they store patterns, notes, instruments, and sample data rather than a full waveform for the entire song. That makes them useful when you want a lot of music in a small download, when your soundtrack needs to scale across many levels or variations, or when you want music data that can drive gameplay and visuals directly.

The other reason is flexibility. Tracker modules expose structure while they play: current order, pattern, row, channel notes, instruments, effects, volume, panning, triggers, and a lightweight audio spectrum. That gives you hooks for beat-reactive UI, note bursts, music-synced animation, debugging tools, or gameplay systems without baking that information into separate metadata files.

Try the included example project to see this in action. It loads sample XM and MOD files from `res/common/assets/` with `levinplayer.load_music_resource()`, plays them from Defold resources, and uses tracker/audio feedback to update the on-screen state and visuals.

The example also includes simple transport controls for testing playback state. The first line shows keyboard shortcuts: `P` toggles play/pause, `S` stops, `N`/`B` changes track, `R` toggles shuffle, left/right rewinds or fast-forwards by 10 seconds, `F`/`G` fade the whole song, `H`/`J` fade the first visible channel, `K` queues a mute toggle for the first visible channel on the next row, and `L` queues a channel fade on the next pattern. The second line shows transport, current track, bank, shuffle, queue, and time. The third line shows tracker counters. The `<` and `>` keys page the top channel debug rows, number keys `1` through `0` mute/unmute the visible channel slots, and the lower particle visual listens to all reported XM/MOD channels directly, up to 32.

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
levinplayer.pause_music()
levinplayer.resume_music()
levinplayer.seek_music(30.0)
levinplayer.set_channel_muted(1, true)
levinplayer.set_channel_volume(2, 0.5)
levinplayer.fade_music_volume(0.0, 3.0, true)
levinplayer.fade_channel_volume(1, 0.25, 1.5)
levinplayer.music_volume(1.0)
levinplayer.music_pitch(1.0)
```

Available functions:

- `levinplayer.master_volume(volume)`
- `levinplayer.music_buffer_size(frames)`
- `levinplayer.load_music(path)`
- `levinplayer.load_music_resource(resource_path)`
- `levinplayer.play_music()`
- `levinplayer.stop_music()`
- `levinplayer.pause_music()`
- `levinplayer.resume_music()`
- `levinplayer.seek_music(seconds)`
- `levinplayer.music_position()`
- `levinplayer.set_channel_muted(channel, muted)`
- `levinplayer.is_channel_muted(channel)`
- `levinplayer.set_channel_volume(channel, volume)`
- `levinplayer.get_channel_volume(channel)`
- `levinplayer.fade_music_volume(target, duration, stop_when_done)`
- `levinplayer.stop_music_fade()`
- `levinplayer.is_music_fading()`
- `levinplayer.fade_channel_volume(channel, target, duration)`
- `levinplayer.stop_channel_fade(channel)`
- `levinplayer.is_channel_fading(channel)`
- `levinplayer.update_fades(dt)`
- `levinplayer.music_volume(volume)`
- `levinplayer.music_pitch(pitch)`
- `levinplayer.music_length()`
- `levinplayer.is_music_playing()`
- `levinplayer.tracker_state()`
- `levinplayer.audio_spectrum()`
- `levinplayer.unload_music()`
- `levinplayer.music_metadata()` — JSON string from a loaded `.levinaudio` file, or `nil` for plain XM/MOD

`tracker_state()` returns `nil` when no supported tracker module is loaded. For XM and MOD playback it returns song position plus a `channels` table containing note, instrument, effect, volume, panning, trigger, mute, and channel gain data for each tracker channel. MOD files report the channel count from the module, so classic ProTracker files usually expose four channels.

Channel `note`, `instrument`, and `volume_column` values are latched playback-state values: they hold the last audible tracker event until the channel receives a new note or note-off. Raw pattern-cell values for the current row are exposed separately as `row_note`, `row_instrument`, `row_volume_column`, `row_effect_type`, and `row_effect_param`.

`audio_spectrum()` returns lightweight mixed-output spectral feedback for visualizers. It currently exposes 16 smoothed frequency bands plus mono `rms`/`peak` and stereo `left_rms`, `right_rms`, `left_peak`, and `right_peak`, derived from the generated PCM stream before it is submitted to Defold audio. This is intended for shader/UI feedback such as logo bars, beat-reactive effects, L/R meters, or future FFT-style visual development.

`seek_music(seconds)` moves the current song cursor to a time position in seconds. For XM and MOD modules this is implemented by resetting the tracker and fast-forwarding to the requested position, so occasional user-driven seeking is fine, but it should not be called every frame.

`set_channel_muted()` and `set_channel_volume()` apply to tracker channels for XM and MOD files. They do not rewrite pattern data; they add runtime mix controls on top of the module playback, which makes them suitable for adaptive arrangement changes.

`fade_music_volume()` and `fade_channel_volume()` ramp the same runtime gain controls over time. The included Lua helper module (`require("levinplayer.levinplayer")`) calls `update_fades(dt)` for you and wraps common transport, seek, mute, and fade operations so game scripts do not need to carry that boilerplate.

The helper module also supports timed musical actions by watching `tracker_state()` boundaries:

```lua
local player = require("levinplayer.levinplayer")

function update(self, dt)
	local state = levinplayer.tracker_state()
	player.update(dt, state)
end

player.queue_channel_mute(4, true, "row")
player.queue_channel_toggle_mute(4, "row")
player.queue_channel_fade(4, 0.0, 1.0, "pattern")
```

Immediate channel helper calls such as `toggle_channel_mute()` and `fade_channel_out()` cancel pending queued actions for that channel first, so manual controls can override scheduled changes cleanly. Immediate mute helpers also restore that channel's gain to `1.0`, which makes them useful as a clear manual reset after an adaptive fade.

The same helper module includes the music manager layer:

```lua
local player = require("levinplayer.levinplayer")

player.define_track("title", "/res/common/assets/title.xm", {
	title = "Title Theme",
	tags = { "menu", "ambient" },
	channels = {
		drums = { 1, 2 },
		bass = { 3 },
	}
})

player.define_playlist("level_1", { "title" }, {
	shuffle = false,
	repeat_policy = "all",
	transition = {
		mode = "fade",
		lead_time = 2.0,
		fade_out = 1.5,
		fade_in = 1.5,
	},
})

player.define_station("radio", {
	playlists = { "level_1" },
	shuffle = true,
})

player.play_playlist("level_1")
player.next_track()
player.toggle_shuffle()
player.set_auto_advance(true)
player.set_transition({ mode = "fade", fade_out = 1.0, fade_in = 1.0, lead_time = 1.0 })
```

## Channel Groups

Give channels musical names and control them together, so game code can say "bring in the drama" instead of tracking channel numbers:

```lua
player.define_track("attic", "/res/common/assets/attic.xm", {
	channels = { drums = { 1, 2 }, bass = { 3 }, drama = { 5, 6 }, mystery = { 7 } },
})

player.fade_group_in("drama", 2.0)                    -- now
player.queue_group_fade("drama", 1.0, 2.0, "pattern") -- on the next pattern
player.queue_group_mute("mystery", true, "row")
player.set_mix({ drama = 1.0, mystery = 0.0 }, { duration = 2.0, boundary = "pattern" })
```

Group functions: `group_channels(name)`, `group_names()`, `set_group_mute`, `set_group_volume`, `fade_group_to`, `fade_group_in`, `fade_group_out`, `queue_group_mute`, `queue_group_volume`, `queue_group_fade` and `set_mix(levels, options)`. Each acts on every channel in the group and returns how many channels it touched (0, with a warning, for an unknown name). A channel in two groups follows whichever group changed last.

## .levinaudio Files

A `.levinaudio` file is a normal XM or MOD module wrapped with metadata: channel groups, a starting mix, cues and tags. Load it exactly like an XM — `load_music_resource`, `load_music`, `define_track` and playlists all accept it — and the helper module applies the metadata automatically. Plain `.xm` and `.mod` files work as before.

The metadata adds about 200 bytes. Make one from an existing module with the included tool:

```sh
python3 tools/levinaudio.py pack attic.xm attic.json -o attic.levinaudio
python3 tools/levinaudio.py info attic.levinaudio
python3 tools/levinaudio.py unpack attic.levinaudio -o out/
```

`attic.json` (every key optional; channels count from 1):

```json
{
  "title": "Whispers in the Attic",
  "tags": ["level", "spooky"],
  "channels": { "drums": [1, 2], "bass": [3], "drama": [5, 6], "mystery": [7] },
  "start_volumes": { "drama": 0.0, "mystery": 0.0 },
  "cues": [ { "id": "chorus", "order": 4, "row": 0 } ]
}
```

On load the helper module:

- uses the file's `channels` as named groups (a group with the same name in `define_track` takes priority),
- sets each group in `start_volumes`, so a layer can start silent until the game brings it in,
- fills in `title` and `tags` if the game didn't set them,
- fires cues as playback reaches them:

```lua
player.on_cue("chorus", function(cue, state) flash_lights() end)
player.on_cue("*", function(cue) print("cue", cue.id) end)
```

Cues need `player.update(dt, levinplayer.tracker_state())` each frame, like queued actions. `player.metadata()` returns the decoded table (or `nil`); call `player.refresh_metadata()` if you load with the native functions directly. Station and tag filtering only sees file tags after a track has been loaded once, so give `define_track` tags you filter on.

The byte layout is documented in `levinplayer/include/levinaudio.h`. Unknown chunks are skipped, so files with extra data (such as editor project data) still play in this player.

## Tests

```sh
g++ -std=c++11 -I levinplayer/include tests/levinaudio_test.cpp -o /tmp/levinaudio_test && /tmp/levinaudio_test
lua tests/levinplayer_test.lua
```

## Notes

Known tested platforms: Windows, macOS, and Android.

The default music stream buffer is set to 8192 frames to reduce underruns when Defold's main update loop has short hitches. You can tune this per project with `levinplayer.music_buffer_size(frames)` before loading music.

## Credits

This extension builds on the work of two excellent open-source projects:

- **[defold-modplayer](https://github.com/selimanac/defold-modplayer)** by selimanac — the Defold native extension this project originated from.
- **[raudio](https://github.com/raysan5/raudio)** / **[raylib](https://github.com/raysan5/raylib)** by Ramon Santamaria (raysan5) — the underlying audio engine powering playback.

We updated and upgraded several internals and exposed additional functionality to Lua, but none of this would exist without their foundational work.
