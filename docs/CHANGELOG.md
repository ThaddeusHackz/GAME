# Changelog



## M20 — Camp network: 11 liberation camps (outposts 2..12) with 3 layouts, streamed tagged garrisons (3+difficulty, pool slot reuse), clear + hold flag 3 s, rewards, map/minimap icons, save ids camp_00..camp_10; Isla Sombra k/12; suite 19 (1063 checks)

## M19 — El Sereno (finale, slot 5): lighthouse lantern needs all 5 lieutenants; P1 THE GATHERING (3 waves, -1 per wave if an outpost is liberated), P2 THE HOST (called cuts, T-parry 0.4 s -> 3 s stagger, "Good. Again."), P3 THE OFFERING ([E] Tobias / [T] ledger / 8 s silence for clean hero); feats THE LAST LIGHT + YOU WERE LISTENING (33); suite 18 (1038 checks)

## M18 — El Limpiador (B5): call-box summon, 2 shield vans + EMP node (6 s stall / 12 s cd), LIMPIO WAVES elites, CRANE riot-shield cycle; feat CIUDAD LIMPIA (31 feats); suite 17 (1012 checks)
## M17 — El Fraile (B4): bell-telegraphed swings, T-parry on the third ring, TOLL acolytes, chain wrap, kneel + SUBDUE/strike; feat LA TERCER CAMPANA (30 feats); suite 16 (983 checks)

## M16 — Doña Marea (B3): gunship runs, harpoon windows, anchor sweep, rising tide at sunset; feat LA AGUA CUSTODIA; suite 15 (956 checks)

## M15b — El Reloj world markers
- Clock hand + bomb columns drawn in 3D (fairness fix). 918 checks.

## M15 — Boss B2 El Reloj
- El Reloj boss fight: 4 bomb columns to defuse, a sweeping clock hand, 3 phases.
- New feat SINCRONIZADO (28 feats). New suite smoke_reloj; 917 checks total.
- Known gap: the clock hand is not drawn in the world (HUD only).

## M13 — Los Cobradores
- Infamy-triggered bounty squad (Rastra, Vidente, Pulpo, Lucky); wound/return with scars, 3rd defeat final + Herald obituary; squad trinket; 2 feats; smoke suite 12.

## M12 — Feats
- 24 feats to earn, 2 of them secret. Each one stamps a wax seal and gives +50 XP. Press **J** to see the poster wall. Stamps are saved with your game.

## M11 — Legend
- Stature: a hidden slider from renegade to hero, moved by what you do (quiet captures, towers, missions and freeing Poncho push it up; kills, alarms and wanted stars push it down).
- The Herald: notable deeds print a front page (13 story types x 3 tones). An EXTRA! banner pops up; press **N** to read the scrapbook (up to 24 pages, saved with your game).
- What reacts to stature: shop prices (±5% per tier), how fast heroes shake the cops, what city pedestrians say, the Herald's tone and street poll, and the ending front page.

## M10 — Performance
- 2D HUD/menu quads batched per texture run: max draw calls 1188 → 193 in the benchmark (Spec 15 budget < 900 at Low).
## M9 — Soul
- Poncho the dog: free him on the beach (E), O = heel/stay/hunt, aim+O = distract a guard, pet to bond, never dies (hold E 3 s or auto 30 s).
- Photo mode (P): frozen world, free camera, 6 filters, time of day, PNG capture to user dir/photos.

## M8: Performance / Ship
- `--benchmark [N]`: runs 3 scenes and writes benchmark.txt with AVG, 1% and 0.1% lows plus a recommended preset.
- `--safe-mode`: software renderer and Low preset. Also available as SafeMode.bat.
- Zip restructured into one `DividedHorizon-win64/` folder with player docs.

## M7: Polish
Added a procedural audio bank and 24-voice mixer, an adaptive score, difficulty levels, camera shake, bird flocks, and an accessibility toggle.

## M6: Story
Added 11 story missions with 49 objectives and 2 endings.

## M5: Systems
Added XP and levels (cap 30), 24 skills, and save slots (3 auto, 10 manual).

## M4: City
Added Meridian City with vehicles, the heat/wanted system and the ferry.

## M3: Island
Added Isla Sombra, outpost capture and the day/night cycle.

## M0–M2
Built the engine, traversal and combat.

## Known gaps (honest)
- Vehicles are box-shaped, the radio is a stub, traffic loops, and bullets don't damage cars. There are no roadblocks or SWAT.
- Pedestrians never die. City fog is not saved.
- Only 1 outpost exists (the spec calls for 12). There are no strongholds, no boss fights, and no companion dog.
- Audio is mono. There are no subtitles and no photo mode.
- There is no HD pack. Total size is under 1 MB, not ~10 GB. It was not padded.

## M14 — Boss B1 La Sargento
- War-drum fight in the combat yard: drummers hold up a shield wall, killing one staggers the unit, 3 phases, whistle stun, FORMAR feat, Herald page. Bosses 2–6 not built yet.
