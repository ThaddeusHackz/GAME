/* DIVIDED HORIZON — M18 smoke: boss B5 El Limpiador (Spec 25/75). */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke18", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke18", "ok: " __VA_ARGS__); } while (0)
#define DT (1.0f / 60.0f)
static void run(Game *g, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof in);
        game_frame(g, &in, DT);
        g->player.health = g->player.health_max;   /* keep the tester alive */
    }
}
static int has(Game *g, const char *name) {
    for (int i = 0; i < game_feat_count(); i++)
        if (!strcmp(game_feat_name(i), name)) return (g->feats.got >> i) & 1;
    return -1;
}
static void kill_hunter(Game *g) {
    Enemy *e = &g->enemies.v[g->cob.slot];
    e->health = 1.f; enemy_apply_damage(e, 9999.f, 0);
}
static int last_head(Game *g, char *b) { return game_herald_headline(g, g->leg.n - 1, b, 128); }

static void press_use(Game *g) {
    PlatInput in; memset(&in, 0, sizeof in); in.pressed = BTN_USE;
    game_frame(g, &in, DT);
}
static void kill(Game *g, int s) {
    Enemy *e = &g->enemies.v[s]; e->armor = 0.f; enemy_apply_damage(e, 99999.f, 0);
}

static void step(Game *g, int use) {
    PlatInput in; memset(&in, 0, sizeof in); if (use) in.pressed = BTN_USE;
    game_frame(g, &in, DT);
}
static Vec3 at(Game *g, Vec3 p) { (void)g; return v3(p.x, p.y, p.z - 1.5f); }

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke18", "════ M18 EL LIMPIADOR SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x18B0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 40);
    (void)kill_hunter; (void)step; (void)kill;
    char b[128];
    CHECK(!strcmp(game_boss_name(4), "EL LIMPIADOR"), "B5 is El Limpiador");
    CHECK(!game_boss_start(&g, 5), "finale refuses until all lieutenants fall");
    CHECK(has(&g, "CIUDAD LIMPIA") == 0, "feat CIUDAD LIMPIA exists, locked");

    Vec3 rp = game_boss_radio_pos(&g);
    g.player.pos = at(&g, rp); g.player.vel = v3(0, 0, 0);
    run(&g, 2);
    CHECK(g.boss.prompt_t > 0.f && g.boss.prompt_who == 4, "call-box prompt shows");
    int n0 = g.enemies.count;
    press_use(&g);
    CHECK(game_boss_active(&g) == 4 && game_boss_phase(&g) == 1, "call-box summons El Limpiador");
    CHECK(g.enemies.count - n0 == 1, "he arrives (%d)", g.enemies.count - n0);
    Enemy *m = &g.enemies.v[g.boss.slot];
    run(&g, 1);
    CHECK(game_boss_shielded(&g), "vans running: shielded");
    float h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health < 11.f, "90%% absorbed (%.1f)", h0 - m->health);
    Vec3 v0 = game_boss_van_pos(&g, 0);
    run(&g, 5 * 60 + 5);
    Vec3 v1 = game_boss_van_pos(&g, 0);
    CHECK(v3_len(v3_sub(v0, v1)) > 1.f, "vans reposition every 5 s");

    /* EMP */
    Vec3 ep = game_boss_emp_pos(&g);
    g.player.pos = v3(ep.x, ep.y, ep.z - 1.f); g.player.vel = v3(0, 0, 0);
    run(&g, 30);
    press_use(&g);
    CHECK(g.boss.emp_t > 5.5f && !game_boss_shielded(&g) && g.boss.emps == 1, "EMP stalls the vans: exposed");
    run(&g, 1);
    h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health > 90.f, "exposed: full damage (%.1f)", h0 - m->health);
    run(&g, 30); press_use(&g);
    CHECK(g.boss.emps == 1 && g.boss.emp_cd > 10.f, "EMP on cooldown refuses");
    Vec3 vs = game_boss_van_pos(&g, 0);
    run(&g, 200);
    CHECK(v3_len(v3_sub(vs, game_boss_van_pos(&g, 0))) < 0.01f, "stalled vans hold position");
    run(&g, 200);
    CHECK(game_boss_shielded(&g), "after 6 s the vans restart");
    game_render(&g); { char pb[256]; snprintf(pb, sizeof pb, "%s/M18_limpiador_vans.png", out); rend_save_png(pb); }

    /* P2 */
    m->health = m->health_max * 0.6f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 2, "P2 LIMPIO WAVES at <66%%");
    int el = 0;
    for (int k = 0; k < 3; k++) if (g.boss.shield[k] >= 0 && g.enemies.v[g.boss.shield[k]].state != EN_DEAD
                                    && g.enemies.v[g.boss.shield[k]].armor > 0.4f) el++;
    CHECK(el == 3, "three armoured elites deploy (%d)", el);
    CHECK(game_boss_shielded(&g), "P2: vans still shield him");

    /* P3 */
    m->health = m->health_max * 0.3f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 3, "P3 CRANE at <33%%");
    CHECK(game_boss_shielded(&g), "riot shield starts up");
    int up = 0, down = 0;
    for (int i = 0; i < 5 * 60; i++) { run(&g, 1); if (game_boss_shielded(&g)) up++; else down++; }
    CHECK(up > 150 && down > 60, "shield cycles up/down (%d/%d)", up, down);
    while (game_boss_shielded(&g)) run(&g, 1);
    run(&g, 1);
    h0 = m->health; enemy_apply_damage(m, 50.f, 0);
    CHECK(h0 - m->health > 45.f, "shield down: hittable (%.1f)", h0 - m->health);
    game_render(&g); { char pb[256]; snprintf(pb, sizeof pb, "%s/M18_limpiador_crane.png", out); rend_save_png(pb); }
    m->armor = 0.f; m->health = 1.f; enemy_apply_damage(m, 99999.f, 0); run(&g, 2);
    CHECK(game_boss_active(&g) == -1 && (g.boss.beaten & 16), "El Limpiador down");
    CHECK(strstr(g.message, "city is clean") != NULL, "down line: %s", g.message);
    CHECK(g.boss.clean_mask & 16, "clean (never below half health)");
    run(&g, 30);
    CHECK(has(&g, "CIUDAD LIMPIA") == 1, "feat CIUDAD LIMPIA unlocked");
    CHECK(last_head(&g, b) && strstr(b, "EL LIMPIADOR"), "herald reports it: %s", b);

    /* rematch, dip below half -> not clean */
    g.boss.clean_mask &= ~16;
    g.player.pos = at(&g, rp); g.player.vel = v3(0, 0, 0); run(&g, 2);
    press_use(&g);
    CHECK(game_boss_active(&g) == 4, "rematch via call-box");
    m = &g.enemies.v[g.boss.slot];
    { PlatInput in; memset(&in, 0, sizeof in); g.player.health = g.player.health_max * 0.4f; game_frame(&g, &in, DT); }
    m->armor = 0.f; m->health = 1.f; enemy_apply_damage(m, 99999.f, 0); run(&g, 2);
    CHECK(game_boss_active(&g) == -1 && !(g.boss.clean_mask & 16), "hurt below half: not clean");

    game_save(&g, 1, 0);
    g.boss.beaten = 0;
    game_load(&g, 1, 0);
    CHECK(g.boss.beaten & 16, "save keeps B5 beaten");

    DH_INFO("smoke18", "════ %d checks, %d failed ════", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
