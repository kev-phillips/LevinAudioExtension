# LevinPlayer Roadmap

This document captures the direction for improving LevinPlayer from a tracker playback extension into a practical music system for Defold games.

The main design goal is to keep the native extension focused on accurate playback, timing, and low-level tracker control, while higher-level game policy stays in Lua.

## Why This Matters

LevinPlayer is useful because XM and MOD music can be compact, structured, and reactive in ways that rendered WAV or Ogg music is not.

Tracker modules store patterns, notes, instruments, effects, and sample data instead of one long rendered waveform. This can make soundtracks smaller and easier to scale across many levels, variations, or game states.

They also expose musical structure while playing. A game can react to rows, patterns, notes, channels, instruments, volume, panning, effects, and spectrum data without maintaining a separate timing file.

That opens up uses such as:

- Beat-reactive UI and animation.
- Music-synced particle effects.
- Debug visualizers for tracker playback.
- Adaptive music that changes with game activity.
- Radio or station-style music selection by style, genre, or player preference.
- Channel-level arrangement changes, such as muting drums outside combat or bringing lead parts in on a pattern boundary.

## Architecture Layers

### Native Player

The native extension should provide reliable playback and tracker-level control.

Good candidates for the native layer:

- Load tracker modules from bundle/resource paths.
- Play, stop, pause, and resume.
- Seek within the current song.
- Report current playback position and length.
- Expose full tracker state for supported formats.
- Support the maximum useful channel count for each format.
- Control per-channel mute and volume.
- Provide accurate timing data for row, pattern, order, and tick changes.
- Provide lightweight mixed-output audio spectrum data.

The native player should avoid owning playlist, tag, station, or game-state logic.

### Lua Music Manager

Lua should own musical policy and game-facing organization.

Good candidates for a reusable Lua layer:

- Tracks.
- Playlists.
- Playgroups.
- Shuffle and repeat behavior.
- Tag filtering.
- Station-style selection.
- Timed action queues, such as muting a channel on the next row or pattern.
- Channel metadata, such as assigning "drums", "bass", "lead", or "pad" tags to tracker channels.

This keeps the native API small and reliable while letting games define their own music behavior.

### Example Project

The example project should demonstrate the capabilities without becoming the core system.

Good candidates for the example:

- Visualizer UI.
- Channel labels and tracker debug readout.
- Keyboard/note display.
- Spectrum-driven logo effects.
- Particle bursts for note triggers.
- Buttons for play, pause, stop, seek, shuffle, and channel mute.
- A demo playlist or radio station built from files in `res/common/assets/`.

## Feature Split

### Max Channel Support

Belongs in the native player and example UI.

XM can support up to 32 channels. MOD files usually expose 4 channels. The native tracker-state API should report all active channels reliably, and Lua should not be constrained by the example's current display count.

The example should adapt its visualizer to the reported channel count.

### Play, Stop, Pause, Resume

Belongs in the native player.

Current playback control should be expanded into explicit functions:

- `levinplayer.play_music()`
- `levinplayer.stop_music()`
- `levinplayer.pause_music()`
- `levinplayer.resume_music()`
- `levinplayer.is_music_playing()`

Games should not need to simulate pause by setting volume to zero or unloading the current module.

### Song Cursor Changes

Belongs primarily in the native player.

A practical first version should expose time-based seeking:

- `levinplayer.music_position()`
- `levinplayer.music_length()`
- `levinplayer.seek_music(seconds)`

Tracker-aware seeking can come later if it can be implemented reliably:

- Seek to order.
- Seek to pattern.
- Seek to row.
- Move to next or previous pattern.

### Playlist

Belongs in Lua.

Playlists are game policy. A playlist may be linear, shuffled, weighted, level-specific, biome-specific, player-selected, or filtered by tags. The native player only needs to load and play the selected track.

### Playlist Shuffle

Belongs in Lua.

Shuffle rules vary by game. Useful policies include:

- Random with repeats allowed.
- Random without repeats until all tracks have played.
- Weighted random.
- Seeded random.
- Tag-filtered random.

### Channel Muting

Immediate muting belongs in the native player. Timed muting belongs in Lua, supported by native timing data.

Useful native API:

- `levinplayer.set_channel_muted(channel, muted)`
- `levinplayer.is_channel_muted(channel)`
- `levinplayer.set_channel_volume(channel, volume)`

Useful Lua scheduling API:

- Mute immediately.
- Mute on next row.
- Mute on next pattern.
- Mute on next order.
- Fade or step volume at musical boundaries.

This allows game-driven arrangement changes without abrupt audio cuts.

## Game Music Model

A higher-level game system could organize music like this:

- `Track`: a resource path plus metadata.
- `Playlist`: a list of tracks plus ordering rules.
- `PlayGroup`: a context such as menu, level, biome, combat, shop, or credits.
- `Station`: a selector that picks tracks or playlists by tags, preferences, or game state.

Example track data:

```lua
{
	id = "cave_ambient_01",
	path = "/res/common/assets/cave_ambient.xm",
	tags = { "ambient", "cave", "low_energy" },
	energy = 0.25,
	channels = {
		[1] = { "lead" },
		[2] = { "bass" },
		[3] = { "drums" },
		[4] = { "pad", "atmosphere" }
	}
}
```

Example use:

```lua
station.play({
	tags = { "cave" },
	max_energy = 0.5,
	shuffle = true
})
```

Example adaptive channel control:

```lua
if enemies_nearby then
	station.unmute_tagged_channels("drums", "next_pattern")
else
	station.mute_tagged_channels("drums", "next_pattern")
end
```

## Implementation Plan

### Phase 1: Core Playback API

- [x] Add `stop_music()`.
- [x] Add `pause_music()`.
- [x] Add `resume_music()`.
- [x] Add `music_position()`.
- [x] Add `seek_music(seconds)`.
- [x] Confirm `music_length()` behavior across XM and MOD.
- [x] Document playback-state behavior in `README.md`.

Phase 1 note: time-based seeking is available for streamed formats and tracker modules. XM and MOD seeking is implemented by resetting the module and fast-forwarding generated samples to the requested position, so it is intended for user-driven cursor changes rather than continuous per-frame scrubbing.

### Phase 2: Full Channel Reporting

- [x] Confirm native tracker-state capacity can report full XM/MOD channel data.
- [x] Confirm MOD channel reporting for classic 4-channel files.
- [x] Remove example-side assumptions that only 10 music channels exist.
- [x] Update visualizer layout to page through the reported channel count.
- [x] Document channel paging and expanded spectrum/level behavior.

Phase 2 note: the example keeps 10 visible channel debug rows and pages those rows with `<`/`>` or the up/down arrow keys. The lower particle visual listens to all reported XM/MOD channels directly, up to 32 channels, so larger XM files still show channel activity without switching the particle view. The existing 16-band spectrum remains available, and `audio_spectrum()` now also exposes L/R RMS and peak values for simple stereo level meters.

### Phase 3: Per-Channel Control

- [x] Add immediate channel mute support.
- [x] Add per-channel volume support if practical.
- [x] Expose channel mute state to Lua.
- [x] Confirm mute behavior does not break tracker-state reporting.
- [x] Add example controls for immediate channel mute.

### Phase 4: Timed Musical Actions

- [x] Add native whole-song volume fades.
- [x] Add native per-channel volume fades.
- [x] Add a reusable Lua helper module for transport, seek, mute, and fade operations.
- [x] Demonstrate song and channel fades in the example.
- [x] Expose enough stable timing data for Lua scheduling.
- [x] Add Lua-side action queue for next row, pattern, or order.
- [x] Support queued channel mute/unmute.
- [x] Support queued channel volume changes.
- [x] Demonstrate non-jarring arrangement changes in the example.

Phase 4 note: fades are stored and applied by the native extension, while the optional Lua helper module calls `levinplayer.update_fades(dt)` from script update. The same helper watches `tracker_state()` and runs queued callbacks on row, pattern, or order boundaries. The example uses this for next-row channel mute and next-pattern channel fade tests.

### Phase 5: Lua Music Manager

- [x] Add reusable Lua module for tracks, playlists, playgroups, and stations.
- [x] Support shuffle and repeat policies.
- [x] Support tag-filtered selection.
- [x] Support channel metadata per track.
- [x] Include the manager in the main helper module so game scripts stay tidy.

Phase 5 note: `levinplayer/levinplayer.lua` now owns track, playlist, playgroup, and station definitions. The example defines a small playlist from bundled XM/MOD files and exposes next, previous, and shuffle controls.

### Phase 6: Example Station Visualizer

- Add sample playlist using XM and MOD files from `res/common/assets/`.
- Add play, pause, stop, next, previous, seek, and shuffle controls.
- Show all active channels.
- Show tracker position and audio spectrum.
- Demonstrate immediate and pattern-timed channel mute.
- Demonstrate station selection by tag or style.

## Open Questions

- How reliable is order/pattern/row seeking with the current XM and MOD backends?
- [x] Should per-channel volume be linear gain only, or should it support fades? Initial answer: support fades on top of linear gain.
- [x] Should fades live entirely in Lua, or should the native player support ramping to avoid clicks? Initial answer: native fade state with Lua `dt` ticking via helper module.
- Should channel metadata live beside tracks in Lua, or should we support optional sidecar files?
- Should the example remain a visualizer, or evolve into a small music workbench for testing module behavior?
- What should the API guarantee when a file is not a supported tracker module but still plays as streamed audio?
