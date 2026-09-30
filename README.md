# DIVIDED HORIZON — Master Build Order v2

`MASTER_PROMPT.md` is the production prompt for an original, photoreal, Unreal Engine 5 open-world action game
(island-ops FPS + city crime sandbox). It is **generated** and ~100,000 lines long.

| Path | What it is |
|------|-----------|
| `MASTER_PROMPT.md` | The assembled v2 prompt (index at the top, Part 0 = Supreme Directive) |
| `prompt_src/00_part0_supreme_directive.md` | v2 audit, supersession register, Godot→UE5 table, quality bar |
| `prompt_src/legacy_parts_I-X.md` | v1 doctrine (lore, balance, campaign, scripts), patched for v2 |
| `prompt_src/11…15_*.md` | v2 hand-written brief: UE5 photoreal charter, movement/animation, combat, open world, production |
| `tools/gen_v2/*.py` | Deterministic generators for the ledgers (Sections 137–156) |
| `tools/build_prompt.py` | Assembles everything into `MASTER_PROMPT.md` (`--check` verifies it is current) |
| `tools/gen_annex.py` | Original v1 generator for legacy Part IX (output frozen in `legacy_parts_I-X.md`) |
| `tools/patch_legacy_v2*.py` | One-shot v1→v2 migration scripts, kept for auditability |

Rebuild: `python3 tools/build_prompt.py` · Verify: `python3 tools/build_prompt.py --check`

**Reading it:** no model context holds 100k lines. Load the index, Part 0 and Sections 101–136 first (~1,500 lines);
treat Sections 137–156 and legacy Parts IX–X as look-up tables (`sed -n 'N,Mp'` using the index, or `grep` by row ID).
