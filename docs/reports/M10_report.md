# M10 — Draw-call budget (Spec 15)

## What changed
Every HUD/menu glyph and 2D quad used to be its own draw call (shadowed text = 2 per glyph), which made up most of the 997–1188 calls in the M8 benchmark.
- `rend_quad2d_run_end()` (rend.c) finds a run of consecutive 2D quads that share the same texture and scissor.
- **GL 1.1 backend**: each run is now one `glBegin(GL_QUADS)…glEnd` with per-vertex colour. Blending is always on (with alpha 1 the output is identical).
- **Software backend**: rasterises the same pixels as before, and counts one call per run, like GL does. That keeps the benchmark metric comparable to the GL path. Pixel output did not change.

## Results (sandbox soft renderer, 640x360, Medium, 120 frames)
| Scene | M8 max draws | M10 max draws |
|---|---|---|
| island | 997 | 111 |
| swarm | 1188 | 193 |
| city | 1077 | 165 |

- At Low in play mode, the suite-9 check measures a worst case of 80 calls, against a budget of 900.
- FPS in this run (47.6 / 41.7 / 41.1) was lower than the M8 numbers. The per-pixel CPU work did not change, so this is sandbox load variance, not a regression. Frame rate should be judged on real hardware with `Benchmark.bat`. On GL, fewer submits should help, but I have **not measured it** on a GPU here.

## Tests
Suite 9 has +4 checks (run grouping plus a Low-preset budget check). Full suite: 762/762.

## Windows build
Exe 794,624 B; zip 364 KB.

## Still open
Triangle count and texture memory are not re-measured on GPU. The gaps carried from M9 remain.
