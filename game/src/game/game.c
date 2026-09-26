/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — game application layer (M1: Traversal)
   ══════════════════════════════════════════════════════════════════════════ */
#include "game.h"
#include "../core/dh_log.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
    "walk-up ramp (8 × 0.45 m wedges) — no stair-stepping artefacts",
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
        DH_WARN("game", "prop cap reached — dropping a graybox volume");
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

    /* drop the player at the south gate, facing the yard */
    g->player.pos = v3(A.x, terrain_height(&g->terrain, A.x, A.z - 46.f) + 0.1f, A.z - 46.f);
    g->player.vel = v3(0.f, 0.f, 0.f);
    g->player.yaw = 0.f;               /* +z: toward the arena centre */
    g->player.pitch = 0.f;
    g->player.health = 100.f;
    g->spawn = g->player.pos;          /* arena checkpoint */
    g->arena_active = 1;
    g->arena_total = g->enemies.count;
    g->kills = 0;
    g->fire_cd = 0.f; g->reloading = 0; g->ads_k = 0.f;
    game_start_play(g);
    game_message(g, "COMBAT ARENA - eliminate %d hostiles. F/LMB fire, R reload, 1/2/3 weapons",
                 g->arena_total);
    g->message_t = 5.f;
    DH_INFO("game", "arena start: %d hostiles (%d grunts, %d bruisers, %d officers)",
            g->enemies.count, n - n/4 - n/10, n/4, n/10);
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
} HitScan;

#define HITSCAN_RANGE 260.0f

static HitScan combat_hitscan(Game *g, Vec3 o, Vec3 d)
{
    HitScan hs;
    memset(&hs, 0, sizeof hs);
    hs.enemy_i = -1;
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
    g->reload_t = w->reload_s;
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

    /* recoil: part permanent climb (into yaw/pitch), part decaying punch */
    Vec3 rec = weapon_recoil(w, g->ads_k, g->shot_index++);
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
            int killed = enemy_apply_damage(e, dmg, hs.zone);
            g->hitmark_t = 0.14f;
            g->hitmark_kill = killed;
            fx_spawn(g, hs.point, v3_mul(pd, -0.4f), 6, 0xFF2828B0u, 2.5f, 0.25f, 2);
            if (killed) {
                g->kills++;
                e->dropped = 1;
                /* loot roll: 30% health, 45% ammo for the gun in hand, else nothing */
                float rr = rng_f(&g->rng);
                if (rr < 0.30f)
                    pickup_add(g, v3_add(e->pos, v3(0.f, 0.4f, 0.f)), PK_HEALTH, 0, 40.f);
                else if (rr < 0.75f)
                    pickup_add(g, v3_add(e->pos, v3(0.f, 0.4f, 0.f)), PK_AMMO, w->ammo,
                               (float)(w->mag * 2));
                if (g->kills % 5 == 0) {
                    game_message(g, "%d HOSTILES DOWN", g->kills);
                    g->message_t = 1.5f;
                }
            }
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
                if (p->health >= 100.f) continue;          /* don't waste it */
                p->health = dh_clampf(p->health + k->amount, 0.f, 100.f);
                game_message(g, "MEDKIT +%.0f HP", (double)k->amount);
            } else {
                g->ammo[k->ammo] += (int)k->amount;
                game_message(g, "%s AMMO +%d", ammo_name(k->ammo), (int)k->amount);
            }
            g->message_t = 1.4f;
            k->live = 0;
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
    int want_ads = (in->buttons & BTN_AIM) && !g->reloading && w && !player_is_down(p);
    float ads_rate = w ? (1.f / w->ads_s) : 6.f;
    if (want_ads)  g->ads_k = dh_clampf(g->ads_k + ads_rate * dt, 0.f, 1.f);
    else           g->ads_k = dh_clampf(g->ads_k - ads_rate * 1.4f * dt, 0.f, 1.f);
    g->cam_fov = 78.f * (1.f - 0.22f * g->ads_k);

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
        !player_is_down(p)) {
        Vec3 eye, dir;
        player_camera(p, &eye, &dir);
        combat_fire(g, eye, dir);
    }

    /* ── enemies ── */
    EnemyView pv;
    pv.eye = v3(p->pos.x, p->pos.y + p->eye, p->pos.z);
    pv.crouched = p->crouched;
    pv.moving = v3_len(v3(p->vel.x, 0.f, p->vel.z)) > 3.0f;
    pv.alive = !player_is_down(p);
    pv.light = 1.0f;                          /* M2: golden-hour daylight */
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
        if (rng_f(&g->rng) < e->shot_acc) {
            float dmg = enemy_shot_damage(e->arch);
            p->health = dh_clampf(p->health - dmg, 0.f, 100.f);
            g->damage_flash = 0.5f;
        }
    }
    if ((int)p->health < hp_before && p->health <= 0.f) {
        game_message(g, "YOU DIED - respawning at checkpoint (loadout kept)");
        g->message_t = 3.f;
    }

    /* arena clear */
    if (g->arena_active && g->enemies.alive_count == 0) {
        g->arena_active = 0;
        game_message(g, "ARENA CLEAR - %d hostiles down. The yard is yours.", g->kills);
        g->message_t = 6.f;
        DH_INFO("game", "arena cleared: %d kills", g->kills);
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
        uint32_t c = (k->kind == PK_HEALTH) ? 0xFF50C860u : 0xFF30A8F0u;
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
}

/* Sun + atmosphere. M1 keeps a fixed golden-hour key light; M3 adds the
   24-minute day/night cycle (Spec 6.7) on top of this same struct. */
static void setup_light(Game *g) {
    SceneLight *L = &g->light;
    memset(L, 0, sizeof(*L));
    L->sun_dir = v3_norm(v3(-0.45f, 0.62f, -0.64f));   /* FROM the sun */
    L->sun_color = v3(1.00f, 0.92f, 0.78f);
    L->sun_intensity = 1.15f;
    L->ambient = v3(0.30f, 0.33f, 0.38f);
    L->hemi_sky = v3(0.42f, 0.55f, 0.72f);
    L->hemi_ground = v3(0.34f, 0.32f, 0.24f);
    L->fog_color = v3(0.72f, 0.80f, 0.88f);
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

    int chunks = terrain_build_chunks(&g->terrain, CHUNK_M);
    if (chunks <= 0) { DH_ERROR("game", "terrain meshing failed"); return 0; }

    /* ── player ── */
    g->spawn = v3(g->course_center.x, 0.0f, g->course_center.z - 4.0f);
    player_init(&g->player, &g->terrain, &g->obs, g->spawn);
    g->player.zips = &g->zips;
    g->player.yaw = 0.0f;             /* facing +z, straight down the course */
    g->player.pitch = -0.05f;

    game_apply_settings(g);
    g->ready = 1;
    DH_INFO("game", "M1 world ready: %d chunks, %d obstacles, %d textures, backend %s",
            chunks, g->obs.count, tex_count(), rend_backend_name());
    game_message(g, "TRAVERSAL COURSE - run south");
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

void game_frame(Game *g, const PlatInput *in, float dt) {
    if (!g || !g->ready || !in) return;
    g->time += dt;
    g->frame_index++;
    if (g->message_t > 0.0f) g->message_t -= dt;

    Settings *s = settings();

    if (in->quit) { g->quit = 1; return; }

    if (g->mode == GM_MENU) {
        if ((in->pressed & (BTN_JUMP | BTN_USE | BTN_MENU)) ||
            (in->buttons & BTN_JUMP)) {
            game_start_play(g);
        }
        /* idle camera drift so the menu is never a frozen image */
        g->player.yaw += 0.06f * dt;
        return;
    }

    if (g->mode == GM_PAUSE) {
        if (in->pressed & (BTN_PAUSE | BTN_MENU)) g->mode = GM_PLAY;
        return;
    }

    if (in->pressed & BTN_PAUSE) { g->mode = GM_PAUSE; return; }

    /* ── combat sim runs first: ADS/recoil state then feeds movement ── */
    combat_update(g, in, dt);

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
       simulation stays device-independent and headless runs are reproducible. */
    pi.look_dx = in->look_dx * s->sensitivity * sx;
    pi.look_dy = in->look_dy * s->sensitivity * sy;
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

    player_input(&g->player, &pi, dt);

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
            player_respawn(&g->player, g->spawn);
            g->damage_flash = 0.0f;
            g->reloading = 0;
            game_message(g, "RESPAWNED at checkpoint - loadout kept, %d hostiles remain",
                         g->enemies.alive_count);
            g->message_t = 3.0f;
        }
    } else {
        g->respawn_t = 0.0f;
    }

    /* ── photo mode / debug toggles ── */
    if (in->pressed & BTN_PHOTO) {
        g->mode = (g->mode == GM_PHOTO) ? GM_PLAY : GM_PHOTO;
        game_message(g, g->mode == GM_PHOTO ? "PHOTO MODE (HUD off)" : "PHOTO MODE off");
    }
    if (in->pressed & BTN_MAP) { g->show_debug = !g->show_debug; }

    /* M2 dev/DoD hotkey: deploy to the combat arena (30 hostiles) */
    if (in->pressed & BTN_ARENA) {
        game_arena_start(g, 30);
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

static void draw_sky(Game *g) {
    Mat4 model = m4_mul(m4_translate(g->cam_pos), m4_scale1(SKY_RADIUS));
    rend_mesh_lit(g->mesh_sky, &model, -1, 0xFFFFFFFFu, 1.0f, 0, 0, 0, 0, 1);
}

/* ── HUD ───────────────────────────────────────────────────────────────── */
static void bar(float x, float y, float w, float h, float frac, uint32_t fg, uint32_t bg) {
    frac = dh_clampf(frac, 0.0f, 1.0f);
    rend_quad2d(x, y, w, h, -1, 0,0,1,1, bg);
    if (frac > 0.0f) rend_quad2d(x + 1.0f, y + 1.0f, (w - 2.0f) * frac, h - 2.0f,
                                 -1, 0,0,1,1, fg);
}

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

    /* top-left: mode + course objective */
    font_text_shadow(pad, pad, fs, "DIVIDED HORIZON", C_GOLD);
    char buf[160];
    snprintf(buf, sizeof(buf), "%s  -  M2 COMBAT  -  %s",
             game_mode_name(g->mode), player_stance_name(p->stance));
    font_text_shadow(pad, pad + 12.0f * fs, fs * 0.85f, buf, C_DIM);

    /* bottom-left: vitals */
    float by = H - pad - 46.0f * ui;
    float bw = 180.0f * ui, bh = 9.0f * ui;
    bar(pad, by, bw, bh, p->health / 100.0f, p->health > 35.0f ? C_JADE : C_RED, C_DARK);
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
        uint32_t a = (uint32_t)(dh_clampf(g->damage_flash, 0.0f, 1.0f) * 80.0f) << 24;
        rend_quad2d(0, 0, W, H * 0.10f, -1, 0,0,1,1, a | 0x001818A0u);
        rend_quad2d(0, H * 0.90f, W, H * 0.10f, -1, 0,0,1,1, a | 0x001818A0u);
        rend_quad2d(0, 0, W * 0.07f, H, -1, 0,0,1,1, a | 0x001818A0u);
        rend_quad2d(W * 0.93f, 0, W * 0.07f, H, -1, 0,0,1,1, a | 0x001818A0u);
    }
    /* hitmarker: 45° ticks around the crosshair; red X on a kill */
    if (g->hitmark_t > 0.0f && s->crosshair_on) {
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
    /* hostile counter (top-right) */
    if (g->arena_total > 0 && g->mode == GM_PLAY) {
        snprintf(buf, sizeof(buf), "HOSTILES %d / %d    KILLS %d",
                 g->enemies.alive_count, g->arena_total, g->kills);
        font_text_shadow(W - pad - 250.0f * ui, pad, fs * 0.9f, buf,
                         g->arena_active ? C_RED : C_JADE);
        if (!g->arena_active && g->kills > 0)
            font_text_shadow(W - pad - 250.0f * ui, pad + 12.0f * ui, fs * 0.8f,
                             "ARENA CLEAR", C_JADE);
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
                         "WASD move  SHIFT sprint  SPACE jump  CTRL crouch  V roll  F/LMB fire  RMB/C aim  R reload  1/2/3 weapon",
                         0xC8FFFFFFu);
    }
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
                     "PRESS SPACE OR ENTER TO PLAY", C_WHITE);
    font_text_center(W * 0.5f, H * 0.60f, 1.8f * ui,
                     "M1 MILESTONE - ISLA SOMBRA TRAVERSAL COURSE", C_TEAL);
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
    if (g->mode == GM_PHOTO) { /* M7 adds free-fly; M1 keeps the player camera */ }
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
    Vec3 target = v3_add(g->cam_pos, g->cam_dir);
    g->view = m4_look_at(g->cam_pos, target, v3(0, 1, 0));
    float aspect = (float)g->w / (float)(g->h > 0 ? g->h : 1);
    g->proj = m4_perspective(g->cam_fov, aspect, CAM_NEAR, CAM_FAR);

    rend_begin_frame(&g->view, &g->proj, g->cam_pos, &g->light);
    draw_sky(g);
    draw_terrain(g);
    draw_props(g);
    draw_enemies(g);
    draw_pickups(g);
    draw_tracers_fx(g);
    draw_zips(g);
    draw_water(g);

    rend_push_2d();
    if (g->show_hud && g->mode != GM_PHOTO) {
        if (g->mode == GM_MENU) draw_menu(g);
        else {
            draw_hud(g);
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
    rend_pop_2d();
    rend_end_frame();
    rend_apply_post();
    rend_present();
}
