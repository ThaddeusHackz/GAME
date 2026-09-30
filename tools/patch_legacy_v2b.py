#!/usr/bin/env python3
"""Second v2 migration pass: string-level edits of remaining v1 statements."""
P = "prompt_src/legacy_parts_I-X.md"
s = open(P, encoding="utf-8").read()
R = [
("P4 RUNS ON ANYTHING — 60 FPS on integrated graphics at 900p is a launch\n   requirement, not a patch.",
 "P4 SCALES GRACEFULLY — photoreal fidelity is the default target; frame-time\n   budgets are enforced per hardware tier T0–T5 (Section 109). 60 FPS on T3+\n   with upscaling is a launch requirement, not a patch."),
("REN VOSS: stubby stylized character (capsule+simple mesh is acceptable through\nM6; upgrade to rigged glTF humanoid with Mixamo anims by M7).",
 "REN VOSS: photoreal MetaHuman-class hero (Section 107) with full-body first-person\nrig and motion-matched animation (Section 111). Capsule graybox allowed ONLY\nthrough M1 of Section 132."),
("and PHOTOREAL MODE (\"it's just lens\n  dirt and bloom\" — an honest joke about our stylized limits; players will\n  love the honesty).",
 "and ARCADE MODE (\"it's just\n  cel-shading and bloom\" — a post-process joke filter for a game that is\n  already photoreal; players will love the wink)."),
("tool wall (Godot, Kenney,\n  Quaternius, KayKit, Mixamo, Sonniss), and an exhibit titled \"Why The\n  Graphics Are Stylized, Not Photoreal\" — honesty as a museum piece.",
 "tool wall (Unreal Engine 5, Fab,\n  Quixel/Megascans, MetaHuman, Houdini/Gaea/Blender, Sonniss), and an exhibit\n  titled \"How The Graphics Were Made\" — Nanite/Lumen breakdown passes as a museum piece."),
("It is not photoreal — it is stylized, finished, and fun, and it runs on\n  your old laptop.",
 "It is photoreal, finished, and fun, and it scales from a handheld\n  to a 4K monster."),
("stylized visuals, ~10 GB of\nhonest content, 60 FPS on low-end PCs, delivered as a working\nDividedHorizon.exe.",
 "photoreal UE5 visuals, real\ncontent at real scale, tiered 60 FPS targets, delivered as a working\nDividedHorizon.exe."),
("12 principles applied to stylized rigs", "12 principles applied to photoreal rigs (exaggeration lives in timing, not proportions)"),
("Stylized + alive beats realistic\n   + vacant.", "Realistic + alive beats realistic\n   + vacant; uncanny gaze is the #1 photoreal failure."),
("Stylized faces + gaze + gesture\n  beats mocap-pastiche.", "MetaHuman faces + gaze + gesture\n  + performance capture beat mocap-pastiche."),
("explaining stylized-vs-photoreal with pride, not apology.", "explaining how the photoreal look was engineered, with pride."),
("stylized, ~10 GB of honesty, 60 FPS\non a laptop from 2016, delivered as DividedHorizon.exe.",
 "photoreal, real content at real scale,\ntiered 60 FPS targets, delivered as DividedHorizon.exe."),
("18.1 BOOT: clean double-click boot < 20 s to menu on min-spec. --safe-mode works.",
 "18.1 BOOT: clean double-click boot < 30 s to menu on T3 (NVMe); < 60 s on T1 (HDD). `-safe` launches SM5/low-scalability path."),
("18.3 PERFORMANCE: 60 FPS avg @ 720p Low on integrated-class GPU (or document\n     measured numbers if the build machine's GPU is the only test rig).",
 "18.3 PERFORMANCE: frame-time gates per tier as defined in Section 109 (e.g. T3: 60 FPS\n     avg, 1% low >= 45 FPS @ 1440p TSR-Quality/High). Measured via Unreal Insights + CSV profiler."),
("clean Windows profile (no Godot install needed).", "clean Windows 10/11 x64 profile (no Unreal Editor install needed; redistributables bundled)."),
("Commit per milestone on branch `arena/01a0d528-game` with message", "Commit per milestone on the session's working branch with message"),
("  godot --headless --import . && godot --headless --export-release \"Windows Desktop\" build/win/DividedHorizon.exe\n  godot --headless -s tests/run_smoke.gd\n  ./tools/build.sh   # one-shot: import → test → export → zip",
 "  RunUAT BuildCookRun -project=DividedHorizon.uproject -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive\n  UnrealEditor-Cmd DividedHorizon -ExecCmds=\"Automation RunTests Project.Smoke; Quit\" -unattended -nullrhi\n  ./tools/build.sh   # one-shot: compile → cook → automation → package → zip (Section 133)"),
]
for a, b in R:
    if a not in s:
        print("MISSING:", a[:60]); continue
    s = s.replace(a, b, 1)
open(P, "w", encoding="utf-8").write(s)
