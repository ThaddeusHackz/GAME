# M8 Report: Performance / Ship

## Delivered
- **`--benchmark [N]`** (Spec 37.1) runs 3 scenes, each with 30 warm-up frames followed by N timed frames:
  - island_vista: daytime pan
  - outpost_swarm: 30 hostiles
  - city_night: Meridian City at night

  Each frame is timed across sim + render + present. The run writes `benchmark.txt` with AVG, 1% low and 0.1% low FPS (the mean of the slowest 1% / 0.1% of frames), the worst frame time, max draw calls, and a recommended preset. The stats code is in `game/src/meta/bench.c`, which is pure C and unit-tested.
- **`--safe-mode`** forces the software renderer and the Low preset.
- **Zip layout** (Spec 17.2): everything sits under `DividedHorizon-win64/`.
  - Beside the exe: the 4 data files, `SafeMode.bat`, `Benchmark.bat` and a player README.
  - In `docs/`: LICENSES, CHANGELOG, DEVELOPER_README and uninstall_notes.
- **Docs:** `docs/README_PLAYER.md`, `docs/CHANGELOG.md` and `tools/uninstall_notes.txt`.
- **Tests:** 8 benchmark checks were added to suite 8. **707/707 pass.**

## Measured (Linux sandbox, headless, software renderer, 640x360, Medium, 120 frames)
| scene | avg | 1% low | 0.1% low | max draws |
|---|---|---|---|---|
| island_vista | 62.4 | 45.5 | 45.5 | 997 |
| outpost_swarm | 58.1 | 42.3 | 42.3 | 1188 |
| city_night | 55.2 | 31.5 | 31.5 | 1077 |

These are CPU software-rasterizer numbers from a sandbox. I could not measure the Windows GL path or real integrated-GPU hardware here.

## Honest status against Spec 15 and 18
- **Draw calls are over budget.** They run from 997 to 1188 at Medium, and the budget is 900 at Low. Draw-call batching was **not** done in M8. I'm logging it as open.
- **Stable 60 FPS on low-end hardware is unverified.** Run `Benchmark.bat` on target hardware.
- **There is no HD pack.** The whole game is 755 KB of exe and the zip is 344 KB, far below the ~10 GB target. Nothing was padded (Section 1).
- Crash logs and rolling autosave already existed from earlier milestones and were not reworked.
- Content gaps carried over are listed in the CHANGELOG under "Known gaps".

## Build
- Exe: PE32+ GUI, 755200 bytes, OS DLL imports only.
- Zip: `dist/DividedHorizon-v0-win64.zip` (344 KB).
