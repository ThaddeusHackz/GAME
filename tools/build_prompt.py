#!/usr/bin/env python3
"""Assemble MASTER_PROMPT.md (v2) from prompt_src/ (hand-written) + tools/gen_v2 (ledgers).

Deterministic: same inputs -> byte-identical output. Usage: python3 tools/build_prompt.py [--check]
  --check : rebuild in memory and fail if MASTER_PROMPT.md differs (CI guard).
"""
import os, sys, re, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from gen_v2 import world, art, anim, combat, systems

SRC = os.path.join(ROOT, "prompt_src")
OUT = os.path.join(ROOT, "MASTER_PROMPT.md")

def read(name):
    with open(os.path.join(SRC, name), encoding="utf-8") as f:
        return f.read().rstrip("\n").split("\n")

def body_lines():
    L = []
    L += read("00_part0_supreme_directive.md")
    L += read("legacy_parts_I-X.md")
    for n in ("11_part11_ue5_photoreal.md", "12_part12_movement_animation.md", "13_part13_combat.md",
              "14_part14_open_world.md", "15_part15_production.md"):
        L += read(n)
    gens = [world.gen_137, world.gen_138, art.gen_139, art.gen_140, anim.gen_141, combat.gen_142, systems.gen_143,
            art.gen_144, art.gen_145, art.gen_146, systems.gen_147, systems.gen_148, systems.gen_149, systems.gen_150,
            world.gen_151, world.gen_152, systems.gen_153, world.gen_154, art.gen_155, systems.gen_156]
    for g in gens:
        L += g()
    L += ["", "═══════════════════════════════════════════════════════════════════════════",
          "#  END OF MASTER_PROMPT.md (v2) — BUILD DIVIDED HORIZON.",
          "#  Rebuild: python3 tools/build_prompt.py   ·   Verify: python3 tools/build_prompt.py --check",
          "═══════════════════════════════════════════════════════════════════════════"]
    return L

HEAD = """# ═══════════════════════════════════════════════════════════════════════════
#  MASTER BUILD ORDER v2 — "PROJECT: DIVIDED HORIZON"
#  A photoreal, fully-3D, Unreal Engine 5 open world: island-ops FPS + city crime sandbox
#  Target: Windows x64, DX12. Tiered T1–T5 (GTX 1060 → RTX 5090-class). Scale: 140 km2.
#  Benchmark: match or exceed the best open-world action games in measured, blind-tested terms.
# ═══════════════════════════════════════════════════════════════════════════
#  HOW TO USE: read the INDEX below, then Part 0 and Sections 101–136 (the brief).
#  Everything else is a library: open by section number / line range (see Part 0, S0.2).
#  This file is GENERATED: python3 tools/build_prompt.py  (sources: prompt_src/, tools/gen_v2/)
# ═══════════════════════════════════════════════════════════════════════════
"""

def make_index(body, offset):
    rows = []
    cur_part = None
    for i, x in enumerate(body):
        ln = i + offset
        if re.match(r"^#  PART ", x) or re.match(r"^PART [IVX0-9]+ ", x):
            rows.append(("PART", x.lstrip("# ").strip()[:90], ln))
        m = re.match(r"^# SECTION (\S+) — (.*)$", x)
        if m:
            rows.append(("S", "SECTION %s — %s" % (m.group(1), m.group(2))[:100], ln))
    return rows

def assemble():
    body = body_lines()
    head = HEAD.rstrip("\n").split("\n")
    # two-pass index: index length depends only on number of rows
    rows = make_index(body, 0)
    idx_len = len(rows) + 8
    for _ in range(3):
        offset = len(head) + idx_len + 1
        rows = make_index(body, offset)
        idx_len = len(rows) + 8
    offset = len(head) + idx_len + 1
    rows = make_index(body, offset)
    idx = ["", "INDEX — line numbers refer to THIS file (sed -n 'N,Mp' MASTER_PROMPT.md)", "| LINE | ENTRY |", "|------|-------|"]
    idx += ["| %d | %s |" % (ln, t) for kind, t, ln in rows]
    # pad index to fixed length with blank-free comment lines if needed (keeps offsets exact)
    while len(idx) < idx_len - 1:
        idx.append("")
    idx.append("")
    text_lines = head + idx + body
    return text_lines

def main():
    t = time.time()
    lines = assemble()
    text = "\n".join(lines) + "\n"
    if "--check" in sys.argv:
        cur = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if cur != text:
            sys.exit("MASTER_PROMPT.md is stale — run python3 tools/build_prompt.py")
        print("OK: MASTER_PROMPT.md up to date (%d lines)" % len(lines)); return
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote %s: %d lines, %.1f MB, %.1fs" % (OUT, len(lines), len(text.encode()) / 1e6, time.time() - t))

if __name__ == "__main__":
    main()
