/* DIVIDED HORIZON — M21 smoke: three multi-stage strongholds. */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke21", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke21", "ok: " __VA_ARGS__); } while (0)
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


static void kill_wave(Game *g, int ci) {
    for (int k = 0; k < g->enemies.count; k++) {
        Enemy *e = &g->enemies.v[k];
        if (e->faction == CAMP_FACTION && e->tag == 100 + ci && e->state != EN_DEAD)
            enemy_apply_damage(e, 9999.f, 0);
    }
}
int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke21", "════ M21 STRONGHOLDS SMOKE ════");
    audio_init(); settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x21C0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g); run(&g, 30);
    char pb[512];
    CHECK(game_stronghold_count(&g) == 3, "3 strongholds built (%d)", game_stronghold_count(&g));
    CHECK(game_camp_count(&g) == 11 && game_outposts_total(&g) == 12, "camp/outpost counts unchanged");
    CHECK(game_strongholds_captured(&g) == 0, "none captured at start");
    int ok = 1, spread = 1;
    for (int k = 0; k < 3; k++) {
        int s = game_stronghold_slot(&g, k); if (s < 0) { ok = 0; continue; }
        if (g.camps[s].stages != 3 || g.camps[s].pad_h < 1.2f) ok = 0;
        for (int j = 0; j < g.camp_n; j++) if (j != s && v3_dist_xz(g.camps[s].center, g.camps[j].center) < 115.f) spread = 0;
        if (v3_dist_xz(g.camps[s].center, g.outpost.center) < 120.f) spread = 0;
    }
    CHECK(ok, "each stronghold has 3 stages on dry land");
    CHECK(spread, "strongholds spaced from camps and Punta Quemada");
    CHECK(!strcmp(g.camps[CAMP_N].name, "MOLINO FORTRESS") && !strcmp(g.camps[CAMP_N+2].name, "FARO VIEJO"), "named ST1/ST3");
    CHECK(g.prop_count < GAME_MAX_PROPS, "props within budget (%d)", g.prop_count);

    int s = game_stronghold_slot(&g, 0); Camp *c = &g.camps[s];
    go(&g, v3(c->center.x, 0, c->center.z + 40.f)); run(&g, 20);
    int dead; int a = census_c(&g, c, &dead);
    CHECK(c->spawned && a == 4, "wave 1 = 4 defenders (%d)", a);
    kill_wave(&g, s); run(&g, 10);
    CHECK(c->stage == 1 && !c->cleared, "wave 1 down -> stage 2 (stage %d)", c->stage);
    run(&g, 10); a = census_c(&g, c, &dead);
    CHECK(a == 6 && dead == 0, "wave 2 = 6 fresh defenders (%d, dead %d)", a, dead);
    kill_wave(&g, s); run(&g, 20); a = census_c(&g, c, &dead);
    CHECK(c->stage == 2 && a == 8, "wave 3 = 8 defenders (%d)", a);
    int off = 0;
    for (int k = 0; k < g.enemies.count; k++) if (g.enemies.v[k].tag == 100 + s && g.enemies.v[k].arch == EN_OFFICER) off = 1;
    CHECK(off, "final wave led by an officer");
    kill_wave(&g, s); run(&g, 10);
    CHECK(c->cleared, "stronghold cleared after 3 waves");
    int cash0 = g.cash;
    go(&g, c->flag_pos); run(&g, 300);
    CHECK(!c->captured, "5 s on the flag is not enough (6 s hold)");
    go(&g, c->flag_pos); run(&g, 90);
    CHECK(c->captured, "captured after 6.5 s hold");
    CHECK(g.cash - cash0 >= 500, "reward +500 cash (%d)", g.cash - cash0);
    CHECK(game_strongholds_captured(&g) == 1 && game_camps_captured(&g) == 0, "stronghold count separate from camps");
    CHECK(g.prog.camps_mask & (1u << s), "camps_mask bit %d set", s);
    game_render(&g);
    snprintf(pb, sizeof pb, "%s/m21_stronghold.png", out); rend_save_png(pb);

    game_save(&g, 1, 0);
    static Game g2;
    game_init(&g2, 640, 360, REND_SOFT, 0x21C0u); game_start_play(&g2);
    game_load(&g2, 1, 0);
    CHECK(g2.camps[s].captured && game_strongholds_captured(&g2) == 1, "stronghold survives save/load");
    CHECK(!g2.camps[CAMP_N+1].captured, "other strongholds still hostile after load");
    DH_INFO("smoke21", "════ %d checks, %d failed ════", g_checks, g_failed);
    printf("smoke21: %d/%d checks passed\n", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
