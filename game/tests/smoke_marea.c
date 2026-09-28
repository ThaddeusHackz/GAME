/* DIVIDED HORIZON — M16 smoke: boss B3 Dona Marea (Spec 25/75). */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke16", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke16", "ok: " __VA_ARGS__); } while (0)
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

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke16", "════ M16 DONA MAREA SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x16B0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 40);
    char b[128];
    (void)kill_hunter; (void)last_head;
    CHECK(!strcmp(game_boss_name(2), "DONA MAREA"), "B3 is Dona Marea");
    CHECK(!game_boss_start(&g, 3), "unbuilt slot 4 still refuses");

    Vec3 bp = game_boss_bell_pos(&g);
    g.player.pos = v3(bp.x, bp.y, bp.z - 1.5f); g.player.vel = v3(0, 0, 0);
    run(&g, 2);
    CHECK(g.boss.prompt_t > 0.f && g.boss.prompt_who == 2, "bell prompt shows near the harbour bell");
    int n0 = g.enemies.count;
    press_use(&g);
    CHECK(game_boss_active(&g) == 2 && game_boss_phase(&g) == 1, "bell calls Dona Marea");
    CHECK(g.enemies.count - n0 == 1, "Marea alone on the shore (%d)", g.enemies.count - n0);
    CHECK(g.day_t > 0.5f && g.day_t < 0.55f, "fight forced to sunset (day_t %.3f)", g.day_t);
    CHECK(g.boss.lines == 7 && game_boss_shielded(&g), "3 fuel lines, gunship protects her");
    Enemy *m = &g.enemies.v[g.boss.slot];
    run(&g, 1);
    float h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health < 6.f, "Marea takes 5%% while gunship lives (%.1f)", h0 - m->health);

    /* harpoon outside a window does nothing */
    Vec3 q0 = game_boss_buoy_pos(&g, 0);
    g.player.pos = v3(q0.x, q0.y, q0.z - 1.f); run(&g, 2); press_use(&g);
    CHECK(g.boss.lines == 7 && g.boss.window_t <= 0.f, "buoy locked before the first pass");

    /* strafe run: stand in the lane */
    CHECK(g.boss.lane_x == 0.f, "first lane down the middle");
    g.player.pos = v3_add(g.arena_center, v3(0.5f, 0.f, 5.f));
    g.player.pos.y = 0.f;
    { int k = 0; while (!g.boss.in_pass && k++ < 400) run(&g, 1); }
    CHECK(g.boss.in_pass, "gunship begins a strafe run");
    game_render(&g);
    CHECK(g.boss.drawn == 13, "lane (9) + gunship + 3 buoys drawn (%d)", g.boss.drawn);
    snprintf(b, sizeof b, "%s/marea_hud.png", out); rend_save_png(b);
    { float hp = g.player.health; for (int i = 0; i < 30; i++) step(&g, 0);
      CHECK(g.player.health < hp - 10.f, "strafe lane burns (%.1f)", hp - g.player.health);
      CHECK(g.boss.strafe_hits == 1 && !g.boss.clean, "strafe hit ruins the clean win"); }
    g.player.health = g.player.health_max;
    { int k = 0; while (g.boss.in_pass && k++ < 400) run(&g, 1); }
    CHECK(g.boss.window_t > 3.5f && g.boss.window_t <= 4.f, "4 s harpoon window opens (%.2f)", g.boss.window_t);
    CHECK(g.boss.lane_x != 0.f, "next lane shifts (%.0f)", g.boss.lane_x);

    /* cut all three lines, one per window */
    for (int k = 0; k < 3; k++) {
        if (g.boss.window_t <= 0.f) { int j = 0; while (g.boss.window_t <= 0.f && j++ < 900) run(&g, 1); }
        Vec3 q = game_boss_buoy_pos(&g, k);
        g.player.pos = v3(q.x, q.y, q.z - 1.f); run(&g, 1); press_use(&g);
        if (k < 2) CHECK(g.boss.lines == (7 & ~((2 << k) - 1)) && g.boss.window_t <= 0.f, "line %d cut, window consumed", k);
    }
    CHECK(g.boss.lines == 0 && game_boss_phase(&g) == 2, "gunship sinks -> P2 PIER BRAWL");
    CHECK(!game_boss_shielded(&g), "Marea exposed after landing");
    run(&g, 1);
    h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health > 90.f, "full damage on the pier (%.1f)", h0 - m->health);

    /* anchor sweep */
    g.player.pos = v3_add(m->pos, v3(1.5f, 0.f, 0.f)); g.boss.sweep_t = 0.01f;
    { float hp = g.player.health; step(&g, 0);
      CHECK(hp - g.player.health >= 14.f, "anchor sweep 3 m hits (%.1f)", hp - g.player.health); }
    g.player.health = g.player.health_max;
    g.player.pos = v3_add(m->pos, v3(6.f, 0.f, 0.f)); g.boss.sweep_t = 0.01f;
    { float hp = g.player.health; step(&g, 0);
      CHECK(hp - g.player.health < 1.f, "sweep misses at 6 m"); }

    /* P3 tide */
    m->health = m->health_max * 0.3f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 3, "P3 TIDE RISING");
    run(&g, 300);
    CHECK(g.boss.tide_r < 26.f && g.boss.tide_r >= 10.f, "dry ground shrinks (%.1f)", g.boss.tide_r);
    g.player.pos = v3_add(g.arena_center, v3(28.f, 0.f, 0.f));
    { float hp = g.player.health; for (int i = 0; i < 30; i++) step(&g, 0);
      CHECK(g.player.health < hp - 3.f, "tide burns outside the ring (%.1f)", hp - g.player.health); }
    g.player.health = g.player.health_max;
    game_render(&g);
    CHECK(g.boss.drawn == 16, "tide ring posts drawn (%d)", g.boss.drawn);
    snprintf(b, sizeof b, "%s/marea_tide.png", out); rend_save_png(b);
    g.player.pos = v3_add(m->pos, v3(2.f, 0.f, 0.f));
    h0 = m->health; step(&g, 1);
    CHECK(g.boss.harpoon_used && h0 - m->health > m->health_max * 0.14f, "last harpoon bites 15%%");
    h0 = m->health; step(&g, 0); step(&g, 1);
    CHECK(h0 - m->health < 1.f, "only one last harpoon");

    /* win (not clean) */
    float st0 = g.leg.stature;
    kill(&g, g.boss.slot); run(&g, 2);
    CHECK(game_boss_active(&g) == -1 && (g.boss.beaten & 4), "Dona Marea defeated");
    CHECK(strstr(g.message, "keeps what it wants") != NULL, "down line: %s", g.message);
    CHECK(g.leg.stature > st0, "stature up");
    CHECK(game_herald_headline(&g, g.leg.n - 1, b, 128) && strstr(b, "MAREA"), "Herald: %s", b);
    run(&g, 70);
    CHECK(has(&g, "LA AGUA CUSTODIA") == 0, "no LA AGUA CUSTODIA after a strafe hit");

    /* clean rematch */
    run(&g, 30);
    g.player.pos = v3(bp.x, bp.y, bp.z - 1.5f); run(&g, 2); press_use(&g);
    CHECK(game_boss_active(&g) == 2 && g.boss.strafe_hits == 0 && g.boss.lines == 7, "rematch resets");
    kill(&g, g.boss.slot); run(&g, 70);
    CHECK(has(&g, "LA AGUA CUSTODIA") == 1, "LA AGUA CUSTODIA for an untouched sunset win");
    CHECK(game_save(&g, 3, 0), "save");
    g.boss.beaten = 0; g.boss.clean_mask = 0;
    CHECK(game_load(&g, 3, 0) && (g.boss.beaten & 4) && (g.boss.clean_mask & 4), "B3 persists");
    DH_INFO("smoke16", "════ %d/%d checks passed ════", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
