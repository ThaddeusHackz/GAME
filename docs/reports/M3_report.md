# M3 REPORT — ISLAND VERTICAL SLICE

**Branch:** `arena/01a0d58d-game` · **Date:** 2026-09-26 · **Status:** COMPLETE (all gates green)
**Build:** `DividedHorizon.exe` v0 "M3 Island Slice" (468 KB PE32+ GUI) + `DividedHorizon-v0-win64.zip` (228 KB)

---

## 1. Definition of Done — result (Spec 16)

> "One km² of island: beach, jungle, one outpost (full stealth-capable), one signal mast,
> wildlife (2 species), day/night cycle. DoD: player can scout, plan and capture the outpost
> in three valid ways (stealth / loud / mixed)."

| DoD item | Result |
|---|---|
| 1 km² island block: beach + jungle | ✅ The M1 heightfield island (1000×1000 m, radial falloff, sea shelf) is now dressed: density-scaled palms / hardwoods / bushes (56/54/90 base × `foliage_density`), beach shelf + jungle band + ridge biome placement, all procedurally textured (`bark`, `leaf`). |
| One outpost, full stealth-capable | ✅ **PUNTA QUEMADA**, difficulty 1 (§28.2): 8 garrison (5 grunt / 2 bruiser / 1 officer) on authored patrol routes incl. a watchtower sentry standing on the 5.2 m deck; sandbag perimeter with gate + west gap, hut with climbable roof, crate-stair watchtower, cover clusters, destructible alarm box (150 hp §52), flag pole. Crouch/LOS/detection-cone stealth from M2 works here unchanged. |
| Capture in 3 valid ways | ✅ All three proven end-to-end in `smoke_island`: **stealth** (8/8 silent takedowns, alarm never raised, 0 waves, yard held → flag), **loud** (gunfire trips alarm, reinforcement wave responds, garrison wiped by force → flag), **mixed** (3 silent takedowns, then gunfire mid-clear trips alarm, force-finish → flag). Capture = garrison dead + hold the yard 3 s (§37.3); flag animates up over 2.5 s; liberation drops health + 556 + 9 mm supplies. |
| One signal mast | ✅ Lattice tower on the Cresta Sombría ridge peak (~31 m): 32 spiral rungs at 0.62 m rises — the player *walks* to the 20.4 m platform using only M1 step-up verbs (no new climb code). `E` at the top syncs: fog-of-war lifts island-wide ("SIGNAL MAST SYNCED — Isla Sombra mapped"). |
| Wildlife, 2 species | ✅ 20 critters: venado (deer, antler nub) + jabalí (boar), graze/wander → alert → flee FSM; flee on close player (crouch radius 9 m vs stand 16 m) or gunfire ≤90 m; simulated only within 165 m of the player; never spawned in the sea. |
| Day/night cycle (6.7) | ✅ 24-minute `day_t` clock (0 = 06:00). Three-stop colour script (night / golden / day) drives sun direction + colour + intensity, ambient, hemisphere fill and fog; sky dome re-tinted per frame via a new vertex-colour × item-tint multiply in the rasterizer. Night = 0.18 sun intensity, cool moon, near-black fog, and §82 exposure drops to 0.18 → **night is stealth weather**. 240-sample sweep proves finite, unit-length sun and intensity ∈ [0,2] everywhere. |
| Scout & plan loop | ✅ Binoculars (`B`, hold): 20° optic, 65% look damping, letterbox + mil-dots; centred hostiles with clear LOS get **tagged** (persistent red diamond + range in world, red pip on minimap). Fog-of-war minimap (§52): 64×64 explore mask over cached terrain colours, 96 m reveal radius, north-up, clock, landmark icons; unexplored cells stay dark until walked or mast-synced. |

Test gates: **smoke 85/85 · traversal 69/69 · combat 112/112 · island 82/82 = 348 checks**, Windows cross-build PE-verified.

## 2. What shipped

| File | Lines | Content |
|---|---|---|
| `game/src/game/game.c` | 2395 (+~700) | `day_palette()` pure colour script + `game_set_time()`; `build_island()` orchestrator (outpost, mast, vegetation, wildlife, minimap cache) called before chunk meshing; `prop_visual()` draw-only volumes; outpost FSM (`island_update`: combat→alarm after 1.2 s sustained contact, gunfire earshot 66 m, two reinforcement waves at 9 s/24 s, capture channel, mast interact); `melee_update` (silent takedown vs 40-dmg swing); `binoculars_update` (cone + LOS tagging); `critters_update`; fog reveal/sync; sky tint; minimap + clock + objective + capture bar + binocular overlay HUD; arena dev-mode now preserves/rebuilds the garrison (faction 2) and arena-clear counts faction 0 only. |
| `game/src/game/game.h` | 228 | `Outpost` (garrison blueprint, alarm box, mast, capture/alarm state), `Critter`, fog mask + cached map colours, `MAPW 64`, `game_outpost_start()`, `day_palette()`, `game_set_time()`. |
| `game/src/ai/enemy.h` | 98 | `Enemy.tagged` (binocular mark). |
| `game/src/rend/soft.c` | 545 | `vcol_tint()`: vertex colour × item tint — lets the baked sky gradient be re-tinted per frame (white-tinted meshes stay identity). |
| `game/src/plat/plat.h` / `win32.c` | — | `BTN_BINOC` (bit 22); Win32: **T** melee/takedown, **B** binoculars (hold). `E` interact and `BTN_MELEE` edges wired. |
| `game/src/world/collision.h` | — | `COLLIDE_MAX` 512 → 1024 (island geometry). |
| `game/src/main.c` | — | `--outpost` (beach-drop DoD start), `--night` (open at ~00:43). |
| `game/tests/smoke_island.c` | 681 | 82 checks (below). |
| `tools/diag/island_demo.txt` | 18 | Headless scout demo (pan → binoculars → tag → crouch), reused for day and `--night` stills. |

### smoke_island.c — 82 checks

- **Day/night (pure):** noon bright + overhead · midnight dim + cool · dawn/dusk warm gold (r−b > 0.2) · wrap both directions · opposite horizons at dawn/dusk · 240-sample finite/unit/intensity sweep.
- **Day/night (live):** `game_set_time` keys atmosphere · night exposure < day exposure (§82) · clock advances in play, stays in range.
- **Outpost build:** named, difficulty 1, 8 garrison slots (5/2/1 mix), pad flattened into the heightfield, mast ≥15 m tall on a >15 m ridge, starts quiet/uncaptured.
- **Stealth path:** 8/8 silent takedowns · alarm never raised · 0 waves · garrison wiped · hold yard → `captured` at frame 179 (~3 s) · flag rises.
- **Loud path:** gunfire inside 66 m raises alarm · wave responds · bodies added · garrison eliminated by force · captured · alarm stayed raised.
- **Mixed path:** 3 silent takedowns quiet → gunfire trips alarm → force-finish → captured.
- **Alarm box:** 150 hp · destroyed by gunfire · destroyed box blocks waves · destroyed box blocks combat-alarm.
- **Waves:** sustained combat contact (live box) raises alarm · wave 1 arrives · bodies spawn.
- **Binoculars:** untagged → tagged in optic · zoom blend reaches full · tagged counter · relaxes on release.
- **Fog of war:** local cell revealed · distant cell dark · partial mask · mast sync maps 4096/4096 cells.
- **Wildlife:** both species present · on land only · flees a close player.
- **Integration/perf:** 600-frame free-run finite · sim worst 0.00 ms/frame · LOW-preset world 114 draws, shipping frame (HUD on) 594 draws, 634 items — all under the §15 <900 budget · foliage stays lean (284 props at 0.35 density).

## 3. Performance vs Spec 15 (LOW preset, measured in CI)

| Metric | Budget (Low) | Measured |
|---|---|---|
| Draw calls (world) | < 900 | **114** |
| Draw calls (shipping frame, HUD on, debug off) | < 900 | **594** |
| Render items | — | 634 |
| Sim cost, full island alive | — | **0.00 ms/frame** worst |
| Props (foliage 0.35) | — | 284 (Medium 0.6 → 332; Ultra 1.0 → 560-ish, cap 640) |

Note on the earlier ~900 readings: that figure included the **dev debug overlay** (~300 glyph quads) which never ships. The gate now measures the shipping frame (gameplay HUD on, debug off) and the world-only cost separately; both are far inside budget. Foliage honours `foliage_density` (Spec 15.3) at spawn time, so Low genuinely builds less jungle instead of only culling it.

## 4. Controls added (M3)

| Input | Action |
|---|---|
| `B` (hold) | Binoculars: 20° optic, tag hostiles in the crosshair cone |
| `T` | Melee / silent takedown (unaware target dies silently; aware target takes 40) |
| `E` | Interact (signal-mast sync at the top platform) |
| `--outpost` / `--night` | Launch straight into the island DoD / at night |

## 5. Honest limitations & backlog (carried, not hidden)

- **Suppressors don't exist yet** — stealth today is takedowns, crouch and night; any gunshot inside 66 m trips the alarm. Suppressed sidearm + body-hiding are combat-v2 backlog (§37 full stealth kit).
- **Corpses don't alert patrols** — bodies are visual only; discovery-of-bodies is backlog.
- **GL11 sky is a flat tint** — the GL11 backend has no vertex-colour path, so the shipping exe shows the time-of-day tint without the soft-renderer's zenith→horizon gradient. Reads correctly (night is dark, golden hour is warm); gradient-on-GL is a polish item.
- **HUD is unbatched quads** — 594 shipping draws is mostly 2-D glyph/minimap quads; fine on any GPU, but a batched 2-D pass is M8 perf work.
- **Foliage density applies at world build** — changing `foliage_density` in settings takes effect on next load (no live re-scatter).
- **First real-hardware Windows boot is still the hard gate** — the exe is structurally verified (PE32+ GUI, OS-only imports) and its simulation is proven headless; no GPU/display exists in this sandbox.

## 6. Stills (regenerate with `tools/build.sh run` / the demo script)

`build/shots/m3day/` — beach approach at 08:01 golden hour · yard pan · binocular optic with a tagged tower sentry at 72 m · crouch view.
`build/shots/m3night/` — same beats at 00:43: indigo sky, silhouetted outpost, moonlit minimap.
(Not committed — `build/` is gitignored; reproduce from `tools/diag/island_demo.txt`.)

---

*M3 closes the island gameplay loop: you can scout Punta Quemada with binoculars, plan a
route through its gaps, and take it silently, loudly, or both — and the island itself breathes
on a 24-minute clock. Next: M4 City Slice (Meridian City block, wanted levels, vehicles).*
