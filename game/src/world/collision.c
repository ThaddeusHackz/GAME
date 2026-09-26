/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — static collision implementation
   ══════════════════════════════════════════════════════════════════════════ */
#include "collision.h"
#include <string.h>
#include <math.h>

void obstacles_init(ObstacleSet *s) { memset(s, 0, sizeof(*s)); }

int obstacles_add(ObstacleSet *s, Vec3 min, Vec3 max, int kind) {
    if (!s || s->count >= COLLIDE_MAX) return -1;
    Obstacle *o = &s->ob[s->count];
    o->min = v3(dh_minf(min.x,max.x), dh_minf(min.y,max.y), dh_minf(min.z,max.z));
    o->max = v3(dh_maxf(min.x,max.x), dh_maxf(min.y,max.y), dh_maxf(min.z,max.z));
    o->kind = kind;
    o->id = s->count;
    return s->count++;
}
int obstacles_add_box(ObstacleSet *s, Vec3 center, Vec3 half, int kind) {
    return obstacles_add(s, v3_sub(center, half), v3_add(center, half), kind);
}

int obstacles_resolve(const ObstacleSet *s, Vec3 *pos, float radius, float height,
                      float *top_within_reach) {
    if (!s || !pos) return 0;
    int pushed = 0;
    float top = -1e9f;
    for (int i = 0; i < s->count; i++) {
        const Obstacle *o = &s->ob[i];
        if (o->kind == 2) continue;                       /* vault-only: no push */
        /* vertical band test with a little step tolerance handled by caller */
        if (pos->y + height <= o->min.y + 0.001f || pos->y >= o->max.y) continue;
        float cx = dh_clampf(pos->x, o->min.x, o->max.x);
        float cz = dh_clampf(pos->z, o->min.z, o->max.z);
        float dx = pos->x - cx, dz = pos->z - cz;
        float d2 = dx*dx + dz*dz;
        if (d2 >= radius*radius) {
            /* still horizontally near? record top for step-up if feet close */
            if (d2 < (radius + 0.35f) * (radius + 0.35f) && o->max.y > top &&
                o->max.y - pos->y < 2.3f && o->max.y > pos->y)
                top = o->max.y;
            continue;
        }
        if (o->max.y > top && o->max.y > pos->y) top = o->max.y;
        float d = sqrtf(d2);
        if (d < 1e-4f) {
            /* centre inside box: push along smallest penetration axis */
            float px1 = pos->x - o->min.x, px2 = o->max.x - pos->x;
            float pz1 = pos->z - o->min.z, pz2 = o->max.z - pos->z;
            float m = dh_minf(dh_minf(px1,px2), dh_minf(pz1,pz2));
            if (m == px1) pos->x = o->min.x - radius;
            else if (m == px2) pos->x = o->max.x + radius;
            else if (m == pz1) pos->z = o->min.z - radius;
            else pos->z = o->max.z + radius;
        } else {
            float nx = dx / d, nz = dz / d;
            pos->x = cx + nx * radius;
            pos->z = cz + nz * radius;
        }
        pushed = 1;
    }
    if (top_within_reach) *top_within_reach = top;
    return pushed;
}

int obstacles_wall_ahead(const ObstacleSet *s, Vec3 pos, Vec3 dir, float radius,
                         float height, float reach, float *top_out) {
    if (!s) return 0;
    Vec3 d = v3_norm(dir);
    if (v3_len2(d) < 0.5f) return 0;
    float best = -1e9f; int hit = 0;
    for (int i = 0; i < s->count; i++) {
        const Obstacle *o = &s->ob[i];
        if (o->max.y <= pos.y + 0.05f) continue;              /* floor, not wall */
        if (o->min.y > pos.y + reach) continue;               /* too high */
        /* Probe just ahead at ANKLE height, not chest: a chest probe cannot
           see a ledge whose top is below the chest — which is exactly the
           falling-into-a-ledge case the air mantle exists for. */
        Vec3 probe = v3_add(pos, v3_mul(d, radius + 0.45f));
        probe.y = pos.y + 0.25f;
        if (probe.x < o->min.x || probe.x > o->max.x) continue;
        if (probe.z < o->min.z || probe.z > o->max.z) continue;
        if (probe.y < o->min.y || probe.y > o->max.y) continue;
        if (o->max.y > best) best = o->max.y;
        hit = 1;
    }
    if (top_out) *top_out = best;
    return hit;
}

float obstacles_support(const ObstacleSet *s, Vec3 pos, float radius, float feet_y,
                        float step) {
    if (!s) return -1e9f;
    float best = -1e9f;
    for (int i = 0; i < s->count; i++) {
        const Obstacle *o = &s->ob[i];
        if (o->kind == 2) continue;
        if (o->max.y > feet_y + step) continue;        /* too high to step */
        if (o->max.y < feet_y - 0.6f) continue;        /* far below: not support */
        float cx = dh_clampf(pos.x, o->min.x, o->max.x);
        float cz = dh_clampf(pos.z, o->min.z, o->max.z);
        float dx = pos.x - cx, dz = pos.z - cz;
        if (dx*dx + dz*dz < radius*radius) {
            if (o->max.y > best) best = o->max.y;
        }
    }
    return best;
}

int obstacles_step_onto(const ObstacleSet *s, Vec3 *pos, float radius,
                        float top, float amount) {
    if (!s || !pos) return 0;
    int best = -1; float best_d = 1e9f;
    for (int i = 0; i < s->count; i++) {
        const Obstacle *o = &s->ob[i];
        if (o->kind == 2) continue;
        if (dh_absf(o->max.y - top) > 0.05f) continue;
        float cx = dh_clampf(pos->x, o->min.x, o->max.x);
        float cz = dh_clampf(pos->z, o->min.z, o->max.z);
        float d = v3_dist_xz(*pos, v3(cx, 0, cz));
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best < 0) return 0;
    const Obstacle *o = &s->ob[best];
    float cx = (o->min.x + o->max.x) * 0.5f;
    float cz = (o->min.z + o->max.z) * 0.5f;
    Vec3 to = v3_norm(v3(cx - pos->x, 0.0f, cz - pos->z));
    pos->x += to.x * amount;
    pos->z += to.z * amount;
    return 1;
}

/* ── ziplines ──────────────────────────────────────────────────────────── */
void zips_init(ZiplineSet *s) { if (s) s->count = 0; }

int zips_add(ZiplineSet *s, Vec3 a, Vec3 b) {
    if (!s || s->count >= ZIP_MAX) return -1;
    if (v3_dist(a, b) < 2.0f) return -1;          /* degenerate cable */
    s->v[s->count].a = a; s->v[s->count].b = b;
    return s->count++;
}

int zip_nearest(const ZiplineSet *s, Vec3 chest, float reach,
                int *out_i, float *out_s, float *out_dist) {
    if (!s) return 0;
    float best = reach; int bi = -1; float bs = 0.0f;
    for (int i = 0; i < s->count; i++) {
        Vec3 ab = v3_sub(s->v[i].b, s->v[i].a);
        float len2 = v3_dot(ab, ab);
        if (len2 < 1e-6f) continue;
        float t = v3_dot(v3_sub(chest, s->v[i].a), ab) / len2;
        t = dh_clampf(t, 0.0f, 1.0f);
        Vec3 q = v3_add(s->v[i].a, v3_mul(ab, t));
        float d = v3_dist(chest, q);
        if (d < best) { best = d; bi = i; bs = t * sqrtf(len2); }
    }
    if (bi < 0) return 0;
    if (out_i) *out_i = bi;
    if (out_s) *out_s = bs;
    if (out_dist) *out_dist = best;
    return 1;
}

/* ── rays (M2: hitscan + AI line-of-sight) ─────────────────────────────── */
/* Standard slab method vs AABB. dir must be normalized by the caller. */
static int ray_aabb_n(Vec3 o, Vec3 d, Vec3 bmin, Vec3 bmax, float tmax,
                    float *t_out, Vec3 *n_out)
{
    float t0 = 0.f, t1 = tmax;
    int axis = -1; float sign = 0.f;
    float po[3] = { o.x, o.y, o.z };
    float pd[3] = { d.x, d.y, d.z };
    float lo[3] = { bmin.x, bmin.y, bmin.z };
    float hi[3] = { bmax.x, bmax.y, bmax.z };
    for (int i = 0; i < 3; i++) {
        if (fabsf(pd[i]) < 1e-8f) {
            if (po[i] < lo[i] || po[i] > hi[i]) return 0;
            continue;
        }
        float inv = 1.f / pd[i];
        float ta = (lo[i] - po[i]) * inv;
        float tb = (hi[i] - po[i]) * inv;
        float nsign = -1.f;
        if (ta > tb) { float tt = ta; ta = tb; tb = tt; nsign = 1.f; }
        if (ta > t0) { t0 = ta; axis = i; sign = nsign * (inv < 0.f ? -1.f : 1.f); }
        if (tb < t1) t1 = tb;
        if (t0 > t1) return 0;
    }
    if (axis < 0) {
        /* origin inside the box: report a zero-distance hit, no face */
        if (t_out) *t_out = 0.f;
        if (n_out) *n_out = v3(0.f, 1.f, 0.f);
        return 1;
    }
    if (t_out) *t_out = t0;
    if (n_out) {
        Vec3 n = v3(0.f, 0.f, 0.f);
        if (axis == 0) n.x = sign; else if (axis == 1) n.y = sign; else n.z = sign;
        *n_out = n;
    }
    return 1;
}

int obstacles_ray_hit(const ObstacleSet *s, Vec3 o, Vec3 d, float tmax,
                      float *t_out, Vec3 *n_out)
{
    if (!s || !(tmax > 0.f)) return 0;
    int hit = 0; float best = tmax; Vec3 bn = v3(0.f, 1.f, 0.f);
    for (int i = 0; i < s->count; i++) {
        const Obstacle *b = &s->ob[i];
        if (b->kind != 0) continue;             /* ladders/vault volumes: bullets pass */
        float t; Vec3 n;
        if (ray_aabb_n(o, d, b->min, b->max, best, &t, &n)) {
            best = t; bn = n; hit = 1;
        }
    }
    if (hit) { if (t_out) *t_out = best; if (n_out) *n_out = bn; }
    return hit;
}

int obstacles_segment_clear(const ObstacleSet *s, Vec3 a, Vec3 b)
{
    Vec3 d = v3_sub(b, a);
    float len = v3_len(d);
    if (len < 1e-6f) return 1;
    d = v3_mul(d, 1.f / len);
    float t;
    return obstacles_ray_hit(s, a, d, len, &t, NULL) ? 0 : 1;
}
