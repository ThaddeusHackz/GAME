/* DIVIDED HORIZON — M15 smoke: boss B2 El Reloj (Spec 25/75). */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke15", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke15", "ok: " __VA_ARGS__); } while (0)
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

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke15", "════ M15 EL RELOJ SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x15B0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 40);
    char b[128];
    (void)kill_hunter; (void)last_head;
    CHECK(!strcmp(game_boss_name(1), "EL RELOJ"), "B2 is El Reloj");
    CHECK(!game_boss_start(&g, 3), "unbuilt slots still refuse");

    Vec3 cp = game_boss_clock_pos(&g);
    g.player.pos = v3(cp.x, cp.y, cp.z - 1.5f); g.player.vel = v3(0, 0, 0);
    run(&g, 2);
    CHECK(g.boss.prompt_t > 0.f && g.boss.prompt_who == 1, "clock prompt shows near the dial");
    int n0 = g.enemies.count;
    press_use(&g);
    CHECK(game_boss_active(&g) == 1 && game_boss_phase(&g) == 1, "clock dial calls El Reloj");
    CHECK(g.enemies.count - n0 == 3, "Reloj + 2 guards (%d)", g.enemies.count - n0);
    CHECK(g.boss.bomb_live == 0xF && g.boss.bomb_t > 44.f, "4 columns armed at 45 s");
    CHECK(game_boss_shielded(&g), "Reloj armored while bombs live");
    Enemy *m = &g.enemies.v[g.boss.slot];
    run(&g, 1);
    float h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health < 11.f, "armored Reloj takes 10%% (%.1f)", h0 - m->health);

    /* detonation */
    g.player.pos = v3_add(g.arena_center, v3(0.f, 0.f, -30.f));
    g.boss.bomb_t = 0.02f;
    { PlatInput in; memset(&in, 0, sizeof in); float hp = g.player.health;
      game_frame(&g, &in, DT); game_frame(&g, &in, DT);
      CHECK(g.boss.dets == 4 && g.player.health < hp - 40.f, "timer expiry detonates all 4 (%d, hp %.0f)", g.boss.dets, g.player.health); }
    CHECK(g.boss.bomb_live == 0xF && g.boss.bomb_t > 40.f && !g.boss.clean, "columns re-arm; clean lost");
    g.player.health = g.player.health_max;

    /* defuse all four */
    for (int k = 0; k < 4; k++) {
        Vec3 bp = game_boss_bomb_pos(&g, k);
        g.player.pos = v3(bp.x + 1.f, bp.y, bp.z); g.player.vel = v3(0, 0, 0);
        run(&g, 25); press_use(&g);
    }
    CHECK(g.boss.bomb_live == 0 && g.boss.expose_t > 5.f, "all defused → exposed (live %x)", g.boss.bomb_live);
    CHECK(!game_boss_shielded(&g), "exposed: armor off");
    h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health > 99.f, "exposed Reloj takes full damage (%.1f)", h0 - m->health);
    run(&g, 400);
    CHECK(g.boss.bomb_live == 0xF && game_boss_shielded(&g), "re-arms after exposure");

    /* clock hand */
    g.boss.hand_spd = 0.f; g.boss.hand_ang = 0.f;
    Vec3 hp = v3_add(g.arena_center, v3(0.f, 0.f, 12.f)); hp.y = 0.f;
    CHECK(game_boss_hand_dist(&g, hp) < 0.1f, "hand lies along its angle");
    hp = v3_add(g.arena_center, v3(5.f, 0.f, 12.f));
    CHECK(game_boss_hand_dist(&g, hp) > 4.9f, "off-hand distance");
    g.player.pos = v3_add(g.arena_center, v3(0.f, 0.f, 12.f));
    g.player.pos.y = terrain_height(&g.terrain, g.player.pos.x, g.player.pos.z);
    g.player.vel = v3(0, 0, 0);
    { PlatInput in; memset(&in, 0, sizeof in); float h1 = g.player.health;
      for (int i = 0; i < 30; i++) { g.boss.hand_spd = 0.f; g.boss.hand_ang = 0.f; game_frame(&g, &in, DT); }
      CHECK(g.player.health < h1 - 5.f, "standing on the hand burns (%.1f)", h1 - g.player.health); }
    g.player.health = g.player.health_max;

    /* phases */
    m->health = m->health_max * 0.6f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 2 && g.boss.hand_spd > 0.5f && g.boss.bomb_t <= 35.f, "P2 TAC: faster hand, 35 s");
    m->health = m->health_max * 0.3f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 3 && g.boss.hand_spd < 0.f, "P3 MEDIANOCHE: hand reverses");
    game_render(&g);
    snprintf(b, sizeof b, "%s/reloj_hud.png", out); rend_save_png(b);

    /* win (not clean) */
    float st0 = g.leg.stature;
    kill(&g, g.boss.slot); run(&g, 2);
    CHECK(game_boss_active(&g) == -1 && (g.boss.beaten & 2), "El Reloj defeated");
    CHECK(strstr(g.message, "Synchronized") != NULL, "down line: %s", g.message);
    CHECK(g.leg.stature > st0, "stature up");
    CHECK(game_herald_headline(&g, g.leg.n - 1, b, 128) && strstr(b, "RELOJ"), "Herald: %s", b);
    run(&g, 70);
    CHECK(has(&g, "SINCRONIZADO") == 0, "no SINCRONIZADO after detonations");

    /* clean rematch */
    run(&g, 30);
    g.player.pos = v3(cp.x, cp.y, cp.z - 1.5f); run(&g, 2); press_use(&g);
    CHECK(game_boss_active(&g) == 1 && g.boss.dets == 0, "rematch resets detonations");
    m = &g.enemies.v[g.boss.slot];
    kill(&g, g.boss.slot); run(&g, 70);
    CHECK(has(&g, "SINCRONIZADO") == 1, "SINCRONIZADO for a zero-detonation win");
    CHECK(game_save(&g, 3, 0), "save");
    g.boss.beaten = 0; g.boss.clean_mask = 0;
    CHECK(game_load(&g, 3, 0) && (g.boss.beaten & 2) && (g.boss.clean_mask & 2), "B2 persists");
    DH_INFO("smoke15", "════ %d/%d checks passed ════", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
