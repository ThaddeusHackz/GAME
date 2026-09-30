# M22 — Vehicle Roster Expansion (Spec 6.6 / §69)

## Done
- `VEH_DEFS_MAX` 8 → 12. New classes: `VC_VAN`, `VC_PICKUP`, `VC_ARMORED`, `VC_BUGGY`, each with its own body size.
- Added to `vehicles.json` **and** the built-in fallback table (so the game still runs if the data file is missing):
  | id | name | class | top km/h | 0-100 s | hp | steal heat |
  |---|---|---|---|---|---|---|
  | V03 | Almuden | van | 120 | 11.0 | 700 | 1 |
  | V04 | Ranchero | pickup | 150 | 7.5 | 650 | 1 |
  | V11 | Obelisco | armored | 140 | 9.5 | 2000 | 3 |
  | V12 | Cosecha Bgy | buggy | 130 | 6.5 | 500 | 1 |
- Meridian parked fleet 8 → 12 (city total 27 vehicles).
- Pursuit: from 4★ every other response vehicle is a Limpio Obelisco. The old 5★ bonus armour now applies only to Sirena cruisers.
- Rendering: armoured vehicles get a light bar, pickups an open bed, buggies cult ochre paint.

## Tests
New suite 21 `smoke_vehicles` (50 checks):
- For all 9 land defs: 0-100 time within ±30% of the table value, top speed within 95–101% of the table value.
- Class, HP and steal heat match the table.
- Fleet placement.
- Stealing an Obelisco adds more evidence than stealing a pickup.
- The 4★ response includes Obeliscos.

Full run: 21 suites / 1133 checks green. Windows PE verified; the zip is 416K.

## Not done (honest)
- V08–V10 (boats and aircraft) still need water and flight handling. Not started.
- Buggies are on the city map only; there are no island roads with vehicle traffic yet.
- `img/M22_roster.png` is a 320×180 headless capture, mostly covered by the HUD. It is weak visual evidence.
