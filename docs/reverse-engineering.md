# Reverse engineering

How the values in [memory-layout.md](memory-layout.md) were found, and how to find new ones.

## Getting readable code

The executable on disk is encrypted by SteamStub, so static analysis of the file only works for
strings and imports. With `DumpProcess=1` in the ini, every F9 press in game writes a full
minidump (`MiniDumpWithFullMemory`, about 1 GB) to the `scripts` folder. It contains the decrypted
code, the class descriptors and the whole heap.

When parsing the dumps, mask memory range addresses to 32 bits. Minidumps of this large address
aware process store addresses above `0x80000000` sign extended to 64 bits.

## Finding a value

1. Note a value shown in game, take a dump, change the value in game, note it again, take a
   second dump. Both dumps come from the same session, so heap addresses stay the same.
2. Search the second dump for the new value and keep the addresses that held the old value in
   the first dump. For the florins this left two addresses out of thousands.
3. Look at the object around the address. Walk backwards to the nearest word that points into
   `.rdata`; that is usually the vtable at the object start.
4. Name the class: read vtable slot 5 (`mov eax, <hash>; ret`) and look the hash up in the
   descriptor table (scan `.rdata` and `.data` for words that point to a name string followed by a
   hash and a size).
5. Find who points to the object, name those objects the same way, and repeat until reaching
   either a static address in `.data` or a well known object like the player.

A plain pointer scan with offset tolerance produces mostly noise in this game. Following only
exact pointers to object starts, with class names at each step, gives readable chains.

## What did not work

- Static pointer chains to the player. None within six levels; the engine reaches the player
  through handles and lists.
- `0x02216C68`, which points to the player's `OutOfBoundsComponent` most of the time but is
  overwritten by other components.
- The `PlayerProgressionManager` inventory, which is the start inventory, not the live one.
- `MissionManager` arrays as a path to the sequences, they only hold locked groups and active
  missions.
- Districts. The class meant for them (`CitySectorRegionCellData`) has no instances, and the
  `District of ...` strings belong to database entries.

## Confirmed against the game

| Value | How |
|---|---|
| Florins | 10911 before and 10661 after spending 250, then several fast travel costs |
| Health | 56 of 56, 47 after a hit, 14 bars confirmed by the player |
| Sequence and memory | sequence 7, memory 4 confirmed by the player |
| Active memory | dump while playing sequence 7, memory 6 ("Frühjahrsputz" in the German DNA menu): index 6 in state 5, the other available memories locked |
| Template player | 20 health and 0 florins in the main menu, position near the origin |
| Landmark | nearest building in Tuscany, Venice and Florence matched the player's location |
| Main menu | no player found while in the main menu after the template fix |
| Regions | Florence, Venice, Monteriggioni, Tuscany switched correctly during fast travel |

## Mistakes worth remembering

- The first unfinished mission is not always the current memory. Sequences offer parallel
  memories, and the one being played is the one in state 5.
- The player template is not exactly at the origin (z is about -0.0003), so an exact zero check
  does not catch it.
