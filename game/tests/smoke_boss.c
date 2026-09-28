/* DIVIDED HORIZON — M14 smoke: boss B1 La Sargento (Spec 25/75). */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke14", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke14", "ok: " __VA_ARGS__); } while (0)
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
    DH_INFO("smoke14", "════ M14 BOSS SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x14B0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 40);
    char b[128];

    CHECK(game_boss_active(&g) == -1 && game_boss_phase(&g) == 0, "no boss at start");
    CHECK(!strcmp(game_boss_name(0), "LA SARGENTO"), "B1 is La Sargento");
    CHECK(!game_boss_start(&g, 6), "unbuilt boss slots refuse to start (honest)");

    /* E far from the drum does nothing; at the drum it starts the fight */
    press_use(&g);
    CHECK(game_boss_active(&g) == -1, "E away from the drum does nothing");
    Vec3 dp = game_boss_drum_pos(&g);
    g.player.pos = v3(dp.x, dp.y, dp.z - 1.5f); g.player.vel = v3(0, 0, 0);
    run(&g, 2);
    CHECK(g.boss.prompt_t > 0.f, "drum prompt shows near the drum");
    int n0 = g.enemies.count;
    press_use(&g);
    CHECK(game_boss_active(&g) == 0 && game_boss_phase(&g) == 1, "war drum calls La Sargento (phase 1)");
    CHECK(g.enemies.count - n0 == 7, "Marta + 4 shields + 2 drummers (%d)", g.enemies.count - n0);
    CHECK(game_boss_shielded(&g), "cadence up: shield wall holds");
    Enemy *m = &g.enemies.v[g.boss.slot];
    CHECK(m->health_max > 500.f, "boss HP pool (%.0f)", m->health_max);
    run(&g, 1);
    float h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health > 9.f && h0 - m->health < 11.f, "shielded Marta takes 10%% (%.1f)", h0 - m->health);
    { Enemy *s = &g.enemies.v[g.boss.shield[0]]; float s0 = s->health; enemy_apply_damage(s, 50.f, 0);
      CHECK(s0 - s->health < 10.5f, "shield-bearer absorbs 80%% (%.1f)", s0 - s->health); }
    run(&g, 1); game_render(&g);
    { char p[256]; snprintf(p, sizeof p, "%s/boss_hud.png", out); rend_save_png(p); }

    /* silence one drummer → stagger */
    kill(&g, g.boss.drum[0]); run(&g, 2);
    CHECK(!game_boss_shielded(&g) && g.boss.stagger_t > 3.5f, "drummer down: STAGGER (%.1f s)", g.boss.stagger_t);
    h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health > 99.f, "staggered Marta takes full damage");
    run(&g, 60 * 5);
    CHECK(game_boss_shielded(&g), "stagger wears off while a drum still beats");
    kill(&g, g.boss.drum[1]); run(&g, 60 * 5);
    CHECK(!game_boss_shielded(&g) && game_boss_phase(&g) == 1, "all drums silent: wall stays broken");

    /* phase 2 */
    m->health = m->health_max * 0.6f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 2, "below 66%%: CORREGIR");
    CHECK(game_boss_shielded(&g), "drummers return in phase 2");
    CHECK(g.boss.flank[0] >= 0 && g.boss.flank[1] >= 0, "two flankers join");
    { int beats = g.boss.beat_i; run(&g, 60); int d = g.boss.beat_i - beats;
      CHECK(d >= 2, "phase-2 beat plays faster (%d beats/s)", d); }

    /* phase 3 */
    m->health = m->health_max * 0.3f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 3 && !game_boss_shielded(&g), "below 33%%: MAS FUERTE, no wall");
    g.player.pos = v3_add(m->pos, v3(3.f, 0.f, 0.f)); g.boss.whistle_t = 0.01f;
    { PlatInput in; memset(&in, 0, sizeof in); game_frame(&g, &in, DT); game_frame(&g, &in, DT); }
    CHECK(g.boss.stun_t > 0.f, "close-range whistle stuns");
    g.player.health = g.player.health_max;

    /* victory */
    float st0 = g.leg.stature; int ed0 = g.leg.n;
    kill(&g, g.boss.slot); run(&g, 2);
    CHECK(game_boss_active(&g) == -1 && (g.boss.beaten & 1), "La Sargento defeated");
    CHECK(strstr(g.message, "The unit... stands.") != NULL, "down line: %s", g.message);
    CHECK(g.leg.stature > st0, "victory raises stature");
    CHECK(g.leg.n > ed0 && game_herald_headline(&g, g.leg.n - 1, b, 128) && strstr(b, "SARGENTO"), "Herald: %s", b);
    { int alive = 0; for (int i = 0; i < g.enemies.count; i++)
        if (g.enemies.v[i].faction == 5 && g.enemies.v[i].state != EN_DEAD) alive++;
      CHECK(alive == 0, "unit disperses (%d left)", alive); }
    run(&g, 70);
    CHECK(has(&g, "FORMAR") == 1, "FORMAR stamped for a clean win");

    /* save round-trip */
    CHECK(game_save(&g, 3, 0), "save");
    g.boss.beaten = 0; g.boss.clean_mask = 0;
    CHECK(game_load(&g, 3, 0), "load");
    CHECK((g.boss.beaten & 1) && (g.boss.clean_mask & 1), "boss defeat persists");

    /* rematch + loss */
    run(&g, 30);
    dp = game_boss_drum_pos(&g);
    g.player.pos = v3(dp.x, dp.y, dp.z - 1.5f); run(&g, 2);
    press_use(&g);
    CHECK(game_boss_active(&g) == 0, "rematch available");
    g.player.health = 0.f;
    { PlatInput in; memset(&in, 0, sizeof in); for (int i = 0; i < 5; i++) game_frame(&g, &in, DT); }
    CHECK(game_boss_active(&g) == -1 && g.boss.losses == 1, "going down ends the fight (loss %d)", g.boss.losses);
    DH_INFO("smoke14", "════ %d/%d checks passed ════", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
