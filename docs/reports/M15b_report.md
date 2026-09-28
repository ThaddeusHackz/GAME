# M15b — El Reloj fairness fix (world markers)

**Problem (flagged in M15):** the clock hand and bomb columns existed only as HUD markers, so the player couldn't see the danger in the world.

**Fix:**
- `boss_draw_world()` in `boss.inl` draws the hand as 13 orange slabs that follow the terrain from the arena centre out to 26 m along `hand_ang`, plus a dark hub. The hand's 1.2 m damage half-width matches the 2.4 m slab width.
- The 4 bomb columns are drawn as tall red pillars that blink white in the last 10 s, and shrink to grey stubs once defused.
- Draw cost is 18 boxes, and only during the El Reloj fight.

**Tests:** suite 14 has a new check (`boss.drawn == 18`). All 14 suites pass with 918 checks. The Windows build and zip are OK.

**Still honest gaps:** these are box markers, not bespoke models. El Reloj still uses the officer mesh. Headless screenshots come out washed out (see `img/M15b_reloj_world.png`).
