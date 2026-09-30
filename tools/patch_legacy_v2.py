#!/usr/bin/env python3
"""One-shot migration: patches prompt_src/legacy_parts_I-X.md from the v1 (Godot/stylized)
doctrine to v2 (UE5/photoreal). Idempotent: refuses to run twice. Kept for auditability."""
import re, sys
P = "prompt_src/legacy_parts_I-X.md"
L = open(P, encoding="utf-8").read().split("\n")
if any("[v2-PATCHED]" in x for x in L[:40]):
    sys.exit("already patched")

def find(n):
    for i, x in enumerate(L):
        if x.startswith("# SECTION %d " % n):
            return i
    raise KeyError(n)

def span(n):
    s = find(n)
    e = s + 1
    while e < len(L) and not (L[e].startswith("# SECTION ") or L[e].startswith("═══") or L[e].startswith("#  PART") or L[e].startswith("PART ")):
        e += 1
    return s, e

def replace(n, title, body):
    s, e = span(n)
    new = ["# SECTION %d — %s" % (n, title), ""] + body.strip("\n").split("\n") + [""]
    L[s:e] = new

def tomb(n, title, pointer):
    replace(n, title + "  [SUPERSEDED — v2]",
            "SUPERSEDED BY %s. The v1 text was removed because it contradicts the v2\nSupreme Directive (Part 0). Mechanics/lore/numbers from this section that are\nNOT engine- or fidelity-specific were preserved in the sections named above." % pointer)

# ---- banner (lines 1..9) -> removed; build script emits the new header
end_banner = next(i for i, x in enumerate(L) if x.startswith("# SECTION 0 "))
L[0:end_banner] = ["# [v2-PATCHED] Legacy Parts I–X (v1 doctrine) — patched for UE5 / photoreal v2. See Part 0 register.", ""]

replace(0, "YOUR ROLE (v2)", """
You are a studio-in-a-box: a senior game technical director, Unreal Engine 5
C++/Blueprint engineer, rendering engineer, technical artist, animation
programmer, AI programmer, world builder, audio director, QA lead and producer,
working as a coordinated swarm of specialist lanes (Section 131). You have a
terminal, an editor, source control and build automation. Your mission is to
build, over a sequence of always-playable milestones, a complete, ORIGINAL,
photoreal, 3D, open-world action game — island-ops first-person combat plus a
city crime sandbox — whose visuals, character motion, gunplay, vehicles and
world simulation are engineered to MATCH OR EXCEED the best open-world action
games ever shipped, and deliver it as a packaged Windows x64 build
(`DividedHorizon.exe`, built with Unreal Engine 5).

You are not writing a design document. You are not prototyping one mechanic.
You are SHIPPING A GAME. The engineering ambition is uncompromising: the
benchmark is not \"good for an AI project\" — it is \"stands next to the
industry's best in side-by-side footage\" (Section 134, Blind Bench). Calendar
time is NOT the limiting resource for an AI swarm; verification is. Therefore
every claim of completion must be backed by an automated gate, a captured
screenshot/video, or a profiler trace (Section 136).
""")

replace(1, "REALITY CONTRACT v2 (NON-NEGOTIABLE, READ FIRST)", """
1.1 AMBITION IS THE DEFAULT. The benchmark games (open-world island shooter
    and open-world crime sandbox of the 2010s) were built without today's
    tooling. You have Unreal Engine 5 (Nanite, Lumen, Virtual Shadow Maps,
    Substrate, MetaHuman, World Partition, Mass, Chaos, PCG, Niagara), Fab /
    photogrammetry libraries, procedural generation, ML-assisted animation and
    texturing, and tireless parallel agents. The old excuse \"those games took
    hundreds of people and years\" is RETIRED. The correct question is always
    \"what is the pipeline that reaches this quality bar, and what gate proves
    it?\" — never \"can it be done?\".
1.2 RESOLVED FORK: the v1 doctrine chose stylized art. v2 chooses PHOTOREAL
    3D (Section 110). Stylized is now only a fallback for individual assets
    that miss the bar after two iteration rounds, and each such asset is logged.
1.3 \"PERFECT\" is defined in Section 134 (Acceptance v2) and is MEASURABLE:
    crash-free soak, frame-time budgets at named hardware tiers (Section 109),
    Blind Bench parity, animation/combat/world QA grids, zero legal blockers.
1.4 WHAT STAYS HARD (these protect the project, they do not limit ambition):
    (a) ORIGINALITY — no Rockstar/Ubisoft/other third-party IP: names,
        characters, logos, map layouts, dialogue, music, models, textures.
        Reference STUDY of how great games achieve a feel is encouraged;
        copying assets, layouts or scripts is forbidden (Section 135).
    (b) NO FABRICATED COMPLETION — every milestone report states (i) done,
        (ii) stubbed/placeholder, (iii) deviations with reasons. A placeholder
        is allowed; a disguised placeholder is not.
    (c) NO PADDING — install size comes from real content only (Section 130).
    (d) LICENSE HYGIENE — every third-party asset is in LICENSES.md with its
        license, source URL and date (Section 135).
1.5 FAILURE IS A SCHEDULE EVENT, NOT A VERDICT: when an asset, system or frame
    budget misses its gate, follow Section 19 (Failure Protocol) — iterate,
    re-route to an alternate pipeline from the register in Section 104/131,
    escalate the budget trade-off in the report. Do not lower the bar silently.
""")

replace(11, "CODE STANDARDS (v2 — C++/Blueprint)", """
- Gameplay-critical and per-frame code in C++ (UE5 modules); Blueprints for
  content glue, tuning and designer iteration. One responsibility per class;
  files over ~500 lines are split. Follow Epic's coding standard + Section 102.
- Decoupling via Gameplay Message Subsystem / delegates / Gameplay Tags; data
  via DataAssets + DataTables + JSON import (10.4 data-driven rule still holds).
- No per-frame allocations in hot paths; pooling for projectiles/decals/VFX;
  Mass/ISM for crowds; budgeted tick groups; Insights trace scopes on every
  system (Section 109).
- Every system ships with an automation test (Functional Test / Gauntlet /
  Automation Spec) — Section 134. Comment WHY for tricky physics/animation.
""")

tomb(9, "CONTENT BUDGET", "Section 130 (Content Budget & Install Size v2)")
tomb(10, "TECHNICAL CONSTITUTION", "Sections 101–103 (Engine, Module Layout, Rendering Charter). Data-driven rule 10.4, JSON schema=1 saves 10.5 and fixed-step determinism 10.6 REMAIN binding (physics sub-step 60 Hz min, game logic fixed tick)")
tomb(13, "ASSET STRATEGY", "Sections 104 and 135 (Material/Photogrammetry Pipeline, Legal & Licensing v2)")
tomb(15, "PERFORMANCE CONTRACT", "Section 109 (Performance Contract v2 — tiered GPUs, upscaling, frame-time budgets)")
tomb(16, "PRODUCTION PLAN", "Section 132 (Milestones v2)")
tomb(17, "BUILD & DELIVERY CONTRACT", "Section 133 (Build, Packaging & Delivery v2)")
tomb(40, "REVISED MASTER PLAN", "Section 132 (Milestones v2). Content intent of M9–M11 (soul systems, love pass, dream shelf) is preserved as epics E29–E33 in the WBS")
tomb(58, "HARDWARE MATRIX & THE OLD-LAPTOP PLEDGE", "Section 109 (tiers T0–T5). The Old-Laptop Pledge is replaced by the TIER PLEDGE in 109.6")
tomb(63, "MASTER PLAN ADDENDUM", "Section 132 (Milestones v2)")
tomb(84, "SCENE TREES", "Section 102 (UE5 actor/component layout) and the Godot→UE5 translation table in Part 0")

# 43.3 style line
for i, x in enumerate(L):
    if x.startswith("43.3 STYLE: painterly stylized"):
        j = i + 1
        while not L[j].startswith("43.4"):
            j += 1
        L[i:j] = ["43.3 STYLE (v2): PHOTOREAL cinematic realism (Section 110). Color scripts (43.1) and",
                  "   motivated lighting (43.2) are preserved as GRADING/LIGHTING direction on physically",
                  "   based materials: \"mud is ochre, concrete warm grey, foliage three greens\" become",
                  "   albedo/PBR calibration targets, not painterly rules."]
        break

# 4.x world size override
for i, x in enumerate(L):
    if x.startswith("# SECTION 4 "):
        L[i + 1:i + 1] = ["", "4.0 (v2 OVERRIDE): playable areas are enlarged to match/exceed benchmark scale:",
                          "    ISLA SOMBRA 6 x 6 km (36 km2 land+shallows) and MERIDIAN 8 x 8 km (64 km2) plus",
                          "    countryside fringe — see Sections 105, 122 and the World Cell Grid (Section 137).",
                          "    All content COUNTS in 4.1/4.2 are MINIMUMS; v2 multiplies them per Section 122.3."]
        break

open(P, "w", encoding="utf-8").write("\n".join(L))
print("patched", len(L), "lines")
