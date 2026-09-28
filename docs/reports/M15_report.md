# M15 — Boss B2 "El Reloj"

**Status:** done. The build stays playable. Tests: 14 suites, 917 checks, all passing (+28 in the new suite `smoke_reloj`).

## What was built
- **Trigger:** a clock dial prop at the combat yard's south-west edge. Press E within 3 m to start the fight or a rematch.
- **Bombs:** four bomb columns at the yard corners share one timer (45 s, dropping to 35 s from P2). Press E within 2.5 m of a column to defuse it. If the timer runs out, every live column detonates for 12 dmg each, and they all re-arm.
- **Exposure:** defusing all four exposes El Reloj for 6 s (his armor goes from 0.9 to 0). Then the columns re-arm.
- **Clock hand:** a hand sweeps the yard out to 26 m. Standing on the ground within 1.2 m of it deals 25 HP/s.
- **Phases:** P1 TIC (hand 0.35 rad/s) → P2 TAC <66% (hand 0.55 rad/s, 35 s timers) → P3 MEDIANOCHE <33% (the hand reverses at 0.85 rad/s and El Reloj moves ×1.4 faster).
- **Win:** 500 XP, +5 stature, a Herald HT_BOSS headline, and a save bit. Winning with zero detonations earns feat **SINCRONIZADO** (feat #28).

## Limitations
- The bombs and the clock hand have no dedicated models. You see them only through the HUD (timer and live-bomb count) and the corner positions, so **the sweeping hand is invisible in the world**. This is a real playability gap.
- El Reloj reuses the officer mesh. Barks are text only.
- Bosses 3–6 are not built.
- Headless screenshots are still washed out (`img/M15_reloj.png`).
