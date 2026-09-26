/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — static collision volumes (graybox + props + buildings)
   Cylinder-vs-AABB resolution. Deliberately simple and *predictable*: an
   open-world controller that surprises the player into geometry is worse
   than one that is slightly conservative (P1, 18.4 no fall-through).
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_COLLISION_H
#define DH_COLLISION_H

#include "../core/dh_types.h"

#define COLLIDE_MAX 512

typedef struct {
    Vec3 min, max;
    int  kind;          /* 0 solid, 1 climbable face (ladder), 2 vault-only */
    int  id;
} Obstacle;

typedef struct {
    Obstacle ob[COLLIDE_MAX];
    int count;
} ObstacleSet;

void obstacles_init(ObstacleSet *s);
int  obstacles_add(ObstacleSet *s, Vec3 min, Vec3 max, int kind);
/* Box helper: centre + half extents. */
int  obstacles_add_box(ObstacleSet *s, Vec3 center, Vec3 half, int kind);

/* Horizontal push-out of a cylinder (radius r, feet at y, height h).
   Returns 1 if any push was applied. Also reports the highest obstacle top
   the cylinder is currently overlapping horizontally (for step/mantle). */
int  obstacles_resolve(const ObstacleSet *s, Vec3 *pos, float radius, float height,
                        float *top_within_reach);
/* Is there an obstacle wall ahead (dir) whose top is between feet+step_min
   and feet+reach? Used for vault/mantle decisions (6.1). */
int  obstacles_wall_ahead(const ObstacleSet *s, Vec3 pos, Vec3 dir, float radius,
                          float height, float reach, float *top_out);
/* Ground support: highest obstacle top at/below feet+step that the cylinder
   rests on (so the player can stand on crates, not just terrain). */
float obstacles_support(const ObstacleSet *s, Vec3 pos, float radius, float feet_y,
                        float step);
/* After a step-up, slide `pos` horizontally onto the volume whose top matches
   `top`, so the support query overlaps it. Returns 1 if nudged. */
int   obstacles_step_onto(const ObstacleSet *s, Vec3 *pos, float radius,
                          float top, float amount);

/* ── rays (M2 hitscan + AI line-of-sight) ─────────────────────────────────
   Nearest solid-kind AABB hit along ray o + t·d, t ∈ (0, tmax]. Writes the
   hit distance and face normal. Returns 1 on hit. (Slab method, no allocs.) */
int   obstacles_ray_hit(const ObstacleSet *s, Vec3 o, Vec3 d, float tmax,
                        float *t_out, Vec3 *n_out);
/* 1 when no solid box intersects segment a→b (AI line-of-sight against
   buildings/crates; terrain occlusion is sampled separately). */
int   obstacles_segment_clear(const ObstacleSet *s, Vec3 a, Vec3 b);

/* ── ziplines (6.1) ─────────────────────────────────────────────────────────
   A zipline is a straight cable between two anchors. The player rides it as a
   1-D arc-length parameter with gravity-along-cable acceleration and a soft
   lateral sway spring (verlet-lite, Spec 48 rope feel without the cost). */
#define ZIP_MAX 24
typedef struct { Vec3 a, b; } Zipline;
typedef struct { Zipline v[ZIP_MAX]; int count; } ZiplineSet;

void zips_init(ZiplineSet *s);
int  zips_add(ZiplineSet *s, Vec3 a, Vec3 b);
/* Nearest cable to `chest` within `reach`. Outputs cable index, arc length s
   of the closest point and the distance. Returns 1 when found. */
int  zip_nearest(const ZiplineSet *s, Vec3 chest, float reach,
                 int *out_i, float *out_s, float *out_dist);

#endif /* DH_COLLISION_H */
