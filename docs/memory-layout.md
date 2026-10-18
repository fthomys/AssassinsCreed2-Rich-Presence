# Memory layout

Everything here applies to the Steam / Uplay digital executable of Assassin's Creed II. All
addresses are virtual addresses in the game process, all offsets are in bytes and hexadecimal.

## Executable

| Property | Value |
|---|---|
| File | `AssassinsCreedIIGame.exe`, 32 bit |
| Build timestamp | 2017-05-24 |
| Image base | `0x400000`, no ASLR (`DllCharacteristics` = 0), so all addresses below are fixed |
| Size of image | `0x2190000` |
| Protection | SteamStub, the `.bind` section holds the entry point and `.text` is encrypted on disk |
| Version check | `*(uint32_t*)0x414A55 == 0x01CA6FC8` once the code is decrypted |

The retail 1.01 executable is a different build. EaglePatch identifies it with
`*(uint32_t*)0x4149F5 == 0x01CA4FA0`. None of the vtables or offsets below apply to it.

Sections: `.text` `0x401000`, `.rdata` `0x1CA5000`, `.data` `0x206B000`, `.tls`, `.data1`, `.rsrc`,
`.reloc`, `.bind` `0x2570000`.

The game is large address aware, so heap addresses above `0x80000000` are common.

## Engine type system

The Scimitar engine has no C++ RTTI for game classes. Instead every class has a descriptor in
`.data` and a hash:

```
descriptor (in .data)
  +0x00  const char* name
  +0x04  uint32_t    hash
  +0x08  uint32_t    hash (second copy, sometimes different)
  +0x0C  uint32_t    size of an instance
```

Every object starts with its vtable. Two slots identify the class:

| Slot | Code | Meaning |
|---|---|---|
| 4 (`+0x10`) | `mov eax, [global]; ret` | pointer to runtime class info |
| 5 (`+0x14`) | `mov eax, <hash>; ret` | class hash, matches a descriptor |

To find the vtable of a class by name: take the hash from its descriptor, search `.text` for
`B8 <hash> C3`, then search `.rdata` for a pointer to that function. The vtable starts `0x14`
bytes before that pointer.

### Arrays

Engine arrays are two words:

```
+0x00  T*       data
+0x04  uint32_t packed, element count = (packed >> 16) & 0x3FFF
```

### Handles

Some references go through a handle record instead of a direct pointer. The first word of the
record is the object pointer:

```
handle record
  +0x00  Object*   object
  +0x04  uint32_t  type or index
  +0x08  uint32_t  flags (0x8000000X)
  +0x0C  uint32_t  hash
```

## Vtables

| Class | Vtable | Instance size |
|---|---|---|
| `Entity` | `0x01CB3720` | `0x100` |
| `EntityGroup` | `0x01CD9DB8` | `0x150` |
| `World` | `0x01CAB51C` | `0x7A0` |
| `BhvAssassin` | `0x01E07898` | `0x1030` |
| `Human` | `0x01DE6B70` | `0xAB0` |
| `CharacterAI` | `0x01E0A738` | `0x78` |
| `CSrvPlayerHealth` | `0x01E0C1A8` | `0xC8` |
| `CSrvPlayerInventoryAccess` | `0x01E0BF98` | `0x7C` |
| `LogicalInventory` | `0x01DC2850` | `0xC` |
| `InventoryRechargeableContainer` | `0x01DE3BA8` | `0x30` |
| `InventoryItemContainer` | `0x01DC2AA8` | `0x30` |
| `MissionManager` | `0x01CAD170` | `0x950` |
| `MissionGroup` | `0x01CB9C88` | `0xB0` |
| `Mission` | `0x01CB9EA8` | `0x1E0` |
| `MainSequenceInfo` | `0x01DE1530` | `0x2C` |
| `PlayerProgressionManager` | `0x01DA7268` | `0x240` |
| `AnimusDatabaseTriggerEvent` | `0x01E24B5C` | `0x1C` |
| `AnimusDatabaseData` | `0x01E26444` | `0x68` |
| `HudMoneyComponent` | `0x01DE7CB8` | `0x3C` |
| `HudMissionState` | `0x01DE7FA0` | `0x138` |

## Static pointers

| Address | Points to | Notes |
|---|---|---|
| `0x02211024` | `MissionManager` | singleton |
| `0x022134BC` | `PlayerProgressionManager` | singleton |
| `0x02216C68` | `OutOfBoundsComponent` | not usable, the game stores whichever component last matched a check |

There is no static pointer chain to the player within six levels. The plugin finds the player by
scanning the heap for the `BhvAssassin` vtable.

## Player

```
BhvAssassin
  +0x08  Entity*
  +0x10  CharacterAI*
  +0x18  Human*

Entity
  +0x10  4x4 transform, rows of 4 floats
  +0x40  float x, y, z  (translation row of the transform)
  +0x6C  World*         null for objects that are not placed in a world
  +0x74  array of components (Entity has the BhvAssassin among them)

Human
  +0x20  pointer to the inventory array (data pointer + packed count)
  +0xFC  HumanData*

CharacterAI
  +0x30  array of services, one of them is CSrvPlayerHealth
  +0x64  BhvAssassin*
  +0x68  Entity*
  +0x74  Human*

CSrvPlayerHealth
  +0x20  CharacterAI*
  +0x58  int current health
  +0x5C  int maximum health
  +0x64  int health units per bar (4)
```

Health bars are `ceil(current / per_bar)` out of `max / per_bar`. 56 units with 4 per bar are the
14 bars shown in game.

### Inventory

The inventory array holds containers of both container classes:

```
InventoryRechargeableContainer / InventoryItemContainer
  +0x08  uint32_t type
  +0x10  int      value
```

| Type | Container | Meaning |
|---|---|---|
| 1 | rechargeable | florins |
| 4 | rechargeable | unknown, 10000 in the start inventory |
| 5, 7, 10, 11 | rechargeable | unknown consumables (7 and 11 held 3 each) |
| 2, 3, 6, 8, 9, 12 to 15, 17, 18 | item | unknown |

`PlayerProgressionManager + 0x88` embeds a `LogicalInventory` with the same layout. It is the
start inventory, not the live one.

### Templates

Several `BhvAssassin` objects exist at the same time:

| Candidate | World | Components | Position |
|---|---|---|---|
| real player | yes | 26 | in game coordinates |
| template in another world | yes | 16 | about `0, 0, -0.0003` |
| two prefabs | no | 15 and 11 | `0, 0, 0` |

The template that sits in a world already exists in the main menu, with 20 health and 0 florins.
The plugin rejects candidates without a world, closer than 1 m to the origin, or not listed in
their entity's components, then takes the one with the most components.

## Story progress

```
MissionGroup
  +0x04  uint32_t id
  +0x14  MissionRoot*
  +0x20  uint32_t state   1 locked, 2 in progress, 3 completed
  +0x50  array of mission handles in story order

Mission
  +0x04  uint32_t id
  +0x14  MissionGroup*
  +0x18  uint32_t state   1 locked, 3 available, 5 active, 6 completed
  +0x1C  int      completion percent (100 when completed)
```

The main sequences are the groups with IDs `0x10001858` (sequence 1) to `0x10001865`
(sequence 14). `0x10001866` follows with a single mission and is not mapped.

Index 0 of a sequence's mission list is the intro (`SQ07_M00` for sequence 7), which the DNA menu
does not count as a memory. The index of the first mission that is not completed is the current
memory number. Example from sequence 7, memory 4:

| Index | State |
|---|---|
| 0 to 3 | 6 (completed) |
| 4 | 3 (available, current memory) |
| 6, 7 | 3 |
| 5, 8, 9, 10 | 1 |

While free roaming, four missions outside the main sequences are always in state 5. The current
main mission is expected to switch to state 5 while it is played; this is not confirmed with a
dump yet.

`MissionManager + 0x8B4` is an array of the active missions, `+0x88C` an array of groups that are
still locked. Neither is a stable way to the sequences, so the plugin scans for the groups.

`MainSequenceInfo` objects are static definitions (ID, localization ID at `+0x0C`, memory count at
`+0x28`), not progress.

## Landmarks

```
AnimusDatabaseTriggerEvent
  +0x08  handle record -> AnimusDatabaseData
  +0x10  handle record -> Entity or EntityGroup (position at +0x40)

AnimusDatabaseData
  +0x04  uint32_t id
  +0x10  uint32_t index
  +0x14  uint32_t category   1 building, 2 shop
  +0x3C  uint32_t localization id of the name
  +0x44  wchar_t* internal name, for buildings usually the Italian name
  +0x48  uint32_t localization id of the description
```

Only the triggers of the currently loaded region exist in memory. Localized names are not resident
while the database menu is closed. Entries named `District of ...` are lore entries, not areas.

`FeatherDistrictInfo` (24 instances, localization ID at `+0x0C`) lists the districts for the
feather statistics. The engine class `CitySectorRegionCellData` has no live instances, so the game
does not track the district the player is in.
