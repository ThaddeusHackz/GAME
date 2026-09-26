# M5 Report: Systems & Progression

**Branch:** `arena/01a0d58d-game` · **Tests:** 85 + 69 + 112 + 82 + 154 + **93** = **595/595** · **Exe:** `dist/DividedHorizon.exe` (643 KB, PE verified: imports only DLLs that ship with Windows) · **Zip:** 300 KB

## What was built
| System | Implementation |
|---|---|
| XP / levels (§71) | Level n→n+1 costs 100 + 50(n-1) XP. Level cap is 30 and each level gives 1 skill point. XP sources: kill 10, headshot +5, takedown 25, loud outpost 250, stealth outpost 375, mast 100, job 60, hunt 15, craft 5. |
| Skills (§71) | 24 skills in 4 trees (Survivor / Hunter / Ghost / Driver), 3 tiers. Tier costs are 1/2/3 SP and tiers unlock at LV1/3/6. A higher tier needs a skill from the tier below in the same tree. **Every skill changes live gameplay:** max HP, breath, stamina, heal/food strength, recoil, reload, hides, binocular range, headshot damage, reserve cap, detection speed, alarm-box damage, alert gain, alarm radius, prices, car top speed, crash damage, wallets, heat escape, job pay, sell prices. |
| Economy (§72) | Vendors load from `data/economy.json`, with an identical fallback built into the exe. Rosa's Trading Post is on the island and Vargas Pawn & Arms is in the city. You can buy items, ammo (limited by the reserve cap), the island chart (reveals the fog) and ammo pouches. You can sell hides and scrap. Both acts use one currency ($). |
| Hunting / crafting | Deer and boar can be shot. Press E to skin a kill for hides. There are 16 herb patches and enemies drop scrap. 4 recipes: medkit, bandages, boar-hide plate, deer-hide pouch. |
| Healing / armor | H uses a medkit (+60) or bandage (+25), or straps on an armor plate. Armor absorbs 60% of enemy damage. |
| Island alert (§13.6) | Scale 0-100 with 4 levels: Calm / Wary / Alert / Lockdown. It goes up with alarms, gunfire and kills, and drops after a stealth capture or a rest. It starts decaying after 30 quiet seconds. Each reinforcement wave has 3 + alert level enemies. |
| Safehouses | One on the west beach and one at the harbor. Press E to heal fully, set your checkpoint, sleep to 07:00 and autosave. Not allowed while wanted or in combat. |
| Map + fast travel | TAB opens a full map with fog of war, icons and 5 travel points (2 safehouses, outpost, mast, job board). Travel points unlock when you find or liberate them. Travel stays within the current act. Not allowed while wanted, in combat or in a vehicle. |
| Saves | JSON save slots: 3 autosave + 10 manual. They store position, act, time, cash, XP/skills, inventory, weapons/ammo, fog, outpost/mast state, travel unlocks, alert and **wanted stars**. Press L on the title screen to continue. |
| Ferry persistence | The island remembers what you did across ferry trips: captured outposts, destroyed alarm, synced mast and explored fog. **Bug fixed:** every world swap used to leak all terrain meshes, so after a few trips the ground became invisible. Mesh slots are now reused, and a test covers 10 crossings. |

## Honest gaps (BLOCKED / deferred)
- City fog is not saved. Only the island map is saved.
- There is only one outpost, so alert level only affects its reinforcement waves and patrols ignore it.
- Carried over from M4: box-car vehicles, radio stub, peds that never die, looped traffic, no bullet damage to cars, no roadblocks/SWAT/bribes.
- The screens were checked in headless 640×360 captures only. No real Windows playtest has happened yet.
- On the paused character screen the text costs ~2000 draw calls (one per glyph). This is over the 900 budget, but only while the game is paused. Gameplay draw counts are unchanged from M4.
