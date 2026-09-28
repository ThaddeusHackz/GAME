# M4 REPORT — CITY VERTICAL SLICE (MERIDIAN CITY)

**Branch:** `arena/01a0d58d-game` · **Date:** 2026-09-26 · **Status:** COMPLETE (all gates green)
**Build:** `DividedHorizon.exe` (524 KB PE32+ GUI, OS-only imports) + `DividedHorizon-v0-win64.zip` (256 KB: exe, README, LICENSES, weapons.json, vehicles.json)

---

## 1. Definition of Done — Spec 16

> "8 city blocks + harbor stub, 3 drivable vehicles with arcade handling + radio stub, traffic 15 cars,
> peds 30, heat system 0→5 with cruisers, 1 odd-job. DoD: steal car → evade 3 stars → buy hot dog at
> stand with stolen cash. THAT loop must grin."

| DoD item | Result |
|---|---|
| 8 blocks + harbor stub | ✅ 3×3 road grid (avenues/streets at 380/500/620, 7 m half-width) split by alleys into **8 blocks**, 4 buildings each (north row = downtown towers 16–42 m, south row 8–20 m), rooftop units, curbed sidewalk slabs, asphalt-painted terrain, lane stripes. Harbor stub: flooded channel, boardwalk + rail, two warehouses, pilings. Act II is a real **world swap** (Spec 4.3): the island is freed and the heightfield rebuilt as bay + 4 m city plate. |
| 3 drivable vehicles, arcade handling | ✅ **5** data-driven defs from `game/data/vehicles.json` (§69 rows V01 Coralina, V02 Tiburón GT, V05 Guagua, V06 Sirena, V07 Fénix) with an embedded fallback. Top speed / 0-100 / grip / HP are **law**; handling is derived: accel = 27.8/t₁₀₀×1.35, quadratic drag chosen so v_top equals the table, steer = 2.4×grip rad/s fading with speed, handbrake drift-lite, reverse, damage-limp, wreck at table HP with ejection damage. Measured: V01 hits 100 km/h inside the ±35 % window and settles on its 145 km/h law speed. |
| Radio stub | ✅ `Q` cycles 5 stations (COSTERA 98.5 … RADIO OFF) on the HUD; motorcycles have none (data flag). **Stub by design** — audio is M7 (Section 14). |
| Traffic 15 cars | ✅ Exactly 15 on 3 rectangular loops (7/4/4), right-hand lane offset, gap rule (slow within 11 m), brakes for the player's car, gunfire panic (×1.45 speed, 5 s). 10 s soak: finite, in bounds, ≥12/15 rolling, no bunching. |
| Peds 30 | ✅ 30 on the 8 sidewalk loops; walk → flee (gunfire ≤45 m / witnessed crime) → hide 10 s → resume. Carjacked drivers spawn as fleeing peds. **Hit-and-run knocks peds down; they get up** (no ped deaths in the slice — kindness/tone call, logged below). |
| Heat 0→5 with cruisers | ✅ `heat.c`: evidence thresholds 1/3/6/10/15, witnesses with LOS add up to +3, response ladder stars+1 cruisers (6 at 5★, armored), **heli at 4★+** (orbits, fires, loses you if you go slow and cold), escape = no LOS + >100 m for 20 s per star, evidence snaps down on each drop, fresh crime resets the streak. Cruisers pursue, ram on foot, PIT your car, drive-by fire with star-scaled accuracy; minimap shows them. |
| 1 odd job | ✅ "HOT DELIVERY": harbor job board → crate across town → hot-dog stand in 90 s → $400. Fail-forward: timer expiry, board re-offers work. |
| **DoD chain** | ✅ Proven end-to-end in `smoke_city` (`test_dod_chain`): carjack traffic (driver drops a **$25–60 wallet = the stolen cash**) → drive-by gunfire escalates to **3★** → hide slow-and-cold until heat reaches **0** → exit, `E` at the stand → **hot dog bought, exactly $5 leaves the wallet**, +15 HP. |

Extra: **Beto's ferry** (`K`) swaps acts in both directions (refused while wanted), cash/loadout ride along — the shipped exe reaches the city without CLI flags.

## 2. Gates

| Suite | Checks |
|---|---|
| core | 85/85 |
| traversal (M1) | 69/69 |
| combat (M2) | 112/112 |
| island (M3) | 82/82 |
| **city (M4, new)** | **154/154** |
| **Total** | **502/502** |

`smoke_city.c` covers: vehicle law + derived handling (13), world swap/layout/minimap (22), in-world enter/drive/steer/brake/reverse/exit-refusal/wreck/harbor wall (21), traffic soak (5), peds + witnesses + hit-and-run (10), heat unit ladder/escape (20), live carjack/pursuit/escape (13), job + hot dog (14), **DoD chain (10)**, radio/render/perf/reload idempotency/ferry round trip (20+).

## 3. Performance (Spec 15.3 LOW, measured in CI)

| Metric | Budget | Measured |
|---|---|---|
| World draw calls | < 900 | **93** |
| Shipping frame (HUD on) | < 900 | **521** |
| Items with 23+ cars, 30 peds live | — | 521 |
| City sim cost | < 6 ms | **0.00 ms** worst |
| Static props | cap 640 | 78 |

Headless 640×360 demo (`tools/diag/city_demo.txt`): 794 draws incl. dev debug overlay, SELF-CHECK PASSED.

## 4. Visual proof

`build/shots/m4/` (regenerate: `dh_headless --headless --city --script tools/diag/city_demo.txt --shots build/shots/m4`): harbor spawn facing the skyline, avenue walk (towers both sides, traffic ahead), skyline turn (peds on the curb, harbor glinting east), downtown. Two visual bugs found this way and fixed: roads rendered as jungle-green terrain (fixed with a terrain paint rect — a single giant asphalt quad was lost to near-plane rejection in the soft rasterizer), and the spawn faced the water.

## 5. Deviations & honest limitations

- **Vehicle models are box-and-cabin primitives** (PLACEHOLDER per Section 13; asset packs unreachable, rigged art is M7). Silhouettes differ by class size; police read via white body + alternating light bar.
- **Radio is a named-station stub**; no audio exists yet (M7).
- **No ped deaths**: hit-and-run is a knockdown + 2 evidence. Deliberate tone choice for the slice (§23 "violence has weight"); revisit with the M7 reduced-gore toggle.
- **Traffic is loop-following, not a junction graph**: no traffic lights or turning decisions yet (Spec 82 graph is M6 city build-out). Cars don't swerve around wrecks.
- **Guns vs cars**: player bullets don't damage vehicles yet; cars wreck via collisions and cop fire. Roadblocks/spike strips/SWAT vans from the 6.5 ladder are not in (cruisers + heli only).
- **Bribe/launder at El Banco Menor**: not in (economy is M5).
- **World persistence across the ferry**: outpost/wildlife reset on return (save-state is M5).
- **Sidewalk curb has no collision step**; peds and player walk the plate.
- **Windows boot on real hardware still unverified** in this sandbox (no GPU/display); PE structure + headless simulation verified.

## 6. Balance log

No §68/§69 numbers changed. Derived constant chosen: `accel = 27.8/t₁₀₀ × 1.35` (drag correction so measured 0-100 lands within ±35 % of the table). Heat thresholds and 20 s/star escape are new design numbers (Spec gives the ladder, not the thresholds).

*Next: M5 Systems & Progression — economy/shops/safehouse, skills, crafting, wanted persistence, island alert, map screen, fast travel.*
