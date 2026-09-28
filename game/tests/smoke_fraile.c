/* DIVIDED HORIZON — M17 smoke: boss B4 El Fraile (Spec 25/75). */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke17", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke17", "ok: " __VA_ARGS__); } while (0)
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

static void press(Game *g, uint32_t b) {
    PlatInput in; memset(&in, 0, sizeof in); in.pressed = b;
    game_frame(g, &in, DT);
}
/* wait (keeping the player healthy) until the parry window opens; returns frames */
static int until_window(Game *g, int max) {
    for (int i = 0; i < max; i++) { if (g->boss.parry_t > 0.f) return i; run(g, 1); }
    return -1;
}
static void near_him(Game *g) {
    Enemy *m = &g->enemies.v[g->boss.slot];
    g->player.pos = v3(m->pos.x, m->pos.y, m->pos.z - 2.5f); g->player.vel = v3(0, 0, 0);
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke17", "════ M17 EL FRAILE SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x17B0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 40);
    (void)kill_hunter; (void)last_head; (void)step; (void)kill;
    char b[128];
    CHECK(!strcmp(game_boss_name(3), "EL FRAILE"), "B4 is El Fraile");
    CHECK(!game_boss_start(&g, 5), "unbuilt slot 6 still refuses");
    CHECK(has(&g, "LA TERCER CAMPANA") == 0, "feat LA TERCER CAMPANA exists, locked");

    Vec3 tp = game_boss_tower_pos(&g);
    g.player.pos = v3(tp.x, tp.y, tp.z - 1.5f); g.player.vel = v3(0, 0, 0);
    run(&g, 2);
    CHECK(g.boss.prompt_t > 0.f && g.boss.prompt_who == 3, "bell-rope prompt shows");
    int n0 = g.enemies.count;
    press_use(&g);
    CHECK(game_boss_active(&g) == 3 && game_boss_phase(&g) == 1, "rope calls El Fraile");
    CHECK(g.enemies.count - n0 == 1, "he comes alone (%d)", g.enemies.count - n0);
    Enemy *m = &g.enemies.v[g.boss.slot];
    run(&g, 1);
    CHECK(game_boss_shielded(&g), "unstaggered he shrugs off hits");
    float h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health < 6.f, "95%% shrug (%.1f)", h0 - m->health);

    /* bells telegraph: 3 rings then a window */
    near_him(&g);
    int f = until_window(&g, 400);
    CHECK(f > 0 && g.boss.ring_i == 3, "third ring opens the parry window (%d frames)", f);
    /* miss it: the swing lands */
    float ph = g.player.health_max; g.player.health = ph;
    for (int i = 0; i < 40 && g.boss.parry_t > 0.f; i++) { near_him(&g); step(&g, 0); }
    CHECK(g.boss.swings_hit == 1 && g.player.health <= ph - 19.f, "missed parry: swing lands (%.0f)", g.player.health);
    g.player.health = ph;
    /* far away: swing whiffs */
    until_window(&g, 400);
    g.player.pos = v3(m->pos.x + 12.f, m->pos.y, m->pos.z); g.player.vel = v3(0, 0, 0);
    for (int i = 0; i < 40 && g.boss.parry_t > 0.f; i++) step(&g, 0);
    CHECK(g.boss.swings_hit == 1, "at range the swing whiffs");
    g.player.health = ph;
    /* parry */
    near_him(&g); until_window(&g, 400); near_him(&g);
    press(&g, BTN_MELEE);
    CHECK(g.boss.parries == 1 && g.boss.stagger_t > 2.5f && !game_boss_shielded(&g), "T on the third bell: PARRY, he staggers");
    run(&g, 1);
    h0 = m->health; enemy_apply_damage(m, 100.f, 0);
    CHECK(h0 - m->health > 90.f, "staggered: full damage (%.1f)", h0 - m->health);

    /* P2 */
    m->health = m->health_max * 0.6f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 2, "P2 TOLL at <66%%");
    int acol = 0;
    for (int k = 0; k < 2; k++) if (g.boss.flank[k] >= 0 && g.enemies.v[g.boss.flank[k]].state != EN_DEAD) acol++;
    CHECK(acol == 2, "two acolytes answer the toll");
    near_him(&g);
    run(&g, 200);  /* let any stagger lapse */
    int f1 = until_window(&g, 400); near_him(&g);
    CHECK(f1 >= 0 && g.boss.parry_t <= 0.36f, "P2 window tighter (%.2f)", g.boss.parry_t);

    /* P3 chain */
    m->health = m->health_max * 0.3f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 3, "P3 LAST BELL at <33%%");
    g.player.pos = v3(m->pos.x, m->pos.y, m->pos.z - 10.f); g.player.vel = v3(0, 0, 0);
    g.boss.chain_t = 0.01f; step(&g, 0);
    Vec3 dv = v3_sub(m->pos, g.player.pos);
    CHECK(g.player.vel.x * dv.x + g.player.vel.z * dv.z > 0.f, "chain wrap drags you toward him");

    /* kneel + spare */
    m->health = m->health_max * 0.1f; run(&g, 2);
    CHECK(g.boss.kneel && !game_boss_shielded(&g), "at 15%% he kneels");
    near_him(&g); g.player.pos.z = m->pos.z - 2.f; run(&g, 1); near_him(&g); g.player.pos.z = m->pos.z - 2.f;
    game_render(&g); { char pb[256]; snprintf(pb, sizeof pb, "%s/M17_fraile_kneel.png", out); rend_save_png(pb); }
    press_use(&g);
    CHECK(game_boss_active(&g) == -1 && (g.boss.beaten & 8) && (g.boss.clean_mask & 8), "SUBDUE: spared, clean");
    CHECK(strstr(g.message, "rings the bell once") != NULL, "down: kneels, rings once, no line");
    run(&g, 30);
    CHECK(has(&g, "LA TERCER CAMPANA") == 1, "feat LA TERCER CAMPANA unlocked");
    CHECK(last_head(&g, b) && strstr(b, "EL FRAILE"), "herald reports it: %s", b);

    /* rematch: strike him down instead -> not clean-counted anew but beaten stays */
    int cm = g.boss.clean_mask;
    g.player.pos = v3(tp.x, tp.y, tp.z - 1.5f); g.player.vel = v3(0, 0, 0); run(&g, 2);
    press_use(&g);
    CHECK(game_boss_active(&g) == 3, "rematch prompt restarts");
    m = &g.enemies.v[g.boss.slot];
    near_him(&g); until_window(&g, 400); near_him(&g); press(&g, BTN_MELEE);
    game_render(&g); { char pb[256]; snprintf(pb, sizeof pb, "%s/M17_fraile_parry.png", out); rend_save_png(pb); }
    m->health = m->health_max * 0.3f; run(&g, 2);
    m->health = m->health_max * 0.1f; run(&g, 2);
    m->armor = 0.f; enemy_apply_damage(m, 99999.f, 0); run(&g, 2);
    CHECK(game_boss_active(&g) == -1 && strstr(g.message, "unrung"), "STRIKE: he falls");
    CHECK(g.boss.clean_mask == cm, "clean mask unchanged by striking");

    /* save roundtrip */
    game_save(&g, 1, 0);
    g.boss.beaten = 0; g.boss.clean_mask = 0;
    game_load(&g, 1, 0);
    CHECK((g.boss.beaten & 8) && (g.boss.clean_mask & 8), "save keeps B4 beaten+spared");

    DH_INFO("smoke17", "════ %d checks, %d failed ════", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
