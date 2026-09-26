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
                   "WASD move - SHIFT sprint - SPACE jump/vault - CTRL crouch - R roll");
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

    /* ── map abstract buttons → player intent ── */
    PlayerInput pi;
    memset(&pi, 0, sizeof(pi));
    pi.mx = dh_clampf(in->mx, -1.0f, 1.0f);
    pi.my = dh_clampf(in->my, -1.0f, 1.0f);
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

    /* ── death → auto-respawn (M2 adds the proper death/last-checkpoint flow) ── */
    if (player_is_down(&g->player)) {
        g->respawn_t += dt;
        if (g->respawn_t > 2.0f) {
            g->respawn_t = 0.0f;
            player_respawn(&g->player, g->spawn);
            game_message(g, "RESPAWNED at course start");
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
    snprintf(buf, sizeof(buf), "%s  -  M1 TRAVERSAL  -  %s",
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
    if (g->mode == GM_PLAY && g->time < 25.0f) {
        font_text_center(W * 0.5f, H - 26.0f * ui, fs * 0.8f,
                         "WASD move   SHIFT sprint   SPACE jump/vault   CTRL crouch   R roll   F5 debug",
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
    Vec3 target = v3_add(g->cam_pos, g->cam_dir);
    g->view = m4_look_at(g->cam_pos, target, v3(0, 1, 0));
    float aspect = (float)g->w / (float)(g->h > 0 ? g->h : 1);
    g->proj = m4_perspective(g->cam_fov, aspect, CAM_NEAR, CAM_FAR);

    rend_begin_frame(&g->view, &g->proj, g->cam_pos, &g->light);
    draw_sky(g);
    draw_terrain(g);
    draw_props(g);
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
