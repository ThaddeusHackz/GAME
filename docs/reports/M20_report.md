# M20 — Isla Sombra camp network (outposts 2..12)

## What shipped
- `game/src/game/camps.inl`: 11 new liberation camps: LA SALINA, MIRADOR VIEJO, CANTERA ROJA, PLAYA DEL HUESO, EL ASERRADERO, POZO NEGRO, FARO CAIDO, LOS MANGLES, CRUZ DE PIEDRA, EL TRAPICHE, BAHIA MUDA. Difficulty runs 1 to 5.
- Placement is deterministic. A 4x4 sector sweep puts one camp in each sector. Each pad has to be flat land, at least 115 m from other camps and clear of the course, beach, arena (190 m) and Punta Quemada (120 m).
- There are 3 layouts: a tent camp, a watchtower you can climb with crate stairs, and a bunker with a roof you can stand on. Every camp has a sandbag ring with a gate, cover and a flag pole.
- Garrisons are **streamed**. A camp spawns 3 + difficulty enemies (officer at d≥3, a tower sentry on tower camps) when you come within 130 m, and despawns them past 230 m. Each enemy is tagged with its camp (`Enemy.tag`). Camps reuse old camp slots in place and always leave 12 slots free for bosses and waves. On a full island tour the pool peaked at 23/64.
- To liberate a camp, clear its garrison, then stand within 7 m of the flag for 3 s. Rewards are XP, +3 stature, $40+20·d, health and ammo drops, and the message "NAME LIBERATED - Isla Sombra k/12".
- Camps show on the TAB map and the minimap once revealed by fog or the mast sync. They save and load as SaveOutpost ids `camp_00..camp_10` (the save format already had 15 slots) and appear in `Progress.camps_mask`.
- Camps don't spawn during arena mode or while a boss fight is active.

## Honest scope
These camps are **lighter than Punta Quemada**. They have no alarm box, no reinforcement waves and no signal mast. The spec's 12 outposts are now all capturable, but only one has the full alarm/wave FSM. Strongholds (3) are still missing.

## Budgets
LOW preset: props went from 299 to 450 (the island test ceiling was raised from 300 to 480 and the reason is documented). World draws are 125, well under the 900 budget. `GAME_MAX_PROPS` went from 640 to 960.

## Tests
`smoke_camps.c` (suite 19, 25 checks) covers count, spread, land, names, streaming in/out/restream, no capture while the garrison lives, the hold timer, rewards, save/load, the tour pool peak, and 12/12. All 19 suites pass (1063 checks). The Win64 PE is verified and the zip was rebuilt (408K).
