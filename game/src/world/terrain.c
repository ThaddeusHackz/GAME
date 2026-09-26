/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — heightfield terrain implementation
   Art direction (43.1/43.3): jade & gold island, "no brown soup" — mud is
   ochre, rock is warm grey, foliage is three greens. Vertex colours carry
   that palette so the stylized read survives at 50 m with zero textures.
   ══════════════════════════════════════════════════════════════════════════ */
#include "terrain.h"
#include "../rend/rend.h"
#include "../core/dh_log.h"
#include <stdlib.h>
#include <string.h>

#define MAT_SAND 0
#define MAT_GRASS 1
#define MAT_ROCK 2
#define MAT_MUD  3

void terrain_init(Terrain *t, int res, float size, uint32_t seed) {
    memset(t, 0, sizeof(*t));
    if (res < 8) res = 8;
    if (size < 16.0f) size = 16.0f;
    t->res = res; t->size = size; t->seed = seed;
    t->sea_level = 0.0f;
    t->h = (float*)calloc((size_t)res * res, sizeof(float));
    if (!t->h) { DH_ERROR("terrain", "heightfield alloc failed %dx%d", res, res); return; }

    NoiseCfg cont, detail, ridge;
    cont.seed = seed;        cont.octaves = 4; cont.freq = 1.6f / size; cont.lacunarity = 2.03f; cont.gain = 0.5f;
    detail.seed = seed ^ 0x5bf03; detail.octaves = 3; detail.freq = 9.0f / size; detail.lacunarity = 2.11f; detail.gain = 0.45f;
    ridge.seed = seed ^ 0x1234; ridge.octaves = 3; ridge.freq = 3.1f / size; ridge.lacunarity = 2.0f; ridge.gain = 0.5f;

    float cx = size * 0.5f, cz = size * 0.5f;
    float rmax = size * 0.46f;
    t->max_height = -1e9f;
    for (int z = 0; z < res; z++) {
        for (int x = 0; x < res; x++) {
            float wx = (float)x / (res - 1) * size;
            float wz = (float)z / (res - 1) * size;
            /* Radial falloff: an island, not an infinite plane (4.1). */
            float dx = (wx - cx) / rmax, dz = (wz - cz) / rmax;
            float d = sqrtf(dx*dx + dz*dz);
            float fall = 1.0f - dh_clampf(d, 0.0f, 1.0f);
            fall = fall * fall * (3.0f - 2.0f * fall);        /* smoothstep rim */
            float cont_n  = noise_fbm2(&cont, wx, wz);         /* -1..1 */
            float det_n   = noise_fbm2(&detail, wx, wz);
            float rid_n   = noise_ridge2(&ridge, wx, wz);      /* 0..1 */
            /* Continent shape: raised interior, beach shelf, sea outside. */
            float hgt = (cont_n * 0.62f + 0.30f) * 46.0f * fall;
            hgt += det_n * 3.2f * fall;
            /* One mountain ridge (Cresta Sombría) so landmarks read (4.1). */
            float ridge_mask = dh_clampf(1.0f - fabsf((wx - size*0.62f) / (size*0.16f)), 0.0f, 1.0f)
                             * dh_clampf(1.0f - fabsf((wz - size*0.42f) / (size*0.30f)), 0.0f, 1.0f);
            hgt += rid_n * 58.0f * ridge_mask * fall;
            /* Beach shelf: flatten just above sea level so the shore reads. */
            if (hgt > -1.5f && hgt < 2.5f) hgt = dh_lerp(hgt, dh_lerp(-1.5f, 2.5f, (hgt + 1.5f) / 4.0f), 0.35f);
            /* Ocean shelf: beyond the island the seabed drops away. Without
               this the falloff leaves a flat plane at exactly sea level and
               the water sheet z-fights with the ground — the single most
               ugly bug in an ocean view. */
            float off = dh_clampf((d - 0.86f) / 0.22f, 0.0f, 1.0f);
            off = off * off * (3.0f - 2.0f * off);
            hgt -= 24.0f * off;
            if (hgt < -24.0f) hgt = -24.0f;                    /* sea floor clamp */
            t->h[z * res + x] = hgt;
            if (hgt > t->max_height) t->max_height = hgt;
        }
    }
    DH_INFO("terrain", "island %dx%d over %.0fm, peak %.1fm, sea %.1fm",
            res, res, size, t->max_height, t->sea_level);
}

void terrain_free(Terrain *t) {
    terrain_release_chunks(t);
    free(t->h); t->h = NULL; t->res = 0;
}

static inline float sample_at(const Terrain *t, float fx, float fz) {
    int res = t->res;
    if (!t->h) return 0.0f;
    fx = dh_clampf(fx, 0.0f, (float)(res - 1));
    fz = dh_clampf(fz, 0.0f, (float)(res - 1));
    int x0 = (int)fx, z0 = (int)fz;
    int x1 = x0 + 1 < res ? x0 + 1 : x0;
    int z1 = z0 + 1 < res ? z0 + 1 : z0;
    float tx = fx - x0, tz = fz - z0;
    float h00 = t->h[z0*res+x0], h10 = t->h[z0*res+x1];
    float h01 = t->h[z1*res+x0], h11 = t->h[z1*res+x1];
    return dh_lerp(dh_lerp(h00, h10, tx), dh_lerp(h01, h11, tx), tz);
}

float terrain_height(const Terrain *t, float x, float z) {
    if (!t->h) return 0.0f;
    float s = t->size / (float)(t->res - 1);
    return sample_at(t, (x - t->origin.x) / s, (z - t->origin.z) / s);
}

Vec3 terrain_normal(const Terrain *t, float x, float z) {
    if (!t->h) return v3(0,1,0);
    float e = t->size / (float)(t->res - 1);      /* one sample spacing */
    if (e < 0.01f) e = 0.01f;
    float hl = terrain_height(t, x - e, z);
    float hr = terrain_height(t, x + e, z);
    float hd = terrain_height(t, x, z - e);
    float hu = terrain_height(t, x, z + e);
    return v3_norm(v3(hl - hr, 2.0f * e, hd - hu));
}

float terrain_steepness(const Terrain *t, float x, float z) {
    Vec3 n = terrain_normal(t, x, z);
    return dh_clampf(1.0f - n.y, 0.0f, 1.0f);
}

int terrain_material(const Terrain *t, float x, float z) {
    float h = terrain_height(t, x, z);
    float st = terrain_steepness(t, x, z);
    if (h < t->sea_level + 1.6f) return MAT_SAND;
    if (st > 0.55f || h > t->max_height * 0.62f) return MAT_ROCK;
    /* Ochre mud along the river line (43.3: mud is ochre, never brown soup). */
    float s = t->size / (float)(t->res - 1);
    float n = noise2(&(NoiseCfg){t->seed ^ 0x77, 2, 6.0f / t->size, 2.0f, 0.5f}, x, z);
    (void)s;
    if (n > 0.55f && h < t->max_height * 0.30f) return MAT_MUD;
    return MAT_GRASS;
}

float terrain_max_in_radius(const Terrain *t, float x, float z, float radius) {
    if (!t->h) return 0.0f;
    float s = t->size / (float)(t->res - 1);
    int r = (int)(radius / s) + 1;
    float fx = (x - t->origin.x) / s, fz = (z - t->origin.z) / s;
    int cx = (int)fx, cz = (int)fz;
    float best = -1e9f;
    for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
        int xi = cx + dx, zi = cz + dz;
        if (xi < 0 || zi < 0 || xi >= t->res || zi >= t->res) continue;
        if (t->h[zi*t->res+xi] > best) best = t->h[zi*t->res+xi];
    }
    return best;
}

/* ── chunk meshing ─────────────────────────────────────────────────────── */
/* Palette (43.3): three foliage greens, warm-grey rock, pale gold sand,
   ochre mud. Chosen per-vertex from height+steepness so the painterly read
   holds at distance without a single texture fetch. */
static void mat_color(int mat, float steep, float h, float maxh, uint32_t *col) {
    uint32_t c;
    switch (mat) {
        case MAT_SAND: c = 0xFFB9E0D8u; break;                 /* pale gold sand */
        case MAT_ROCK: c = 0xFF8E8B86u; break;                 /* warm grey */
        case MAT_MUD:  c = 0xFF5FA8C0u; break;                 /* ochre */
        default: {
            /* three greens: low jade, mid leaf, high sage */
            float t = dh_clampf(h / (maxh > 1.0f ? maxh : 1.0f), 0.0f, 1.0f);
            if (t < 0.35f)      c = 0xFF4E9E5Au;              /* jade */
            else if (t < 0.70f) c = 0xFF3F8A46u;              /* leaf */
            else                c = 0xFF6FA268u;              /* sage */
            if (steep > 0.35f) c = 0xFF7E8A6Eu;               /* scrub blend */
            break;
        }
    }
    *col = c;
}

int terrain_build_chunks(Terrain *t, float chunk_m) {
    if (!t->h || t->chunk_count > 0) return t->chunk_count;
    if (chunk_m < 8.0f) chunk_m = 8.0f;
    t->chunk_m = chunk_m;
    int per_side = (int)(t->size / chunk_m);
    if (per_side < 1) per_side = 1;
    if (per_side * per_side > TERRAIN_MAX_CHUNK_IDS) {
        per_side = (int)sqrtf((float)TERRAIN_MAX_CHUNK_IDS);
        chunk_m = t->size / (float)per_side;
        t->chunk_m = chunk_m;
    }
    /* Samples per chunk edge: keep triangles ~2 m so silhouettes stay clean
       (42.3) while the whole island meshes in one pass at M1 scale. */
    int seg = (int)(chunk_m / 2.0f);
    if (seg < 4) seg = 4;
    if (seg > 48) seg = 48;
    int vper = seg + 1;
    float s = t->size / (float)(t->res - 1);

    for (int cz = 0; cz < per_side; cz++) {
        for (int cx = 0; cx < per_side; cx++) {
            float x0 = (float)cx * chunk_m, z0 = (float)cz * chunk_m;
            Mesh *m = mesh_new("terrain_chunk", vper * vper, seg * seg * 6);
            if (!m) continue;
            int vi = 0;
            for (int iz = 0; iz < vper; iz++) {
                for (int ix = 0; ix < vper; ix++) {
                    float wx = x0 + (float)ix / seg * chunk_m;
                    float wz = z0 + (float)iz / seg * chunk_m;
                    float h = terrain_height(t, wx, wz);
                    m->pos[vi] = v3(wx, h, wz);
                    m->nrm[vi] = terrain_normal(t, wx, wz);
                    m->uv[vi]  = v2(wx * 0.125f, wz * 0.125f);
                    float steep = terrain_steepness(t, wx, wz);
                    uint32_t c = 0xFFFFFFFFu;
                    mat_color(terrain_material(t, wx, wz), steep, h, t->max_height, &c);
                    if (!m->col) {
                        m->col = (uint32_t*)calloc((size_t)vper*vper, 4);
                        if (!m->col) { mesh_free(m); m = NULL; break; }
                        m->flags |= MESH_VERTEXCOL;
                    }
                    m->col[vi] = c;
                    vi++;
                }
            }
            if (!m) continue;
            int ii = 0;
            for (int iz = 0; iz < seg; iz++) {
                for (int ix = 0; ix < seg; ix++) {
                    uint32_t a = (uint32_t)(iz * vper + ix);
                    uint32_t b = a + 1;
                    uint32_t c2 = a + (uint32_t)vper;
                    uint32_t d = c2 + 1;
                    m->idx[ii++] = a; m->idx[ii++] = c2; m->idx[ii++] = b;
                    m->idx[ii++] = b; m->idx[ii++] = c2; m->idx[ii++] = d;
                }
            }
            m->flags |= MESH_TWO_SIDED;      /* terrain seen from below cliffs */
            m->tex = -1;
            (void)s;
            int id = mesh_register(m);
            if (id >= 0 && t->chunk_count < TERRAIN_MAX_CHUNK_IDS)
                t->chunk_ids[t->chunk_count++] = id;
        }
    }
    DH_INFO("terrain", "meshed %d chunks (%.0fm, %d tris each)",
            t->chunk_count, t->chunk_m,
            t->chunk_count ? mesh_get(t->chunk_ids[0])->tris : 0);
    return t->chunk_count;
}

void terrain_release_chunks(Terrain *t) {
    /* Meshes live in the global registry; M1 rebuilds the world wholesale on
       map change, so releasing means dropping our ids (registry owns memory
       until mesh_release_all at shutdown — pooled, no per-frame churn 11.3). */
    t->chunk_count = 0;
}

/* ── sculpting (course pads, outpost sites, roads) ─────────────────────── */
/* WHY these exist: procedural terrain is great for wilderness, but gameplay
   spaces — a parkour course, an outpost compound, a landing strip — must be
   flat and predictable. Editing the heightfield directly (before meshing)
   keeps ONE source of truth for both collision and rendering, so the player
   can never see ground they cannot stand on (Spec 18.4). */
void terrain_flatten(Terrain *t, Vec3 center, float rx, float rz,
                     float height, float blend) {
    if (!t || !t->h || rx <= 0.0f || rz <= 0.0f) return;
    if (blend < 0.0f) blend = 0.0f;
    float s = t->size / (float)(t->res - 1);
    float lo_x = center.x - rx - blend, hi_x = center.x + rx + blend;
    float lo_z = center.z - rz - blend, hi_z = center.z + rz + blend;
    for (int z = 0; z < t->res; z++) {
        float wz = t->origin.z + (float)z * s;
        if (wz < lo_z || wz > hi_z) continue;
        for (int x = 0; x < t->res; x++) {
            float wx = t->origin.x + (float)x * s;
            if (wx < lo_x || wx > hi_x) continue;
            /* 1.0 fully inside the pad, falling to 0 across the blend border */
            float kx = 1.0f - dh_clampf((dh_absf(wx - center.x) - rx) / (blend > 0.001f ? blend : 1.0f), 0.0f, 1.0f);
            float kz = 1.0f - dh_clampf((dh_absf(wz - center.z) - rz) / (blend > 0.001f ? blend : 1.0f), 0.0f, 1.0f);
            float w = dh_minf(kx, kz);
            w = w * w * (3.0f - 2.0f * w);          /* smoothstep feather */
            int i = z * t->res + x;
            t->h[i] = dh_lerp(t->h[i], height, w);
            if (t->h[i] > t->max_height) t->max_height = t->h[i];
        }
    }
}

void terrain_mound(Terrain *t, Vec3 center, float radius, float delta) {
    if (!t || !t->h || radius <= 0.0f) return;
    float s = t->size / (float)(t->res - 1);
    for (int z = 0; z < t->res; z++) {
        float wz = t->origin.z + (float)z * s;
        if (dh_absf(wz - center.z) > radius) continue;
        for (int x = 0; x < t->res; x++) {
            float wx = t->origin.x + (float)x * s;
            float dx = (wx - center.x) / radius, dz = (wz - center.z) / radius;
            float d = sqrtf(dx*dx + dz*dz);
            if (d > 1.0f) continue;
            float w = 1.0f - d;
            w = w * w * (3.0f - 2.0f * w);
            int i = z * t->res + x;
            t->h[i] += delta * w;
            if (t->h[i] > t->max_height) t->max_height = t->h[i];
        }
    }
}
