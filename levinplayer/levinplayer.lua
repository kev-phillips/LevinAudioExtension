local M = {}

---@diagnostic disable-next-line: undefined-global
local native = levinplayer
local queued_actions = {}
local last_position = nil
local tracks = {}
local playlists = {}
local playgroups = {}
local stations = {}
local manager = {
	current_track = nil,
	current_playlist = nil,
	current_index = 0,
	shuffle = false,
	repeat_policy = "all",
	auto_advance = true,
	last_music_position = nil,
	transition = nil,
	transition_settings = {
		mode = "fade",
		lead_time = 2.0,
		fade_out = 1.5,
		fade_in = 1.5,
	},
}

local function clamp(value, min_value, max_value)
	return math.max(min_value, math.min(max_value, value))
end

local function same_position(a, b, key)
	return a and b and a[key] == b[key]
end

local function boundary_changed(boundary, previous, current)
	if not previous or not current then
		return false
	end

	if boundary == "order" then
		return not same_position(previous, current, "order")
	elseif boundary == "pattern" then
		return not same_position(previous, current, "order") or not same_position(previous, current, "pattern")
	end

	return not same_position(previous, current, "order")
		or not same_position(previous, current, "pattern")
		or not same_position(previous, current, "row")
end

local function run_queued_actions(state)
	if not state then
		return 0
	end

	if not last_position then
		last_position = {
			order = state.order,
			pattern = state.pattern,
			row = state.row,
		}
		return 0
	end

	local executed = 0
	for i = #queued_actions, 1, -1 do
		local action = queued_actions[i]
		if boundary_changed(action.boundary, last_position, state) then
			table.remove(queued_actions, i)
			action.fn(state)
			executed = executed + 1
		end
	end

	last_position.order = state.order
	last_position.pattern = state.pattern
	last_position.row = state.row
	return executed
end

local function remove_channel_actions(channel)
	for i = #queued_actions, 1, -1 do
		if queued_actions[i].channel == channel then
			table.remove(queued_actions, i)
		end
	end
end

local function reset_channel_control(channel, reset_volume)
	remove_channel_actions(channel)
	native.stop_channel_fade(channel)
	if reset_volume then
		native.set_channel_volume(channel, 1.0)
	end
end

local function copy_array(values)
	local result = {}
	for i = 1, #(values or {}) do
		result[i] = values[i]
	end
	return result
end

local function option_or(options, key, fallback)
	if options and options[key] ~= nil then
		return options[key]
	end
	return fallback
end

local function copy_table(values)
	local result = {}
	for key, value in pairs(values or {}) do
		result[key] = value
	end
	return result
end

local function as_set(values)
	local result = {}
	for i = 1, #(values or {}) do
		result[values[i]] = true
	end
	return result
end

local function track_matches_tags(track, required_tags)
	if not required_tags or #required_tags == 0 then
		return true
	end

	local tags = track.tags
	if not tags then
		return false
	end

	local tag_set = tags[1] and as_set(tags) or tags
	for i = 1, #required_tags do
		if not tag_set[required_tags[i]] then
			return false
		end
	end
	return true
end

local function resolve_track(track)
	if type(track) == "string" then
		return tracks[track]
	end
	return track
end

local function resolve_playlist(playlist)
	if type(playlist) == "string" then
		return playlists[playlist]
	end
	return playlist
end

local function playlist_track_ids(playlist)
	return playlist and (playlist.tracks or playlist) or {}
end

local function find_track_index(playlist, track_id)
	local ids = playlist_track_ids(playlist)
	for i = 1, #ids do
		if ids[i] == track_id then
			return i
		end
	end
	return 0
end

local function choose_next_index(playlist, direction)
	local ids = playlist_track_ids(playlist)
	local count = #ids
	if count == 0 then
		return 0
	end

	if manager.shuffle and count > 1 and direction >= 0 then
		local next_index = math.random(count)
		if next_index == manager.current_index then
			next_index = next_index % count + 1
		end
		return next_index
	end

	local next_index = manager.current_index + direction
	if next_index > count then
		return manager.repeat_policy == "all" and 1 or 0
	elseif next_index < 1 then
		return manager.repeat_policy == "all" and count or 0
	end
	return next_index
end

local function merged_transition_settings(track, playlist)
	local settings = copy_table(manager.transition_settings)
	for key, value in pairs(playlist and playlist.transition or {}) do
		settings[key] = value
	end
	for key, value in pairs(track and track.transition or {}) do
		settings[key] = value
	end
	return settings
end

local function load_track(track, options)
	options = options or {}
	M.clear_queue()
	last_position = nil
	manager.last_music_position = nil
	if not options.keep_transition then
		manager.transition = nil
	end
	native.stop_music_fade()
	if track.resource_path or track.resource ~= false then
		native.load_music_resource(track.resource_path or track.path)
	else
		native.load_music(track.path)
	end
	native.play_music()
	native.music_volume(options.volume ~= nil and options.volume or track.volume or 1.0)
	native.music_pitch(track.pitch or 1.0)
	manager.current_track = track
	return track
end

local function play_track_at_index(index, options)
	local playlist = resolve_playlist(manager.current_playlist)
	local track_id = playlist_track_ids(playlist)[index]
	local track = resolve_track(track_id)
	if not track then
		return nil
	end

	manager.current_index = index
	return load_track(track, options)
end

local function finish_playlist_transition()
	local transition = manager.transition
	if not transition then
		return false
	end

	local next_track = play_track_at_index(transition.next_index, {
		volume = 0.0,
		keep_transition = true,
	})
	if not next_track then
		manager.transition = nil
		return false
	end
	native.fade_music_volume(next_track.volume or 1.0, transition.fade_in or 0.0, false)
	manager.transition = nil
	return true
end

local function start_playlist_transition(next_index, settings)
	if manager.transition then
		return false
	end

	if settings.mode == "none" or (settings.fade_out or 0) <= 0 and (settings.fade_in or 0) <= 0 then
		return play_track_at_index(next_index) ~= nil
	end

	manager.transition = {
		next_index = next_index,
		timer = 0.0,
		fade_out = settings.fade_out or 0.0,
		fade_in = settings.fade_in or 0.0,
	}
	native.fade_music_volume(0.0, manager.transition.fade_out, false)
	return true
end

local function update_auto_advance(dt)
	if not manager.auto_advance or not manager.current_track or not manager.current_playlist then
		manager.last_music_position = nil
		return false
	end

	if manager.transition then
		manager.transition.timer = manager.transition.timer + (dt or 0)
		if manager.transition.timer >= manager.transition.fade_out or not native.is_music_fading() then
			return finish_playlist_transition()
		end
		return false
	end

	if not native.is_music_playing() then
		manager.last_music_position = native.music_position()
		return false
	end

	local length = native.music_length()
	local position = native.music_position()
	local previous = manager.last_music_position
	manager.last_music_position = position

	if not previous or length <= 0 then
		return false
	end

	local playlist = resolve_playlist(manager.current_playlist)
	local settings = merged_transition_settings(manager.current_track, playlist)
	local next_index = choose_next_index(playlist, 1)
	if next_index == 0 then
		return false
	end

	local lead_time = settings.mode == "none" and 0.0 or (settings.lead_time or settings.fade_out or 0.0)
	if position >= length - lead_time then
		return start_playlist_transition(next_index, settings)
	end

	if previous > 1.0 and position < 0.5 and previous > length - 2.0 then
		return start_playlist_transition(next_index, settings)
	end

	return false
end

function M.update(dt, state)
	native.update_fades(dt or 0)
	local executed = run_queued_actions(state)
	if update_auto_advance(dt or 0) then
		executed = executed + 1
	end
	return executed
end

function M.play()
	native.stop_music_fade()
	native.play_music()
	return "PLAYING"
end

function M.pause()
	native.stop_music_fade()
	native.pause_music()
	return "PAUSED"
end

function M.resume()
	native.stop_music_fade()
	native.resume_music()
	return "PLAYING"
end

function M.stop()
	M.clear_queue()
	manager.transition = nil
	native.stop_music()
	return "STOPPED"
end

function M.toggle_play_pause(current_state)
	if native.is_music_playing() then
		return M.pause()
	elseif current_state == "STOPPED" then
		return M.play()
	end

	return M.resume()
end

function M.seek_relative(delta)
	local length = native.music_length()
	local position = native.music_position()
	local target = position + delta
	if length > 0 then
		target = clamp(target, 0, length)
	else
		target = math.max(0, target)
	end
	native.seek_music(target)
	last_position = nil
	manager.last_music_position = nil
	manager.transition = nil
	M.clear_queue()
	return target
end

function M.toggle_channel_mute(channel)
	reset_channel_control(channel, true)
	local muted = not native.is_channel_muted(channel)
	native.set_channel_muted(channel, muted)
	return muted
end

function M.set_channel_mute(channel, muted)
	reset_channel_control(channel, true)
	native.set_channel_muted(channel, muted)
	return muted
end

function M.set_channel_volume(channel, volume)
	reset_channel_control(channel, false)
	native.set_channel_volume(channel, volume)
	return volume
end

function M.fade_music_to(volume, duration, stop_when_done)
	native.stop_music_fade()
	native.fade_music_volume(volume, duration or 0, stop_when_done or false)
end

function M.fade_music_in(duration, target_volume)
	native.stop_music_fade()
	native.play_music()
	native.fade_music_volume(target_volume or 1.0, duration or 0, false)
	return "PLAYING"
end

function M.fade_music_out(duration, stop_when_done)
	native.stop_music_fade()
	native.fade_music_volume(0.0, duration or 0, stop_when_done ~= false)
end

function M.fade_channel_to(channel, volume, duration)
	remove_channel_actions(channel)
	native.stop_channel_fade(channel)
	native.fade_channel_volume(channel, volume, duration or 0)
end

function M.fade_channel_in(channel, duration, target_volume)
	remove_channel_actions(channel)
	native.stop_channel_fade(channel)
	native.fade_channel_volume(channel, target_volume or 1.0, duration or 0)
end

function M.fade_channel_out(channel, duration)
	remove_channel_actions(channel)
	native.stop_channel_fade(channel)
	native.fade_channel_volume(channel, 0.0, duration or 0)
end

function M.queue(boundary, fn, metadata)
	assert(type(fn) == "function", "queued musical action requires a function")
	local action = metadata or {}
	action.boundary = boundary or action.boundary or "row"
	action.fn = fn
	queued_actions[#queued_actions + 1] = action
	return #queued_actions
end

function M.queue_channel_toggle_mute(channel, boundary)
	remove_channel_actions(channel)
	return M.queue(boundary or "row", function()
		native.stop_channel_fade(channel)
		native.set_channel_muted(channel, not native.is_channel_muted(channel))
	end, {
		channel = channel,
		kind = "mute",
	})
end

function M.queue_channel_mute(channel, muted, boundary)
	remove_channel_actions(channel)
	return M.queue(boundary or "row", function()
		native.stop_channel_fade(channel)
		native.set_channel_muted(channel, muted)
	end, {
		channel = channel,
		kind = "mute",
	})
end

function M.queue_channel_volume(channel, volume, boundary)
	remove_channel_actions(channel)
	return M.queue(boundary or "row", function()
		native.stop_channel_fade(channel)
		native.set_channel_volume(channel, volume)
	end, {
		channel = channel,
		kind = "volume",
	})
end

function M.queue_channel_fade(channel, volume, duration, boundary)
	remove_channel_actions(channel)
	return M.queue(boundary or "row", function()
		native.stop_channel_fade(channel)
		native.fade_channel_volume(channel, volume, duration or 0)
	end, {
		channel = channel,
		kind = "fade",
	})
end

function M.clear_channel_actions(channel)
	reset_channel_control(channel, false)
end

function M.queue_next_row(fn)
	return M.queue("row", fn)
end

function M.queue_next_pattern(fn)
	return M.queue("pattern", fn)
end

function M.queue_next_order(fn)
	return M.queue("order", fn)
end

function M.clear_queue()
	queued_actions = {}
end

function M.pending_actions()
	return #queued_actions
end

function M.define_track(id, path_or_track, options)
	assert(id, "track id is required")
	local track = {}
	if type(path_or_track) == "table" then
		for key, value in pairs(path_or_track) do
			track[key] = value
		end
	else
		track.path = path_or_track
	end

	for key, value in pairs(options or {}) do
		track[key] = value
	end

	track.id = track.id or id
	track.title = track.title or track.id
	tracks[id] = track
	return track
end

function M.define_playlist(id, track_ids, options)
	assert(id, "playlist id is required")
	local playlist = {}
	for key, value in pairs(options or {}) do
		playlist[key] = value
	end
	playlist.id = playlist.id or id
	playlist.title = playlist.title or playlist.id
	playlist.tracks = copy_array(track_ids)
	playlists[id] = playlist
	return playlist
end

function M.define_playgroup(id, playlist_ids, options)
	assert(id, "playgroup id is required")
	local playgroup = {}
	for key, value in pairs(options or {}) do
		playgroup[key] = value
	end
	playgroup.id = playgroup.id or id
	playgroup.title = playgroup.title or playgroup.id
	playgroup.playlists = copy_array(playlist_ids)
	playgroups[id] = playgroup
	return playgroup
end

function M.define_station(id, options)
	assert(id, "station id is required")
	local station = {}
	for key, value in pairs(options or {}) do
		station[key] = value
	end
	station.id = station.id or id
	station.title = station.title or station.id
	stations[id] = station
	return station
end

function M.track(id)
	return tracks[id]
end

function M.playlist(id)
	return playlists[id]
end

function M.playgroup(id)
	return playgroups[id]
end

function M.station(id)
	return stations[id]
end

function M.find_tracks(required_tags)
	local result = {}
	for _, track in pairs(tracks) do
		if track_matches_tags(track, required_tags) then
			result[#result + 1] = track.id
		end
	end
	table.sort(result)
	return result
end

function M.play_track(track_or_id)
	local track = resolve_track(track_or_id)
	assert(track, "track not found")
	local playlist = resolve_playlist(manager.current_playlist)
	manager.current_index = playlist and find_track_index(playlist, track.id) or 0
	load_track(track)
	return track
end

function M.play_playlist(playlist_or_id, options)
	local playlist = resolve_playlist(playlist_or_id)
	assert(playlist, "playlist not found")
	manager.current_playlist = playlist
	manager.current_index = (options and options.index) or 1
	manager.shuffle = option_or(options, "shuffle", playlist.shuffle or false)
	manager.repeat_policy = option_or(options, "repeat_policy", playlist.repeat_policy or "all")
	manager.auto_advance = option_or(options, "auto_advance", playlist.auto_advance ~= false)
	manager.transition = nil

	local track_id = playlist_track_ids(playlist)[manager.current_index]
	assert(track_id, "playlist has no track at selected index")
	return M.play_track(track_id)
end

function M.next_track()
	local playlist = resolve_playlist(manager.current_playlist)
	if not playlist then
		return nil
	end

	manager.transition = nil
	local next_index = choose_next_index(playlist, 1)
	if next_index == 0 then
		return nil
	end

	return play_track_at_index(next_index)
end

function M.previous_track()
	local playlist = resolve_playlist(manager.current_playlist)
	if not playlist then
		return nil
	end

	manager.transition = nil
	local previous_index = choose_next_index(playlist, -1)
	if previous_index == 0 then
		return nil
	end

	return play_track_at_index(previous_index)
end

function M.set_shuffle(enabled)
	manager.shuffle = enabled and true or false
	return manager.shuffle
end

function M.toggle_shuffle()
	manager.shuffle = not manager.shuffle
	return manager.shuffle
end

function M.set_repeat_policy(policy)
	manager.repeat_policy = policy or "all"
	return manager.repeat_policy
end

function M.set_auto_advance(enabled)
	manager.auto_advance = enabled ~= false
	manager.last_music_position = nil
	manager.transition = nil
	return manager.auto_advance
end

function M.set_transition(settings)
	manager.transition_settings = copy_table(settings or {})
	manager.transition_settings.mode = manager.transition_settings.mode or "fade"
	manager.transition_settings.lead_time = manager.transition_settings.lead_time or manager.transition_settings.fade_out or 0.0
	manager.transition_settings.fade_out = manager.transition_settings.fade_out or 0.0
	manager.transition_settings.fade_in = manager.transition_settings.fade_in or 0.0
	manager.transition = nil
	return manager.transition_settings
end

function M.current_track()
	return manager.current_track
end

function M.current_playlist()
	return resolve_playlist(manager.current_playlist)
end

function M.manager_state()
	local track = manager.current_track
	local playlist = resolve_playlist(manager.current_playlist)
	return {
		track = track,
		playlist = playlist,
		index = manager.current_index,
		shuffle = manager.shuffle,
		repeat_policy = manager.repeat_policy,
		auto_advance = manager.auto_advance,
		transition = manager.transition,
	}
end

function M.play_playgroup(playgroup_or_id, playlist_id, options)
	local playgroup = type(playgroup_or_id) == "string" and playgroups[playgroup_or_id] or playgroup_or_id
	assert(playgroup, "playgroup not found")
	local selected_playlist_id = playlist_id or playgroup.default_playlist or playgroup.playlists[1]
	return M.play_playlist(selected_playlist_id, options)
end

function M.play_station(station_or_id, required_tags, options)
	local station = type(station_or_id) == "string" and stations[station_or_id] or station_or_id
	assert(station, "station not found")

	local track_ids = {}
	local seen = {}
	local source_playlists = copy_array(station.playlists)
	for _, playgroup_id in ipairs(station.playgroups or {}) do
		local playgroup = playgroups[playgroup_id]
		for _, playlist_id in ipairs(playgroup and playgroup.playlists or {}) do
			source_playlists[#source_playlists + 1] = playlist_id
		end
	end

	for i = 1, #source_playlists do
		local playlist = resolve_playlist(source_playlists[i])
		for _, track_id in ipairs(playlist_track_ids(playlist)) do
			local track = tracks[track_id]
			if track and not seen[track_id] and track_matches_tags(track, required_tags or station.tags) then
				track_ids[#track_ids + 1] = track_id
				seen[track_id] = true
			end
		end
	end

	for _, track_id in ipairs(station.tracks or {}) do
		local track = tracks[track_id]
		if track and not seen[track_id] and track_matches_tags(track, required_tags or station.tags) then
			track_ids[#track_ids + 1] = track_id
			seen[track_id] = true
		end
	end

	assert(#track_ids > 0, "station has no tracks for selected tags")
	local playlist = M.define_playlist("__station_" .. station.id, track_ids, {
		title = station.title,
		shuffle = option_or(options, "shuffle", station.shuffle),
		repeat_policy = option_or(options, "repeat_policy", station.repeat_policy or "all"),
	})
	return M.play_playlist(playlist, options)
end

return M
