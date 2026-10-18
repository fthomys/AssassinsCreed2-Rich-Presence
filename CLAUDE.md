# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An ASI plugin (a 32 bit DLL renamed to `.asi`) that gives Assassin's Creed II Discord Rich Presence.
Ultimate ASI Loader (`dinput8.dll` in the game folder) loads it from the game's `scripts` folder.
This repo lives inside the game install (`...\Assassin's Creed 2\DiscordPresenceAC2_src`).

## Build and test

```
build.bat                          builds build\DiscordPresenceAC2.asi (MSVC, always x86)
build.bat install "<game folder>"  builds and copies .asi (and the ini, if missing) into <game>\scripts
```

- `build.bat` runs `git submodule update --init` when `third_party` is empty, finds Visual Studio via
  vswhere (falls back to VS 18 Community) and compiles `src\*.cpp` plus the discord-rpc sources in one
  `cl` call. There is no CMake project, no linter and no test suite.
- Installing fails while the game runs, because the game locks the `.asi`. Check for the
  `AssassinsCreedIIGame` process first.
- Testing means running the game. Set `DebugLog=1` in `scripts\DiscordPresenceAC2.ini` and read
  `scripts\DiscordPresenceAC2.log` (archives opened, region switches, heap scan results, stats every
  10 s, every presence sent). `DumpProcess=1` makes F9 write a full minidump (about 1 GB) next to it.
- `dist\DiscordPresenceAC2.ini` is the release template. The installed ini in `scripts` is the one
  the plugin reads.

## Architecture

`dllmain.cpp` checks the host is `AssassinsCreedIIGame.exe`, loads the ini, installs the file hooks
inside `DllMain`, then starts one worker thread. The worker ticks every 100 ms and does the real work
every second: update region, read game state, update presence.

- `region_detector.cpp`: patches the game exe's import table (`CreateFileA/W`, `ReadFile(Ex)`,
  `CloseHandle`), maps open `DataPC_*.forge` handles to regions and counts bytes read per region.
  The region with the most recent traffic (decaying score, switch threshold and ratio) wins. Works on
  any exe version because it needs no memory addresses.
- `game_state.cpp`: everything that reads game memory. Uplay/Steam digital exe only, checked at
  runtime with `IsSupportedExecutable()` (the EaglePatch version check at `0x414A55`).
- `presence.cpp`: builds the Discord lines from templates and sends them with discord-rpc, at most
  every 15 s unless the region changed.
- `texts.cpp`: region table (archive name, ini key, German and English name and info text) and the
  default line templates. `config.cpp` parses the UTF-8 ini; `[Text_DE]` and `[Text_EN]` override
  texts through `OverrideText`.

### Line templates

`Details`, `State` and `LargeText` use placeholders (`{region}`, `{region_info}`, `{sequence}`,
`{memory}`, `{exploring}`, `{landmark}`, `{florins}`, `{health}`, `{health_max}`). A template is split
at `" · "`; a segment whose placeholder has no value is dropped with its separator. Which values exist
is decided in `CollectValues` (for example `{sequence}` only while a memory is played, `{exploring}`
and `{landmark}` only while free roaming).

### Reading game memory

The exe is wrapped in SteamStub, so its code is encrypted on disk and only readable from a runtime
dump. It has no ASLR (base `0x400000`), so vtable addresses are fixed. There are no stable static
pointers to the player, so `game_state.cpp` scans committed private `PAGE_READWRITE` memory for known
vtables and caches what it finds:

- Player: `BhvAssassin` objects. Template copies exist (main menu, other worlds), so a candidate must
  have an entity with a `World`, be at least 1 m from the origin and list itself among the entity's
  components; the one with the most components wins (the real Ezio has 26).
- Story: `MissionGroup` objects with IDs `0x10001858` (sequence 1) to `0x10001865` (sequence 14).
- Landmarks: `AnimusDatabaseTriggerEvent` objects, linking a database entry (category 1 = building)
  to an entity position.

`docs/memory-layout.md` is the reference for every vtable, offset, ID and state value, including
what is known to be wrong or unconfirmed; update it together with `game_state.cpp`.
`docs/reverse-engineering.md` describes the dump based workflow, `docs/regions.md` the archive
to region mapping.

Every read goes through the SEH guarded `Read<T>()`; functions that contain `__try` must not hold
C++ objects with destructors. Cached pointers are revalidated on each read, and a rescan runs after
region changes, every 30 s, or every 5 s while nothing valid is cached. Engine arrays store the
element count in the high 16 bits (masked with `0x3FFF`) of the word after the data pointer, see
`ReadArray`. Offsets and vtables are named constants at the top of `game_state.cpp`; the README
"How it works" section documents the object paths.


## Keeping docs current

Every change has to leave the documentation true. This is required, not optional:

- `docs/memory-layout.md`, `docs/regions.md` and `docs/reverse-engineering.md` must match the code
  and the latest findings. New or changed offsets, vtables, IDs, states or detection rules go into
  the docs in the same change, and so do things that turned out wrong or got confirmed in game.
- `ROADMAP.md` must be updated whenever something is added, finished, confirmed by a test, dropped
  or newly planned. Tick items (`- [x]`) and move them to Done when they are finished, move
  "Needs testing" items to Done once the user confirms them in game.
- README, `dist\DiscordPresenceAC2.ini` and the defaults in `texts.cpp` stay in sync for settings,
  placeholders and default lines.

## Conventions

- No comments in source files.
- Never use em dashes or en dashes anywhere (code, strings, docs).
- README, ROADMAP and ini comments are in English; the user talks German.
- Never create git commits; the user commits and pushes. discord-rpc and rapidjson (pinned to
  v1.1.0) are submodules, do not vendor them.
- Requires Windows 8 or newer (the MSVC runtime imports `GetSystemTimePreciseAsFileTime`).
