# DiscordPresenceAC2

Discord Rich Presence for Assassin's Creed II (PC). It is an ASI plugin, so it loads through
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) next to other mods
like [EaglePatch](https://github.com/Sergeanur/EaglePatch).

Your Discord profile shows where you are in the game and how Ezio is doing:

```
Assassin's Creed II                 Assassin's Creed II
In Venice · Sequence 7              In Tuscany · Near San Gimignano
Memory 4 · 10,661 ƒ · ❤ 14/14       Free roaming · 2,657 ƒ · ❤ 14/14
00:42 elapsed                       01:15 elapsed
```

## Features

- Current region: Florence, Venice, Tuscany, Monteriggioni, Romagna (Forlì), the Apennines, Rome,
  Abstergo, the hideout, all six Assassin tombs, the bonus maps, Altaïr's memory and the main menu
- Current sequence and memory, or free roaming with the nearest landmark
- Florins and health bars
- German and English text, picked from your Windows language or set in the ini
- Every line can be changed in the ini
- Uses the official [discord-rpc](https://github.com/discord/discord-rpc) library, linked statically,
  so there is no extra DLL

## Supported game versions

| Version | Region | Sequence, florins and health |
|---|---|---|
| Steam / Uplay digital | yes | yes |
| Retail 1.01 | yes | no |

The plugin checks the executable at runtime. On an unknown version it still shows the region and
simply leaves out everything else.

## Installation

1. Install [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) as
   `dinput8.dll` in the game folder, if you don't have it already.
2. Download `DiscordPresenceAC2.zip` from the
   [latest release](https://github.com/fthomys/AssassinsCreed2-Rich-Presence/releases/latest) and
   extract it into the game folder, so that `DiscordPresenceAC2.asi` and `DiscordPresenceAC2.ini`
   end up in the `scripts` folder.
3. Start the game with Discord running.

## Configuration

All settings live in `DiscordPresenceAC2.ini` and are explained there. The most useful ones:

| Setting | Meaning |
|---|---|
| `Language` | `auto`, `de` or `en` |
| `ShowProgress` | Show sequence and memory |
| `ShowLandmark` | Show the nearest landmark while free roaming |
| `ShowStats` | Show florins and health |
| `ShowElapsedTime` | Show the play time |
| `ApplicationId` | Use your own Discord application instead of the default one |

Texts are changed per language in the `[Text_DE]` and `[Text_EN]` sections. `Details` and `State`
are the two lines in Discord, `LargeText` appears when hovering the picture. They take these
placeholders:

| Placeholder | Value |
|---|---|
| `{region}` | Region name, like "In Venice" |
| `{region_info}` | Extra region text, like "Ezio Auditore da Firenze" or the city of a tomb |
| `{sequence}`, `{memory}` | Story progress, only while a memory is being played |
| `{exploring}` | "Free roaming" while no memory is being played, changeable with the `Exploring` key |
| `{landmark}` | Nearest landmark within 150 m while free roaming |
| `{florins}`, `{health}`, `{health_max}` | Player stats |

A line is split into parts at ` · `. A part whose placeholder has no value right now, like the
sequence while free roaming, is left out together with its separator. With the default lines this
gives:

```
In Venice · Sequence 7              In Venice · Near Ponte Rialto
Memory 4 · 10,661 ƒ · ❤ 14/14       Free roaming · 10,661 ƒ · ❤ 14/14
```

```ini
[Text_EN]
Details={region}
State=Sequence {sequence}, Memory {memory} · {florins} florins
Firenze=Walking around Florence|Ezio Auditore
```

### Using your own Discord application

1. Create an application at the [Discord Developer Portal](https://discord.com/developers/applications).
   Its name is what Discord shows after "Playing".
2. Under Rich Presence, Art Assets, upload a logo with the key `ac2_logo`.
3. Put the application ID into `ApplicationId`.

## How it works

This is the short version. The full memory layout, the region evidence and the reverse
engineering notes are in [docs](docs):

- [docs/memory-layout.md](docs/memory-layout.md): vtables, offsets and object paths
- [docs/regions.md](docs/regions.md): which archive is which region, and why
- [docs/reverse-engineering.md](docs/reverse-engineering.md): how the values were found

**Region.** The game streams each area from its own archive, for example
`DataPC_Venezia.forge`. The plugin hooks the file functions in the game's import table and
measures how much data is read from each archive. The archive with the most recent traffic is
the current region. This needs no memory addresses and works on every version.

**Stats.** The game engine registers every class with a name and a hash, and each object starts
with a pointer to its class vtable. The executable is not relocated, so these vtables have fixed
addresses. The plugin searches the heap once for the player's `BhvAssassin` object, checks that it
really belongs to the player entity, and follows it from there:

```
BhvAssassin
  +0x08 Entity            position at +0x40
  +0x10 CharacterAI       services at +0x30, one of them is CSrvPlayerHealth
  +0x18 Human             inventory at +0x20, the money container has type 1
```

The heap also holds template copies of the player. Only the candidate that is placed in the
world and has the most components (Ezio has 26) is used.

**Story progress.** The main sequences are `MissionGroup` objects with fixed IDs, `0x10001858`
for sequence 1 up to `0x10001865` for sequence 14. Each group has a state (`2` while it is
running) and an ordered list of its missions, where index 0 is the intro and the list index is
the memory number. The memory being played has state `5`; if no mission of the running sequence
is in that state, the player is free roaming.

**Landmarks.** The game has no notion of a current district. What it does have are the Animus
database triggers that unlock an entry when Ezio gets close to a building. Each
`AnimusDatabaseTriggerEvent` links a database entry (name at +0x44, category 1 for buildings,
2 for shops) to a world entity with a position. The plugin picks the nearest building within
150 m. The names are the ones stored in the game data, which are mostly the Italian names and
in a few cases spelled differently from the real places.

The pointers are cached and validated on every update, and the search runs again after a load.
All reads are guarded, so a stale pointer can never crash the game.

## Building

Requirements: Visual Studio 2022 or newer with the C++ workload, and Git.

```
git clone --recursive https://github.com/fthomys/AssassinsCreed2-Rich-Presence.git
cd AssassinsCreed2-Rich-Presence
build.bat
```

The plugin ends up in `build\`. To build and copy it into the game in one step:

```
build.bat install "C:\Program Files (x86)\Steam\steamapps\common\Assassin's Creed 2"
```

The build is always 32 bit, because the game is.

GitHub Actions builds every push and pull request and keeps the result as a workflow artifact.
Pushing a tag like `v1.0.0` also creates a GitHub release with `DiscordPresenceAC2.zip`:

```
git tag v1.0.0
git push origin v1.0.0
```

## Debugging

Set `DebugLog=1` to get `DiscordPresenceAC2.log` in the `scripts` folder. It lists the archives
the game opens, the detected region, the player stats, the story progress and the Discord
connection state.

`DumpProcess=1` writes a full memory dump on every F9 press. This is only useful for finding new
values in the game's memory.

## Credits

- [discord-rpc](https://github.com/discord/discord-rpc) by Discord (MIT)
- [RapidJSON](https://github.com/Tencent/rapidjson) by Tencent (MIT)
- [EaglePatch](https://github.com/Sergeanur/EaglePatch) by Sergeanur, for the executable version check

## License

MIT, see [LICENSE](LICENSE).

Assassin's Creed is a trademark of Ubisoft. This project is not affiliated with Ubisoft or Discord.

## AI disclaimer

The reverse engineering of the game's memory layout was done with the help of an AI model
(Claude by Anthropic). That covers finding the engine classes, their offsets and the way the
plugin locates the player, the story progress and the landmarks at runtime. All of it was
verified against memory dumps of the running game.
