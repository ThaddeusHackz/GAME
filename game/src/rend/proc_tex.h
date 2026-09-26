/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — procedural texture set
   Every surface in the game is generated at load time from noise (Spec 3:
   zero external image files). Palette follows Art Direction 43.3 — warm,
   saturated, readable; no brown soup, no photoreal grunge.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_PROC_TEX_H
#define DH_PROC_TEX_H

#include "../core/dh_types.h"

typedef struct {
    int white;      /* 4×4 flat white — tint-only surfaces */
    int wood;       /* crates, docks, shelters */
    int concrete;   /* graybox platforms, city slabs */
    int metal;      /* rails, containers, ladders */
    int sand;       /* beaches */
    int rock;       /* cliffs, boulders */
    int water;      /* sea surface (tiled, alpha) */
    int bark;       /* palm trunks */
    int leaf;       /* palm fronds / canopy (alpha cutout) */
    int fabric;     /* tarps, flags, awnings */
    int asphalt;    /* Meridian City roads (M4) */
    int glow;       /* radial soft dot — particles, lights, sun disc */
    int count;
} ProcTex;

extern ProcTex proc_tex;

void proc_tex_build(void);      /* idempotent; registers textures */
void proc_tex_release(void);

#endif /* DH_PROC_TEX_H */
