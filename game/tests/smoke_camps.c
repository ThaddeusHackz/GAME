/* DIVIDED HORIZON — M20 smoke: Isla Sombra camp network (outposts 2..12). */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif
#include "../src/core/dh_types.h"
#include "../src/core/dh_log.h"
#include "../src/core/settings.h"
#include "../src/rend/rend.h"
#include "../src/game/game.h"
#include "../src/plat/plat.h"
#include "../src/audio/audio.h"
#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke20", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke20", "ok: " __VA_ARGS__); } while (0)
#define DT (1.0f / 60.0f)
static void run(Game *g, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof in);
        game_frame(g, &in, DT);
        g->player.health = g->player.health_max;   /* keep the tester alive */
    }
}
static Camp *camp_at(Game *g, Vec3 c) { for (int i = 0; i < g->camp_n; i++) if (v3_dist_xz(g->camps[i].center, c) < 1.f) return &g->camps[i]; return &g->camps[0]; }
static int census_c(Game *g, Camp *cp, int *dead) {
    int a = 0, d = 0;
    for (int k = 0; k < g->enemies.count; k++) {
        Enemy *e = &g->enemies.v[k];
        if (e->faction != CAMP_FACTION || e->tag != 100 + (int)(cp - g->camps)) continue;
        if (e->state == EN_DEAD) d++; else a++;
    }
    if (dead) *dead = d;
    return a;
}
#define census(g, C, d) census_c(g, camp_at(g, C), d)
static void go(Game *g, Vec3 p) {
    g->player.pos = v3(p.x, terrain_height(&g->terrain, p.x, p.z) + 0.1f, p.z);
    g->player.vel = v3(0, 0, 0);
}
static Vec3 far_from(Game *g, Vec3 c) {   /* a point on the island >240 m away */
    float best = 0; Vec3 bp = c;
    for (float x = 100; x <= 900; x += 50) for (float z = 100; z <= 900; z += 50) {
        float d = v3_dist_xz(v3(x,0,z), c), mn = 1e9f;
        for (int i = 0; i < g->camp_n; i++) { float di = v3_dist_xz(v3(x,0,z), g->camps[i].center); if (di < mn) mn = di; }
        if (d > 240.f && mn > best && terrain_height(&g->terrain, x, z) > 1.f) { best = mn; bp = v3(x,0,z); }
    }
    return bp;
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke20", "════ M20 CAMP NETWORK SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x20C0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 30);
    char pb[512];

    CHECK(game_camp_count(&g) == 11, "11 camps built (%d)", game_camp_count(&g));
    CHECK(game_outposts_total(&g) == 12, "12 outposts total");
    CHECK(game_camps_captured(&g) == 0, "none liberated at start");
    int spread = 1, land = 1;
    for (int i = 0; i < g.camp_n; i++) {
        Camp *c = &g.camps[i];
        if (c->pad_h < 1.2f) land = 0;
        if (v3_dist_xz(c->center, g.outpost.center) < 120.f) spread = 0;
        for (int j = i + 1; j < g.camp_n; j++)
            if (v3_dist_xz(c->center, g.camps[j].center) < 115.f) spread = 0;
    }
    CHECK(spread, "camps spread >=115 m apart and clear of Punta Quemada");
    CHECK(land, "every camp pad above the waterline");
    int names = 1;
    for (int i = 0; i < g.camp_n; i++) for (int j = i + 1; j < g.camp_n; j++)
        if (!strcmp(g.camps[i].name, g.camps[j].name)) names = 0;
    CHECK(names, "11 unique names");
    CHECK(g.prop_count < GAME_MAX_PROPS, "prop budget ok (%d/%d)", g.prop_count, GAME_MAX_PROPS);

    /* streaming */
    Camp *c0 = &g.camps[0];
    int n0 = 3 + c0->difficulty;
    go(&g, far_from(&g, c0->center)); run(&g, 5);
    CHECK(census(&g, c0->center, NULL) == 0, "far away: camp 0 garrison not streamed");
    go(&g, v3(c0->center.x, 0, c0->center.z + 60.f)); run(&g, 3);
    CHECK(census(&g, c0->center, NULL) == n0, "at 60 m: garrison streamed in (%d/%d)", census(&g, c0->center, NULL), n0);
    game_render(&g);
    snprintf(pb, sizeof pb, "%s/M20_camp.png", out); rend_save_png(pb);
    go(&g, far_from(&g, c0->center)); run(&g, 5);
    CHECK(census(&g, c0->center, NULL) == 0 && !c0->spawned, "left: garrison reclaimed");
    go(&g, v3(c0->center.x, 0, c0->center.z + 60.f)); run(&g, 3);
    CHECK(census(&g, c0->center, NULL) == n0, "back: garrison restreamed");

    /* liberation */
    go(&g, c0->flag_pos); run(&g, 200);
    CHECK(game_camp_state(&g, 0) == 0, "cannot liberate with the garrison alive");
    for (int k = 0; k < g.enemies.count; k++) {
        Enemy *e = &g.enemies.v[k];
        if (e->faction == CAMP_FACTION && e->state != EN_DEAD && e->tag == 100)
            { e->armor = 0.f; enemy_apply_damage(e, 99999.f, 0); }
    }
    run(&g, 2);
    CHECK(game_camp_state(&g, 0) == 1 && strstr(g.message, "hold the flag"), "garrison down: %s", g.message);
    int cash0 = g.cash;
    go(&g, c0->flag_pos); run(&g, 100);
    CHECK(game_camp_state(&g, 0) == 1, "not yet (1.7 s)");
    run(&g, 90);
    CHECK(game_camp_state(&g, 0) == 2, "held 3 s: liberated");
    CHECK(strstr(g.message, "LIBERATED") && strstr(g.message, "1/12"), "message: %s", g.message);
    CHECK(g.cash > cash0 && (g.prog.camps_mask & 1u), "reward + mask");
    run(&g, 200);
    game_render(&g);
    snprintf(pb, sizeof pb, "%s/M20_liberated.png", out); rend_save_png(pb);

    /* persistence */
    CHECK(game_save(&g, 2, 0), "save");
    g.prog.camps_mask = 0; g.camps[0].captured = 0; g.camps[0].cleared = 0;
    CHECK(game_load(&g, 2, 0), "load");
    CHECK(game_camp_state(&g, 0) == 2 && (g.prog.camps_mask & 1u), "liberation survives save/load");
    CHECK(game_camp_state(&g, 1) == 0, "camp 1 still hostile after load");

    /* tour: the pool never overflows */
    int maxc = 0, all_streamed = 1;
    for (int i = 1; i < g.camp_n; i++) {
        Camp *c = &g.camps[i];
        go(&g, v3(c->center.x, 0, c->center.z + 50.f)); run(&g, 20);
        if (census(&g, c->center, NULL) != (c->stages > 1 ? 4 : 3 + c->difficulty)) {
            all_streamed = 0; DH_WARN("smoke20", "camp %d streamed %d", i, census(&g, c->center, NULL));
        }
        if (g.enemies.count > maxc) maxc = g.enemies.count;
    }
    CHECK(all_streamed, "every camp streams its garrison on a full tour");
    CHECK(maxc <= ENEMY_MAX, "enemy pool peak %d/%d", maxc, ENEMY_MAX);

    /* liberate the rest the quick way → 12/12 incl. Punta Quemada */
    for (int i = 1; i < g.camp_n; i++) { g.camps[i].cleared = 1; g.camps[i].spawned = 1; }
    for (int i = 1; i < g.camp_n; i++) { go(&g, g.camps[i].flag_pos); run(&g, 190); }
    CHECK(game_camps_captured(&g) == 11, "all 11 camps liberated (%d)", game_camps_captured(&g));
    g.outpost.captured = 1;
    CHECK(game_camps_captured(&g) == 12, "12/12 with Punta Quemada");

    DH_INFO("smoke20", "════ %d checks, %d failed ════", g_checks, g_failed);
    printf("smoke20: %d/%d checks passed\n", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
