# Roadmap

## Done

- [x] Region detection from archive streaming, works on every game version
- [x] German and English texts, every line configurable in the ini
- [x] Florins and health bars
- [x] Current sequence and memory
- [x] Free roaming state with the nearest landmark
- [x] Official discord-rpc library, linked statically

## Needs testing

- [ ] Memory detection while a memory is actually being played (mission state 5)
- [ ] Landmarks in Florence and Venice, so far only checked around San Gimignano
- [ ] Main menu no longer showing the player template as free roaming

## Planned

- [ ] Tell the main menu, loading screens and the pause menu apart. All three stream from
  `DataPC_WhiteRoom.forge`, so this needs a value from memory.
- [ ] Localized landmark names. The game only loads them when the database menu is open, so the
  plugin would have to read them from the localization data itself.
- [ ] Notoriety level of the current city (`NotorietyManager`)
- [ ] Side activities: assassination contracts, races, courier and beat up missions
- [ ] Collectibles: feathers, glyphs and codex pages
- [ ] Better texts for the Desmond segments and Altaïr's memory
- [ ] Player stats on the retail 1.01 executable. This needs memory dumps from that version.
- [ ] GitHub Actions workflow that builds the plugin and attaches a zip to each release

## Not planned

- Windows 7 support. The current toolchain needs Windows 8 or newer.
- Exact districts. The game does not track them, see the landmarks section in the README.
