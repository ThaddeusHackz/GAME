# DIVIDED HORIZON

An original, stylized open-world action game with two maps: the tropical island
Isla Sombra and the crime city Meridian City.
Built by AI under human direction. DRM-free and fully offline.

## Requirements
- Windows 10/11, 64-bit. The game is one ~0.8 MB exe with no installer and no runtimes.
- Any GPU that supports OpenGL 1.1, or none at all: `--safe-mode` uses the CPU renderer.
- About 5 MB of disk space.

## Start
Unzip the folder anywhere and run `DividedHorizon.exe`.
- **Game won't start, shows a black screen or glitches?** Run `SafeMode.bat`. It forces the software renderer and the Low preset.
- **Unsure which settings to use?** Run `Benchmark.bat`. It plays 3 scenes (island vista, outpost fight, city at night) and writes `benchmark.txt` with average FPS, 1% and 0.1% lows, and a recommended preset.

## Controls
| Action | Key |
|---|---|
| Move / sprint / jump / crouch | WASD / Shift / Space / Ctrl |
| Aim / fire / reload | Right mouse or C / Left mouse or F / R |
| Roll / takedown / binoculars | V / T / B (hold) |
| Weapon slots | 1 / 2 / 3 |
| Pause menu | Esc |
| Interact, enter vehicle | E |
| Radio | Q |
| Ferry between island and city | K |
| Map / character | TAB / I |
| Heal | H |
| Load / continue | L |
| Mission card choices | 1 / 2 |
| Performance overlay | F3 |

## Files the game writes (next to the exe)
`settings.json`, `save_*.json`, `logs/`, `crash-log.txt`, `benchmark.txt`.

## Honest scope statement
This is a small indie-scale build and not a AAA game. See `docs/CHANGELOG.md`
for exactly what is in it and what is known to be missing. The graphics are flat
and stylized on purpose. All art, audio and music are generated in code, and
no copyrighted material is used.

## Manifesto
The game respects your time and your machine. It has no launcher, no account,
no telemetry and no DRM. Everything runs offline.
