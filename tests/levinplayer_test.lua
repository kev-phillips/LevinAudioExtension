-- Tests for levinplayer/levinplayer.lua using a fake native module.
-- Run from the repo root with any Lua 5.1+ (Defold uses LuaJIT):
--   lua tests/levinplayer_test.lua

package.path = "./?.lua;" .. package.path

local calls = {}
local metadata_text = nil
local muted = {}

local function record(name)
	return function(...)
		calls[#calls + 1] = { name, ... }
	end
end

levinplayer = {
	load_music_resource = record("load_music_resource"),
	load_music = record("load_music"),
	play_music = record("play_music"),
	stop_music_fade = record("stop_music_fade"),
	music_volume = record("music_volume"),
	music_pitch = record("music_pitch"),
	update_fades = function() end,
	set_channel_volume = record("set_channel_volume"),
	stop_channel_fade = function() end,
	fade_channel_volume = record("fade_channel_volume"),
	set_channel_muted = function(ch, m) muted[ch] = m; calls[#calls + 1] = { "set_channel_muted", ch, m } end,
	is_channel_muted = function(ch) return muted[ch] or false end,
	is_music_playing = function() return true end,
	is_music_fading = function() return false end,
	music_length = function() return 100 end,
	music_position = function() return 1 end,
	music_metadata = function() return metadata_text end,
}

local player = require("levinplayer.levinplayer")

local fixtures = {
	META = {
		title = "Whispers in the Attic",
		tags = { "level", "spooky" },
		channels = { drama = { 3 }, bass = { 2 }, mystery = { 7 } },
		start_volumes = { drama = 0.0, mystery = 0.25 },
		cues = { { id = "chorus", order = 4, row = 0 } },
	},
}
player.set_json_decoder(function(text)
	local value = fixtures[text]
	if not value then error("bad json") end
	return value
end)

local passed, failed = 0, 0
local function test(name, fn)
	calls = {}
	muted = {}
	local ok, err = pcall(fn)
	if ok then passed = passed + 1 else failed = failed + 1; print("FAIL " .. name .. ": " .. tostring(err)) end
end

local function eq(a, b, msg)
	if a ~= b then error((msg or "") .. " expected " .. tostring(b) .. ", got " .. tostring(a), 2) end
end

local function find_calls(name)
	local out = {}
	for _, c in ipairs(calls) do if c[1] == name then out[#out + 1] = c end end
	return out
end

local function state(order, pattern, row) return { order = order, pattern = pattern, row = row } end

test("plain XM: groups come from define_track", function()
	metadata_text = nil
	player.define_track("plain", "/res/a.xm", { channels = { drums = { 1, 2 }, drama = { 5, 6 } } })
	player.play_track("plain")
	eq(player.metadata(), nil, "metadata")
	eq(table.concat(player.group_channels("drama"), ","), "5,6")
	eq(table.concat(player.group_names(), ","), "drama,drums")
end)

test("queued group fade waits for the pattern boundary", function()
	metadata_text = nil
	player.play_track("plain")
	player.update(0, state(0, 0, 0))
	eq(player.queue_group_fade("drama", 1.0, 2.0, "pattern"), 2, "channels touched")
	eq(player.pending_actions(), 2, "pending")
	player.update(0.016, state(0, 0, 1))
	eq(#find_calls("fade_channel_volume"), 0, "no fade on row change")
	player.update(0.016, state(1, 3, 0))
	local fades = find_calls("fade_channel_volume")
	eq(#fades, 2, "fades after pattern change")
	eq(fades[1][2] + fades[2][2], 11, "channels 5 and 6")
	eq(fades[1][3], 1.0) ; eq(fades[1][4], 2.0)
end)

test("unknown group touches nothing", function()
	metadata_text = nil
	player.play_track("plain")
	eq(player.fade_group_to("nope", 1, 1), 0)
	eq(#find_calls("fade_channel_volume"), 0)
end)

test("set_mix without a boundary fades now", function()
	metadata_text = nil
	player.play_track("plain")
	eq(player.set_mix({ drama = 0.5, drums = 0.0 }, { duration = 1.5 }), 4)
	eq(#find_calls("fade_channel_volume"), 4)
end)

test("set_mix with a boundary queues", function()
	metadata_text = nil
	player.play_track("plain")
	player.set_mix({ drama = 1.0 }, { duration = 1.0, boundary = "row" })
	eq(#find_calls("fade_channel_volume"), 0)
	eq(player.pending_actions(), 2)
end)

test("group mute and unmute", function()
	metadata_text = nil
	player.play_track("plain")
	player.set_group_mute("drums", true)
	eq(muted[1], true) ; eq(muted[2], true)
	player.set_group_mute("drums", false)
	eq(muted[1], false)
end)

test(".levinaudio metadata: merge, game overrides, start volumes", function()
	metadata_text = "META"
	player.define_track("attic", "/res/attic.levinaudio", { channels = { drama = { 5, 6 } } })
	local track = player.play_track("attic")
	eq(player.metadata().title, "Whispers in the Attic")
	eq(track.title, "Whispers in the Attic", "title filled from file")
	eq(track.tags[2], "spooky", "tags filled from file")
	eq(table.concat(player.group_channels("drama"), ","), "5,6", "game definition wins")
	eq(table.concat(player.group_channels("bass"), ","), "2", "file group used")
	local vols = {}
	for _, c in ipairs(find_calls("set_channel_volume")) do vols[c[2]] = c[3] end
	eq(vols[5], 0.0) ; eq(vols[6], 0.0) ; eq(vols[7], 0.25) ; eq(vols[3], nil, "overridden group not used")
	-- start volumes are applied before playback starts
	local first_vol, first_play
	for i, c in ipairs(calls) do
		if c[1] == "set_channel_volume" and not first_vol then first_vol = i end
		if c[1] == "play_music" and not first_play then first_play = i end
	end
	assert(first_vol < first_play, "start volumes before play")
end)

test("game-set title and tags are kept", function()
	metadata_text = "META"
	player.define_track("attic2", "/res/attic.levinaudio", { title = "Attic (menu)", tags = { "menu" } })
	local track = player.play_track("attic2")
	eq(track.title, "Attic (menu)") ; eq(track.tags[1], "menu") ; eq(#track.tags, 1)
end)

test("cues fire once on arrival", function()
	metadata_text = "META"
	player.play_track("attic")
	local hits, any = 0, 0
	local fn = player.on_cue("chorus", function(cue) hits = hits + 1; eq(cue.id, "chorus") end)
	local all = player.on_cue("*", function() any = any + 1 end)
	player.update(0, state(3, 2, 63))
	player.update(0, state(4, 1, 0))
	player.update(0, state(4, 1, 0)) -- same row again (several frames per row)
	player.update(0, state(4, 1, 1))
	eq(hits, 1, "chorus hits") ; eq(any, 1, "wildcard hits")
	player.update(0, state(4, 1, 0)) -- comes round again after a loop
	eq(hits, 2)
	player.remove_cue_listener("chorus", fn)
	player.remove_cue_listener("*", all)
	player.update(0, state(5, 0, 0)); player.update(0, state(4, 1, 0))
	eq(hits, 2, "removed listener")
end)

test("cues from define_track", function()
	metadata_text = nil
	player.define_track("cued", "/res/c.xm", { cues = { { id = "boss", order = 2, row = 16 } } })
	player.play_track("cued")
	local hit = false
	local fn = player.on_cue("boss", function() hit = true end)
	player.update(0, state(2, 5, 16))
	eq(hit, true)
	player.remove_cue_listener("boss", fn)
end)

test("bad metadata is ignored, music still plays", function()
	metadata_text = "not json"
	player.define_track("broken", "/res/b.levinaudio")
	local old_print = print
	local warned = false
	print = function() warned = true end
	player.play_track("broken")
	print = old_print
	eq(player.metadata(), nil) ; eq(warned, true) ; eq(#find_calls("play_music"), 1)
end)

test("older native binary without music_metadata", function()
	local saved = levinplayer.music_metadata
	levinplayer.music_metadata = nil
	player.play_track("plain")
	eq(player.metadata(), nil)
	eq(table.concat(player.group_channels("drama"), ","), "5,6")
	levinplayer.music_metadata = saved
end)

print(string.format("%d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
