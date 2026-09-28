/* DIVIDED HORIZON — M13 smoke: Los Cobradores bounty squad (Spec 26.2).
   Infamy from tallies, threshold trigger, Poncho warning for the tracker,
   per-hunter gimmicks, wound/return with scars, escape, third-defeat
   obituary, squad-broken trinket + feat, save round-trip, city spawn. */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke13", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke13", "ok: " __VA_ARGS__); } while (0)
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

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke13", "════ M13 BOUNTY SQUAD SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x13B0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 40);
    char b[128];

    CHECK(game_cob_infamy(&g) < 1.f, "fresh game: no infamy (%.1f)", game_cob_infamy(&g));
    CHECK(game_cob_active(&g) == -1, "no hunter at start");
    CHECK(!strcmp(game_cob_name(0), "RASTRA") && !strcmp(game_cob_name(3), "LUCKY"), "four named hunters");

    g.prog.ev_count[EV_KILL] += 10; run(&g, 2);
    CHECK(game_cob_infamy(&g) >= 9.f, "kills raise infamy (%.1f)", game_cob_infamy(&g));
    run(&g, 120);
    CHECK(game_cob_active(&g) == -1, "below threshold: nobody comes");
    g.dog.state = DOG_FOLLOW;   /* Poncho freed -> he warns about the tracker */
    g.prog.ev_count[EV_ALARM] += 6; run(&g, 2);
    CHECK(game_cob_infamy(&g) >= 30.f, "alarms push infamy over threshold (%.1f)", game_cob_infamy(&g));

    /* Poncho is trapped at start -> Rastra comes straight in; free him to test the warning path */
    CHECK(game_cob_active(&g) == -1 && g.cob.warn_t > 0.f, "Poncho growls before the tracker arrives");
    run(&g, 200);
    CHECK(game_cob_active(&g) == 0, "RASTRA arrives after the growl");
    Enemy *e = &g.enemies.v[g.cob.slot];
    Vec3 fw = v3(sinf(g.player.yaw), 0.f, cosf(g.player.yaw));
    Vec3 dp = v3_sub(e->pos, g.player.pos);
    CHECK(dp.x * fw.x + dp.z * fw.z < 0.f, "tracker spawns behind the player");
    CHECK(e->state == EN_COMBAT, "tracker hunts immediately");
    float hp1 = e->health_max;
    run(&g, 1); game_render(&g);
    { char p[256]; snprintf(p, sizeof p, "%s/bounty_hud.png", out); rend_save_png(p); }
    kill_hunter(&g); run(&g, 2);
    CHECK(game_cob_active(&g) == -1 && g.cob.enc[0] == 1, "RASTRA wounded (1/3)");
    run(&g, 31);
    CHECK(has(&g, "COLLECTION REFUSED") == 1, "COLLECTION REFUSED stamped");
    CHECK(g.cob.cd > 60.f, "cooldown after an encounter");

    /* Vidente: far marksman */
    CHECK(game_cob_spawn(&g, 1), "spawn VIDENTE");
    e = &g.enemies.v[g.cob.slot];
    float dv = v3_len(v3_sub(e->pos, g.player.pos));
    CHECK(dv > 35.f && e->arch == EN_OFFICER, "marksman spawns far (%.0f m), officer", dv);
    CHECK(!game_cob_spawn(&g, 2), "one hunter at a time");
    /* escape */
    g.cob.t = 119.f; run(&g, 90);
    CHECK(game_cob_active(&g) == -1 && g.cob.enc[1] == 0, "VIDENTE withdraws on timeout, no defeat");

    /* Pulpo: brawler + goons */
    g.cob.active = 0;
    int c0 = g.enemies.count;
    CHECK(game_cob_spawn(&g, 2), "spawn PULPO");
    CHECK(g.enemies.count - c0 == 3 && g.cob.goons == 2, "PULPO brings two goons");
    CHECK(g.enemies.v[g.cob.slot].arch == EN_BRUISER, "PULPO is a bruiser");
    kill_hunter(&g); run(&g, 2);

    /* Lucky: calm until close */
    CHECK(game_cob_spawn(&g, 3), "spawn LUCKY");
    e = &g.enemies.v[g.cob.slot];
    CHECK(e->state != EN_COMBAT, "LUCKY walks up calm");
    e->pos = v3_add(g.player.pos, v3(1.5f, 0.f, 0.f)); run(&g, 2);
    CHECK(e->state == EN_COMBAT, "LUCKY turns hostile up close");
    kill_hunter(&g); run(&g, 2);

    /* scars + fall */
    CHECK(game_cob_spawn(&g, 0), "RASTRA returns");
    CHECK(g.enemies.v[g.cob.slot].health_max > hp1 * 1.2f, "returns scarred (+HP)");
    kill_hunter(&g); run(&g, 2);
    int pages = g.leg.n;
    CHECK(game_cob_spawn(&g, 0), "RASTRA third hunt");
    kill_hunter(&g); run(&g, 2);
    CHECK(g.cob.enc[0] == 3 && game_cob_fallen(&g) == 1, "RASTRA falls for good");
    CHECK(g.leg.n > pages && last_head(&g, b) && strstr(b, "RASTRA"), "Herald obituary: %s", b);
    CHECK(!game_cob_spawn(&g, 0), "fallen hunter never returns");

    /* save round-trip */
    int enc[4]; memcpy(enc, g.cob.enc, sizeof enc);
    CHECK(game_save(&g, 3, 0), "save");
    memset(g.cob.enc, 0, sizeof g.cob.enc);
    CHECK(game_load(&g, 3, 0), "load");
    CHECK(!memcmp(enc, g.cob.enc, sizeof enc), "hunter scars persist (%d %d %d %d)", g.cob.enc[0], g.cob.enc[1], g.cob.enc[2], g.cob.enc[3]);
    CHECK(g.cob.active == 0, "no hunter mid-load");

    /* break the squad */
    float st0 = g.leg.stature;
    for (int who = 1; who < 4; who++) while (g.cob.enc[who] < 3) {
        g.cob.active = 0;
        if (!game_cob_spawn(&g, who)) break;
        kill_hunter(&g); run(&g, 2);
    }
    CHECK(game_cob_fallen(&g) == 4 && g.cob.trinket, "all four fallen: trinket taken");
    CHECK(g.leg.stature > st0, "breaking the squad raises stature");
    CHECK(last_head(&g, b) && (strstr(b, "COBRADORES") || strstr(b, "HUNTER") || strstr(b, "SQUAD")), "Herald: %s", b);
    run(&g, 70);
    CHECK(has(&g, "LUCKY'S COIN") == 1, "LUCKY'S COIN stamped");
    g.cob.infamy = 100.f; g.cob.cd = 0.f; run(&g, 300);
    CHECK(game_cob_active(&g) == -1, "broken squad sends nobody");

    /* city */
    game_load_city(&g);
    memset(g.cob.enc, 0, sizeof g.cob.enc); g.cob.trinket = 0;
    run(&g, 30);
    g.cob.infamy = 50.f; g.cob.cd = 0.f; g.cob.next = 2;
    for (int i = 0; i < 180 && game_cob_active(&g) < 0; i++) run(&g, 1);
    DH_INFO("smoke13", "city dbg: mode=%d enemies=%d cd=%.1f inf=%.1f arena=%d act=%d who=%d down=%d", g.mode, g.enemies.count, g.cob.cd, g.cob.infamy, g.arena_active, g.cob.active, g.cob.who, player_is_down(&g.player));
    CHECK(game_cob_active(&g) == 2, "hunters follow you to Meridian");
    DH_INFO("smoke13", "════ %d/%d checks passed ════", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
