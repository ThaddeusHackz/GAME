# M19 — El Sereno (finale boss, slot 5)

**Status:** done. 18 suites, 1038 checks, all green. Win64 PE verified; zip is `dist/DividedHorizon-v0-win64.zip` (396K).

## What was built
- **Gate:** the lighthouse lantern prop at arena + (0, -52). [E] opens the fight only when B1–B5 are all beaten. Otherwise it shows "The lantern is cold. N of 5 lieutenants have fallen."
- **P1 THE GATHERING:** 3 cult waves of 4. If the island outpost is liberated, each wave is one cultist smaller (the "garrison" help). El Sereno stands still and takes no damage (armor 1.0).
- **P2 THE HOST:** a cane duel. He calls each cut out loud ("High." / "Low." / "Your left."), then T opens a 0.4 s parry window. A parry staggers him for 3 s with armor 0, and he says "Good. Again." A missed parry costs 18 HP. Otherwise his armor is 0.9.
- **P3 THE OFFERING:** at 35% HP he stops fighting and his health is held there.
  - [E] cut Tobias down: "Then the family was always the ledger."
  - [T] hand over the ledger: "Thank you. It was only ever paper." (−10 stature)
  - Silence: 8 s with no input. This is only available with stature tier ≥1 and all five lieutenants beaten clean. His line: "Ah. You were listening."
  - If he dies to damage before P3, the game uses the blade ending.
- **Feats:** THE LAST LIGHT and YOU WERE LISTENING (33 total).
- **Other:** the save keeps bit 5. The herald now uses the name EL SERENO.

## Honest deviations
- The fight takes place in the arena yard, not a monsoon lighthouse.
- Allied garrisons only make the waves smaller; you don't see allies on the field.
- There is no cutscene camera, no voice, and no music duck.
- The ending line is a message; it does not change the story state.

Screenshots: `img/M19_gathering.png`, `img/M19_host.png`, `img/M19_offering.png`. Headless captures come out washed out, which is a known issue.
