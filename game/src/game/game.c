/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — game application layer (M1: Traversal)
   ══════════════════════════════════════════════════════════════════════════ */
#include "game.h"
#include "../audio/audio.h"
#include "../core/save.h"
#include "../core/dh_log.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* M7 polish (polish.inl) — used by combat before the include point */
static float pol_diff_damage(void);
static float pol_diff_aim(void);
static void  pol_flocks_draw(struct Game *g);
static void  pol_apply_camera(struct Game *g);
/* M9 (poncho.inl / photo.inl) */
static void  poncho_init(struct Game *g);
static void  poncho_frame(struct Game *g, const PlatInput *in, float dt);
static void  draw_poncho(struct Game *g);
static void  poncho_draw_hud(struct Game *g);
static void  photo_frame(struct Game *g, const PlatInput *in, float dt);
static void  photo_draw_filter(struct Game *g);
static void  photo_draw_hint(struct Game *g);
static void  photo_capture(struct Game *g);
static void  legend_frame(struct Game *g, float dt);
static void  herald_draw_banner(struct Game *g);
static void  feats_frame(struct Game *g, float dt);
static void  feats_input(struct Game *g, const PlatInput *in);
static void  feats_draw_toast(struct Game *g);
static void  cob_frame(struct Game *g, float dt);
static void  cob_draw_hud(struct Game *g);
static void  herald_input(struct Game *g, const PlatInput *in);

/* ── world constants ───────────────────────────────────────────────────── */
#define WORLD_SIZE      1000.0f   /* metres per side */
#define WORLD_RES       257       /* heightfield samples per side (~3.9 m) */
#define CHUNK_M         62.5f     /* 16×16 = 256 chunks */
#define OCEAN_TILE      64.0f
#define SKY_RADIUS      1400.0f
#define CAM_NEAR        0.12f
#define CAM_FAR         2600.0f
#define WATER_UV_TILES  8.0f

/* HUD palette (0xAABBGGRR). Warm gold accent, jade for stamina — matches the
   island art direction (43.3) so the UI never fights the world. */
#define C_WHITE   0xFFFFFFFFu
#define C_GOLD    0xFF64C8F0u
#define C_JADE    0xFF4E9E5Au
#define C_TEAL    0xFFD8C84Fu
#define C_RED     0xFF4050E0u
#define C_DARK    0xB0141008u
#define C_SHADOW  0xA0000000u
#define C_DIM     0xFFC8C0B4u

/* ── traversal course table (printed in the M1 report) ─────────────────── */
static const char *COURSE_NAME[] = {
    "step_trio", "vault_crate", "mantle_wall", "gap_jump",
    "crate_tower", "walk_ramp", "swim_pool", "slide_hill", "vista_ridge",
    "zipline"
};
static const char *COURSE_TESTS[] = {
    "auto step-up chaining (0.5 m rises, limit 1.2 m)",
    "vault: 1.8 m obstacle, above step limit, below 2.2 m vault limit",
    "mantle: 2.1 m wall + landing deck behind it",
    "sprint long-jump across a 3.9 m gap at +2.1 m",
    "7.5 m drop → fall damage; roll-cancel inside 0.35 s negates it",
    "walk-up ramp (8 × 0.45 m wedges) - no stair-stepping artefacts",
    "swim: enter, submerge, breath countdown, surface and refill",
    "steep-slope slide (terrain steepness > 0.62)",
    "postcard law (42.2): ridge landmark readable from the pad",
    "E mounts the cable from the crate tower; SPACE drops, CTRL brakes"
};
#define COURSE_N ((int)(sizeof(COURSE_NAME)/sizeof(COURSE_NAME[0])))

int game_course_element_count(void) { return COURSE_N; }
const char *game_course_element_name(int i) {
    return (i >= 0 && i < COURSE_N) ? COURSE_NAME[i] : "";
}
const char *game_course_element_tests(int i) {
    return (i >= 0 && i < COURSE_N) ? COURSE_TESTS[i] : "";
}

const char *game_mode_name(GameMode m) {
    switch (m) {
    case GM_MENU:  return "MENU";
    case GM_PLAY:  return "PLAY";
    case GM_PAUSE: return "PAUSE";
    case GM_PHOTO: return "PHOTO";
    }
    return "?";
}

void game_message(Game *g, const char *fmt, ...) {
    if (!g) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(g->message, sizeof(g->message), fmt, ap);
    va_end(ap);
    g->message_t = 4.0f;
    dh_log_action("%s", g->message);
}

/* ══════════════════════════════ mesh builders ═══════════════════════════ */

static void quad_verts(Mesh *m, int *vi, int *ii, Vec3 a, Vec3 b, Vec3 c, Vec3 d,
                       Vec3 n, float us, float vs) {
    /* WINDING GUARD. The first version of this helper authored each face by
       hand and got the top/bottom faces wound opposite to the four sides, so
       mesh_finalize's winding vote picked one convention and backface-culled
       half the box — every graybox prop in the world was invisible while the
       terrain (two-sided) looked fine. Rather than trust hand-wound quads ever
       again, orient every quad against its own normal here. */
    Vec3 fn = v3_cross(v3_sub(b, a), v3_sub(c, b));
    if (v3_dot(fn, n) < 0.0f) { Vec3 t = b; b = d; d = t; }
    int base = *vi;
    m->pos[base+0] = a; m->pos[base+1] = b; m->pos[base+2] = c; m->pos[base+3] = d;
    for (int k = 0; k < 4; k++) m->nrm[base+k] = n;
    m->uv[base+0] = v2(0.0f, 0.0f);
    m->uv[base+1] = v2(us,   0.0f);
    m->uv[base+2] = v2(us,   vs);
    m->uv[base+3] = v2(0.0f, vs);
    m->idx[(*ii)++] = (uint32_t)(base+0);
    m->idx[(*ii)++] = (uint32_t)(base+1);
    m->idx[(*ii)++] = (uint32_t)(base+2);
    m->idx[(*ii)++] = (uint32_t)(base+0);
    m->idx[(*ii)++] = (uint32_t)(base+2);
    m->idx[(*ii)++] = (uint32_t)(base+3);
    *vi += 4;
}

static int build_box_mesh(void) {
    Mesh *m = mesh_new("box", 24, 36);
    if (!m) return -1;
    int vi = 0, ii = 0;
    const float H = 0.5f;
    quad_verts(m,&vi,&ii, v3(H,-H,-H), v3(H,-H,H), v3(H,H,H), v3(H,H,-H), v3(1,0,0), 1,1);
    quad_verts(m,&vi,&ii, v3(-H,-H,H), v3(-H,-H,-H), v3(-H,H,-H), v3(-H,H,H), v3(-1,0,0), 1,1);
    quad_verts(m,&vi,&ii, v3(-H,H,-H), v3(-H,H,H), v3(H,H,H), v3(H,H,-H), v3(0,1,0), 1,1);   /* top */
    quad_verts(m,&vi,&ii, v3(-H,-H,H), v3(-H,-H,-H), v3(H,-H,-H), v3(H,-H,H), v3(0,-1,0), 1,1);
    quad_verts(m,&vi,&ii, v3(-H,-H,H), v3(H,-H,H), v3(H,H,H), v3(-H,H,H), v3(0,0,1), 1,1);
    quad_verts(m,&vi,&ii, v3(H,-H,-H), v3(-H,-H,-H), v3(-H,H,-H), v3(H,H,-H), v3(0,0,-1), 1,1);
    mesh_finalize(m);
    return mesh_register(m);
}

static int build_quad_mesh(void) {
    Mesh *m = mesh_new("quad_xz", 4, 6);
    if (!m) return -1;
    int vi = 0, ii = 0;
    quad_verts(m,&vi,&ii, v3(0,0,0), v3(1,0,0), v3(1,0,1), v3(0,0,1), v3(0,1,0),
               WATER_UV_TILES, WATER_UV_TILES);
    m->flags |= MESH_TWO_SIDED;      /* water is seen from underwater too */
    mesh_finalize(m);
    return mesh_register(m);
}

/* Sky dome: unit hemisphere, vertex-colour gradient (zenith → horizon).
   Unlit + unfogged + no depth write, drawn as an ordinary opaque item so the
   depth test naturally leaves it only where nothing else painted. */
static int build_sky_mesh(void) {
    const int SEG = 20, RING = 8;
    int vcount = (SEG + 1) * (RING + 1);
    int icount = SEG * RING * 6;
    Mesh *m = mesh_new("sky_dome", vcount, icount);
    if (!m) return -1;
    m->col = (uint32_t*)calloc((size_t)vcount, 4);
    if (!m->col) { mesh_free(m); return -1; }
    m->flags |= MESH_VERTEXCOL | MESH_UNLIT | MESH_NO_FOG | MESH_TWO_SIDED;

    /* zenith: deep tropical blue. horizon: warm pale gold (43.1 jade & gold). */
    float zen[3] = { 0.24f, 0.52f, 0.86f };
    float hor[3] = { 0.93f, 0.83f, 0.66f };
    int vi = 0;
    for (int r = 0; r <= RING; r++) {
        float phi = (float)r / (float)RING * (DH_PI * 0.5f);   /* 0 = zenith */
        float y = cosf(phi), rad = sinf(phi);
        float k = (float)r / (float)RING;
        float cr = dh_lerp(zen[0], hor[0], k);
        float cg = dh_lerp(zen[1], hor[1], k);
        float cb = dh_lerp(zen[2], hor[2], k);
        for (int s = 0; s <= SEG; s++) {
            float th = (float)s / (float)SEG * DH_TAU;
            m->pos[vi] = v3(rad * cosf(th), y, rad * sinf(th));
            m->nrm[vi] = v3_neg(v3_norm(m->pos[vi]));           /* inward */
            m->uv[vi]  = v2((float)s / SEG, k);
            int R = dh_clampi((int)(cr*255.0f),0,255), G = dh_clampi((int)(cg*255.0f),0,255);
            int B = dh_clampi((int)(cb*255.0f),0,255);
            m->col[vi] = (uint32_t)(0xFF000000u | ((uint32_t)B << 16) | ((uint32_t)G << 8) | (uint32_t)R);
            vi++;
        }
    }
    int ii = 0, stride = SEG + 1;
    for (int r = 0; r < RING; r++) {
        for (int s = 0; s < SEG; s++) {
            int a = r * stride + s, b = a + 1, c = a + stride, d = c + 1;
            m->idx[ii++] = (uint32_t)a; m->idx[ii++] = (uint32_t)c; m->idx[ii++] = (uint32_t)b;
            m->idx[ii++] = (uint32_t)b; m->idx[ii++] = (uint32_t)c; m->idx[ii++] = (uint32_t)d;
        }
    }
    m->tex = -1;
    mesh_finalize(m);
    return mesh_register(m);
}

/* ══════════════════════════════ world building ══════════════════════════ */

static void prop_box(Game *g, Vec3 center, Vec3 half, int tex, uint32_t color, int kind) {
    if (g->prop_count < GAME_MAX_PROPS) {
        PropDraw *d = &g->props[g->prop_count++];
        d->model = m4_mul(m4_translate(center),
                          m4_scale(v3(half.x * 2.0f, half.y * 2.0f, half.z * 2.0f)));
        d->tex = tex;
        d->color = color;
        d->alpha = 1.0f;
        d->two_sided = 0;
        d->alpha_test = 0;
        d->center = center;
        d->radius = v3_len(half);
    } else {
        DH_WARN("game", "prop cap reached - dropping a graybox volume");
    }
    obstacles_add_box(&g->obs, center, half, kind);
}

/* Box specified by its TOP height (much easier to author a course with). */
static void prop_top_box(Game *g, float cx, float top_y, float cz,
                         float hx, float hy, float hz, int tex, uint32_t color) {
    Vec3 c = v3(cx, top_y - hy, cz);
    prop_box(g, c, v3(hx, hy, hz), tex, color, 0);
}

static void build_traversal_course(Game *g) {
    const float P = g->pad_h;
    const Vec3  C = g->course_center;
    const int   WOOD = proc_tex.wood, CONC = proc_tex.concrete, MET = proc_tex.metal;
    const uint32_t CW = 0xFFB0C8D8u;   /* cool white-gray modulate for graybox */

    /* 1 ── step trio: chained auto step-up (each rise 0.5 m < 1.2 m limit) */
    prop_top_box(g, C.x + 0.0f, P + 0.5f, C.z +  8.0f, 2.0f, 0.25f, 2.0f, CONC, CW);
    prop_top_box(g, C.x + 0.0f, P + 1.0f, C.z + 12.0f, 2.0f, 0.50f, 2.0f, CONC, CW);
    prop_top_box(g, C.x + 0.0f, P + 1.5f, C.z + 16.0f, 2.0f, 0.75f, 2.0f, CONC, CW);

    /* 2 ── vault crate ON the run line: 1.8 m — above step, below 2.2 m vault */
    prop_top_box(g, C.x + 0.0f, P + 1.8f, C.z + 6.0f, 1.5f, 0.9f, 1.5f, WOOD, 0xFFC0D0E0u);
    /* runway carrying the step trio's top height into the mantle wall, so the
       chain of steps leads somewhere instead of dropping back to the pad */
    prop_top_box(g, C.x + 0.0f, P + 1.5f, C.z + 22.0f, 4.0f, 0.75f, 6.0f, CONC, CW);

    /* 3 ── mantle wall + landing deck behind it */
    prop_top_box(g, C.x + 0.0f, P + 2.1f, C.z + 26.0f, 4.0f, 1.05f, 0.4f, CONC, CW);
    prop_top_box(g, C.x + 0.0f, P + 2.1f, C.z + 30.0f, 4.0f, 1.05f, 3.6f, CONC, CW);

    /* 4 ── gap jump: 5.0 m of empty air between two decks at +2.1 m */
    prop_top_box(g, C.x + 0.0f, P + 2.1f, C.z + 41.0f, 4.0f, 1.05f, 3.5f, CONC, CW);

    /* 5 ── crate tower to +7.5 m (fall-damage + roll-cancel test) */
    for (int i = 0; i < 5; i++) {
        float top = P + 1.5f * (float)(i + 1);
        float zz  = C.z + 10.0f - 3.0f * (float)i;
        prop_top_box(g, C.x - 14.0f, top, zz, 1.6f, 0.75f, 1.6f, WOOD, 0xFFC0D0E0u);
    }
    /* deck on top of the 5th crate (P+7.5 m) — jumping off costs health */
    prop_top_box(g, C.x - 14.0f, P + 7.7f, C.z - 2.0f, 3.0f, 0.10f, 3.0f, CONC, CW);
    prop_top_box(g, C.x - 14.0f, P + 8.5f, C.z - 4.9f, 3.0f, 0.4f, 0.1f, MET, 0xFFD8D8D8u);

    /* 6 ── walk-up ramp: 8 wedges of 0.45 m, 1.6 m deep each */
    for (int i = 0; i < 8; i++) {
        float top = P + 0.45f * (float)(i + 1);
        prop_top_box(g, C.x + 20.0f, top, C.z + 12.0f + 1.6f * (float)i,
                     3.0f, 0.225f * (float)(i + 1), 0.8f, CONC, CW);
    }

    /* 7 ── swim pool: carved into the pad, water 1.0 m below deck level */
    g->pool_center = v3(C.x + 34.0f, P - 1.0f, C.z - 14.0f);
    g->pool_rx = 13.0f; g->pool_rz = 10.0f;
    g->pool_level = P - 1.0f;
    terrain_flatten(&g->terrain, v3(g->pool_center.x, 0, g->pool_center.z),
                    g->pool_rx, g->pool_rz, P - 3.2f, 5.0f);
    /* pool rim so the water has an edge to read against */
    prop_top_box(g, g->pool_center.x, P + 0.35f, g->pool_center.z - g->pool_rz - 0.6f,
                 g->pool_rx + 1.2f, 0.35f, 0.6f, CONC, CW);
    prop_top_box(g, g->pool_center.x, P + 0.35f, g->pool_center.z + g->pool_rz + 0.6f,
                 g->pool_rx + 1.2f, 0.35f, 0.6f, CONC, CW);

    /* 8 ── slide hill (terrain_mound makes a steep flank; steepness > 0.62) */
    terrain_mound(&g->terrain, v3(C.x - 95.0f, 0, C.z + 30.0f), 16.0f, 13.0f);

    /* 9 ── vista marker post pointing at the eastern ridge */
    prop_top_box(g, C.x + 42.0f, P + 3.0f, C.z + 30.0f, 0.25f, 1.5f, 0.25f, MET, C_GOLD);

    /* 10 ── ziplines (6.1): tower→pad line and a long slide-hill→pad line */
    zips_init(&g->zips);
    zips_add(&g->zips, v3(C.x - 14.0f, P + 8.9f, C.z - 2.0f),
                       v3(C.x -  6.0f, P + 2.6f, C.z + 34.0f));
    float my_ = terrain_height(&g->terrain, C.x - 95.0f, C.z + 30.0f);
    zips_add(&g->zips, v3(C.x - 95.0f, my_ + 0.4f, C.z + 30.0f),
                       v3(C.x - 40.0f, P + 3.0f, C.z + 30.0f));

    DH_INFO("game", "traversal course built at (%.0f,%.0f) deck %.1f m, %d props, %d zips",
            C.x, C.z, P, g->prop_count, g->zips.count);
}

/* ══════════════════════════════ M2: combat ═══════════════════════════════
   Arena, loadout, hitscan, enemy↔player damage flow, pickups, tracers/FX.
   Design rule (§68): the player should never wonder "did that hit?" — every
   shot answers with a tracer, an impact, a hitmarker or a corpse.          */

static void pickup_add(Game *g, Vec3 pos, int kind, int ammo, float amount)
{
    for (int i = 0; i < PICKUP_MAX; i++) {
        if (g->pickups.v[i].live) continue;
        Pickup *k = &g->pickups.v[i];
        k->pos = pos; k->kind = kind; k->ammo = ammo;
        k->amount = amount; k->live = 1; k->bob = rng_f(&g->rng) * 6.28f;
        if (i >= g->pickups.count) g->pickups.count = i + 1;
        return;
    }
}

static void tracer_add(Game *g, Vec3 a, Vec3 b, uint32_t color)
{
    for (int i = 0; i < TRACER_MAX; i++) {
        if (g->tracers.v[i].life <= 0.f) {
            g->tracers.v[i].a = a; g->tracers.v[i].b = b;
            g->tracers.v[i].life = 0.07f;
            g->tracers.v[i].color = color;
            if (i >= g->tracers.count) g->tracers.count = i + 1;
            return;
        }
    }
}

static void fx_spawn(Game *g, Vec3 pos, Vec3 dir, int n, uint32_t color,
                     float speed, float life, int kind)
{
    for (int i = 0; i < n && g->fx_count < (int)(sizeof g->fx / sizeof g->fx[0]); i++) {
        Particle *pt = &g->fx[g->fx_count++];
        pt->pos = pos;
        pt->vel = v3_add(v3_mul(dir, speed * (0.4f + rng_f(&g->rng))),
                         v3(rng_range(&g->rng, -1.f, 1.f),
                            rng_range(&g->rng, 0.2f, 2.f),
                            rng_range(&g->rng, -1.f, 1.f)));
        pt->vel = v3_mul(pt->vel, speed * 0.5f);
        pt->life = pt->max_life = life * (0.6f + 0.6f * rng_f(&g->rng));
        pt->size = 0.05f + 0.05f * rng_f(&g->rng);
        pt->color = color;
        pt->kind = kind;
    }
}

/* Public FX wrappers (M4): city.c and future acts reuse the same helpers. */
void game_fx(Game *g, Vec3 pos, Vec3 dir, int n, uint32_t color,
             float speed, float life, int kind) {
    fx_spawn(g, pos, dir, n, color, speed, life, kind);
}
void game_tracer(Game *g, Vec3 a, Vec3 b, uint32_t color) {
    tracer_add(g, a, b, color);
}

static void fx_update(Game *g, float dt)
{
    for (int i = 0; i < g->fx_count; ) {
        Particle *pt = &g->fx[i];
        pt->life -= dt;
        if (pt->life <= 0.f) { g->fx[i] = g->fx[--g->fx_count]; continue; }
        pt->vel.y -= 14.f * dt;
        pt->pos = v3_add(pt->pos, v3_mul(pt->vel, dt));
        i++;
    }
}

/* ── the combat arena: a flat yard east of the traversal course with crates,
   low walls and two platforms — 30-hostile DoD playground (§16 M2) ─────── */
static void build_combat_arena(Game *g)
{
    const Vec3 A = v3(440.0f, 0.0f, 630.0f);
    const int WOOD = proc_tex.wood, CONC = proc_tex.concrete;
    const uint32_t CW = 0xFFB0C8D8u;
    float h = terrain_height(&g->terrain, A.x, A.z);
    terrain_flatten(&g->terrain, A, 46.0f, 42.0f, h, 14.0f);
    g->arena_center = v3(A.x, h, A.z);

    /* cover crates (vault-height: 2.2 m tops are climbable — M1 verbs work here) */
    static const float cx[] = { -14, -6, 3, 11, 18, -18, -9, 0, 8, 15, -3, 6 };
    static const float cz[] = { -10, 6, -14, 4, -6, 12, 16, 10, 18, 12, -20, -8 };
    for (size_t i = 0; i < sizeof cx / sizeof cx[0]; i++)
        prop_top_box(g, A.x + cx[i], h + 2.2f, A.z + cz[i], 1.1f, 1.1f, 1.1f,
                     WOOD, 0xFFC0D0E0u);
    /* low walls (crouch-cover: 1.2 m tops hide a crouched player) */
    prop_top_box(g, A.x - 20.f, h + 1.2f, A.z - 2.f, 5.0f, 0.6f, 0.4f, CONC, CW);
    prop_top_box(g, A.x + 20.f, h + 1.2f, A.z + 2.f, 5.0f, 0.6f, 0.4f, CONC, CW);
    prop_top_box(g, A.x - 2.f, h + 1.2f, A.z - 22.f, 0.4f, 0.6f, 5.0f, CONC, CW);
    prop_top_box(g, A.x + 2.f, h + 1.2f, A.z + 22.f, 0.4f, 0.6f, 5.0f, CONC, CW);
    /* two raised platforms with crate stairs (height advantage language) */
    prop_top_box(g, A.x - 30.f, h + 3.6f, A.z - 16.f, 3.0f, 0.3f, 3.0f, CONC, CW);
    prop_top_box(g, A.x - 30.f, h + 1.2f, A.z - 10.5f, 1.2f, 0.6f, 1.2f, WOOD, 0xFFC0D0E0u);
    prop_top_box(g, A.x - 30.f, h + 2.4f, A.z - 13.f, 1.2f, 0.6f, 1.2f, WOOD, 0xFFC0D0E0u);
    prop_top_box(g, A.x + 30.f, h + 3.6f, A.z + 16.f, 3.0f, 0.3f, 3.0f, CONC, CW);
    prop_top_box(g, A.x + 30.f, h + 1.2f, A.z + 10.5f, 1.2f, 0.6f, 1.2f, WOOD, 0xFFC0D0E0u);
    prop_top_box(g, A.x + 30.f, h + 2.4f, A.z + 13.f, 1.2f, 0.6f, 1.2f, WOOD, 0xFFC0D0E0u);

    DH_INFO("game", "combat arena built at (%.0f,%.0f) deck %.1f m", A.x, A.z, h);
}

/* ══════════════════════════════════════════════════════════════════════════
   M3 · ISLAND VERTICAL SLICE (Spec 16 DoD)
   One 1 km² island block: beach + jungle + one full stealth-capable outpost
   (Punta Quemada) + one climbable signal mast + two wildlife species + the
   24-minute day/night cycle wired above. Everything authored from boxes and
   the procedural texture set — zero external assets (Spec 3).
   ══════════════════════════════════════════════════════════════════════════ */
static void game_give_weapon(Game *g, int slot, const char *id, int fill);

/* Draw-only box (no collision) — canopies, flags, railings, scenery. */
static void prop_visual(Game *g, Vec3 center, Vec3 half, int tex, uint32_t color,
                        int two_sided, int alpha_test) {
    if (g->prop_count >= GAME_MAX_PROPS) {
        DH_WARN("game", "prop cap reached - dropping scenery");
        return;
    }
    PropDraw *d = &g->props[g->prop_count++];
    d->model = m4_mul(m4_translate(center),
                      m4_scale(v3(half.x * 2.0f, half.y * 2.0f, half.z * 2.0f)));
    d->tex = tex;
    d->color = color;
    d->alpha = 1.0f;
    d->two_sided = two_sided;
    d->alpha_test = alpha_test;
    d->center = center;
    d->radius = v3_len(half);
}

/* Is (x,z) inside a reserved flat site? Keeps jungle off the pads. */
static int veg_excluded(Game *g, float x, float z) {
    if (fabsf(x - g->course_center.x) < 92.f && fabsf(z - g->course_center.z) < 76.f) return 1;
    if (fabsf(x - 150.f) < 78.f && fabsf(z - 620.f) < 66.f) return 1;         /* landing beach */
    if (fabsf(x - g->arena_center.x) < 72.f && fabsf(z - g->arena_center.z) < 66.f) return 1;
    /* hug the flattened pads (52x40 m outpost, 16x16 m mast) with a thin
       margin so the jungle frames the sites instead of balding around them */
    if (g->outpost.built &&
        fabsf(x - g->outpost.center.x) < g->outpost.radius + 6.f &&
        fabsf(z - g->outpost.center.z) < 26.f) return 1;
    if (g->outpost.built &&
        fabsf(x - g->outpost.mast_pos.x) < 12.f &&
        fabsf(z - g->outpost.mast_pos.z) < 12.f) return 1;
    return 0;
}

/* Jungle + beach scatter: palms on the low ring, hardwoods inland, bushes
   everywhere green. Deterministic from g->rng so CI screenshots reproduce. */
static void build_vegetation(Game *g) {
    const int BARK = proc_tex.bark, LEAF = proc_tex.leaf;
    const uint32_t TRUNK = 0xFF4E6E96u;        /* warm gray-brown modulate */
    const uint32_t PALM  = 0xFF3E9C50u;        /* jade fronds */
    const uint32_t HARD  = 0xFF2E7C34u;        /* deep jungle green */
    const uint32_t BUSH  = 0xFF368C40u;
    Rng *r = &g->rng;
    int palms = 0, trees = 0, bushes = 0;

    /* Foliage scales with the quality preset (Spec 15.3: Low 0.35 → Ultra 1.0)
       so the Low draw-call budget is met by spawning less, not just by cull. */
    float dens = dh_clampf(settings()->foliage_density, 0.1f, 1.f);
    int max_palms = (int)(56.f * dens);
    int max_trees = (int)(54.f * dens);
    int max_bush  = (int)(90.f * dens);

    for (int i = 0; i < 420 && palms < max_palms; i++) {
        float ang = rng_f(r) * 6.2831853f;
        float rad = 150.f + rng_f(r) * 260.f;
        float x = 500.f + sinf(ang) * rad, z = 500.f + cosf(ang) * rad;
        float h = terrain_height(&g->terrain, x, z);
        if (h < 1.2f || h > 9.f) continue;                 /* beach/shelf band */
        if (terrain_steepness(&g->terrain, x, z) > 0.4f) continue;
        if (veg_excluded(g, x, z)) continue;
        float th = 4.2f + rng_f(r) * 2.6f;
        prop_box(g, v3(x, h + th * 0.5f, z), v3(0.18f, th * 0.5f, 0.18f), BARK, TRUNK, 0);
        Vec3 top = v3(x, h + th, z);
        prop_visual(g, v3_add(top, v3(0.f, 0.35f, 0.f)), v3(2.3f, 0.32f, 2.3f), LEAF, PALM, 1, 1);
        prop_visual(g, v3_add(top, v3(0.f, 0.85f, 0.f)), v3(1.4f, 0.26f, 1.4f), LEAF, PALM, 1, 1);
        palms++;
    }
    for (int i = 0; i < 520 && trees < max_trees; i++) {
        float ang = rng_f(r) * 6.2831853f;
        float rad = 90.f + rng_f(r) * 300.f;
        float x = 500.f + sinf(ang) * rad, z = 500.f + cosf(ang) * rad;
        float h = terrain_height(&g->terrain, x, z);
        if (h < 5.f || h > 34.f) continue;                 /* inland jungle band */
        if (terrain_steepness(&g->terrain, x, z) > 0.5f) continue;
        if (veg_excluded(g, x, z)) continue;
        float th = 5.0f + rng_f(r) * 3.5f;
        prop_box(g, v3(x, h + th * 0.5f, z), v3(0.28f, th * 0.5f, 0.28f), BARK, TRUNK, 0);
        float cr = 1.9f + rng_f(r) * 1.3f;
        prop_visual(g, v3(x, h + th + cr * 0.4f, z), v3(cr, cr * 0.62f, cr), LEAF, HARD, 1, 1);
        prop_visual(g, v3(x + cr * 0.4f, h + th - cr * 0.1f, z - cr * 0.3f),
                    v3(cr * 0.6f, cr * 0.4f, cr * 0.6f), LEAF, HARD, 1, 1);
        trees++;
    }
    for (int i = 0; i < 700 && bushes < max_bush; i++) {
        float ang = rng_f(r) * 6.2831853f;
        float rad = 70.f + rng_f(r) * 360.f;
        float x = 500.f + sinf(ang) * rad, z = 500.f + cosf(ang) * rad;
        float h = terrain_height(&g->terrain, x, z);
        if (h < 2.f || h > 30.f) continue;
        if (veg_excluded(g, x, z)) continue;
        float br = 0.7f + rng_f(r) * 0.9f;
        prop_visual(g, v3(x, h + br * 0.4f, z), v3(br, br * 0.5f, br), LEAF, BUSH, 1, 1);
        bushes++;
    }
    DH_INFO("game", "vegetation: %d palms, %d hardwoods, %d bushes", palms, trees, bushes);
}

/* Pick a flat, dry, far-from-the-course site for the outpost. Deterministic
   candidate list so the slice layout is stable across runs. */
static Vec3 find_outpost_site(Game *g, float *h_out) {
    static const float cand[][2] = {
        { 250.f, 790.f }, { 770.f, 720.f }, { 800.f, 300.f },
        { 250.f, 300.f }, { 520.f, 850.f },
    };
    for (size_t i = 0; i < sizeof cand / sizeof cand[0]; i++) {
        float x = cand[i][0], z = cand[i][1];
        float h = terrain_height(&g->terrain, x, z);
        if (h < 2.5f || h > 12.f) continue;
        if (terrain_steepness(&g->terrain, x, z) > 0.22f) continue;
        if (v3_dist_xz(v3(x, 0, z), g->course_center) < 150.f) continue;
        if (v3_dist_xz(v3(x, 0, z), g->arena_center) < 130.f) continue;
        *h_out = h;
        return v3(x, h, z);
    }
    /* fallback: never happens with the shipping seed, but stay honest */
    *h_out = terrain_height(&g->terrain, 250.f, 790.f);
    return v3(250.f, *h_out, 790.f);
}

/* Punta Quemada — difficulty 1 (§28.2: 8 enemies, alarm 40 m, 2 waves of 3).
   Layout language: sandbag perimeter with a south gate + west gap, hut with a
   roof you can take, a watchtower with crate stairs (M1 verbs only), cover
   clusters, a destructible alarm box (§52: 150 hp) and a flag pole. */
static void build_outpost(Game *g) {
    Outpost *o = &g->outpost;
    memset(o, 0, sizeof(*o));
    float h;
    Vec3 C = find_outpost_site(g, &h);
    terrain_flatten(&g->terrain, C, 26.f, 20.f, h, 10.f);
    /* dirt approach road from the south so the site reads from the ridge */
    terrain_flatten(&g->terrain, v3(C.x, h, C.z + 40.f), 5.f, 22.f, h, 8.f);
    const float P = h;

    o->built = 1;
    dh_strcpy_safe(o->name, sizeof o->name, "PUNTA QUEMADA");
    o->difficulty = 1;
    o->center = v3(C.x, P, C.z);
    o->radius = 26.f;
    o->pad_h = P;

    const int WOOD = proc_tex.wood, CONC = proc_tex.concrete, MET = proc_tex.metal;
    const uint32_t BAGS = 0xFF6EA0B4u;         /* sandbag tan */
    const uint32_t CW   = 0xFFB0C8D8u;
    const uint32_t WOODC= 0xFFC0D0E0u;

    /* perimeter sandbags (1.1 m — crouch cover, §37) with gate + west gap */
    prop_top_box(g, C.x - 14.f, P + 1.1f, C.z + 20.f, 10.f, 0.55f, 0.4f, CONC, BAGS);
    prop_top_box(g, C.x + 14.f, P + 1.1f, C.z + 20.f, 10.f, 0.55f, 0.4f, CONC, BAGS);
    prop_top_box(g, C.x,        P + 1.1f, C.z - 20.f, 24.f, 0.55f, 0.4f, CONC, BAGS);
    prop_top_box(g, C.x - 24.f, P + 1.1f, C.z - 12.f, 0.4f, 0.55f, 8.f,  CONC, BAGS);
    prop_top_box(g, C.x - 24.f, P + 1.1f, C.z + 11.f, 0.4f, 0.55f, 9.f,  CONC, BAGS);
    prop_top_box(g, C.x + 24.f, P + 1.1f, C.z,        0.4f, 0.55f, 20.f, CONC, BAGS);

    /* main hut (west yard): walls + door gap + flat roof you can take */
    prop_top_box(g, C.x - 13.f, P + 2.6f, C.z - 11.6f, 4.6f, 1.3f, 0.3f, CONC, CW);
    prop_top_box(g, C.x - 18.2f,P + 2.6f, C.z - 8.f,   0.3f, 1.3f, 3.6f, CONC, CW);
    prop_top_box(g, C.x - 7.8f, P + 2.6f, C.z - 8.f,   0.3f, 1.3f, 3.6f, CONC, CW);
    prop_top_box(g, C.x - 16.1f,P + 2.6f, C.z - 4.4f,  2.4f, 1.3f, 0.3f, CONC, CW);
    prop_top_box(g, C.x - 10.6f,P + 2.6f, C.z - 4.4f,  2.1f, 1.3f, 0.3f, CONC, CW);
    prop_top_box(g, C.x - 13.f, P + 2.9f, C.z - 8.f,   5.2f, 0.15f, 4.2f, CONC, 0xFF9AA8B8u);
    /* crate step to the roof (M1 vault chain: 1.1 → 2.9 top) */
    prop_top_box(g, C.x - 8.9f, P + 1.1f, C.z - 4.0f,  0.9f, 0.55f, 0.9f, WOOD, WOODC);

    /* watchtower (NE): legs + deck at 5.2 m + crate stairs 1.1/2.5/3.9 */
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            prop_box(g, v3(C.x + 15.f + sx * 1.4f, P + 2.6f, C.z - 11.f + sz * 1.4f),
                     v3(0.16f, 2.6f, 0.16f), WOOD, WOODC, 0);
    prop_top_box(g, C.x + 15.f, P + 5.2f, C.z - 11.f,  2.1f, 0.2f, 2.1f, WOOD, WOODC);
    for (int i = 0; i < 4; i++) {                     /* railing, draw-only */
        float sx = (i == 0) ? -2.1f : (i == 1) ? 2.1f : 0.f;
        float sz = (i == 2) ? -2.1f : (i == 3) ? 2.1f : 0.f;
        prop_visual(g, v3(C.x + 15.f + sx, P + 5.65f, C.z - 11.f + sz),
                    v3(sx == 0.f ? 2.1f : 0.07f, 0.25f, sz == 0.f ? 0.07f : 2.1f),
                    WOOD, WOODC, 0, 0);
    }
    prop_top_box(g, C.x + 11.6f, P + 1.1f, C.z - 8.2f, 0.9f, 0.55f, 0.9f, WOOD, WOODC);
    prop_top_box(g, C.x + 12.9f, P + 2.5f, C.z - 9.4f, 0.9f, 0.7f,  0.9f, WOOD, WOODC);
    prop_top_box(g, C.x + 14.0f, P + 3.9f, C.z - 10.6f,0.9f, 0.7f,  0.9f, WOOD, WOODC);

    /* alarm box on the hut's south wall (§52: shoot it, 150 hp) — draw-only;
       bullet tests live in combat_hitscan so it can take damage. */
    o->alarm_pos = v3(C.x - 11.4f, P + 1.9f, C.z - 4.1f);
    o->alarm_hp = 150.f;
    prop_visual(g, o->alarm_pos, v3(0.35f, 0.4f, 0.22f), MET, 0xFF3030E0u, 0, 0);

    /* flag pole at the yard centre (flag cloth itself is drawn per-frame so
       it can rise when the outpost is liberated) */
    o->flag_pos = v3(C.x + 5.f, P, C.z + 2.f);
    prop_top_box(g, C.x + 5.f, P + 7.f, C.z + 2.f, 0.09f, 3.5f, 0.09f, MET, 0xFFB0B4B8u);

    /* yard cover: crates + barrels */
    static const float ccx[] = { -2.f, -3.6f, 9.f, 10.4f, -1.f, 18.f };
    static const float ccz[] = { 9.f, 10.2f, 5.f, 6.2f, -14.f, 2.f };
    for (size_t i = 0; i < sizeof ccx / sizeof ccx[0]; i++)
        prop_top_box(g, C.x + ccx[i], P + 1.2f, C.z + ccz[i], 0.8f, 0.6f, 0.8f, WOOD, WOODC);
    prop_top_box(g, C.x + 1.f,  P + 1.1f, C.z - 2.f, 0.4f, 0.55f, 0.4f, MET, 0xFF4E5A64u);
    prop_top_box(g, C.x + 2.f,  P + 1.1f, C.z - 2.6f,0.4f, 0.55f, 0.4f, MET, 0xFF4E5A64u);

    /* reinforcement spawn points: south road + west track */
    o->reinforce[0] = v3(C.x + 2.f, terrain_height(&g->terrain, C.x + 2.f, C.z + 52.f), C.z + 52.f);
    o->reinforce[1] = v3(C.x - 44.f, terrain_height(&g->terrain, C.x - 44.f, C.z + 12.f), C.z + 12.f);

    /* garrison blueprint (8): 5 grunts, 2 bruisers, 1 officer — spawned by
       outpost_spawn_garrison() so the arena dev-mode can rebuild them. */
    #define GSLOT(px, pz, ar, ax, az, bx, bz) do {                         \
        GarrisonSlot *gs = &o->garrison[o->garrison_n++];                  \
        gs->pos = v3(C.x + (px), P, C.z + (pz));                           \
        gs->patrol_a = v3(C.x + (ax), P, C.z + (az));                      \
        gs->patrol_b = v3(C.x + (bx), P, C.z + (bz));                      \
        gs->arch = (ar);                                                   \
    } while (0)
    GSLOT(-2.5f, 19.f, EN_GRUNT,   -6.f, 19.f,   2.f, 19.f);   /* south gate L */
    GSLOT( 2.5f, 19.f, EN_GRUNT,    2.f, 19.f,  10.f, 17.f);   /* south gate R */
    GSLOT(-2.f,  8.f,  EN_GRUNT,   -2.f,  8.f,  10.f, -6.f);   /* yard roamer */
    GSLOT(-20.f, 12.f, EN_BRUISER,-20.f, 12.f, -20.f, -14.f);  /* west wall */
    GSLOT( 18.f, 8.f,  EN_GRUNT,   18.f, 8.f,  20.f, -8.f);    /* east yard */
    GSLOT(-13.f,-6.5f, EN_BRUISER,-13.f,-6.5f,-10.f,-9.5f);    /* hut guard */
    GSLOT( 2.f, -4.f,  EN_OFFICER,  2.f, -4.f, -4.f,  4.f);    /* officer */
    #undef GSLOT
    /* tower sentry stands ON the deck (obstacle support keeps him up there) */
    {
        GarrisonSlot *s = &o->garrison[o->garrison_n++];
        s->pos = v3(C.x + 15.f, P + 5.4f, C.z - 11.f);
        s->patrol_a = s->pos;
        s->patrol_b = v3(C.x + 15.4f, P + 5.4f, C.z - 11.4f);
        s->arch = EN_GRUNT;
    }

    DH_INFO("game", "outpost %s built at (%.0f,%.0f) pad %.1f m, %d garrison slots",
            o->name, C.x, C.z, P, o->garrison_n);
}

/* Re-spawn the authored garrison (faction 2 = outpost). Called at world build
   and whenever the arena dev-mode wipes the enemy set. */
static void outpost_spawn_garrison(Game *g) {
    Outpost *o = &g->outpost;
    if (!o->built || o->captured) return;
    for (int i = 0; i < o->garrison_n; i++) {
        const GarrisonSlot *s = &o->garrison[i];
        enemies_spawn(&g->enemies, s->pos, s->arch, 2, s->patrol_a, s->patrol_b);
    }
    o->alarm = 0; o->alarm_t = 0.f; o->combat_seen_t = 0.f; o->waves_spawned = 0;
    o->capture_t = 0.f; o->flag_raise = 0.f;
}

/* Cresta Sombría signal mast (§52): find the ridge peak, flatten a pad, build
   a 20 m lattice tower whose zig-zag rungs (0.62 m rises < 1.2 m step limit)
   let the player WALK to the top — no new verbs, pure M1 traversal. */
static void build_signal_mast(Game *g) {
    Outpost *o = &g->outpost;
    /* ridge peak search around the authored ridge mask (terrain.c) */
    float best_h = -1e9f; Vec3 M = v3(620.f, 0.f, 420.f);
    for (float x = 540.f; x <= 700.f; x += 8.f)
        for (float z = 330.f; z <= 510.f; z += 8.f) {
            float h = terrain_height(&g->terrain, x, z);
            if (h > best_h) { best_h = h; M = v3(x, h, z); }
        }
    terrain_flatten(&g->terrain, M, 8.f, 8.f, best_h, 6.f);
    const float P = best_h;
    o->mast_pos = v3(M.x, P, M.z);
    o->mast_top_y = P + 20.4f;

    const int MET = proc_tex.metal;
    const uint32_t MG = 0xFFB0B4B8u;
    /* four corner legs */
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            prop_box(g, v3(M.x + sx * 1.9f, P + 10.f, M.z + sz * 1.9f),
                     v3(0.18f, 10.f, 0.18f), MET, MG, 0);
    /* spiral rungs: 32 steps, 0.62 m rise each — walk-to-the-top ladder */
    for (int i = 0; i < 32; i++) {
        float th = (float)i * 0.62f;
        float ry = P + 0.62f * (float)(i + 1) - 0.31f;
        Vec3 rp = v3(M.x + sinf(th) * 1.15f, ry, M.z + cosf(th) * 1.15f);
        prop_box(g, rp, v3(0.75f, 0.14f, 0.75f), MET, MG, 0);
        if ((i & 1) == 0) {                        /* cross-brace, draw-only */
            prop_visual(g, v3(M.x, ry, M.z), v3(1.7f, 0.06f, 0.06f), MET, MG, 0, 0);
        }
    }
    /* top platform + railing + antenna + red beacon */
    prop_top_box(g, M.x, P + 20.4f, M.z, 2.6f, 0.2f, 2.6f, MET, MG);
    for (int i = 0; i < 4; i++) {
        float sx = (i == 0) ? -2.6f : (i == 1) ? 2.6f : 0.f;
        float sz = (i == 2) ? -2.6f : (i == 3) ? 2.6f : 0.f;
        prop_visual(g, v3(M.x + sx, P + 20.95f, M.z + sz),
                    v3(sx == 0.f ? 2.6f : 0.06f, 0.35f, sz == 0.f ? 0.06f : 2.6f),
                    MET, MG, 0, 0);
    }
    prop_visual(g, v3(M.x, P + 24.5f, M.z), v3(0.09f, 4.f, 0.09f), MET, MG, 0, 0);
    prop_visual(g, v3(M.x, P + 28.6f, M.z), v3(0.22f, 0.22f, 0.22f),
                proc_tex.glow, 0xFF2020F0u, 1, 0);
    DH_INFO("game", "signal mast built at (%.0f,%.0f) peak %.1f m, top %.1f m",
            M.x, M.z, P, o->mast_top_y);
}

/* Two island species (§4.2): venado (deer) + jabalí (boar). Placed in the
   jungle band away from authored sites; simulated only near the player. */
static void spawn_wildlife(Game *g) {
    Rng *r = &g->rng;
    g->critter_count = 0;
    for (int i = 0; i < 400 && g->critter_count < CRITTER_MAX; i++) {
        float ang = rng_f(r) * 6.2831853f;
        float rad = 130.f + rng_f(r) * 250.f;
        float x = 500.f + sinf(ang) * rad, z = 500.f + cosf(ang) * rad;
        float h = terrain_height(&g->terrain, x, z);
        if (h < 2.5f || h > 32.f) continue;
        if (terrain_steepness(&g->terrain, x, z) > 0.35f) continue;
        if (veg_excluded(g, x, z)) continue;
        Critter *c = &g->critters[g->critter_count++];
        memset(c, 0, sizeof *c);
        c->species = (g->critter_count & 1) ? 0 : 1;      /* alternate deer/boar */
        c->pos = v3(x, h, z);
        c->home = c->pos;
        c->goal = c->pos;
        c->yaw = rng_f(r) * 6.2831853f;
        c->state = 0;
        c->state_t = rng_f(r) * 4.f;
    }
    DH_INFO("game", "wildlife: %d critters (venado/jabalí)", g->critter_count);
}

/* Fog-of-war (§52): coarse 64×64 mask over the 1 km island. Cells start
   unexplored; proximity reveals; syncing the signal mast maps the island. */
static void fog_reveal(Game *g, float x, float z, float radius) {
    float cell = WORLD_SIZE / (float)MAPW;
    int c0 = dh_clampi((int)((x - radius) / cell), 0, MAPW - 1);
    int c1 = dh_clampi((int)((x + radius) / cell), 0, MAPW - 1);
    int r0 = dh_clampi((int)((z - radius) / cell), 0, MAPW - 1);
    int r1 = dh_clampi((int)((z + radius) / cell), 0, MAPW - 1);
    for (int rz = r0; rz <= r1; rz++)
        for (int cx = c0; cx <= c1; cx++) {
            float wx = (cx + 0.5f) * cell, wz = (rz + 0.5f) * cell;
            float dx = wx - x, dz = wz - z;
            if (dx * dx + dz * dz <= radius * radius && g->fog[rz * MAPW + cx] < 1)
                g->fog[rz * MAPW + cx] = 1;
        }
}

static void fog_sync_all(Game *g) {
    for (int i = 0; i < MAPW * MAPW; i++)
        if (g->fog[i] < 2) g->fog[i] = 2;
}

/* Pre-colour every map cell from the heightfield (water/sand/grass/rock +
   relief shading) so the minimap draws cached colours, never terrain reads. */
static void build_minimap_cache(Game *g) {
    float cell = WORLD_SIZE / (float)MAPW;
    float maxh = g->terrain.max_height > 1.f ? g->terrain.max_height : 1.f;
    for (int rz = 0; rz < MAPW; rz++)
        for (int cx = 0; cx < MAPW; cx++) {
            float wx = (cx + 0.5f) * cell, wz = (rz + 0.5f) * cell;
            float h = terrain_height(&g->terrain, wx, wz);
            float st = terrain_steepness(&g->terrain, wx, wz);
            float r, gg, b;
            if (h < -1.2f)      { r = 16.f;  gg = 54.f;  b = 96.f;  }   /* deep sea */
            else if (h < 0.6f)  { r = 44.f;  gg = 112.f; b = 142.f; }   /* shallows */
            else if (st > 0.55f || h > 34.f) { r = 118.f; gg = 114.f; b = 108.f; } /* rock */
            else if (h < 2.6f)  { r = 222.f; gg = 204.f; b = 150.f; }   /* sand */
            else                { r = 78.f;  gg = 132.f; b = 62.f;  }   /* jungle */
            float shade = 0.72f + 0.45f * dh_clampf(h / maxh, 0.f, 1.f);
            uint32_t R = (uint32_t)dh_clampi((int)(r * shade), 0, 255);
            uint32_t G = (uint32_t)dh_clampi((int)(gg * shade), 0, 255);
            uint32_t B = (uint32_t)dh_clampi((int)(b * shade), 0, 255);
            g->map_cell[rz * MAPW + cx] = 0xFF000000u | (B << 16) | (G << 8) | R;
        }
}

/* Orchestrator — called from game_init BEFORE terrain_build_chunks (the pads
   must exist in the heightfield before meshing). */
static void sys_build_island_extras(Game *g);   /* M5, systems.inl */
static void sys_build_city_extras(Game *g);
static void sys_island_restore(Game *g);
static void build_island(Game *g) {
    memset(g->fog, 0, sizeof g->fog);
    build_outpost(g);
    build_signal_mast(g);
    build_vegetation(g);
    spawn_wildlife(g);
    build_minimap_cache(g);
    outpost_spawn_garrison(g);
    sys_build_island_extras(g);
}

/* Deploy to the outpost DoD start: beach south of Punta Quemada, full
   loadout, garrison fresh. This is the scout→plan→capture entry point. */
void game_outpost_start(Game *g) {
    if (!g || !g->ready || !g->outpost.built) return;
    game_give_weapon(g, 0, "W01", 1);
    game_give_weapon(g, 1, "W05", 1);
    game_give_weapon(g, 2, "W08", 1);
    g->wpn_slot = 0;

    /* clear dev/arena hostiles, rebuild the authored garrison */
    enemies_init(&g->enemies);
    outpost_spawn_garrison(g);
    g->arena_active = 0;
    g->arena_total = 0;
    g->kills = 0;

    const Vec3 C = g->outpost.center;
    Vec3 sp = v3(C.x, 0.f, C.z + 62.f);
    sp.y = terrain_height(&g->terrain, sp.x, sp.z) + 0.1f;
    g->player.pos = sp;
    g->player.vel = v3(0.f, 0.f, 0.f);
    g->player.yaw = 3.1415927f;          /* face -z: the outpost gate */
    g->player.pitch = 0.f;
    g->player.health = g->player.health_max;
    g->spawn = sp;
    game_start_play(g);
    game_message(g, "%s - 8 HOSTILES. B binoculars / T takedown / E interact. Stealth, guns, or both.",
                 g->outpost.name);
    g->message_t = 6.f;
    DH_INFO("game", "outpost start: player at (%.0f,%.0f), %d garrison alive",
            sp.x, sp.z, g->enemies.alive_count);
}

/* Grant a weapon into a slot; `fill` also stocks mag + reserve. */
static void game_give_weapon(Game *g, int slot, const char *id, int fill)
{
    if (!g || slot < 0 || slot >= WPN_SLOT_MAX) return;
    const WeaponDef *w = weapon_by_id(id);
    if (!w) { DH_WARN("game", "give: unknown weapon %s", id ? id : "?"); return; }
    int idx = (int)(w - weapons_get(0));
    g->wpn_def[slot] = idx;
    if (fill) {
        g->wpn_mag[slot] = w->mag;
        g->ammo[w->ammo] += w->mag * 4;
    }
}

void game_arena_start(Game *g, int n_enemies)
{
    if (!g || !g->ready) return;
    int n = dh_clampi(n_enemies, 1, ENEMY_MAX);

    /* armory: sidearm + rifle + shotgun (M2 DoD; story unlocks come in M6) */
    game_give_weapon(g, 0, "W01", 1);
    game_give_weapon(g, 1, "W05", 1);
    game_give_weapon(g, 2, "W08", 1);
    g->wpn_slot = 0;

    enemies_init(&g->enemies);
    for (int i = 0; i < PICKUP_MAX; i++) g->pickups.v[i].live = 0;
    g->pickups.count = 0;
    for (int i = 0; i < TRACER_MAX; i++) g->tracers.v[i].life = 0.f;
    g->tracers.count = 0;
    g->fx_count = 0;
    g->hitmark_t = 0.f; g->damage_flash = 0.f; g->noise_t = 0.f;
    g->rec_pitch = 0.f; g->rec_yaw = 0.f;
    const Vec3 A = g->arena_center;
    for (int i = 0; i < n; i++) {
        float ang = (float)i * 6.2831853f / (float)n;
        float rad = 18.f + (float)(i % 4) * 4.5f;
        Vec3 pos = v3(A.x + sinf(ang) * rad, 0.f, A.z + cosf(ang) * rad);
        pos.y = terrain_height(&g->terrain, pos.x, pos.z);
        int arch = (i % 10 == 9) ? EN_OFFICER : ((i % 4 == 3) ? EN_BRUISER : EN_GRUNT);
        Vec3 pa = pos;
        Vec3 pb = v3(A.x + sinf(ang + 0.6f) * (rad * 0.6f), 0.f,
                     A.z + cosf(ang + 0.6f) * (rad * 0.6f));
        pb.y = terrain_height(&g->terrain, pb.x, pb.z);
        enemies_spawn(&g->enemies, pos, arch, 0 /* cult: fights to the death */, pa, pb);
    }
    /* a few field supplies so the arena is self-sustaining */
    for (int i = 0; i < 4; i++) {
        float ang = (float)i * 1.5707963f + 0.4f;
        Vec3 pos = v3(A.x + sinf(ang) * 12.f, 0.f, A.z + cosf(ang) * 12.f);
        pos.y = terrain_height(&g->terrain, pos.x, pos.z);
        pickup_add(g, pos, (i & 1) ? PK_AMMO : PK_HEALTH,
                   (i & 1) ? AMMO_556 : 0, (i & 1) ? 60.f : 40.f);
    }
    /* M3: the world persists — rebuild the outpost garrison the wipe cleared
       (faction 2, so the arena-clear check below ignores them). */
    outpost_spawn_garrison(g);

    /* drop the player at the south gate, facing the yard */
    g->player.pos = v3(A.x, terrain_height(&g->terrain, A.x, A.z - 46.f) + 0.1f, A.z - 46.f);
    g->player.vel = v3(0.f, 0.f, 0.f);
    g->player.yaw = 0.f;               /* +z: toward the arena centre */
    g->player.pitch = 0.f;
    g->player.health = g->player.health_max;
    g->spawn = g->player.pos;          /* arena checkpoint */
    g->arena_active = 1;
    g->arena_total = n;
    g->kills = 0;
    g->fire_cd = 0.f; g->reloading = 0; g->ads_k = 0.f;
    game_start_play(g);
    game_message(g, "COMBAT ARENA - eliminate %d hostiles. F/LMB fire, R reload, 1/2/3 weapons",
                 g->arena_total);
    g->message_t = 5.f;
    DH_INFO("game", "arena start: %d hostiles (%d grunts, %d bruisers, %d officers)",
            n, n - n/4 - n/10, n/4, n/10);
}

/* ── hitscan ─────────────────────────────────────────────────────────────── */
static int ray_box_t(Vec3 o, Vec3 d, Vec3 bmin, Vec3 bmax, float tmax, float *t_out)
{
    float t0 = 0.f, t1 = tmax;
    float po[3] = { o.x, o.y, o.z }, pd[3] = { d.x, d.y, d.z };
    float lo[3] = { bmin.x, bmin.y, bmin.z }, hi[3] = { bmax.x, bmax.y, bmax.z };
    for (int i = 0; i < 3; i++) {
        if (fabsf(pd[i]) < 1e-8f) { if (po[i] < lo[i] || po[i] > hi[i]) return 0; continue; }
        float inv = 1.f / pd[i];
        float ta = (lo[i] - po[i]) * inv, tb = (hi[i] - po[i]) * inv;
        if (ta > tb) { float tt = ta; ta = tb; tb = tt; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
        if (t0 > t1) return 0;
    }
    if (t_out) *t_out = t0;
    return 1;
}

typedef struct {
    int   hit;
    float t;
    Vec3  point, normal;
    int   enemy_i;           /* -1 = world */
    int   zone;
    int   alarm_hit;         /* M3: hit the outpost alarm box */
    int   critter_i;         /* M5: hunted animal (-1 none) */
} HitScan;

#define HITSCAN_RANGE 260.0f

static HitScan combat_hitscan(Game *g, Vec3 o, Vec3 d)
{
    HitScan hs;
    memset(&hs, 0, sizeof hs);
    hs.enemy_i = -1;
    hs.critter_i = -1;
    hs.t = HITSCAN_RANGE;
    hs.normal = v3_neg(d);

    float tw; Vec3 nw;
    if (obstacles_ray_hit(&g->obs, o, d, hs.t, &tw, &nw)) {
        hs.hit = 1; hs.t = tw; hs.normal = nw;
    }
    /* terrain march (0.75 m steps — plenty for 260 m hitscan at 60 fps) */
    {
        float prev_h = terrain_height(&g->terrain, o.x, o.z);
        float prev_s = 0.f;
        for (float s = 0.75f; s < hs.t; s += 0.75f) {
            Vec3 p = v3_add(o, v3_mul(d, s));
            float h = terrain_height(&g->terrain, p.x, p.z);
            if (p.y <= h) {
                float f = (s - prev_s) > 1e-4f ? (p.y - prev_h) / ((p.y - prev_h) + (h - p.y) + 1e-4f) : 1.f;
                hs.hit = 1; hs.t = prev_s + 0.75f * dh_clampf(f, 0.f, 1.f);
                hs.normal = terrain_normal(&g->terrain, p.x, p.z);
                break;
            }
            prev_h = h; prev_s = s;
        }
    }
    /* enemies: oriented-ish boxes + zone bands (§6.3 damage zones) */
    for (int i = 0; i < g->enemies.count; i++) {
        const Enemy *e = &g->enemies.v[i];
        if (e->state == EN_DEAD) continue;
        float r = (e->arch == EN_BRUISER) ? 0.55f : 0.42f;
        float hh = (e->arch == EN_BRUISER) ? 1.95f : 1.80f;
        Vec3 bmin = v3(e->pos.x - r, e->pos.y, e->pos.z - r);
        Vec3 bmax = v3(e->pos.x + r, e->pos.y + hh, e->pos.z + r);
        float te;
        if (ray_box_t(o, d, bmin, bmax, hs.t, &te)) {
            hs.hit = 1; hs.t = te; hs.enemy_i = i;
            Vec3 p = v3_add(o, v3_mul(d, te));
            float rel = p.y - e->pos.y;
            hs.zone = (rel >= hh * 0.82f) ? ZONE_HEAD :
                      (rel >= hh * 0.45f) ? ZONE_TORSO : ZONE_LIMB;
            hs.normal = v3_neg(d);
        }
    }
    /* outpost alarm box (§52: 150 hp destructible — draw-only volume so this
       dedicated test owns the semantics) */
    if (g->outpost.built && !g->outpost.alarm_destroyed && !g->outpost.captured) {
        Vec3 ap = g->outpost.alarm_pos;
        float ta;
        if (ray_box_t(o, d, v3(ap.x - 0.45f, ap.y - 0.45f, ap.z - 0.45f),
                      v3(ap.x + 0.45f, ap.y + 0.45f, ap.z + 0.45f), hs.t, &ta)) {
            hs.hit = 1; hs.t = ta; hs.enemy_i = -1; hs.alarm_hit = 1;
            hs.normal = v3_neg(d);
        }
    }
    /* M5 hunting: living wildlife are shootable (§70 hides) */
    if (g->act == 0) {
        for (int i = 0; i < g->critter_count; i++) {
            const Critter *c = &g->critters[i];
            if (c->state >= 3) continue;
            float tc;
            if (ray_box_t(o, d, v3(c->pos.x - 0.5f, c->pos.y, c->pos.z - 0.5f),
                          v3(c->pos.x + 0.5f, c->pos.y + 1.1f, c->pos.z + 0.5f), hs.t, &tc)) {
                hs.hit = 1; hs.t = tc; hs.enemy_i = -1; hs.alarm_hit = 0; hs.critter_i = i;
                hs.normal = v3_neg(d);
            }
        }
    }
    hs.point = v3_add(o, v3_mul(d, hs.t));
    if (!v3_valid(hs.point)) { hs.hit = 0; hs.point = o; }   /* 18.4: no NaN */
    return hs;
}

/* Cone sample for spread/pellets: orthonormal basis around d. */
static Vec3 spread_dir(Game *g, Vec3 d, float cone_rad)
{
    if (cone_rad <= 0.f) return d;
    Vec3 up = fabsf(d.y) > 0.95f ? v3(1.f, 0.f, 0.f) : v3(0.f, 1.f, 0.f);
    Vec3 u = v3_norm(v3_cross(d, up));
    Vec3 w = v3_cross(d, u);
    float a = rng_f(&g->rng) * 6.2831853f;
    float rr = cone_rad * sqrtf(rng_f(&g->rng));
    return v3_norm(v3_add(d, v3_add(v3_mul(u, cosf(a) * rr), v3_mul(w, sinf(a) * rr))));
}

static void combat_start_reload(Game *g)
{
    int sl = g->wpn_slot;
    const WeaponDef *w = weapons_get(g->wpn_def[sl]);
    if (!w || g->reloading) return;
    if (g->wpn_mag[sl] >= w->mag) return;
    if (g->ammo[w->ammo] <= 0) {
        game_message(g, "NO %s RESERVE", ammo_name(w->ammo));
        g->message_t = 1.2f;
        g->fire_cd = 0.4f;
        return;
    }
    g->reloading = 1;
    g->reload_slot = sl;
    g->reload_t = w->reload_s * prog_mod(&g->prog, MOD_RELOAD);   /* QUICK HANDS */
}

static void outpost_raise_alarm(Game *g);   /* fwd: gunfire can trip the alarm */

/* M5: award XP for a gameplay event; announce level-ups (§71). */
static void sys_xp(Game *g, ProgEvent ev) {
    int lv = g->prog.level;
    prog_event(&g->prog, ev, 0);
    if (g->prog.level > lv) {
        game_message(g, "LEVEL %d - +%d SKILL POINT%s (I opens skills)", g->prog.level,
                     g->prog.level - lv, g->prog.level - lv > 1 ? "S" : "");
        g->message_t = 3.5f;
        DH_INFO("game", "level up -> %d (points %d)", g->prog.level, g->prog.skill_points);
    }
}

static void combat_fire(Game *g, Vec3 eye, Vec3 dir)
{
    int sl = g->wpn_slot;
    const WeaponDef *w = weapons_get(g->wpn_def[sl]);
    Player *p = &g->player;
    if (!w) return;
    if (g->wpn_mag[sl] <= 0) {                 /* dry fire → auto reload */
        combat_start_reload(g);
        g->fire_cd = 0.25f;
        return;
    }
    g->wpn_mag[sl]--;
    g->fire_cd = weapon_shot_interval(w);
    g->noise_t = 0.6f;                         /* gunshots carry (§82) */

    /* unsuppressed gunfire inside the outpost's earshot wakes the garrison
       (no suppressors until the combat-v2 backlog — honest and loud, §37) */
    if (g->outpost.built && !g->outpost.captured && !g->outpost.alarm_destroyed &&
        v3_dist_xz(eye, g->outpost.center) <
            (g->outpost.radius + 40.f) * prog_mod(&g->prog, MOD_ALARM_RADIUS))
        outpost_raise_alarm(g);
    if (g->act == 0) prog_event(&g->prog, EV_LOUD_SHOT, 0);   /* island alert (§13.6) */

    /* recoil: part permanent climb (into yaw/pitch), part decaying punch */
    Vec3 rec = v3_mul(weapon_recoil(w, g->ads_k, g->shot_index++),
                      prog_mod(&g->prog, MOD_RECOIL));   /* STEADY HANDS */
    p->pitch += rec.y * 0.55f;
    p->yaw   += rec.x * 0.55f;
    if (p->pitch > 1.5f) p->pitch = 1.5f;
    g->rec_pitch += rec.y;
    g->rec_yaw   += rec.x;
    g->rec_pitch = dh_clampf(g->rec_pitch, -0.2f, 0.2f);
    g->rec_yaw   = dh_clampf(g->rec_yaw, -0.2f, 0.2f);

    fx_spawn(g, v3_add(eye, v3_mul(dir, 0.5f)), dir, 5, 0xFF40A0FFu, 2.2f, 0.06f, 0);

    float cone = weapon_spread_rad(w, g->ads_k,
                                   v3_len(v3(p->vel.x, 0.f, p->vel.z)) > 2.6f,
                                   p->crouched);
    int pellets = w->pellets;
    for (int k = 0; k < pellets; k++) {
        Vec3 pd = (pellets > 1 || cone > 0.f) ? spread_dir(g, dir, cone) : dir;
        HitScan hs = combat_hitscan(g, eye, pd);
        if (hs.enemy_i >= 0) {
            Enemy *e = &g->enemies.v[hs.enemy_i];
            float dmg = weapon_damage(w, hs.zone, hs.t);
            if (hs.zone == ZONE_HEAD) dmg *= prog_mod(&g->prog, MOD_HEADSHOT);
            int killed = enemy_apply_damage(e, dmg, hs.zone);
            g->hitmark_t = 0.14f;
            g->hitmark_kill = killed;
            fx_spawn(g, hs.point, v3_mul(pd, -0.4f), 6, 0xFF2828B0u, 2.5f, 0.25f, 2);
            if (killed) {
                g->kills++;
                e->dropped = 1;
                sys_xp(g, EV_KILL);
                if (hs.zone == ZONE_HEAD) sys_xp(g, EV_HEADSHOT);
                /* loot roll: 30% health, 45% ammo for the gun in hand, else nothing */
                float rr = rng_f(&g->rng);
                if (rr < 0.30f)
                    pickup_add(g, v3_add(e->pos, v3(0.f, 0.4f, 0.f)), PK_HEALTH, 0, 40.f);
                else if (rr < 0.75f)
                    pickup_add(g, v3_add(e->pos, v3(0.f, 0.4f, 0.f)), PK_AMMO, w->ammo,
                               (float)(w->mag * 2));
                else
                    pickup_add(g, v3_add(e->pos, v3(0.f, 0.4f, 0.f)), PK_SCRAP, 0, 1.f);
                if (g->kills % 5 == 0) {
                    game_message(g, "%d HOSTILES DOWN", g->kills);
                    g->message_t = 1.5f;
                }
            }
        } else if (hs.alarm_hit) {
            Outpost *o = &g->outpost;
            float dmg = weapon_damage(w, ZONE_TORSO, hs.t);
            if (prog_has(&g->prog, SK_WIRE_CUTTER)) dmg *= 2.f;
            o->alarm_hp -= dmg;
            g->hitmark_t = 0.14f; g->hitmark_kill = 0;
            fx_spawn(g, hs.point, v3_mul(pd, -0.4f), 8, 0xFF40C8FFu, 2.5f, 0.25f, 1);
            if (o->alarm_hp <= 0.f) {
                o->alarm_destroyed = 1;
                game_message(g, "ALARM BOX DESTROYED - reinforcements cut off");
                g->message_t = 3.f;
                DH_INFO("game", "outpost alarm box destroyed");
            }
        } else if (hs.critter_i >= 0) {
            Critter *c = &g->critters[hs.critter_i];
            c->state = 3; c->vel = v3(0, 0, 0); c->state_t = 0.f;
            g->hitmark_t = 0.14f; g->hitmark_kill = 1;
            fx_spawn(g, hs.point, v3_mul(pd, -0.4f), 6, 0xFF2828B0u, 2.5f, 0.25f, 2);
            game_message(g, "%s DOWN - [E] to skin", c->species == 0 ? "VENADO" : "JABALI");
            g->message_t = 2.f;
        } else if (hs.hit) {
            fx_spawn(g, hs.point, hs.normal, 7, 0xFF90B8D8u, 3.f, 0.3f, 0);
        }
        tracer_add(g, v3_add(eye, v3_mul(pd, 0.6f)),
                   hs.hit ? hs.point : v3_add(eye, v3_mul(pd, HITSCAN_RANGE)),
                   0xFF50C8FFu);
    }
    if (g->wpn_mag[sl] == 0 && g->ammo[w->ammo] > 0) combat_start_reload(g);
}

static void combat_pickups_update(Game *g, float dt)
{
    Player *p = &g->player;
    for (int i = 0; i < g->pickups.count; i++) {
        Pickup *k = &g->pickups.v[i];
        if (!k->live) continue;
        k->bob += dt * 2.4f;
        if (player_is_down(p)) continue;
        if (v3_dist_xz(p->pos, k->pos) < 1.3f && fabsf(p->pos.y - k->pos.y) < 2.2f) {
            if (k->kind == PK_HEALTH) {
                if (p->health >= p->health_max) continue;          /* don't waste it */
                p->health = dh_clampf(p->health + k->amount, 0.f, p->health_max);
                game_message(g, "MEDKIT +%.0f HP", (double)k->amount);
            } else if (k->kind == PK_HERB) {
                g->prog.mat[MAT_HERB]++;
                g->stat_herbs++;
                game_message(g, "HERB +1 (%d) - craft medkits (I)", g->prog.mat[MAT_HERB]);
            } else if (k->kind == PK_SCRAP) {
                g->prog.mat[MAT_SCRAP]++;
                game_message(g, "SCRAP +1 (%d)", g->prog.mat[MAT_SCRAP]);
            } else {
                int cap = game_ammo_cap(g, k->ammo);
                if (g->ammo[k->ammo] >= cap) continue;          /* full: leave it */
                int add = (int)k->amount;
                if (g->ammo[k->ammo] + add > cap) add = cap - g->ammo[k->ammo];
                g->ammo[k->ammo] += add;
                game_message(g, "%s AMMO +%d", ammo_name(k->ammo), add);
            }
            g->message_t = 1.4f;
            k->live = 0;
        }
    }
}

/* ── island simulation (M3) ──────────────────────────────────────────────── */

static void outpost_raise_alarm(Game *g) {
    Outpost *o = &g->outpost;
    if (!o->built || o->alarm || o->captured) return;
    o->alarm = 1;
    o->alarm_t = 0.f;
    prog_event(&g->prog, EV_ALARM, 0);          /* M5: island alert jumps */
    game_message(g, "ALARM RAISED - %s is awake. Reinforcements inbound.", o->name);
    g->message_t = 3.5f;
    DH_INFO("game", "outpost alarm raised (alarm box %s)",
            o->alarm_destroyed ? "destroyed - no waves" : "live");
}

/* Segment line-of-sight: obstacle boxes + terrain march (2 m steps). */
static int los_clear(Game *g, Vec3 a, Vec3 b) {
    if (!obstacles_segment_clear(&g->obs, a, b)) return 0;
    float d = v3_dist(a, b);
    int steps = (int)(d * 0.5f) + 1;
    if (steps > 96) steps = 96;
    for (int i = 1; i < steps; i++) {
        Vec3 p = v3_lerp(a, b, (float)i / (float)steps);
        if (p.y < terrain_height(&g->terrain, p.x, p.z)) return 0;
    }
    return 1;
}

/* Binoculars (§44): hold-to-zoom 20° optic; centred hostiles with LOS get
   tagged — the tag persists and feeds HUD markers + the minimap. */
static void binoculars_update(Game *g, const PlatInput *in, float dt) {
    Player *p = &g->player;
    g->binocs = (in->buttons & BTN_BINOC) && !g->reloading && !player_is_down(p);
    g->binoc_k = dh_clampf(g->binoc_k + (g->binocs ? 6.f : -8.f) * dt, 0.f, 1.f);
    g->cam_fov = dh_lerp(g->cam_fov, 20.f, g->binoc_k);

    int tagged = 0;
    if (g->binoc_k > 0.8f) {
        Vec3 eye, dir;
        player_camera(p, &eye, &dir);
        for (int i = 0; i < g->enemies.count; i++) {
            Enemy *e = &g->enemies.v[i];
            if (e->state == EN_DEAD) continue;
            Vec3 chest = v3_add(e->pos, v3(0.f, 1.2f, 0.f));
            Vec3 to = v3_sub(chest, eye);
            float dist = v3_len(to);
            if (dist > 220.f * prog_mod(&g->prog, MOD_TAG_TIME) || dist < 0.5f) continue;
            /* perpendicular miss distance vs a 0.9 m + 2% slack cone */
            Vec3 cr = v3_cross(to, dir);
            float miss = v3_len(cr);
            if (miss > 0.9f + 0.02f * dist) continue;
            if (!los_clear(g, eye, chest)) continue;
            e->tagged = 1;
        }
    }
    for (int i = 0; i < g->enemies.count; i++)
        if (g->enemies.v[i].tagged && g->enemies.v[i].state != EN_DEAD) tagged++;
    g->tagged_count = tagged;
}

/* Melee / silent takedown (T). Unaware enemies within 2 m in front die
   instantly and SILENTLY (no noise event → no alarm). Alert enemies take a
   40-damage swing instead — the stealth/loud/mixed choice, §37. */
static void melee_update(Game *g, const PlatInput *in, float dt) {
    Player *p = &g->player;
    if (g->melee_cd > 0.f) g->melee_cd -= dt;
    if (!(in->pressed & BTN_MELEE) || g->melee_cd > 0.f || player_is_down(p)) return;
    g->melee_cd = 0.65f;

    Vec3 eye, dir;
    player_camera(p, &eye, &dir);
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < g->enemies.count; i++) {
        const Enemy *e = &g->enemies.v[i];
        if (e->state == EN_DEAD) continue;
        float dx = e->pos.x - p->pos.x, dz = e->pos.z - p->pos.z;
        float dh2 = sqrtf(dx * dx + dz * dz);
        if (dh2 > 2.0f || fabsf(e->pos.y - p->pos.y) > 1.7f) continue;
        if (dh2 > 0.05f && (dx * dir.x + dz * dir.z) / dh2 < 0.25f) continue;
        if (dh2 < bd) { bd = dh2; best = i; }
    }
    if (best < 0) {
        fx_spawn(g, v3_add(eye, v3_mul(dir, 0.9f)), dir, 2, 0xFF90B8D8u, 2.f, 0.2f, 0);
        return;
    }
    Enemy *e = &g->enemies.v[best];
    if (e->state != EN_COMBAT && e->state != EN_SEARCH) {
        enemy_apply_damage(e, 1000.f, ZONE_HEAD);
        g->kills++;
        g->hitmark_t = 0.14f; g->hitmark_kill = 1;
        fx_spawn(g, v3_add(e->pos, v3(0.f, 1.2f, 0.f)), v3(0.f, 1.f, 0.f),
                 8, 0xFF2828B0u, 2.f, 0.25f, 2);
        sys_xp(g, EV_TAKEDOWN);
        if (prog_has(&g->prog, SK_CHAIN_TAKEDOWN)) g->melee_cd = 0.f;
        game_message(g, "SILENT TAKEDOWN - no one heard a thing");
        g->message_t = 1.6f;
        DH_INFO("game", "silent takedown at (%.0f,%.0f)", e->pos.x, e->pos.z);
    } else {
        int killed = enemy_apply_damage(e, 40.f, ZONE_TORSO);
        g->hitmark_t = 0.14f; g->hitmark_kill = killed;
        if (killed) { g->kills++; sys_xp(g, EV_KILL); }
        fx_spawn(g, v3_add(e->pos, v3(0.f, 1.2f, 0.f)), v3(0.f, 1.f, 0.f),
                 6, 0xFF2828B0u, 2.f, 0.25f, 2);
        game_message(g, killed ? "MELEE KILL" : "MELEE SWING");
        g->message_t = 1.2f;
    }
}

/* Wildlife FSM: wander/graze around a home anchor → alert → flee. Gunfire
   (g->noise_t) within 90 m or a close player spooks them (§4.2 ambience). */
static void critters_update(Game *g, float dt) {
    Rng *r = &g->rng;
    const Player *p = &g->player;
    for (int i = 0; i < g->critter_count; i++) {
        Critter *c = &g->critters[i];
        if (c->state >= 3) continue;              /* M5: carcass / skinned */
        float dp = v3_dist_xz(p->pos, c->pos);
        if (dp > 165.f) continue;                 /* simulate near the player */
        float speed_walk = (c->species == 0) ? 1.4f : 1.0f;
        float speed_run  = (c->species == 0) ? 7.2f : 5.6f;

        /* spook checks */
        int gunshot = g->noise_t > 0.f && dp < 90.f;
        if (c->state == 0 && (gunshot || (dp < (p->crouched ? 9.f : 16.f)))) {
            c->state = 1; c->state_t = gunshot ? 0.3f : 0.9f;
        }
        c->state_t -= dt;
        if (c->state == 1 && c->state_t <= 0.f) { c->state = 2; c->state_t = 7.f; }
        if (c->state == 2 && (c->state_t <= 0.f || dp > 60.f)) c->state = 0;

        if (c->state == 2) {
            Vec3 away = v3_norm(v3(c->pos.x - p->pos.x, 0.f, c->pos.z - p->pos.z));
            if (!v3_valid(away)) away = v3(1.f, 0.f, 0.f);
            c->vel = v3_mul(away, speed_run);
            c->yaw = atan2f(away.x, away.z);
        } else {
            if (c->state_t <= 0.f) {
                /* pick a new goal near home (or pause to graze) */
                if (rng_f(r) < 0.35f) { c->vel = v3(0, 0, 0); c->state_t = 1.5f + rng_f(r) * 3.f; }
                else {
                    float a = rng_f(r) * 6.2831853f, d = 6.f + rng_f(r) * 20.f;
                    c->goal = v3(c->home.x + sinf(a) * d, 0.f, c->home.z + cosf(a) * d);
                    float gh = terrain_height(&g->terrain, c->goal.x, c->goal.z);
                    if (gh < 1.5f) c->goal = c->home;         /* never walk into the sea */
                    c->state_t = 4.f + rng_f(r) * 4.f;
                }
            }
            Vec3 to = v3(c->goal.x - c->pos.x, 0.f, c->goal.z - c->pos.z);
            float dl = v3_len(to);
            if (dl > 0.6f) {
                to = v3_mul(to, 1.f / dl);
                c->vel = v3_mul(to, speed_walk);
                c->yaw = atan2f(to.x, to.z);
            } else c->vel = v3(0, 0, 0);
        }
        c->pos = v3_add(c->pos, v3_mul(c->vel, dt));
        c->pos.y = terrain_height(&g->terrain, c->pos.x, c->pos.z);
        c->hop += dt * (c->state == 2 ? 14.f : 6.f) *
                  dh_clampf(v3_len(c->vel) / 2.f, 0.f, 1.f);
        /* dragged too far from home (flee panic) → soft reset */
        if (v3_dist_xz(c->pos, c->home) > 120.f) { c->pos = c->home; c->state = 0; }
    }
}

/* Outpost FSM: sustained garrison contact or unsuppressed gunfire raises the
   alarm → two reinforcement waves (§28.2). Clear the garrison and hold the
   yard 3 s to liberate (§37.3). Alarm box destroyed = no waves. */
static void island_update(Game *g, const PlatInput *in, float dt) {
    Outpost *o = &g->outpost;
    if (!o->built) return;
    Player *p = &g->player;
    float dp = v3_dist_xz(p->pos, o->center);

    /* garrison census (faction 2) */
    int alive = 0;
    for (int i = 0; i < g->enemies.count; i++)
        if (g->enemies.v[i].faction == 2 && g->enemies.v[i].state != EN_DEAD) alive++;

    /* contact → alarm (only while the alarm box still works) */
    if (!o->alarm && !o->alarm_destroyed && !o->captured && dp < o->radius + 70.f) {
        int any_combat = 0;
        for (int i = 0; i < g->enemies.count; i++)
            if (g->enemies.v[i].faction == 2 && g->enemies.v[i].state == EN_COMBAT)
                any_combat = 1;
        if (any_combat) o->combat_seen_t += dt;
        else o->combat_seen_t = dh_maxf(0.f, o->combat_seen_t - dt * 0.5f);
        if (o->combat_seen_t > 1.2f) outpost_raise_alarm(g);
    }

    /* reinforcement waves */
    if (o->alarm && !o->captured) {
        o->alarm_t += dt;
        if (o->waves_spawned < 2 && !o->alarm_destroyed &&
            o->alarm_t > (o->waves_spawned == 0 ? 9.f : 24.f)) {
            Vec3 sp = o->reinforce[o->waves_spawned];
            int wn = prog_reinforce_size(&g->prog);   /* M5: 3 + island alert level */
            for (int i = 0; i < wn; i++) {
                Vec3 pos = v3(sp.x + (float)(i - 1) * 2.5f, sp.y, sp.z + (float)(i / 3) * 2.5f);
                enemies_spawn(&g->enemies, pos,
                              i == 2 ? EN_BRUISER : EN_GRUNT, 2,
                              pos, o->center);
            }
            o->waves_spawned++;
            game_message(g, "REINFORCEMENTS - wave %d entering the yard", o->waves_spawned);
            g->message_t = 3.f;
            DH_INFO("game", "outpost reinforcement wave %d spawned", o->waves_spawned);
        }
    }

    /* liberation channel (§37.3: clear hostiles, hold the yard) */
    if (!o->captured) {
        if (alive == 0 && dp < o->radius && !player_is_down(p)) {
            o->capture_t += dt;
            if (o->capture_t >= 3.f) {
                o->captured = 1;
                o->capture_t = 3.f;
                sys_xp(g, o->alarm ? EV_OUTPOST_LOUD : EV_OUTPOST_STEALTH);
                g->prog.ft_unlocked[FT_OUTPOST] = 1;
                g->prog.ft[FT_OUTPOST].pos = o->flag_pos;
                game_message(g, "%s LIBERATED - Isla Sombra 1/1. Supplies dropped.", o->name);
                g->message_t = 6.f;
                pickup_add(g, v3(o->flag_pos.x - 2.f, o->pad_h + 0.4f, o->flag_pos.z),
                           PK_HEALTH, 0, 40.f);
                pickup_add(g, v3(o->flag_pos.x + 2.f, o->pad_h + 0.4f, o->flag_pos.z),
                           PK_AMMO, AMMO_556, 90.f);
                pickup_add(g, v3(o->flag_pos.x, o->pad_h + 0.4f, o->flag_pos.z + 2.f),
                           PK_AMMO, AMMO_9MM, 48.f);
                DH_INFO("game", "outpost captured: %s (alarm=%d waves=%d tagged=%d)",
                        o->name, o->alarm, o->waves_spawned, g->tagged_count);
            }
        } else if (o->capture_t > 0.f) {
            o->capture_t = dh_maxf(0.f, o->capture_t - dt * 2.f);
        }
    } else if (o->flag_raise < 1.f) {
        o->flag_raise = dh_minf(1.f, o->flag_raise + dt / 2.5f);
    }

    /* signal mast interact (E on the top platform) */
    if ((in->pressed & BTN_USE) && !o->mast_synced) {
        Vec3 top = v3(o->mast_pos.x, o->mast_top_y, o->mast_pos.z);
        if (v3_dist(p->pos, top) < 3.2f) {
            o->mast_synced = 1;
            fog_sync_all(g);
            sys_xp(g, EV_MAST);
            g->prog.ft_unlocked[FT_MAST] = 1;
            game_message(g, "SIGNAL MAST SYNCED - Isla Sombra mapped");
            g->message_t = 5.f;
            DH_INFO("game", "signal mast synced");
        }
    }
}

static void combat_update(Game *g, const PlatInput *in, float dt)
{
    Player *p = &g->player;

    /* timers */
    if (g->fire_cd > 0.f)     g->fire_cd -= dt;
    if (g->hitmark_t > 0.f)   g->hitmark_t -= dt;
    if (g->damage_flash > 0.f)g->damage_flash -= dt * 2.5f;
    if (g->noise_t > 0.f)     g->noise_t -= dt;
    {
        float k = dh_clampf(10.f * dt, 0.f, 1.f);
        g->rec_pitch *= (1.f - k);
        g->rec_yaw   *= (1.f - k);
    }
    for (int i = 0; i < g->tracers.count; i++)
        if (g->tracers.v[i].life > 0.f) g->tracers.v[i].life -= dt;
    fx_update(g, dt);
    combat_pickups_update(g, dt);

    /* ADS blend (§68: 180 ms pistol — weapon-specific) */
    const WeaponDef *w = weapons_get(g->wpn_def[g->wpn_slot]);
    binoculars_update(g, in, dt);
    int want_ads = (in->buttons & BTN_AIM) && !g->reloading && w && !player_is_down(p) &&
                   g->binoc_k < 0.2f;
    float ads_rate = w ? (1.f / w->ads_s) : 6.f;
    if (want_ads)  g->ads_k = dh_clampf(g->ads_k + ads_rate * dt, 0.f, 1.f);
    else           g->ads_k = dh_clampf(g->ads_k - ads_rate * 1.4f * dt, 0.f, 1.f);
    if (g->binoc_k < 0.2f) g->cam_fov = 78.f * (1.f - 0.22f * g->ads_k);

    /* weapon slot switch */
    if (in->pressed & BTN_SLOT1) { if (g->wpn_def[0] >= 0) { g->wpn_slot = 0; g->reloading = 0; g->fire_cd = 0.3f; } }
    if (in->pressed & BTN_SLOT2) { if (g->wpn_def[1] >= 0) { g->wpn_slot = 1; g->reloading = 0; g->fire_cd = 0.3f; } }
    if (in->pressed & BTN_SLOT3) { if (g->wpn_def[2] >= 0) { g->wpn_slot = 2; g->reloading = 0; g->fire_cd = 0.3f; } }

    /* reload FSM */
    if ((in->pressed & BTN_RELOAD) && !g->reloading) combat_start_reload(g);
    if (g->reloading) {
        g->reload_t -= dt;
        if (g->reload_t <= 0.f) {
            int sl = g->reload_slot;
            const WeaponDef *rw = weapons_get(g->wpn_def[sl]);
            if (rw) {
                int need = rw->mag - g->wpn_mag[sl];
                int take = need < g->ammo[rw->ammo] ? need : g->ammo[rw->ammo];
                if (take > 0) { g->wpn_mag[sl] += take; g->ammo[rw->ammo] -= take; }
            }
            g->reloading = 0;
        }
    }

    /* fire (full-auto cadence from rpm — §68 rate table) */
    if (w && (in->buttons & BTN_FIRE) && g->fire_cd <= 0.f && !g->reloading &&
        !g->binocs && g->binoc_k < 0.2f && !player_is_down(p)) {
        Vec3 eye, dir;
        player_camera(p, &eye, &dir);
        combat_fire(g, eye, dir);
    }

    /* melee / silent takedown (T) */
    melee_update(g, in, dt);

    /* ── enemies ── */
    EnemyView pv;
    pv.eye = v3(p->pos.x, p->pos.y + p->eye, p->pos.z);
    pv.crouched = p->crouched;
    pv.moving = v3_len(v3(p->vel.x, 0.f, p->vel.z)) > 3.0f;
    pv.alive = !player_is_down(p);
    pv.light = dh_clampf(g->light.sun_intensity, 0.15f, 1.0f);  /* M3: night hides you (§82) */
    pv.light *= prog_mod(&g->prog, MOD_NOISE);                 /* M5: SOFT STEP */
    pv.noise = g->noise_t > 0.f ? 1.0f : 0.0f;
    int hp_before = (int)p->health;
    enemies_update(&g->enemies, &pv, dt, &g->terrain, &g->obs);

    /* enemy shots resolve against the player (accuracy roll, §82) */
    for (int i = 0; i < g->enemies.count; i++) {
        Enemy *e = &g->enemies.v[i];
        if (!e->shot_this_frame) continue;
        Vec3 muz = v3_add(e->pos, v3(0.f, 1.45f, 0.f));
        tracer_add(g, muz, v3_add(pv.eye, v3(rng_range(&g->rng, -0.4f, 0.4f),
                                             rng_range(&g->rng, -0.4f, 0.4f),
                                             rng_range(&g->rng, -0.4f, 0.4f))),
                   0xFF3030F0u);
        fx_spawn(g, muz, v3_mul(v3_sub(pv.eye, muz), 0.05f), 3, 0xFF40A0FFu, 2.f, 0.06f, 0);
        if (rng_f(&g->rng) < e->shot_acc * pol_diff_aim()) {
            float dmg = enemy_shot_damage(e->arch) * pol_diff_damage();   /* M7 difficulty */
            if (g->armor > 0.f) {                    /* M5 plate absorbs 60% */
                float ab = dh_minf(g->armor, dmg * 0.6f);
                g->armor -= ab; dmg -= ab;
            }
            p->health = dh_clampf(p->health - dmg, 0.f, p->health_max);
            g->damage_flash = 0.5f;
            if (p->health <= 0.f && prog_has(&g->prog, SK_LAST_STAND) &&
                g->prog.last_stand_cd <= 0.f) {
                p->health = 1.f;                     /* M5: LAST STAND */
                g->prog.last_stand_cd = 120.f;
                game_message(g, "LAST STAND - GET TO COVER");
                g->message_t = 2.5f;
            }
        }
    }
    if ((int)p->health < hp_before && p->health <= 0.f) {
        game_message(g, "YOU DIED - respawning at checkpoint (loadout kept)");
        g->message_t = 3.f;
    }

    /* arena clear (faction 0 only — the outpost garrison shares the set) */
    if (g->arena_active) {
        int arena_alive = 0;
        for (int i = 0; i < g->enemies.count; i++)
            if (g->enemies.v[i].faction == 0 && g->enemies.v[i].state != EN_DEAD)
                arena_alive++;
        if (arena_alive == 0) {
            g->arena_active = 0;
            game_message(g, "ARENA CLEAR - %d hostiles down. The yard is yours.", g->kills);
            g->message_t = 6.f;
            DH_INFO("game", "arena cleared: %d kills", g->kills);
        }
    }

    /* ── M3 island systems: outpost FSM, wildlife ── */
    if (g->act == 0) {           /* M4: island systems only run in act I */
        island_update(g, in, dt);
        critters_update(g, dt);
    }
}

/* ── combat rendering ───────────────────────────────────────────────────── */
static void draw_enemies(Game *g)
{
    Mat4 ident_base = m4_identity();
    (void)ident_base;
    for (int i = 0; i < g->enemies.count; i++) {
        const Enemy *e = &g->enemies.v[i];
        if (!rend_should_draw(&e->pos, 2.2f)) continue;
        float w = (e->arch == EN_BRUISER) ? 0.62f : 0.46f;
        float hh = (e->arch == EN_BRUISER) ? 1.95f : 1.80f;
        uint32_t body, head = 0xFF6A8CB4u;
        if (e->state == EN_DEAD)       body = 0xFF202830u;
        else if (e->hit_t > 0.f)       body = 0xFFE0E0E0u;      /* hit flash */
        else if (e->state == EN_COMBAT)body = (e->arch == EN_OFFICER) ? 0xFF283068u : 0xFF2E3E8Cu;
        else if (e->state == EN_SEARCH || e->state == EN_SUSPICIOUS)
                                       body = 0xFF2E4A7Au;
        else                           body = (e->arch == EN_BRUISER) ? 0xFF3A4664u : 0xFF2E4A6Eu;
        if (e->state == EN_DEAD) {
            /* corpse: flat box, keeps the yard honest about what you did */
            Mat4 m = m4_mul(m4_translate(v3(e->pos.x, e->pos.y + 0.22f, e->pos.z)),
                            m4_scale(v3(w * 2.4f, 0.44f, w * 1.6f)));
            rend_mesh_lit(g->mesh_box, &m, -1, body, 1.f, 1, 1, 1, 0, 1);
            continue;
        }
        Mat4 mb = m4_mul(m4_translate(v3(e->pos.x, e->pos.y + hh * 0.42f, e->pos.z)),
                         m4_scale(v3(w * 2.f, hh * 0.84f, w * 1.4f)));
        rend_mesh_lit(g->mesh_box, &mb, -1, body, 1.f, 1, 1, 1, 0, 1);
        Mat4 mh = m4_mul(m4_translate(v3(e->pos.x, e->pos.y + hh * 0.92f, e->pos.z)),
                         m4_scale1(0.42f));
        rend_mesh_lit(g->mesh_box, &mh, -1, head, 1.f, 1, 1, 1, 0, 1);
        /* facing nub: which way is this thing looking? (readability, §33) */
        Vec3 fw = v3(sinf(e->yaw), 0.f, cosf(e->yaw));
        Mat4 mg = m4_mul(m4_translate(v3_add(e->pos,
                            v3(fw.x * (w + 0.3f), hh * 0.55f, fw.z * (w + 0.3f)))),
                         m4_scale(v3(0.5f, 0.12f, 0.12f)));
        rend_mesh_lit(g->mesh_box, &mg, -1, 0xFF303050u, 1.f, 1, 1, 1, 0, 1);
    }
}

static void draw_pickups(Game *g)
{
    for (int i = 0; i < g->pickups.count; i++) {
        const Pickup *k = &g->pickups.v[i];
        if (!k->live || !rend_should_draw(&k->pos, 1.f)) continue;
        float y = k->pos.y + 0.55f + sinf(k->bob) * 0.09f;
        Mat4 m = m4_mul(m4_translate(v3(k->pos.x, y, k->pos.z)), m4_scale1(0.44f));
        uint32_t c = (k->kind == PK_HEALTH) ? 0xFF50C860u :
                     (k->kind == PK_HERB)   ? 0xFF3CD27Au :     /* M5 leafy green */
                     (k->kind == PK_SCRAP)  ? 0xFF8C9296u : 0xFF30A8F0u;
        if (k->kind == PK_HERB) m = m4_mul(m4_translate(v3(k->pos.x, k->pos.y + 0.25f, k->pos.z)),
                                           m4_scale(v3(0.35f, 0.5f, 0.35f)));
        rend_mesh_lit(g->mesh_box, &m, -1, c, 1.f, 1, 1, 1, 0, 1);
    }
}

static void draw_tracers_fx(Game *g)
{
    for (int i = 0; i < g->tracers.count; i++) {
        const Tracer *tr = &g->tracers.v[i];
        if (tr->life <= 0.f) continue;
        Vec3 pts[2] = { tr->a, tr->b };
        rend_lines(pts, 1, tr->color, 1.6f);
    }
    if (g->fx_count > 0) rend_particles(g->fx, g->fx_count, -1, 1.f);
    pol_flocks_draw(g);                      /* M7 ambient birds */
}

/* ── island rendering (M3) ───────────────────────────────────────────────── */

/* Wildlife: two boxes + a tail nub per animal, species-coloured. Only drawn
   near the camera (they're ambience, not a draw-call budget problem). */
static void draw_critters(Game *g) {
    for (int i = 0; i < g->critter_count; i++) {
        const Critter *c = &g->critters[i];
        if (c->state == 4) continue;                 /* skinned */
        if (!rend_should_draw(&c->pos, 2.0f)) continue;
        if (c->state == 3) {                         /* carcass lies on its side */
            uint32_t bc = (c->species == 0) ? 0xFF56708Eu : 0xFF30384Au;
            Mat4 mc = m4_mul(m4_translate(v3(c->pos.x, c->pos.y + 0.22f, c->pos.z)),
                             m4_mul(m4_rot_y(c->yaw), m4_scale(v3(0.8f, 0.4f, 0.9f))));
            rend_mesh_lit(g->mesh_box, &mc, -1, bc, 1.f, 1, 1, 1, 0, 1);
            continue;
        }
        float bob = sinf(c->hop) * 0.04f;
        uint32_t body = (c->species == 0) ? 0xFF6E8CB2u : 0xFF3C465Au;  /* venado / jabalí */
        float bl = (c->species == 0) ? 0.85f : 0.75f;
        float bh = (c->species == 0) ? 0.52f : 0.46f;
        Mat4 rot = m4_rot_y(c->yaw);
        Mat4 mb = m4_mul(m4_translate(v3(c->pos.x, c->pos.y + bh + 0.42f + bob, c->pos.z)),
                         m4_mul(rot, m4_scale(v3(0.42f, bh * 0.8f, bl))));
        rend_mesh_lit(g->mesh_box, &mb, -1, body, 1.f, 1, 1, 1, 0, 1);
        /* head forward of the body */
        Vec3 fw = v3(sinf(c->yaw), 0.f, cosf(c->yaw));
        Mat4 mh = m4_mul(m4_translate(v3(c->pos.x + fw.x * (bl * 0.5f + 0.18f),
                                         c->pos.y + bh + 0.58f + bob,
                                         c->pos.z + fw.z * (bl * 0.5f + 0.18f))),
                         m4_mul(rot, m4_scale(v3(0.22f, 0.24f, 0.34f))));
        rend_mesh_lit(g->mesh_box, &mh, -1, body, 1.f, 1, 1, 1, 0, 1);
        if (c->species == 0) {                       /* antlers read as deer */
            Mat4 ma = m4_mul(m4_translate(v3(c->pos.x + fw.x * (bl * 0.5f + 0.2f),
                                             c->pos.y + bh + 0.86f + bob,
                                             c->pos.z + fw.z * (bl * 0.5f + 0.2f))),
                             m4_scale(v3(0.34f, 0.05f, 0.05f)));
            rend_mesh_lit(g->mesh_box, &ma, -1, 0xFFC8D8E8u, 1.f, 1, 1, 1, 0, 1);
        }
    }
}

/* The liberation flag: rises with outpost.flag_raise once captured. */
static void draw_outpost_flag(Game *g) {
    const Outpost *o = &g->outpost;
    if (!o->built) return;
    Vec3 fp = o->flag_pos;
    if (!rend_should_draw(&fp, 8.f)) return;
    float fy = fp.y + 1.4f + o->flag_raise * 4.6f;
    float wave = sinf(g->time * 3.f) * 0.06f;
    Mat4 m = m4_mul(m4_translate(v3(fp.x + 0.75f, fy + wave, fp.z)),
                    m4_scale(v3(1.5f, 0.5f, 0.03f)));
    rend_mesh_lit(g->mesh_box, &m, proc_tex.fabric,
                  o->captured ? 0xFF8CBE3Cu : 0xFF3030A0u, 1.f, 1, 1, 1, 0, 1);
    /* alarm box beacon glow while it still works */
    if (!o->alarm_destroyed && !o->captured && rend_should_draw(&o->alarm_pos, 1.f)) {
        float pulse = 0.5f + 0.5f * sinf(g->time * (o->alarm ? 14.f : 4.f));
        Mat4 ma = m4_mul(m4_translate(o->alarm_pos), m4_scale1(0.16f + 0.05f * pulse));
        rend_mesh_lit(g->mesh_box, &ma, proc_tex.glow,
                      o->alarm ? 0xFF2020F0u : 0xFF40A0F0u, 0.5f + 0.5f * pulse, 1, 1, 0, 0, 1);
    }
}

/* Project a world point to 2D screen coords through the frame's VP matrix.
   Returns 0 when behind the camera. Used for tagged-enemy markers. */
static int world_to_screen(Game *g, Vec3 p, float *sx, float *sy) {
    Mat4 vp = m4_mul(g->proj, g->view);
    const float *m = vp.m;
    float cw = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (cw <= 0.02f) return 0;
    float cx = (m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12]) / cw;
    float cy = (m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13]) / cw;
    if (cx < -1.2f || cx > 1.2f || cy < -1.2f || cy > 1.2f) return 0;
    *sx = (cx * 0.5f + 0.5f) * (float)g->w;
    *sy = (1.f - (cy * 0.5f + 0.5f)) * (float)g->h;
    return 1;
}

/* ── Sun + atmosphere: the 24-minute day/night cycle (Spec 6.7). ──────────
   day_t ∈ [0,1): 0 = 06:00 dawn, 0.25 = noon, 0.5 = 18:00 dusk, 0.75 = midnight.
   A three-stop colour script (night / golden / day) drives sun, ambient,
   hemisphere fill and fog — Act I "jade and gold" (Art Direction 43.1) means
   the golden stops are pushed warm, never gray. Pure function: smoke_island
   verifies the envelope without rendering a single frame. */
#define DAY_LENGTH_S 1440.0f          /* 24 minutes of real time per day */
static void setup_light(Game *g);     /* fwd: game_set_time applies the palette */

static Vec3 v3_lerp3(Vec3 a, Vec3 b, float t) {
    return v3(dh_lerp(a.x, b.x, t), dh_lerp(a.y, b.y, t), dh_lerp(a.z, b.z, t));
}

void day_palette(float day_t, SceneLight *L) {
    if (!L) return;
    day_t = day_t - floorf(day_t);                      /* wrap into [0,1) */
    float a  = (day_t - 0.25f) * 6.2831853f;            /* noon at angle 0 */
    float el = cosf(a);                                 /* +1 noon, -1 midnight */
    float hz = sinf(a);                                 /* -1 dawn, +1 dusk */

    float k = dh_clampf((el + 0.12f) / 0.40f, 0.f, 1.f);/* 0 night → 1 day */
    /* golden hour owns the horizon: full strength as the sun nears it, but
       gated by daylight so the moon never turns orange (Act I jade & gold). */
    float golden = dh_clampf(1.f - fabsf(el) / 0.45f, 0.f, 1.f) *
                   dh_clampf(k * 3.f, 0.f, 1.f);

    /* sun travels east → west by day; a cool high "moon" takes the night shift */
    Vec3 sun_day   = v3_norm(v3(0.55f * hz, -el * 0.90f - 0.05f, 0.35f));
    Vec3 sun_night = v3_norm(v3(-0.40f * hz, -0.50f, -0.30f));
    L->sun_dir = v3_norm(v3_lerp3(sun_night, sun_day, k));

    Vec3 day_c   = v3(1.00f, 0.96f, 0.86f);
    Vec3 gold_c  = v3(1.00f, 0.62f, 0.32f);
    Vec3 night_c = v3(0.55f, 0.62f, 0.85f);
    L->sun_color = v3_lerp3(v3_lerp3(night_c, day_c, k), gold_c, golden);
    L->sun_intensity = dh_lerp(0.18f, 1.15f, k) - golden * 0.10f;

    L->ambient     = v3_lerp3(v3(0.09f, 0.11f, 0.17f), v3(0.30f, 0.33f, 0.38f), k);
    L->hemi_sky    = v3_lerp3(v3(0.10f, 0.13f, 0.24f), v3(0.42f, 0.55f, 0.72f), k);
    L->hemi_ground = v3_lerp3(v3(0.08f, 0.08f, 0.07f), v3(0.34f, 0.32f, 0.24f), k);
    L->fog_color   = v3_lerp3(v3_lerp3(v3(0.05f, 0.07f, 0.12f),
                                       v3(0.72f, 0.80f, 0.88f), k),
                              v3(0.88f, 0.70f, 0.50f), golden);
}

void game_set_time(Game *g, float day_t) {
    if (!g) return;
    g->day_t = day_t - floorf(day_t);
    setup_light(g);
}

/* Advance the clock and re-key the atmosphere. Runs in PLAY and MENU so the
   title screen drifts through the same sky the player will live in. */
static void day_night_update(Game *g, float dt) {
    g->day_t += dt / DAY_LENGTH_S;
    if (g->day_t >= 1.f) g->day_t -= floorf(g->day_t);
    Settings *s = settings();
    float nn = s->draw_distance * 1.2f, nf = s->draw_distance * 2.4f;
    day_palette(g->day_t, &g->light);
    g->light.fog_enabled = 1;
    g->light.fog_near = nn;
    g->light.fog_far  = nf;
    /* night = a darker world to see AND to be seen in (§82): enemy perception
       consumes sun intensity as the player-exposure term. */
}

static void setup_light(Game *g) {
    SceneLight *L = &g->light;
    memset(L, 0, sizeof(*L));
    day_palette(g->day_t, L);
    L->fog_enabled = 1;
    L->point_count = 0;
    Settings *s = settings();
    L->fog_near = s->draw_distance * 1.2f;
    L->fog_far  = s->draw_distance * 2.4f;   /* matches rend_should_draw's cull */
}

void game_apply_settings(Game *g) {
    RendState *r = rend();
    Settings *s = settings();
    if (!r || !s) return;
    r->render_scale    = s->render_scale;
    r->shadows_on      = s->shadows;
    r->ssao_on         = s->ssao;
    r->motion_blur_on  = s->motion_blur;
    r->aa_mode         = s->aa_mode;
    r->hd_textures     = s->hd_textures;
    r->photosensitivity= s->photosensitivity;
    r->colorblind_mode = s->colorblind_mode;
    r->brightness      = s->brightness;
    r->potato_hud      = s->potato_hud;
    g->cam_fov         = s->fov;
    g->show_hud        = !s->clean_capture;
    setup_light(g);
}

/* ══════════════════════════════ lifecycle ═══════════════════════════════ */

/* ══════════════════════ act II: Meridian City (M4) ══════════════════════
   Spec 4.3 keeps ONE streaming world per act: loading the city frees the
   island content and rebuilds on the same heightfield footprint. Static
   geometry lives here (it needs prop_box/prop_visual); the living city is
   city.c. */

static void city_paint_map(Game *g) {
    float cell = WORLD_SIZE / (float)MAPW;
    for (int rz = 0; rz < MAPW; rz++)
        for (int cx = 0; cx < MAPW; cx++) {
            float wx = (cx + 0.5f) * cell, wz = (rz + 0.5f) * cell;
            int f = city_feature_at(&g->city, wx, wz);
            float r, gg, b;
            if (f == 1)      { r = 58.f;  gg = 58.f;  b = 64.f;  }  /* asphalt */
            else if (f == 2) { r = 96.f;  gg = 84.f;  b = 70.f;  }  /* blocks  */
            else if (f == 3) { r = 40.f;  gg = 92.f;  b = 130.f; }  /* harbor  */
            else {
                float h = terrain_height(&g->terrain, wx, wz);
                if (h < 0.6f) { r = 44.f; gg = 112.f; b = 142.f; }
                else          { r = 78.f; gg = 132.f; b = 62.f;  }
            }
            g->map_cell[rz * MAPW + cx] = 0xFF000000u |
                (((uint32_t)b & 255u) << 16) | (((uint32_t)gg & 255u) << 8) |
                ((uint32_t)r & 255u);
        }
}

static void city_build_props(Game *g) {
    City *c = &g->city;
    Rng *r = &g->rng;
    /* 8 blocks × 4 buildings — downtown (north row) grows towers (§6 stylized) */
    static const uint32_t FACADE[5] = {
        0xFF8A8478u, 0xFF9A9080u, 0xFF706A64u, 0xFFA09888u, 0xFF5E6A72u
    };
    for (int i = 0; i < CITY_BLOCKS; i++) {
        Vec2 bc = c->blocks[i];
        int downtown = (bc.y < 500.0f);
        for (int k = 0; k < 4; k++) {
            float ox = ((k & 1) ? 1.0f : -1.0f) * (11.0f + rng_f(r) * 3.0f);
            float oz = ((k & 2) ? 1.0f : -1.0f) * (24.0f + rng_f(r) * 14.0f);
            float hx = 8.0f + rng_f(r) * 3.5f;
            float hz = 12.0f + rng_f(r) * 8.0f;
            float hh = (downtown ? 16.0f : 8.0f) + rng_f(r) * (downtown ? 26.0f : 12.0f);
            Vec3 ctr = v3(bc.x + ox, CITY_GROUND_Y + hh * 0.5f, bc.y + oz);
            uint32_t col = FACADE[(int)(rng_f(r) * 5.0f) % 5];
            prop_box(g, ctr, v3(hx, hh * 0.5f, hz), proc_tex.concrete, col, 0);
            if (k & 1)             /* rooftop unit (deterministic count) */
                prop_visual(g, v3(ctr.x + hx * 0.3f, CITY_GROUND_Y + hh + 0.6f, ctr.z),
                            v3(1.6f, 0.6f, 1.6f), proc_tex.metal, 0xFF707478u, 0, 0);
        }
        /* sidewalk slab (draw-only curb) */
        prop_visual(g, v3(bc.x, CITY_GROUND_Y + 0.10f, bc.y),
                    v3(c->block_rx + 2.5f, 0.10f, c->block_rz + 2.5f),
                    proc_tex.concrete, 0xFF7A766Eu, 0, 0);
    }
    for (int i = 0; i < 3; i++) {     /* lane stripes */
        prop_visual(g, v3(c->avenues[i], CITY_GROUND_Y + 0.05f, 500.f), v3(0.12f, 0.02f, 128.f),
                    -1, 0xFF50C8E8u, 0, 0);
        prop_visual(g, v3(500.f, CITY_GROUND_Y + 0.05f, c->streets[i]), v3(128.f, 0.02f, 0.12f),
                    -1, 0xFF50C8E8u, 0, 0);
    }
    /* harbor stub: boardwalk + two warehouses + pilings */
    prop_box(g, v3(500.f, 3.7f, 651.f), v3(60.f, 0.2f, 9.f), proc_tex.wood, 0xFF9A8A70u, 0);
    prop_box(g, v3(432.f, CITY_GROUND_Y + 4.f, 642.f), v3(14.f, 4.f, 8.f),
             proc_tex.metal, 0xFF6A7A82u, 0);
    prop_box(g, v3(568.f, CITY_GROUND_Y + 4.f, 642.f), v3(14.f, 4.f, 8.f),
             proc_tex.metal, 0xFF7A6A62u, 0);
    for (int i = 0; i < 7; i++)
        prop_visual(g, v3(448.f + i * 17.f, 1.8f, 658.f), v3(0.5f, 2.f, 0.5f),
                    proc_tex.wood, 0xFF6A5A48u, 0, 0);
    /* job board + hot-dog stand (interactables; city.c checks the ranges) */
    prop_visual(g, v3(c->job_board.x, CITY_GROUND_Y + 1.5f, c->job_board.z),
                v3(1.3f, 0.9f, 0.08f), proc_tex.wood, 0xFFC8B890u, 0, 0);
    prop_visual(g, v3(c->job_board.x - 1.1f, CITY_GROUND_Y + 0.8f, c->job_board.z),
                v3(0.08f, 0.8f, 0.08f), proc_tex.wood, 0xFF8A7A60u, 0, 0);
    prop_visual(g, v3(c->job_board.x + 1.1f, CITY_GROUND_Y + 0.8f, c->job_board.z),
                v3(0.08f, 0.8f, 0.08f), proc_tex.wood, 0xFF8A7A60u, 0, 0);
    prop_visual(g, v3(c->hotdog_stand.x, CITY_GROUND_Y + 0.55f, c->hotdog_stand.z),
                v3(0.9f, 0.55f, 0.5f), proc_tex.metal, 0xFFD0D4D8u, 0, 0);
    prop_visual(g, v3(c->hotdog_stand.x, CITY_GROUND_Y + 2.1f, c->hotdog_stand.z),
                v3(1.5f, 0.06f, 1.5f), proc_tex.fabric, 0xFF5050C0u, 1, 0);
    prop_visual(g, v3(c->hotdog_stand.x + 1.2f, CITY_GROUND_Y + 1.05f, c->hotdog_stand.z),
                v3(0.05f, 1.05f, 0.05f), proc_tex.metal, 0xFF909498u, 0, 0);
    /* invisible boundary walls (traffic/peds/player stay in the slice) */
    obstacles_add_box(&g->obs, v3(362.f, 8.f, 500.f), v3(2.f, 8.f, 145.f), 0);
    obstacles_add_box(&g->obs, v3(638.f, 8.f, 500.f), v3(2.f, 8.f, 145.f), 0);
    obstacles_add_box(&g->obs, v3(500.f, 8.f, 361.f), v3(145.f, 8.f, 2.f), 0);
    obstacles_add_box(&g->obs, v3(401.f, 8.f, 646.f), v3(41.f, 8.f, 1.5f), 0);
    obstacles_add_box(&g->obs, v3(599.f, 8.f, 646.f), v3(41.f, 8.f, 1.5f), 0);
    obstacles_add_box(&g->obs, v3(500.f, 5.f, 661.f), v3(60.f, 1.2f, 0.4f), 0); /* boardwalk rail */
}

void game_load_city(Game *g) {
    if (!g || !g->ready) return;
    DH_INFO("game", "loading act II: Meridian City (world swap, Spec 4.3)");
    game_island_store(g);        /* M5: the island remembers what you did */
    g->act = 1;
    g->stat_ferries++;
    g->in_vehicle = -1;
    /* clear act-I dynamic content */
    g->enemies.count = 0; g->enemies.alive_count = 0;
    g->critter_count = 0;
    g->pickups.count = 0;
    g->tracers.count = 0;
    g->fx_count = 0;
    g->outpost.built = 0;
    g->arena_active = 0; g->arena_total = 0;
    g->prop_count = 0;
    g->zips.count = 0;
    obstacles_init(&g->obs);
    /* rebuild the heightfield: bay water, city plate, harbor channel */
    terrain_release_chunks(&g->terrain);
    terrain_init(&g->terrain, WORLD_RES, WORLD_SIZE, g->seed);
    terrain_flatten(&g->terrain, v3(500.f, 0.f, 500.f), 500.f, 500.f, -2.0f, 20.f);
    terrain_flatten(&g->terrain, v3(500.f, 0.f, 500.f), 190.f, 190.f, CITY_GROUND_Y, 12.f);
    terrain_flatten(&g->terrain, v3(500.f, 0.f, 716.f), 190.f, 66.f, -1.4f, 10.f);
    g->terrain.paint_x0 = 300.f; g->terrain.paint_x1 = 700.f;
    g->terrain.paint_z0 = 300.f; g->terrain.paint_z1 = 700.f;
    g->terrain.paint_min_h = 3.0f;
    g->terrain.paint_col = 0xFF5A5652u;          /* warm-grey asphalt (§43.3 no brown soup) */
    terrain_build_chunks(&g->terrain, CHUNK_M);
    city_init(&g->city, g);
    city_build_props(g);
    sys_build_city_extras(g);
    city_paint_map(g);
    memset(g->fog, 0, sizeof g->fog);
    g->spawn = g->city.player_spawn;
    player_respawn(&g->player, g->spawn);
    g->player.yaw = 3.14159265f;   /* face the skyline, harbor at your back */
    g->player.health = g->player.health_max;
    g->respawn_t = 0.f;
    g->damage_flash = 0.f;
    game_set_time(g, g->day_t);
    game_message(g, "MERIDIAN CITY - ACT II. STEAL A CAR, LOSE THE HEAT, EAT.");
    g->message_t = 5.0f;
}

/* Return trip on Beto's ferry: rebuild act I exactly as game_init does
   (same seed = same island). Inventory and cash ride along; the outpost
   and wildlife reset (world persistence is M5 save-state work). */
void game_load_island(Game *g) {
    if (!g || !g->ready) return;
    DH_INFO("game", "loading act I: Isla Sombra (ferry return)");
    g->act = 0;
    g->stat_ferries++;
    g->in_vehicle = -1;
    g->city.built = 0;
    g->prop_count = 0;
    terrain_release_chunks(&g->terrain);
    terrain_init(&g->terrain, WORLD_RES, WORLD_SIZE, g->seed);
    g->terrain.paint_x1 = g->terrain.paint_x0 = 0.f;
    terrain_flatten(&g->terrain, g->course_center, 70.0f, 52.0f, g->pad_h, 12.0f);
    terrain_flatten(&g->terrain, v3(150.0f, 0.0f, 620.0f), 60.0f, 45.0f, 1.2f, 25.0f);
    obstacles_init(&g->obs);
    zips_init(&g->zips);
    build_traversal_course(g);
    enemies_init(&g->enemies);
    g->pickups.count = 0; g->tracers.count = 0; g->fx_count = 0;
    g->arena_active = 0; g->arena_total = 0;
    build_combat_arena(g);
    memset(&g->outpost, 0, sizeof g->outpost);
    build_island(g);
    sys_island_restore(g);       /* M5: captured stays captured, fog stays lifted */
    terrain_build_chunks(&g->terrain, CHUNK_M);
    g->spawn = v3(150.0f, 0.0f, 620.0f);          /* Beto drops you on the west beach */
    g->spawn.y = terrain_height(&g->terrain, g->spawn.x, g->spawn.z) + 0.1f;
    player_respawn(&g->player, g->spawn);
    g->player.zips = &g->zips;
    g->player.yaw = 1.5708f;
    g->player.health = g->player.health_max;
    game_apply_skills(g);
    g->player.health = g->player.health_max;
    game_message(g, "ISLA SOMBRA - BETO'S FERRY DROPS YOU ON THE WEST BEACH");
    g->message_t = 4.0f;
}

int game_init(Game *g, int w, int h, int backend, uint32_t seed) {
    if (!g) return 0;
    memset(g, 0, sizeof(*g));
    g->w = w; g->h = h;
    g->backend = backend;
    g->seed = seed ? seed : 0xD1CE5EEDu;
    g->mode = GM_MENU;
    g->cam_fov = 78.0f;
    g->show_debug = 1;
    g->show_hud = 1;
    g->ocean_tile = OCEAN_TILE;
    g->act = 0;
    g->in_vehicle = -1;
    g->cash = 0;

    settings_defaults(settings());
    settings_load(settings());          /* keeps user prefs across runs (6.11) */
    settings_clamp(settings());
    if (!rend_init(w, h, backend == REND_GL11 ? REND_GL11 : REND_SOFT)) {
        DH_ERROR("game", "renderer init failed");
        return 0;
    }
    font_init();
    proc_tex_build();

    g->mesh_box  = build_box_mesh();
    g->mesh_quad = build_quad_mesh();
    g->mesh_sky  = build_sky_mesh();
    if (g->mesh_box < 0 || g->mesh_quad < 0 || g->mesh_sky < 0) {
        DH_ERROR("game", "core mesh build failed");
        return 0;
    }

    /* ── terrain ── */
    terrain_init(&g->terrain, WORLD_RES, WORLD_SIZE, g->seed);
    if (!g->terrain.h) { DH_ERROR("game", "terrain alloc failed"); return 0; }

    /* Course pad: flatten first, then sculpt the pool into it. */
    g->course_center = v3(330.0f, 0.0f, 560.0f);
    g->pad_h = 8.0f;
    terrain_flatten(&g->terrain, g->course_center, 70.0f, 52.0f, g->pad_h, 12.0f);
    /* A landing beach west of the pad so the shore is reachable on foot. */
    terrain_flatten(&g->terrain, v3(150.0f, 0.0f, 620.0f), 60.0f, 45.0f, 1.2f, 25.0f);

    obstacles_init(&g->obs);
    zips_init(&g->zips);
    build_traversal_course(g);

    /* ── combat (M2) ── */
    rng_seed(&g->rng, g->seed ^ 0xC0FFEEu);
    weapons_load(NULL);
    enemies_init(&g->enemies);
    g->pickups.count = 0;
    g->tracers.count = 0;
    g->fx_count = 0;
    for (int i = 0; i < WPN_SLOT_MAX; i++) { g->wpn_def[i] = -1; g->wpn_mag[i] = 0; }
    g->wpn_slot = 0;
    build_combat_arena(g);

    /* ── island vertical slice (M3): outpost, mast, jungle, wildlife, fog ── */
    g->day_t = 0.08f;                 /* ~07:55 — Act I jade-and-gold morning */
    prog_init(&g->prog);              /* M5: xp / skills / economy */
    economy_load(&g->econ);
    missions_load(&g->missions);     /* M6: story campaign (data file or embedded) */
    mission_reset(&g->ms);
    save_init();
    build_island(g);

    int chunks = terrain_build_chunks(&g->terrain, CHUNK_M);
    if (chunks <= 0) { DH_ERROR("game", "terrain meshing failed"); return 0; }

    /* ── player ── */
    g->spawn = v3(g->course_center.x, 0.0f, g->course_center.z - 4.0f);
    player_init(&g->player, &g->terrain, &g->obs, g->spawn);
    g->player.zips = &g->zips;
    g->player.yaw = 0.0f;             /* facing +z, straight down the course */
    g->player.pitch = -0.05f;

    game_apply_settings(g);
    game_apply_skills(g);
    g->ready = 1;
    poncho_init(g);
    DH_INFO("game", "M1 world ready: %d chunks, %d obstacles, %d textures, backend %s",
            chunks, g->obs.count, tex_count(), rend_backend_name());
    game_message(g, "TRAVERSAL COURSE - run south");
    if (!audio_init()) DH_WARN("audio", "SFX bank synthesis failed - running silent");
    return 1;
}

void game_start_play(Game *g) {
    if (!g || !g->ready) return;
    g->mode = GM_PLAY;
    g->message_t = 5.0f;
    dh_strcpy_safe(g->message, sizeof(g->message),
                   "WASD move - SHIFT sprint - SPACE jump/vault - CTRL crouch - V roll");
    DH_INFO("game", "entering PLAY");
}

void game_free(Game *g) {
    if (!g) return;
    terrain_free(&g->terrain);
    obstacles_init(&g->obs);
    zips_init(&g->zips);
    g->prop_count = 0;
    g->ready = 0;
    font_shutdown();
    rend_shutdown();
}

/* ══════════════════════════════ simulation ══════════════════════════════ */

#include "systems.inl"   /* M5 systems & progression glue */
#include "story.inl"     /* M6 story missions glue */
#include "polish.inl"    /* M7 audio cues / flocks / camera FX / difficulty */

static void game_frame_inner(Game *g, const PlatInput *in, float dt);
void game_frame(Game *g, const PlatInput *in, float dt) {
    if (!g || !g->ready || !in) return;
    game_frame_inner(g, in, dt);
    pol_frame(g, dt);                        /* M7: cues after the sim settles */
}
static void game_frame_inner(Game *g, const PlatInput *in, float dt) {
    if (!g || !g->ready || !in) return;
    g->time += dt;
    g->frame_index++;
    if (g->message_t > 0.0f) g->message_t -= dt;

    Settings *s = settings();

    if (in->quit) { g->quit = 1; return; }

    if (g->mode == GM_MENU) {
        if ((in->pressed & BTN_LOAD) && save_slot_exists(save_slot_index(0, 1))) {
            if (game_load(g, 0, 1)) return;
            game_message(g, "SAVE COULD NOT BE READ - starting fresh"); g->message_t = 3.f;
        }
        if ((in->pressed & (BTN_JUMP | BTN_USE | BTN_MENU)) ||
            (in->buttons & BTN_JUMP)) {
            game_start_play(g);
        }
        /* idle camera drift so the menu is never a frozen image */
        g->player.yaw += 0.06f * dt;
        day_night_update(g, dt);
        return;
    }

    if (g->mode == GM_PAUSE) {
        if (in->pressed & (BTN_PAUSE | BTN_MENU)) g->mode = GM_PLAY;
        return;
    }

    if (in->pressed & BTN_PAUSE) { g->mode = GM_PAUSE; return; }
    if (g->mode == GM_PHOTO) { photo_frame(g, in, dt); return; }   /* M9: sim frozen */

    /* ── M5 modal screens pause the simulation (map / character / shop) ── */
    if (g->ui == UI_HERALD) { herald_input(g, in); return; }
    if (g->ui == UI_FEATS) { feats_input(g, in); return; }
    if (g->ui != UI_NONE) { sys_ui_input(g, in); return; }
    if (in->pressed & BTN_HERALD) { g->ui = UI_HERALD; g->leg.view = g->leg.n - 1; return; }
    if (in->pressed & BTN_FEATS) { g->ui = UI_FEATS; g->ui_sel = 0; return; }
    if (in->pressed & BTN_MAP)  { g->ui = UI_MAP;  g->ui_sel = 0; return; }
    if (in->pressed & BTN_CHAR) { g->ui = UI_CHAR; g->ui_sel = 0; g->ui_tab = 0; return; }
    if (in->pressed & BTN_DEBUG) g->show_debug = !g->show_debug;

    /* ── combat sim runs first: ADS/recoil state then feeds movement ── */
    combat_update(g, in, dt);

    /* ── M3: the island lives whether or not you're shooting ── */
    day_night_update(g, dt);
    fog_reveal(g, g->player.pos.x, g->player.pos.z, 95.f);

    /* ── map abstract buttons → player intent ── */
    PlayerInput pi;
    memset(&pi, 0, sizeof(pi));
    pi.mx = dh_clampf(in->mx, -1.0f, 1.0f);
    pi.my = dh_clampf(in->my, -1.0f, 1.0f);
    /* ADS movement penalty: aiming trades mobility for accuracy (§6.2) */
    {
        float ads_mul = 1.0f - 0.45f * g->ads_k;
        pi.mx *= ads_mul;
        pi.my *= ads_mul;
    }
    float sx = s->invert_x ? -1.0f : 1.0f;
    float sy = s->invert_y ? -1.0f : 1.0f;
    /* PlatInput look deltas are RADIANS per frame — the platform converts its
       device units (mouse pixels, stick deflection) before publishing, so the
       simulation stays device-independent and headless runs are reproducible.
       Binoculars damp the look rate 65% so 20° zoom stays aimable (§44). */
    float look_damp = 1.0f - 0.65f * g->binoc_k;
    pi.look_dx = in->look_dx * s->sensitivity * sx * look_damp;
    pi.look_dy = in->look_dy * s->sensitivity * sy * look_damp;
    pi.jump        = (in->buttons & BTN_JUMP)   ? 1 : 0;
    pi.jump_pressed= (in->pressed & BTN_JUMP)   ? 1 : 0;
    pi.sprint      = (in->buttons & BTN_SPRINT) ? 1 : 0;
    pi.crouch      = (in->buttons & BTN_CROUCH) ? 1 : 0;
    pi.roll        = (in->pressed & BTN_ROLL)   ? 1 : 0;
    pi.walk_only   = (in->buttons & BTN_WALK)   ? 1 : 0;
    pi.use_pressed = (in->pressed & BTN_USE)    ? 1 : 0;

    /* ── simulate ── */
    PlayerStance before = g->player.stance;
    int jumps_before = g->player.stats.jumps;
    int zips_before = g->player.stats.zip_rides;
    int mantles_before = g->player.stats.mantles + g->player.stats.vaults;
    float health_before = g->player.health;

    int driving = (g->act == 1 && g->in_vehicle >= 0);
    if (driving) {
        /* M4: the vehicle owns the body; the player only looks around (arcade
           driving, Section 38 — no on-foot physics while seated). */
        g->player.yaw += pi.look_dx;
        g->player.pitch = dh_clampf(g->player.pitch + pi.look_dy, -1.45f, 1.45f);
    } else {
        player_input(&g->player, &pi, dt);
    }

    /* ── feedback on notable events (M2 replaces this with SFX/particles) ── */
    (void)jumps_before;
    if (g->player.stats.mantles + g->player.stats.vaults > mantles_before) {
        game_message(g, g->player.stats.vaults ? "VAULT" : "MANTLE");
        g->message_t = 1.2f;
    }
    if (g->player.stats.zip_rides > zips_before) {
        game_message(g, "ZIPLINE - SPACE drops, CTRL brakes");
        g->message_t = 2.5f;
    }
    if (health_before - g->player.health > 0.5f) {
        game_message(g, "FALL DAMAGE -%.0f HP (roll within %.2fs to cancel)",
                     health_before - g->player.health, (double)PL_ROLL_CANCEL);
        g->message_t = 2.5f;
    }
    if (before != PL_ST_SWIM && g->player.stance == PL_ST_SWIM) {
        game_message(g, "SWIMMING - CTRL dives, SPACE surfaces, breath %.0fs",
                     g->player.breath_max);
        g->message_t = 3.0f;
    }
    if (g->player.breath < 6.0f && g->player.stance == PL_ST_SWIM &&
        (int)(g->player.breath * 2.0f) != (int)((g->player.breath + dt) * 2.0f)) {
        game_message(g, "OUT OF AIR");
        g->message_t = 1.0f;
    }

    /* ── death → respawn at last checkpoint; loadout kept, dead stay dead (§6.2) ── */
    if (player_is_down(&g->player)) {
        g->respawn_t += dt;
        g->damage_flash = 1.0f;
        if (g->respawn_t > 2.5f) {
            g->respawn_t = 0.0f;
            if (g->act == 1 && g->in_vehicle >= 0) {   /* die driving = wake up on foot */
                g->city.veh[g->in_vehicle].state = VS_PARKED;
                g->in_vehicle = -1;
            }
            player_respawn(&g->player, g->spawn);
            if (g->story) { MissionCtx mc; story_ctx(g, &mc); mission_on_death(&g->ms, &g->missions, &mc); }
            g->damage_flash = 0.0f;
            g->reloading = 0;
            game_message(g, "RESPAWNED at checkpoint - loadout kept, %d hostiles remain",
                         g->enemies.alive_count);
            g->message_t = 3.0f;
        }
    } else {
        g->respawn_t = 0.0f;
    }

    /* ── M5: systems tick (heal, safehouse/vendor/skin interact, alert) ── */
    sys_frame(g, in, dt);
    poncho_frame(g, in, dt);                 /* M9 companion */

    /* ── M4: Meridian City lives when act II is loaded ── */
    if (g->act == 1) city_frame(&g->city, g, in, dt);
    legend_frame(g, dt);                     /* M11 stature + Herald */
    feats_frame(g, dt);                      /* M12 feats */
    cob_frame(g, dt);                        /* M13 bounty squad */

    /* ── photo mode / debug toggles ── */
    if ((in->pressed & BTN_PHOTO) && game_photo_enter(g)) {
        game_message(g, "PHOTO MODE - WORLD PAUSED");
        g->message_t = 2.f;
    }

    /* M2 dev/DoD hotkey: deploy to the combat arena (30 hostiles) */
    if (in->pressed & BTN_ARENA && g->act == 0) {
        game_arena_start(g, 30);
    }
    /* M4: Beto's ferry (Spec 4.3 act travel). Blocked while wanted. */
    if (in->pressed & BTN_FERRY) {
        if (g->act == 1 && g->city.heat.stars > 0) {
            game_message(g, "BETO WON'T SAIL WITH COPS ON YOUR TAIL - LOSE THE HEAT");
            g->message_t = 3.0f;
        } else if (g->act == 0) {
            game_load_city(g);
        } else {
            game_load_island(g);
        }
    }
}

/* ══════════════════════════════ rendering ═══════════════════════════════ */

static void draw_terrain(Game *g) {
    const Terrain *t = &g->terrain;
    Mat4 ident = m4_identity();
    int drawn = 0;
    for (int i = 0; i < t->chunk_count; i++) {
        const Mesh *m = mesh_get(t->chunk_ids[i]);
        if (!m) continue;
        if (!rend_should_draw(&m->center, m->radius)) continue;
        rend_mesh_lit(t->chunk_ids[i], &ident, -1, 0xFFFFFFFFu, 1.0f,
                      1, 1, 1, 0, 1);
        drawn++;
    }
    (void)drawn;
}

static void draw_water(Game *g) {
    const float sea = g->terrain.sea_level;
    float dd = settings()->draw_distance * 2.4f;
    int n = (int)(dd / OCEAN_TILE) + 1;
    if (n > 14) n = 14;
    int cx = (int)floorf(g->cam_pos.x / OCEAN_TILE);
    int cz = (int)floorf(g->cam_pos.z / OCEAN_TILE);
    int tiles = 0;
    for (int dz = -n; dz <= n; dz++) {
        for (int dx = -n; dx <= n; dx++) {
            float wx = (float)(cx + dx) * OCEAN_TILE;
            float wz = (float)(cz + dz) * OCEAN_TILE;
            if (wx < -OCEAN_TILE || wz < -OCEAN_TILE) continue;
            if (wx > WORLD_SIZE || wz > WORLD_SIZE) continue;
            /* skip tiles whose whole area is above sea level (inland) */
            float h0 = terrain_height(&g->terrain, wx + OCEAN_TILE*0.5f, wz + OCEAN_TILE*0.5f);
            if (h0 > sea + 0.6f) continue;
            Vec3 center = v3(wx + OCEAN_TILE*0.5f, sea, wz + OCEAN_TILE*0.5f);
            if (!rend_should_draw(&center, OCEAN_TILE * 0.75f)) continue;
            Mat4 model = m4_mul(m4_translate(v3(wx, sea - 0.02f, wz)),
                                m4_scale(v3(OCEAN_TILE, 1.0f, OCEAN_TILE)));
            rend_mesh_lit(g->mesh_quad, &model, proc_tex.water, 0xFFFFFFFFu, 0.88f,
                          1, 1, 0, 0, 1);
            tiles++;
        }
    }
    /* course pool */
    if (g->pool_rx > 0.0f) {
        Vec3 pc = g->pool_center;
        if (rend_should_draw(&pc, dh_maxf(g->pool_rx, g->pool_rz) * 1.5f)) {
            Mat4 model = m4_mul(m4_translate(v3(pc.x - g->pool_rx, g->pool_level - 0.01f,
                                                pc.z - g->pool_rz)),
                                m4_scale(v3(g->pool_rx * 2.0f, 1.0f, g->pool_rz * 2.0f)));
            rend_mesh_lit(g->mesh_quad, &model, proc_tex.water, 0xFFE8F0D8u, 0.90f,
                          1, 1, 0, 0, 1);
        }
    }
    (void)tiles;
}

static void draw_props(Game *g) {
    int submitted = 0;
    for (int i = 0; i < g->prop_count; i++) {
        const PropDraw *d = &g->props[i];
        if (!rend_should_draw(&d->center, d->radius)) continue;
        rend_mesh_lit(g->mesh_box, &d->model, d->tex, d->color, d->alpha,
                      1, 1, 1, d->alpha_test, d->two_sided);
        submitted++;
    }
    (void)submitted;
}

/* Sky tint from the day phase (6.7). The dome's vertex gradient is baked
   (jade-and-gold zenith→horizon); this per-frame tint is what makes the same
   dome read as deep tropical blue at noon, ember-orange at golden hour, and
   near-black indigo at night. Multiplied over the vertex colours. */
static uint32_t sky_tint(Game *g) {
    float a  = (g->day_t - 0.25f) * 6.2831853f;
    float el = cosf(a);
    float k  = dh_clampf((el + 0.12f) / 0.40f, 0.f, 1.f);
    float golden = dh_clampf(1.f - fabsf(el) / 0.45f, 0.f, 1.f) *
                   dh_clampf(k * 3.f, 0.f, 1.f);
    Vec3 t = v3_lerp3(v3(0.20f, 0.24f, 0.44f), v3(1.f, 1.f, 1.f), k);
    t = v3_lerp3(t, v3(1.00f, 0.72f, 0.46f), golden * 0.85f);
    uint32_t R = (uint32_t)dh_clampi((int)(t.x * 255.f), 0, 255);
    uint32_t G = (uint32_t)dh_clampi((int)(t.y * 255.f), 0, 255);
    uint32_t B = (uint32_t)dh_clampi((int)(t.z * 255.f), 0, 255);
    return 0xFF000000u | (B << 16) | (G << 8) | R;
}

static void draw_sky(Game *g) {
    Mat4 model = m4_mul(m4_translate(g->cam_pos), m4_scale1(SKY_RADIUS));
    rend_mesh_lit(g->mesh_sky, &model, -1, sky_tint(g), 1.0f, 0, 0, 0, 0, 1);
}

/* ── HUD ───────────────────────────────────────────────────────────────── */
static void bar(float x, float y, float w, float h, float frac, uint32_t fg, uint32_t bg) {
    frac = dh_clampf(frac, 0.0f, 1.0f);
    rend_quad2d(x, y, w, h, -1, 0,0,1,1, bg);
    if (frac > 0.0f) rend_quad2d(x + 1.0f, y + 1.0f, (w - 2.0f) * frac, h - 2.0f,
                                 -1, 0,0,1,1, fg);
}

/* Fog-of-war minimap (§52): cached cell colours + explore mask, north-up,
   tagged hostiles + landmark icons + a 24 h clock. Pure 2D quads so it runs
   identically on soft and GL11. */
static void draw_minimap(Game *g) {
    Settings *s = settings();
    float ui = s->ui_scale;
    float W = (float)g->w;
    float pad = 12.0f * ui;
    float mm = 132.0f * ui;
    float mx = W - pad - mm, my = pad;
    float fs = 2.0f * ui;
    const Player *p = &g->player;
    const Outpost *o = &g->outpost;

    rend_quad2d(mx - 3.0f*ui, my - 3.0f*ui, mm + 6.0f*ui, mm + 6.0f*ui,
                -1, 0,0,1,1, 0xC00C1014u);

    const float world_r = 96.f;                 /* metres per half-map */
    float scale = mm / (2.f * world_r);
    float cell = WORLD_SIZE / (float)MAPW;
    int ic = (int)(cell * scale) + 1;           /* cell size in px */
    int span = (int)(world_r / cell) + 1;
    int pcx = dh_clampi((int)(p->pos.x / cell), 0, MAPW - 1);
    int pcz = dh_clampi((int)(p->pos.z / cell), 0, MAPW - 1);
    for (int rz = pcz - span; rz <= pcz + span; rz++) {
        for (int cx = pcx - span; cx <= pcx + span; cx++) {
            if (cx < 0 || cx >= MAPW || rz < 0 || rz >= MAPW) continue;
            float wx = (cx + 0.5f) * cell, wz = (rz + 0.5f) * cell;
            float sx = mx + mm * 0.5f + (wx - p->pos.x) * scale;
            float sy = my + mm * 0.5f + (wz - p->pos.z) * scale;
            if (sx + ic < mx || sx > mx + mm || sy + ic < my || sy > my + mm) continue;
            uint32_t col = g->fog[rz * MAPW + cx] ? g->map_cell[rz * MAPW + cx]
                                                  : 0xF010161Au;
            rend_quad2d(sx - ic * 0.5f, sy - ic * 0.5f, (float)ic + 1.f, (float)ic + 1.f,
                        -1, 0,0,1,1, col);
        }
    }

    #define MM_ICON(wx, wz, col, sz) do {                                    \
        float ix = mx + mm * 0.5f + ((wx) - p->pos.x) * scale;               \
        float iy = my + mm * 0.5f + ((wz) - p->pos.z) * scale;               \
        if (ix >= mx - 2.f && ix <= mx + mm + 2.f &&                         \
            iy >= my - 2.f && iy <= my + mm + 2.f)                           \
            rend_quad2d(ix - (sz) * 0.5f, iy - (sz) * 0.5f, sz, sz,          \
                        -1, 0,0,1,1, col);                                   \
    } while (0)

    if (o->built) {
        int ocx = dh_clampi((int)(o->center.x / cell), 0, MAPW - 1);
        int ocz = dh_clampi((int)(o->center.z / cell), 0, MAPW - 1);
        int outpost_known = o->mast_synced || g->fog[ocz * MAPW + ocx] >= 1;
        if (outpost_known)
            MM_ICON(o->center.x, o->center.z, o->captured ? C_JADE : C_RED, 5.0f * ui);
        int mcx = dh_clampi((int)(o->mast_pos.x / cell), 0, MAPW - 1);
        int mcz = dh_clampi((int)(o->mast_pos.z / cell), 0, MAPW - 1);
        if (o->mast_synced || g->fog[mcz * MAPW + mcx] >= 1)
            MM_ICON(o->mast_pos.x, o->mast_pos.z,
                    o->mast_synced ? C_JADE : 0xFFC8C8C8u, 4.0f * ui);
    }
    for (int i = 0; i < g->enemies.count; i++) {
        const Enemy *e = &g->enemies.v[i];
        if (e->tagged && e->state != EN_DEAD)
            MM_ICON(e->pos.x, e->pos.z, 0xFF4040F0u, 3.0f * ui);
    }
    if (g->act == 1) {   /* M4: pursuing cruisers + heli on the minimap */
        for (int i = g->city.chase0; i < g->city.veh_count; i++) {
            const Vehicle *cv = &g->city.veh[i];
            if (cv->state == VS_CHASE) MM_ICON(cv->pos.x, cv->pos.z, 0xC04040F0u, 3.0f * ui);
        }
        if (g->city.heli_active)
            MM_ICON(g->player.pos.x + cosf(g->city.heli_ang) * 18.0f,
                    g->player.pos.z + sinf(g->city.heli_ang) * 18.0f,
                    0xC04040F0u, 4.0f * ui);
    }
    {   /* M6: mission waypoint (clamped to the plate edge when far) */
        Vec3 wp;
        if (story_waypoint(g, &wp)) {
            float dx = (wp.x - p->pos.x) * scale, dz = (wp.z - p->pos.z) * scale;
            float lim = mm * 0.5f - 4.0f * ui, mxv = fmaxf(fabsf(dx), fabsf(dz));
            if (mxv > lim) { dx *= lim / mxv; dz *= lim / mxv; }
            rend_quad2d(mx + mm * 0.5f + dx - 3.0f * ui, my + mm * 0.5f + dz - 3.0f * ui,
                        6.0f * ui, 6.0f * ui, -1, 0,0,1,1, 0xFF00C8FFu);
        }
    }
    #undef MM_ICON

    /* player: white dot + gold heading tick */
    float hx = sinf(p->yaw), hz = cosf(p->yaw);
    rend_quad2d(mx + mm*0.5f - 2.0f*ui, my + mm*0.5f - 2.0f*ui, 4.0f*ui, 4.0f*ui,
                -1, 0,0,1,1, C_WHITE);
    rend_quad2d(mx + mm*0.5f + hx*7.0f*ui - ui, my + mm*0.5f + hz*7.0f*ui - ui,
                2.0f*ui, 2.0f*ui, -1, 0,0,1,1, C_GOLD);
    font_text_shadow(mx + mm*0.5f - 3.0f*ui, my + 2.0f*ui, fs*0.7f, "N", C_DIM);
    /* 24 h clock, bottom-right of the plate (Spec 6.7 day/night) */
    char buf[16];
    float hours = fmodf(6.f + g->day_t * 24.f, 24.f);
    snprintf(buf, sizeof buf, "%02d:%02d", (int)hours, (int)(fmodf(hours, 1.f) * 60.f));
    font_text_shadow(mx + mm - 42.0f*ui, my + mm - 13.0f*ui, fs*0.85f, buf, C_WHITE);
}

#include "systems_ui.inl"   /* M5 screens + HUD extras */
#include "poncho.inl"       /* M9 companion */
#include "photo.inl"        /* M9 photo mode */
#include "herald.inl"       /* M11 stature + the Herald */
#include "feats.inl"        /* M12 feats poster */
#include "cobradores.inl"   /* M13 bounty squad */

static void draw_hud(Game *g) {
    Settings *s = settings();
    float ui = s->ui_scale;
    float fs = 2.0f * ui;      /* 2 px cells = crisp at 720p, scaled by ui_scale */
    float W = (float)g->w, H = (float)g->h;
    float pad = 12.0f * ui;
    Player *p = &g->player;

    /* crosshair */
    if (s->crosshair_on && g->mode == GM_PLAY) {
        float cx = W * 0.5f, cy = H * 0.5f, len = 5.0f * ui, gap = 3.0f * ui;
        uint32_t cc = s->reticle_color == 1 ? C_GOLD : (s->reticle_color == 2 ? C_JADE : C_WHITE);
        rend_quad2d(cx - gap - len, cy - 0.5f*ui, len, 1.0f*ui, -1, 0,0,1,1, cc);
        rend_quad2d(cx + gap,       cy - 0.5f*ui, len, 1.0f*ui, -1, 0,0,1,1, cc);
        rend_quad2d(cx - 0.5f*ui, cy - gap - len, 1.0f*ui, len, -1, 0,0,1,1, cc);
        rend_quad2d(cx - 0.5f*ui, cy + gap,       1.0f*ui, len, -1, 0,0,1,1, cc);
    }

    /* top-right: fog-of-war minimap + clock (M3) */
    if (g->mode == GM_PLAY) draw_minimap(g);

    /* top-left: mode + course objective */
    font_text_shadow(pad, pad, fs, "DIVIDED HORIZON", C_GOLD);
    char buf[160];
    snprintf(buf, sizeof(buf), "%s  -  %s  -  %s",
             game_mode_name(g->mode),
             g->act == 1 ? "M4 MERIDIAN CITY" : "M3 ISLA SOMBRA",
             player_stance_name(p->stance));
    font_text_shadow(pad, pad + 12.0f * fs, fs * 0.85f, buf, C_DIM);

    /* outpost objective (only while the site is relevant, §33 tell the player) */
    const Outpost *o = &g->outpost;
    if (o->built && g->mode == GM_PLAY &&
        v3_dist_xz(p->pos, o->center) < 130.f) {
        int alive = 0;
        for (int i = 0; i < g->enemies.count; i++)
            if (g->enemies.v[i].faction == 2 && g->enemies.v[i].state != EN_DEAD) alive++;
        uint32_t oc = o->captured ? C_JADE : (o->alarm ? C_RED : C_GOLD);
        if (o->captured)
            snprintf(buf, sizeof(buf), "%s  LIBERATED  /  ISLA SOMBRA 1/1", o->name);
        else
            snprintf(buf, sizeof(buf), "%s  /  %d HOSTILES%s%s", o->name, alive,
                     o->alarm ? "  /  " : "", o->alarm ? "ALARM RAISED" : "");
        font_text_shadow(pad, pad + 26.0f * fs, fs * 0.9f, buf, oc);
        if (!o->captured && alive == 0)
            font_text_shadow(pad, pad + 38.0f * fs, fs * 0.8f,
                             "HOLD THE YARD TO RAISE THE FLAG", C_JADE);
        else if (!o->captured && !o->alarm_destroyed && !o->alarm)
            font_text_shadow(pad, pad + 38.0f * fs, fs * 0.75f,
                             "scout: B binoculars / quiet: T takedown / loud: guns", C_DIM);
    }

    /* bottom-left: vitals */
    float by = H - pad - 46.0f * ui;
    float bw = 180.0f * ui, bh = 9.0f * ui;
    bar(pad, by, bw, bh, p->health / p->health_max, p->health > 35.0f ? C_JADE : C_RED, C_DARK);
    font_text_shadow(pad, by - 11.0f * ui, fs * 0.8f, "HEALTH", C_DIM);
    float sy2 = by + bh + 12.0f * ui;
    font_text_shadow(pad, sy2 - 10.0f * ui, fs * 0.8f, "STAMINA", C_DIM);
    bar(pad, sy2, bw, bh, p->stamina / PL_STAMINA_MAX, C_GOLD, C_DARK);
    if (p->stance == PL_ST_SWIM || p->breath < p->breath_max - 0.5f) {
        float sy3 = sy2 + bh + 12.0f * ui;
        font_text_shadow(pad, sy3 - 10.0f * ui, fs * 0.8f, "AIR", C_DIM);
        bar(pad, sy3, bw, bh, p->breath / p->breath_max, C_TEAL, C_DARK);
    }
    snprintf(buf, sizeof(buf), "%.1f m/s", sqrtf(p->vel.x*p->vel.x + p->vel.z*p->vel.z));
    font_text_shadow(pad + bw + 8.0f*ui, by - 2.0f*ui, fs * 0.8f, buf, C_DIM);

    /* ── M2: combat HUD ── */
    /* damage vignette */
    if (g->damage_flash > 0.0f) {
        /* photosensitivity: capped, dimmer edge instead of a bright pulse */
        float fa = s->photosensitivity ? dh_minf(g->damage_flash, 0.4f) * 60.0f
                                       : dh_clampf(g->damage_flash, 0.0f, 1.0f) * 80.0f;
        uint32_t a = (uint32_t)fa << 24;
        rend_quad2d(0, 0, W, H * 0.10f, -1, 0,0,1,1, a | 0x001818A0u);
        rend_quad2d(0, H * 0.90f, W, H * 0.10f, -1, 0,0,1,1, a | 0x001818A0u);
        rend_quad2d(0, 0, W * 0.07f, H, -1, 0,0,1,1, a | 0x001818A0u);
        rend_quad2d(W * 0.93f, 0, W * 0.07f, H, -1, 0,0,1,1, a | 0x001818A0u);
    }
    /* hitmarker: 45° ticks around the crosshair; red X on a kill */
    if (g->hitmark_t > 0.0f && s->crosshair_on && s->hit_marker) {
        float cx = W * 0.5f, cy = H * 0.5f, o = 6.0f * ui, L = 5.0f * ui, t2 = 2.0f * ui;
        uint32_t hc = g->hitmark_kill ? C_RED : C_WHITE;
        rend_quad2d(cx - o - L, cy - o - t2*0.5f, L, t2, -1, 0,0,1,1, hc);
        rend_quad2d(cx + o,     cy - o - t2*0.5f, L, t2, -1, 0,0,1,1, hc);
        rend_quad2d(cx - o - L, cy + o - t2*0.5f, L, t2, -1, 0,0,1,1, hc);
        rend_quad2d(cx + o,     cy + o - t2*0.5f, L, t2, -1, 0,0,1,1, hc);
    }
    /* ammo block (bottom-right, above the debug readout) */
    const WeaponDef *cw = weapons_get(g->wpn_def[g->wpn_slot]);
    if (cw) {
        float ax = W - pad - 210.0f * ui, ay = H - pad - 158.0f * ui;
        uint32_t low = g->wpn_mag[g->wpn_slot] <= cw->mag / 4 ? C_RED : C_WHITE;
        snprintf(buf, sizeof(buf), "%s  [%d/%d]", cw->name, g->wpn_slot + 1, WPN_SLOT_MAX);
        font_text_shadow(ax, ay, fs * 0.9f, buf, C_GOLD);
        snprintf(buf, sizeof(buf), "%2d  |  %d %s", g->wpn_mag[g->wpn_slot],
                 g->ammo[cw->ammo], ammo_name(cw->ammo));
        font_text_shadow(ax, ay + 12.0f * ui, fs * 1.4f, buf, low);
        if (g->reloading) {
            float frac = cw->reload_s > 0.0f
                ? dh_clampf(1.0f - g->reload_t / cw->reload_s, 0.0f, 1.0f) : 0.0f;
            bar(ax, ay + 34.0f * ui, 120.0f * ui, 5.0f * ui, frac, C_TEAL, C_DARK);
            font_text_shadow(ax, ay + 42.0f * ui, fs * 0.7f, "RELOADING", C_DIM);
        }
    }
    /* hostile counter (below the minimap, top-right) */
    if (g->arena_total > 0 && g->mode == GM_PLAY) {
        snprintf(buf, sizeof(buf), "HOSTILES %d / %d    KILLS %d",
                 g->enemies.alive_count, g->arena_total, g->kills);
        font_text_shadow(W - pad - 250.0f * ui, pad + 146.0f * ui, fs * 0.9f, buf,
                         g->arena_active ? C_RED : C_JADE);
        if (!g->arena_active && g->kills > 0)
            font_text_shadow(W - pad - 250.0f * ui, pad + 158.0f * ui, fs * 0.8f,
                             "ARENA CLEAR", C_JADE);
    }

    /* tagged-hostile markers: projected diamonds + range (M3 binoculars) */
    if (g->mode == GM_PLAY) {
        for (int i = 0; i < g->enemies.count; i++) {
            const Enemy *e = &g->enemies.v[i];
            if (!e->tagged || e->state == EN_DEAD) continue;
            float sx, sy2;
            Vec3 head = v3_add(e->pos, v3(0.f, 2.25f, 0.f));
            if (!world_to_screen(g, head, &sx, &sy2)) continue;
            float d = v3_dist(g->cam_pos, e->pos);
            if (d > 170.f) continue;
            float q = 3.0f * ui;
            rend_quad2d(sx - q, sy2 - q, q * 2.f, q * 0.7f, -1, 0,0,1,1, 0xC84040F0u);
            rend_quad2d(sx - q * 0.35f, sy2 - q, q * 0.7f, q * 2.f, -1, 0,0,1,1, 0xC84040F0u);
            snprintf(buf, sizeof(buf), "%dm", (int)d);
            font_text_shadow(sx + 6.0f * ui, sy2 - 4.0f * ui, fs * 0.7f, buf, 0xC86060F0u);
        }
    }

    /* binocular overlay (M3) */
    if (g->binoc_k > 0.02f) {
        uint32_t a = (uint32_t)(g->binoc_k * 220.0f) << 24;
        float t = 60.0f * ui * g->binoc_k;
        rend_quad2d(0, 0, W, t, -1, 0,0,1,1, a | 0x000A0A0Au);
        rend_quad2d(0, H - t, W, t, -1, 0,0,1,1, a | 0x000A0A0Au);
        rend_quad2d(0, 0, t, H, -1, 0,0,1,1, a | 0x000A0A0Au);
        rend_quad2d(W - t, 0, t, H, -1, 0,0,1,1, a | 0x000A0A0Au);
        if (g->binoc_k > 0.7f) {
            snprintf(buf, sizeof(buf), "BINOCULARS  /  TAGGED %d", g->tagged_count);
            font_text_center(W * 0.5f, H * 0.5f + 40.0f * ui, fs * 0.85f, buf,
                             0xC8FFFFFFu);
            /* mil-dot ticks around the optic centre */
            for (int k = 1; k <= 3; k++) {
                float off = 12.0f * ui * (float)k;
                rend_quad2d(W*0.5f - off - ui, H*0.5f - ui, 2.0f*ui, 2.0f*ui, -1, 0,0,1,1, 0xA0FFFFFFu);
                rend_quad2d(W*0.5f + off - ui, H*0.5f - ui, 2.0f*ui, 2.0f*ui, -1, 0,0,1,1, 0xA0FFFFFFu);
                rend_quad2d(W*0.5f - ui, H*0.5f - off - ui, 2.0f*ui, 2.0f*ui, -1, 0,0,1,1, 0xA0FFFFFFu);
                rend_quad2d(W*0.5f - ui, H*0.5f + off - ui, 2.0f*ui, 2.0f*ui, -1, 0,0,1,1, 0xA0FFFFFFu);
            }
        }
    }

    /* liberation channel (M3): hold the cleared yard */
    if (o->built && !o->captured && o->capture_t > 0.05f) {
        float frac = dh_clampf(o->capture_t / 3.0f, 0.f, 1.f);
        bar(W * 0.5f - 90.0f * ui, H - 130.0f * ui, 180.0f * ui, 8.0f * ui,
            frac, C_JADE, C_DARK);
        font_text_center(W * 0.5f, H - 146.0f * ui, fs * 0.8f, "RAISING THE FLAG", C_JADE);
    }

    /* ── M4 city HUD: heat stars, cash, speedo, radio, job, escape clock ── */
    if (g->act == 1 && g->mode == GM_PLAY) {
        City *c = &g->city;
        float stx = W - pad - 132.0f * ui, sty = pad + 142.0f * ui;
        for (int i = 0; i < 5; i++) {
            uint32_t scol = (i < c->heat.stars) ? 0xFF30C8F0u : 0xC0302820u;
            rend_quad2d(stx + i * 15.0f * ui, sty, 12.0f * ui, 12.0f * ui,
                        -1, 0,0,1,1, scol);
        }
        font_text_shadow(stx + 80.0f * ui, sty + 1.0f * ui, fs * 0.65f,
                         heat_star_name(c->heat.stars),
                         c->heat.stars >= 3 ? C_RED : C_DIM);
        if (c->heat.searching && c->heat.stars > 0) {
            bar(stx, sty + 16.0f * ui, 72.0f * ui, 5.0f * ui,
                dh_clampf(c->heat.escape_t / HEAT_ESCAPE_S, 0.0f, 1.0f), C_TEAL, C_DARK);
            font_text_shadow(stx, sty + 24.0f * ui, fs * 0.62f, "LOSING THEM...", C_TEAL);
        }
        snprintf(buf, sizeof(buf), "CASH $%d", g->cash);
        font_text_shadow(pad + bw + 8.0f * ui, by - 14.0f * ui, fs * 0.9f, buf, C_GOLD);
        if (g->in_vehicle >= 0) {
            const Vehicle *v = &c->veh[g->in_vehicle];
            const VehicleDef *vd = &c->defs[v->def];
            snprintf(buf, sizeof(buf), "%s   %3.0f KM/H", vd->name,
                     (double)(fabsf(v->speed) * 3.6));
            font_text_center(W * 0.5f, H - 62.0f * ui, fs * 0.9f, buf, C_WHITE);
            if (vd->radio) {
                snprintf(buf, sizeof(buf), "[Q] RADIO: %s", city_radio_name(c->radio_station));
                font_text_center(W * 0.5f, H - 48.0f * ui, fs * 0.7f, buf, C_DIM);
            }
            float dmg = dh_clampf(v->damage / vd->hp, 0.0f, 1.0f);
            if (dmg > 0.25f)
                bar(W * 0.5f - 90.0f * ui, H - 40.0f * ui, 180.0f * ui, 4.0f * ui,
                    1.0f - dmg, dmg > 0.7f ? C_RED : C_GOLD, C_DARK);
        } else if (v3_dist_xz(p->pos, c->hotdog_stand) < 6.0f) {
            font_text_center(W * 0.5f, H - 120.0f * ui, fs * 0.85f,
                             "[E] BUY HOT DOG - $5", C_JADE);
        } else if (v3_dist_xz(p->pos, c->job_board) < 6.0f) {
            font_text_center(W * 0.5f, H - 120.0f * ui, fs * 0.85f,
                             "[E] ODD JOB BOARD", C_GOLD);
        } else if (city_nearest_vehicle(c, p->pos, 3.5f, 0) >= 0) {
            font_text_center(W * 0.5f, H - 120.0f * ui, fs * 0.85f,
                             "[E] ENTER VEHICLE", C_WHITE);
        }
        if (c->job.active == 1 || c->job.active == 2) {
            snprintf(buf, sizeof(buf), "HOT DELIVERY  %.0fs  $%d  %s",
                     (double)c->job.timer, c->job.reward,
                     c->job.active == 1 ? "- GET THE CRATE" : "- DELIVER TO THE STAND");
            font_text_shadow(pad, pad + 26.0f * fs, fs * 0.9f, buf, C_GOLD);
        }
    }

    /* toast message */
    if (g->message_t > 0.0f && g->message[0]) {
        float a = dh_clampf(g->message_t, 0.0f, 1.0f);
        uint32_t col = (uint32_t)(((int)(a * 255.0f) & 0xFF) << 24) | (C_WHITE & 0x00FFFFFFu);
        font_text_center(W * 0.5f, H - 96.0f * ui, fs, g->message, col);
    }

    /* bottom-right: debug/perf */
    if (g->show_debug) {
        RendState *r = rend();
        float x = W - pad - 210.0f * ui, y = H - pad - 74.0f * ui;
        snprintf(buf, sizeof(buf), "fps %.1f  frame %.2f ms", r->fps, r->frame_ms);
        font_text_shadow(x, y, fs * 0.8f, buf, C_DIM);
        snprintf(buf, sizeof(buf), "draws %d  items %d  tris %dk",
                 r->draw_calls, r->items, r->tris_submitted / 1000);
        font_text_shadow(x, y + 11.0f*ui, fs * 0.8f, buf, C_DIM);
        snprintf(buf, sizeof(buf), "pos %.1f %.1f %.1f", p->pos.x, p->pos.y, p->pos.z);
        font_text_shadow(x, y + 22.0f*ui, fs * 0.8f, buf, C_DIM);
        snprintf(buf, sizeof(buf), "ground %.2f  steep %.2f  %s",
                 p->ground_y, p->steepness, p->grounded ? "ON GROUND" : "AIRBORNE");
        font_text_shadow(x, y + 33.0f*ui, fs * 0.8f, buf, C_DIM);
        snprintf(buf, sizeof(buf), "jumps %d mantle %d fall %.1fm dmg %.0f",
                 p->stats.jumps, p->stats.mantles + p->stats.vaults,
                 p->stats.max_fall_m, p->stats.fall_damage_taken);
        font_text_shadow(x, y + 44.0f*ui, fs * 0.8f, buf, C_DIM);
        snprintf(buf, sizeof(buf), "backend %s  %dx%d  tex %dkb",
                 rend_backend_name(), g->w, g->h, r->texture_mem_kb);
        font_text_shadow(x, y + 55.0f*ui, fs * 0.8f, buf, C_DIM);
    }

    /* controls hint (bottom centre) — Spec 33: always tell the player */
    if (g->mode == GM_PLAY && g->time < 30.0f) {
        font_text_center(W * 0.5f, H - 26.0f * ui, fs * 0.8f,
                         "WASD move  SHIFT sprint  SPACE jump  CTRL crouch  V roll  LMB fire  RMB aim  R reload  1/2/3 weapon  T takedown  B binoculars  E interact  H heal  TAB map  I skills  K ferry",
                         0xC8FFFFFFu);
    }
    /* debug: add the world clock line */
    if (g->show_debug) {
        float hours = fmodf(6.f + g->day_t * 24.f, 24.f);
        int seen = 0;
        for (int i = 0; i < MAPW * MAPW; i++) if (g->fog[i]) seen++;
        snprintf(buf, sizeof(buf), "clock %02d:%02d  sun %.2f  map %d%%  tagged %d",
                 (int)hours, (int)(fmodf(hours, 1.f) * 60.f),
                 (double)g->light.sun_intensity, seen * 100 / (MAPW * MAPW),
                 g->tagged_count);
        font_text_shadow(W - pad - 210.0f * ui, H - pad - 85.0f * ui, fs * 0.8f, buf, C_DIM);
    }
    sys_draw_hud(g);
    story_draw_hud(g);
}

static void draw_menu(Game *g) {
    float W = (float)g->w, H = (float)g->h;
    float ui = settings()->ui_scale;
    /* vignette-ish dark band so the title reads over any world background */
    rend_quad2d(0.0f, 0.0f, W, H * 0.42f, -1, 0,0,1,1, 0x8C0A0806u);
    rend_quad2d(0.0f, H * 0.72f, W, H * 0.28f, -1, 0,0,1,1, 0x8C0A0806u);

    float ts = 5.0f * ui;
    font_text_center(W * 0.5f, H * 0.20f, ts, "DIVIDED HORIZON", C_GOLD);
    font_text_center(W * 0.5f, H * 0.20f + ts * 10.0f, 2.0f * ui,
                     "AN ORIGINAL OPEN-WORLD ACTION GAME", C_DIM);
    font_text_center(W * 0.5f, H * 0.52f, 2.2f * ui,
                     save_slot_exists(save_slot_index(0, 1)) ? "SPACE NEW GAME   -   L CONTINUE" :
                     "PRESS SPACE OR ENTER TO PLAY", C_WHITE);
    font_text_center(W * 0.5f, H * 0.60f, 1.8f * ui,
                     "M5 BUILD - ISLA SOMBRA + MERIDIAN CITY, SKILLS, SHOPS, SAVES", C_TEAL);
    font_text_center(W * 0.5f, H * 0.86f, 1.6f * ui,
                     "BUILT BY AI UNDER HUMAN DIRECTION - DRM FREE, OFFLINE COMPLETE",
                     0xC8B0A898u);
    font_text_center(W * 0.5f, H * 0.90f, 1.6f * ui,
                     "v" DH_VERSION_STRING " - " DH_BUILD_NAME, 0xC8A09888u);
}

/* zipline cables: two-point 3D line segments (soft + GL11 both support them) */
static void draw_zips(Game *g) {
    for (int i = 0; i < g->zips.count; i++) {
        Vec3 pts[2] = { g->zips.v[i].a, g->zips.v[i].b };
        rend_lines(pts, 1, 0xFF2A2A30u, 2.0f);
    }
}

void game_render(Game *g) {
    if (!g || !g->ready) return;
    RendState *r = rend();
    if (!r) return;

    /* ── camera: first person, eye height eased by the controller ── */
    player_camera(&g->player, &g->cam_pos, &g->cam_dir);
    if (g->mode == GM_PHOTO) {               /* M9 free camera */
        g->cam_pos = g->ph_pos;
        g->cam_dir = v3(sinf(g->ph_yaw) * cosf(g->ph_pitch), sinf(g->ph_pitch), cosf(g->ph_yaw) * cosf(g->ph_pitch));
    }
    /* recoil view punch: visual-only offset on top of the permanent climb
       that combat already baked into pitch/yaw (decays at 10/s) */
    if (g->rec_pitch != 0.0f || g->rec_yaw != 0.0f) {
        Vec3 up = v3(0.0f, 1.0f, 0.0f);
        Vec3 right = v3_norm(v3_cross(g->cam_dir, up));
        Vec3 cup = v3_cross(right, g->cam_dir);
        Vec3 d1 = v3_add(v3_mul(g->cam_dir, cosf(g->rec_yaw)),
                         v3_mul(v3_cross(up, g->cam_dir), sinf(g->rec_yaw)));
        g->cam_dir = v3_norm(v3_add(v3_mul(d1, cosf(g->rec_pitch)),
                                    v3_mul(cup, sinf(g->rec_pitch))));
    }
    pol_apply_camera(g);                     /* M7 shake (settings-gated) */
    Vec3 target = v3_add(g->cam_pos, g->cam_dir);
    g->view = m4_look_at(g->cam_pos, target, v3(0, 1, 0));
    float aspect = (float)g->w / (float)(g->h > 0 ? g->h : 1);
    g->proj = m4_perspective(g->mode == GM_PHOTO ? g->ph_fov : g->cam_fov, aspect, CAM_NEAR, CAM_FAR);

    rend_begin_frame(&g->view, &g->proj, g->cam_pos, &g->light);
    draw_sky(g);
    draw_terrain(g);
    draw_props(g);
    if (g->act == 1) city_render(&g->city, g);
    draw_enemies(g);
    if (g->act == 0) {
        draw_critters(g);
        draw_outpost_flag(g);
    }
    draw_poncho(g);
    draw_pickups(g);
    draw_tracers_fx(g);
    draw_zips(g);
    draw_water(g);

    rend_push_2d();
    if (g->show_hud && g->mode != GM_PHOTO) {
        if (g->mode == GM_MENU) draw_menu(g);
        else {
            draw_hud(g);
            poncho_draw_hud(g);
            herald_draw_banner(g);
            feats_draw_toast(g);
            cob_draw_hud(g);
            if (g->ui != UI_NONE) sys_draw_screen(g);
            const Player *pp = &g->player;
            if (pp->stance != PL_ST_ZIP && pp->zips) {
                int zi; float zs, zd;
                float pui = settings()->ui_scale;
                Vec3 chest = v3(pp->pos.x, pp->pos.y + pp->eye * 0.85f, pp->pos.z);
                if (zip_nearest(pp->zips, chest, 1.8f, &zi, &zs, &zd))
                    font_text_center((float)g->w * 0.5f, (float)g->h - 120.0f * pui,
                                     1.8f * pui, "[E] RIDE ZIPLINE", C_GOLD);
            }
        }
        if (g->mode == GM_PAUSE) {
            rend_quad2d(0, 0, (float)g->w, (float)g->h, -1, 0,0,1,1, 0xA00A0806u);
            font_text_center((float)g->w * 0.5f, (float)g->h * 0.45f,
                             4.0f * settings()->ui_scale, "PAUSED", C_GOLD);
        }
    }
    if (g->mode == GM_PHOTO) {
        photo_draw_filter(g);
        if (!g->ph_capture && g->show_hud) photo_draw_hint(g);
    }
    rend_pop_2d();
    rend_end_frame();
    rend_apply_post();
    photo_capture(g);
    rend_present();
}
