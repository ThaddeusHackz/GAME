/* DIVIDED HORIZON — M19 smoke: finale El Sereno (Spec 25/75). */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke19", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke19", "ok: " __VA_ARGS__); } while (0)
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

static void press(Game *g, unsigned b) {
    PlatInput in; memset(&in, 0, sizeof in); in.pressed = b; game_frame(g, &in, DT);
    g->player.health = g->player.health_max;
}
static void clear_wave(Game *g) { for (int k = 0; k < 4; k++) if (g->boss.shield[k] >= 0) kill(g, g->boss.shield[k]); run(g, 2); }
static void to_offer(Game *g, Vec3 lp) {
    g->player.pos = at(g, lp); g->player.vel = v3(0, 0, 0); run(g, 2);
    press_use(g);
    for (int w = 0; w < 3; w++) clear_wave(g);
    Enemy *m = &g->enemies.v[g->boss.slot];
    m->armor = 0.f; g->boss.stagger_t = 5.f;
    m->health = m->health_max * 0.3f; run(g, 2);
    g->player.pos = v3(m->pos.x, m->pos.y, m->pos.z - 2.f); run(g, 1);
}
int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke19", "════ M19 EL SERENO SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x19B0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 40);
    (void)kill_hunter; (void)step; (void)last_head;
    char pb[512];
    CHECK(!strcmp(game_boss_name(5), "EL SERENO"), "slot 5 is El Sereno");
    CHECK(has(&g, "YOU WERE LISTENING") == 0 && has(&g, "THE LAST LIGHT") == 0, "finale feats exist, locked");
    Vec3 lp = game_boss_lantern_pos(&g);
    g.player.pos = at(&g, lp); g.player.vel = v3(0, 0, 0); run(&g, 2);
    CHECK(g.boss.prompt_t > 0.f && g.boss.prompt_who == 5, "lantern prompt shows");
    press_use(&g);
    CHECK(game_boss_active(&g) == -1 && strstr(g.message, "0 of 5"), "lantern cold without lieutenants: %s", g.message);
    g.boss.beaten = 0x1F; g.boss.clean_mask = 0;
    run(&g, 25);
    press_use(&g);
    CHECK(game_boss_active(&g) == 5 && game_boss_phase(&g) == 1, "finale begins: THE GATHERING");
    Enemy *m = &g.enemies.v[g.boss.slot];
    run(&g, 1);
    float h0 = m->health; enemy_apply_damage(m, 200.f, 0);
    CHECK(m->health > h0 - 1.f, "P1: untouchable");
    CHECK(g.boss.wave == 1, "wave 1 spawned");
    clear_wave(&g); CHECK(g.boss.wave == 2 && game_boss_phase(&g) == 1, "wave 2");
    clear_wave(&g); CHECK(g.boss.wave == 3, "wave 3");
    game_render(&g);
    snprintf(pb, sizeof pb, "%s/M19_gathering.png", out); rend_save_png(pb);
    clear_wave(&g); CHECK(game_boss_phase(&g) == 2, "waves cleared: THE HOST");
    m = &g.enemies.v[g.boss.slot];
    g.player.pos = v3(m->pos.x, m->pos.y, m->pos.z - 2.f);
    int called = 0;
    for (int i = 0; i < 400 && !called; i++) { run(&g, 1); m->vel = v3(0,0,0); g.player.pos = v3(m->pos.x, m->pos.y, m->pos.z - 2.f); called = g.boss.parry_t > 0.f; }
    CHECK(called && game_boss_shielded(&g), "he calls the cut; guarded (0.9)");
    press(&g, BTN_MELEE);
    CHECK(g.boss.stagger_t > 2.5f && strstr(g.message, "Good. Again."), "parry: \"%s\"", g.message);
    CHECK(!game_boss_shielded(&g) && m->armor == 0.f, "staggered: open");
    game_render(&g);
    snprintf(pb, sizeof pb, "%s/M19_host.png", out); rend_save_png(pb);
    m->health = m->health_max * 0.3f; run(&g, 2);
    CHECK(game_boss_phase(&g) == 3 && game_boss_active(&g) == 5, "THE OFFERING");
    CHECK(m->state != EN_DEAD, "he stands to offer");
    /* not a hero yet: silence does nothing */
    run(&g, 60 * 9);
    CHECK(game_boss_active(&g) == 5, "silence is not offered to the unproven");
    game_render(&g);
    snprintf(pb, sizeof pb, "%s/M19_offering.png", out); rend_save_png(pb);
    g.player.pos = v3(m->pos.x, m->pos.y, m->pos.z - 2.f); g.player.vel = v3(0, 0, 0);
    press(&g, BTN_USE);
    CHECK(game_boss_active(&g) == -1 && g.boss.ending == 1 && (g.boss.beaten & 32), "E: Tobias ending");
    CHECK(strstr(g.message, "family was always the ledger") != NULL, "line: %s", g.message);
    run(&g, 30);
    CHECK(has(&g, "THE LAST LIGHT") == 1 && has(&g, "YOU WERE LISTENING") == 0, "LAST LIGHT unlocked, third way not");

    run(&g, 25); to_offer(&g, lp);
    CHECK(game_boss_phase(&g) == 3, "rematch reaches the offering");
    press(&g, BTN_MELEE);
    CHECK(g.boss.ending == 2 && strstr(g.message, "only ever paper"), "T: ledger: %s", g.message);

    run(&g, 25);
    g.boss.clean_mask = 0x1F; game_stature_add(&g, 200.f);
    CHECK(game_stature_tier(&g) >= 1, "hero stature (%d)", game_stature_tier(&g));
    to_offer(&g, lp);
    int frames = 0;
    while (game_boss_active(&g) == 5 && frames < 60 * 10) { run(&g, 1); frames++; }
    CHECK(g.boss.ending == 3 && frames >= 60 * 8 - 3, "silence 8 s: third way (%d frames)", frames);
    CHECK(strstr(g.message, "You were listening") != NULL, "line: %s", g.message);
    run(&g, 30);
    CHECK(has(&g, "YOU WERE LISTENING") == 1, "feat YOU WERE LISTENING");

    game_save(&g, 1, 0); g.boss.beaten = 0; game_load(&g, 1, 0);
    CHECK(g.boss.beaten & 32, "save keeps the finale");
    DH_INFO("smoke19", "════ %d checks, %d failed ════", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
