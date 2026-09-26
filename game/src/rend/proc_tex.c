/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — procedural texture implementation
   ══════════════════════════════════════════════════════════════════════════ */
#include "proc_tex.h"
#include "rend.h"
#include "../core/dh_log.h"
#include <string.h>

ProcTex proc_tex;
static int s_built = 0;

/* Pack floats (0..1) into the framebuffer's 0xAABBGGRR order. */
static inline uint32_t packf(float r, float g, float b, float a) {
    int R = dh_clampi((int)(r * 255.0f + 0.5f), 0, 255);
    int G = dh_clampi((int)(g * 255.0f + 0.5f), 0, 255);
    int B = dh_clampi((int)(b * 255.0f + 0.5f), 0, 255);
    int A = dh_clampi((int)(a * 255.0f + 0.5f), 0, 255);
    return (uint32_t)((A << 24) | (B << 16) | (G << 8) | R);
}

static Texture *make(int w, int h, const char *name) {
    Texture *t = tex_new(w, h, name);
    return t;
}

static int finish(Texture *t) {
    if (!t) return -1;
    tex_build_mips(t);
    return tex_register(t);
}

/* ── individual generators ─────────────────────────────────────────────── */

static void gen_white(Texture *t) {
    for (int i = 0; i < t->w * t->h; i++) t->px[i] = 0xFFFFFFFFu;
}

/* Wood: warm ochre planks with grain and seam lines (43.3 warm palette). */
static void gen_wood(Texture *t, uint32_t seed) {
    NoiseCfg grain = { seed, 4, 0.35f, 2.1f, 0.55f };
    NoiseCfg knot  = { seed ^ 0x91, 2, 0.06f, 2.0f, 0.5f };
    int plank = t->h / 4;
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float g = noise_fbm2(&grain, (float)x * 0.18f, (float)y * 3.2f) * 0.5f + 0.5f;
            float k = noise2(&knot, (float)x, (float)y) * 0.5f + 0.5f;
            float base = 0.42f + 0.26f * g + 0.10f * k;
            /* seams */
            int seam = (y % plank) < 2;
            if (seam) base *= 0.55f;
            /* slight per-plank hue shift so boards read as separate */
            int p = (y / plank) & 1;
            float r = base * (p ? 0.94f : 1.06f);
            float gr = base * 0.66f;
            float b = base * 0.38f;
            t->px[y * t->w + x] = packf(r, gr, b, 1.0f);
        }
    }
}

/* Concrete: cool light grey with fine speckle and expansion joints. */
static void gen_concrete(Texture *t, uint32_t seed) {
    NoiseCfg fine = { seed, 3, 0.9f, 2.2f, 0.5f };
    NoiseCfg blot = { seed ^ 0x33, 2, 0.08f, 2.0f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float f = noise_fbm2(&fine, (float)x, (float)y) * 0.5f + 0.5f;
            float b = noise2(&blot, (float)x, (float)y) * 0.5f + 0.5f;
            float v = 0.62f + 0.14f * f - 0.10f * b;
            int joint = ((x % 32) < 1) || ((y % 32) < 1);
            if (joint) v *= 0.78f;
            t->px[y * t->w + x] = packf(v * 1.00f, v * 1.00f, v * 0.98f, 1.0f);
        }
    }
}

/* Metal: brushed steel, warm-grey highlights, rivet dots. */
static void gen_metal(Texture *t, uint32_t seed) {
    NoiseCfg brush = { seed, 3, 0.5f, 2.0f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float n = noise_fbm2(&brush, (float)x * 6.0f, (float)y * 0.35f) * 0.5f + 0.5f;
            float v = 0.48f + 0.20f * n;
            float rust = 0.0f;
            /* a little weathering at the corners of panels */
            if ((x % 32) < 2 || (y % 32) < 2) v *= 0.82f;
            int rx = x % 32, ry = y % 32;
            if ((rx == 4 || rx == 27) && (ry == 4 || ry == 27)) v *= 1.25f;
            float r = v * (1.0f + rust), g = v * 0.99f, b = v * 1.02f;
            t->px[y * t->w + x] = packf(r, g, b, 1.0f);
        }
    }
}

/* Sand: pale gold with ripple lines that read as beach from above. */
static void gen_sand(Texture *t, uint32_t seed) {
    NoiseCfg g = { seed, 4, 0.5f, 2.1f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float n = noise_fbm2(&g, (float)x, (float)y) * 0.5f + 0.5f;
            float ripple = 0.5f + 0.5f * sinf(((float)x + n * 7.0f) * 0.36f);
            float v = 0.78f + 0.13f * n + 0.07f * ripple;
            t->px[y * t->w + x] = packf(v * 1.00f, v * 0.93f, v * 0.72f, 1.0f);
        }
    }
}

/* Rock: warm grey strata — matches the terrain vertex palette. */
static void gen_rock(Texture *t, uint32_t seed) {
    NoiseCfg g = { seed, 5, 0.22f, 2.05f, 0.52f };
    NoiseCfg crack = { seed ^ 0x7f, 2, 0.12f, 2.0f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float n = noise_fbm2(&g, (float)x, (float)y * 1.7f) * 0.5f + 0.5f;
            float c = noise2(&crack, (float)x, (float)y) * 0.5f + 0.5f;
            float v = 0.40f + 0.30f * n;
            if (c > 0.78f) v *= 0.62f;                   /* cracks */
            t->px[y * t->w + x] = packf(v * 1.04f, v * 1.00f, v * 0.95f, 1.0f);
        }
    }
}

/* Water: teal jade with foam filaments; alpha 0.82 so the seabed shows. */
static void gen_water(Texture *t, uint32_t seed) {
    NoiseCfg a = { seed, 4, 0.14f, 2.0f, 0.55f };
    NoiseCfg b = { seed ^ 0x5a, 3, 0.31f, 2.1f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float n1 = noise_fbm2(&a, (float)x, (float)y) * 0.5f + 0.5f;
            float n2 = noise_fbm2(&b, (float)x * 1.7f, (float)y * 1.7f) * 0.5f + 0.5f;
            float crest = dh_clampf((n1 * 0.6f + n2 * 0.4f - 0.62f) * 3.4f, 0.0f, 1.0f);
            float r = 0.10f + 0.30f * n1 + 0.55f * crest;
            float g = 0.42f + 0.26f * n1 + 0.50f * crest;
            float bl = 0.48f + 0.24f * n2 + 0.48f * crest;
            float al = 0.80f + 0.18f * crest;
            t->px[y * t->w + x] = packf(r, g, bl, al);
        }
    }
}

/* Bark: vertical fibrous strips in warm brown-grey. */
static void gen_bark(Texture *t, uint32_t seed) {
    NoiseCfg g = { seed, 4, 0.25f, 2.0f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float n = noise_fbm2(&g, (float)x * 2.4f, (float)y * 0.22f) * 0.5f + 0.5f;
            float v = 0.30f + 0.34f * n;
            t->px[y * t->w + x] = packf(v * 1.10f, v * 0.86f, v * 0.62f, 1.0f);
        }
    }
}

/* Leaf: alpha cutout frond — a bright jade blob with notched edges. */
static void gen_leaf(Texture *t, uint32_t seed) {
    NoiseCfg g = { seed, 3, 0.16f, 2.0f, 0.55f };
    int cx = t->w / 2, cy = t->h / 2;
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float dx = (float)(x - cx) / (float)cx, dy = (float)(y - cy) / (float)cy;
            float d = sqrtf(dx*dx + dy*dy);
            float n = noise_fbm2(&g, (float)x, (float)y) * 0.5f + 0.5f;
            float edge = 0.72f + 0.26f * n;
            float a = d < edge ? 1.0f : 0.0f;
            if (a > 0.5f && d > edge - 0.06f) a = 0.55f;      /* soft rim */
            float v = 0.42f + 0.30f * n - 0.18f * d;
            t->px[y * t->w + x] = packf(v * 0.55f, v * 1.25f, v * 0.62f, a);
        }
    }
}

/* Fabric: woven tarp, saturated teal with stitch rows. */
static void gen_fabric(Texture *t, uint32_t seed) {
    NoiseCfg g = { seed, 2, 1.4f, 2.0f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float n = noise2(&g, (float)x, (float)y) * 0.5f + 0.5f;
            float weave = ((x + y) & 1) ? 1.0f : 0.92f;
            float v = (0.55f + 0.20f * n) * weave;
            t->px[y * t->w + x] = packf(v * 0.55f, v * 0.95f, v * 1.00f, 1.0f);
        }
    }
}

/* Asphalt: dark neutral with aggregate speckle and a lane strip. */
static void gen_asphalt(Texture *t, uint32_t seed) {
    NoiseCfg g = { seed, 4, 1.1f, 2.1f, 0.5f };
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float n = noise_fbm2(&g, (float)x, (float)y) * 0.5f + 0.5f;
            float v = 0.16f + 0.16f * n;
            float r = v, gr = v, b = v * 1.02f;
            if ((y % 32) > 14 && (y % 32) < 18 && (x % 16) < 10) { /* dashes */
                r = 0.86f; gr = 0.82f; b = 0.62f;
            }
            t->px[y * t->w + x] = packf(r, gr, b, 1.0f);
        }
    }
}

/* Glow: radial falloff dot for particles, sun disc, light halos. */
static void gen_glow(Texture *t) {
    int cx = t->w / 2, cy = t->h / 2;
    for (int y = 0; y < t->h; y++) {
        for (int x = 0; x < t->w; x++) {
            float dx = (float)(x - cx) / (float)cx, dy = (float)(y - cy) / (float)cy;
            float d = sqrtf(dx*dx + dy*dy);
            float a = dh_clampf(1.0f - d, 0.0f, 1.0f);
            a = a * a * (3.0f - 2.0f * a);
            t->px[y * t->w + x] = packf(1.0f, 1.0f, 1.0f, a);
        }
    }
}

/* ── build all ─────────────────────────────────────────────────────────── */
typedef void (*GenFn)(Texture *);

static Texture *build(int w, int h, const char *name, uint32_t seed,
                      void (*gen)(Texture*, uint32_t)) {
    Texture *t = make(w, h, name);
    if (!t) return NULL;
    gen(t, seed);
    return t;
}

#define GEN_WRAP(fn) static void fn##_w(Texture *t, uint32_t s) { (void)s; fn(t); }
GEN_WRAP(gen_white)
GEN_WRAP(gen_glow)

void proc_tex_build(void) {
    if (s_built) return;
    memset(&proc_tex, 0, sizeof(proc_tex));
    uint32_t seed = 0xD1CE5EEDu;
    Texture *t;

    t = build(4, 4, "white", seed, gen_white_w);         proc_tex.white    = finish(t);
    t = build(64, 64, "wood", seed ^ 1, gen_wood);        proc_tex.wood     = finish(t);
    t = build(64, 64, "concrete", seed ^ 2, gen_concrete);proc_tex.concrete = finish(t);
    t = build(64, 64, "metal", seed ^ 3, gen_metal);      proc_tex.metal    = finish(t);
    t = build(64, 64, "sand", seed ^ 4, gen_sand);        proc_tex.sand     = finish(t);
    t = build(64, 64, "rock", seed ^ 5, gen_rock);        proc_tex.rock     = finish(t);
    t = build(128, 128, "water", seed ^ 6, gen_water);    proc_tex.water    = finish(t);
    t = build(32, 64, "bark", seed ^ 7, gen_bark);        proc_tex.bark     = finish(t);
    t = build(64, 64, "leaf", seed ^ 8, gen_leaf);        proc_tex.leaf     = finish(t);
    t = build(64, 64, "fabric", seed ^ 9, gen_fabric);    proc_tex.fabric   = finish(t);
    t = build(64, 64, "asphalt", seed ^ 10, gen_asphalt); proc_tex.asphalt  = finish(t);
    t = build(32, 32, "glow", seed, gen_glow_w);          proc_tex.glow     = finish(t);

    proc_tex.count = tex_count();
    s_built = 1;
    DH_INFO("proc_tex", "generated 12 procedural textures (%d registered total)",
            proc_tex.count);
}

void proc_tex_release(void) {
    /* textures are owned by the registry; tex_release_all() frees them */
    memset(&proc_tex, 0, sizeof(proc_tex));
    s_built = 0;
}
