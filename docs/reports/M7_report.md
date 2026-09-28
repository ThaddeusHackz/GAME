# M7 — Polish (audio, settings that matter, ambient life)

## What shipped
- **Audio, first time in the project.** `game/src/audio/audio.c` builds 20 sounds from code when the game starts: 4 gunshot classes, reload, dry fire, footsteps (ground and water), hit, kill, hurt, UI click, story-card chime, mission-complete jingle, outpost alarm, police siren, pickup, jump, land and birdsong. No sample files are used.
  - **Mixer:** 22.05 kHz mono with 24 voices. When all voices are busy, the oldest one is cut off. Output goes through a soft clipper.
  - **Volume:** the master, SFX and music sliders from Settings are applied. Pause and the menu screens lower (duck) the audio.
  - **Adaptive music:** the score is generated in code, with a calm pad (Am–F–C–G at 84 BPM) and a combat layer (kick, hat and bass). The combat layer fades in over about 2 s based on how many enemies are fighting within 120 m, whether an outpost alarm is on, and city heat stars. Setting `adaptive_music = 0` keeps only the calm layer.
  - **Windows output:** a waveOut ring of 6 × 1024 samples, using winmm, which was already linked. If the device won't open, the game logs a warning and runs silent. `--headless` never opens a device.
- **How sounds get triggered:** `game/src/game/polish.inl` compares the game state with the previous frame once per frame and plays sounds from the differences. No gameplay call sites were changed.
- **Settings that now work (they existed before but did nothing):**
  - `difficulty` changes enemy damage (Explorer ×0.5 / Normal ×1 / Hardcore ×1.5) and hit chance (×0.7 / ×1 / ×1.2). Both are also multiplied by `enemy_damage` and `enemy_aim`.
  - `camera_shake` controls the new screen shake on shots and hits. `photosensitivity` always turns shake off and also dims and caps the red damage flash.
  - `hit_marker` now hides both the hit-marker graphic and its sound.
- **Ambient life:** 3 bird flocks of 9 birds each circle over Isla Sombra. They are drawn as flapping line "V" shapes with distance and frustum culling, at about 54 line segments in total. Gunfire within 90 m scatters a flock (with a birdsong sound) and it settles back after 10 s. Birdsong also plays randomly now and then.

## Tests
New suite `smoke_polish.c` (23 checks):
- the mixer is silent when idle and makes sound when a voice plays
- volume and ducking work
- the 24-voice cap holds, and voices stop when their sound ends
- the music eases between layers
- footstep and jump cues fire
- a shot plays a sound, adds shake and scatters a flock
- photosensitivity and the camera_shake setting each turn shake off
- the difficulty scaling works
- combat raises the music intensity.

**Total: 699/699 across 8 suites.** The exe is 750 KB and the zip is 336 KB.

## Not done (honest)
- There is no recorded voice acting, no Foley recordings and no licensed music. Everything is synthesised, so it sounds retro. CC0 sample packs could not be reached from this build sandbox.
- Audio is mono with no 3D positioning (volume doesn't fall off with distance, no stereo panning).
- The radio stations are still only a stub, with no sound.
- Subtitles: the story is shown as text cards, and there is no spoken dialogue to subtitle yet.
- No photo-mode free camera yet.
- Carried over from earlier milestones: box-shaped cars, peds can't die, 1 of 12 outposts, no bosses and no Poncho.
