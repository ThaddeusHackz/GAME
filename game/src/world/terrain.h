/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — heightfield terrain (Spec 4.1 island, 45 water line)
   One unified streaming world per act (4.3): the heightfield IS the island;
   chunks are meshed on demand and culled by the renderer.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_TERRAIN_H
#define DH_TERRAIN_H

#include "../core/dh_types.h"

#define TERRAIN_MAX_CHUNK_IDS 1024

typedef struct {
    int      res;              /* samples per side (power of two + 1 ideal) */
    float    size;             /* world metres per side */
    float   *h;                /* res*res heights, row-major [z*res+x] */
    float    sea_level;
    float    max_height;
    uint32_t seed;
    Vec3     origin;           /* world position of sample (0,0) */
    int      chunk_ids[TERRAIN_MAX_CHUNK_IDS];
    int      chunk_count;
    float    chunk_m;
    /* M4: urban paint — samples inside this XZ rect (and above paint_min_h)
       get the asphalt colour instead of the biome colour. 0 size = off. */
    float    paint_x0, paint_z0, paint_x1, paint_z1, paint_min_h;
    uint32_t paint_col;
} Terrain;

/* Generate island: fbm continents + radial falloff so the rim is beach/sea. */
void  terrain_init(Terrain *t, int res, float size, uint32_t seed);
void  terrain_free(Terrain *t);

/* Bilinear height sample — the single source of truth for ground collision. */
float terrain_height(const Terrain *t, float x, float z);
/* Central-difference normal (unit). */
Vec3  terrain_normal(const Terrain *t, float x, float z);
/* 0 = flat, 1 = cliff. Used for slide / stand / footstep-material choice. */
float terrain_steepness(const Terrain *t, float x, float z);
/* Surface material id for footstep SFX + texture blending (Spec 14/A-0185+). */
int   terrain_material(const Terrain *t, float x, float z);   /* 0 sand 1 grass 2 rock 3 mud */

/* Mesh the world into registered chunks (renderer ids). Idempotent. */
int   terrain_build_chunks(Terrain *t, float chunk_m);
void  terrain_release_chunks(Terrain *t);

/* Postcard-law helper (42.2): highest point within radius, for vista checks. */
float terrain_max_in_radius(const Terrain *t, float x, float z, float radius);

/* Carve a flat pad into the heightfield (traversal course, outpost sites,
   roads). `blend` metres of smoothstep feathering at the border so the pad
   never leaves a visible cliff seam. Call BEFORE terrain_build_chunks(). */
void  terrain_flatten(Terrain *t, Vec3 center, float rx, float rz,
                      float height, float blend);
/* Raise/lower a circular area smoothly (hills, craters, arena bowls). */
void  terrain_mound(Terrain *t, Vec3 center, float radius, float delta);

#endif /* DH_TERRAIN_H */
