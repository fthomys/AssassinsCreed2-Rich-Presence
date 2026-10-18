# Regions

The game streams each area from its own archive. The plugin counts how much data is read from
each archive and shows the one with the most recent traffic.

| Archive | Key | Region | Evidence |
|---|---|---|---|
| `DataPC_TitleScreen.forge` | `TitleScreen` | Main menu | name |
| `DataPC_WhiteRoom.forge` | `WhiteRoom` | Animus | also streamed in the main menu, loading screens and the pause menu |
| `DataPC_Abstergo.forge` | `Abstergo` | Abstergo Industries | name |
| `DataPC_Hideout.forge` | `Hideout` | Assassin hideout | name |
| `DataPC_Firenze.forge` | `Firenze` | Florence | name, confirmed in game |
| `DataPC_Toscana.forge` | `Toscana` | Tuscany | San Gimignano landmarks in memory, confirmed in game |
| `DataPC_Villa.forge` | `Villa` | Monteriggioni | confirmed in game |
| `DataPC_Venezia.forge` | `Venezia` | Venice | confirmed in game |
| `DataPC_Roma.forge` | `Roma` | Rome | name |
| `DataPC_Wetlands.forge` | `Wetlands` | Romagna (Forlì) | contains Caterina Sforza, Checco and Ludovico Orsi, Mario Auditore |
| `DataPC_Mountains.forge` | `Mountains` | Apennines | own minimap, Leonardo |
| `DataPC_Altair_Memory.forge` | `Altair_Memory` | Altaïr's memory | name |
| `DataPC_LGS01_Duomo.forge` | `LGS01_Duomo` | Tomb: Santa Maria del Fiore | Assassin tomb |
| `DataPC_LGS02_Palazzo_Medic.forge` | `LGS02_Palazzo_Medic` | Palazzo Medici | Uplay bonus map |
| `DataPC_LGS03_Gimignano.forge` | `LGS03_Gimignano` | Tomb: Torre Grossa | Assassin tomb |
| `DataPC_LGS04_Novella.forge` | `LGS04_Novella` | Tomb: Santa Maria Novella | Assassin tomb |
| `DataPC_LGS05_Forli_Citadel.forge` | `LGS05_Forli_Citadel` | Tomb: Rocca di Ravaldino | Assassin tomb |
| `DataPC_LGS06_Frari.forge` | `LGS06_Frari` | Santa Maria Gloriosa dei Frari | Uplay bonus map |
| `DataPC_LGS07_San_Marco.forge` | `LGS07_San_Marco` | Tomb: St. Mark's Basilica | Assassin tomb |
| `DataPC_LGS08_ArsenalShipya.forge` | `LGS08_ArsenalShipya` | Arsenale di Venezia | Uplay bonus map |
| `DataPC_LGS09_Monteregionni.forge` | `LGS09_Monteregionni` | Auditore family crypt | Uplay bonus, Tuscany ambience |
| `DataPC_LGS10_Visitazione.forge` | `LGS10_Visitazione` | Tomb: Santa Maria della Visitazione | Assassin tomb |

The bonus maps follow EaglePatch's list of Uplay rewards: Palazzo Medici, Frari, Arsenale and the
Auditore crypt. The remaining six LGS archives are the six Assassin tombs.

`DataPC.forge`, `DataPC_extra.forge` and `DataPC_extra_dlc.forge` are shared by all areas and
ignored.

## Detection

The archives contain references to each other, so their content alone does not identify the area
the player is in. Detection is based on traffic only:

- every read from a tracked archive adds its size to that region's score
- every second all scores decay by the factor 0.85
- a region takes over once its score is at least 512 KB and 1.5 times the current region's score
- before the first read, the last opened region archive is used
