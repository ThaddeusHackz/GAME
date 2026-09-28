/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M9 "soul" smoke test: Poncho + photo mode.
   Pure dog sim guarantees (never dies, lost ≤ 8 s, revive 3 s / auto 30 s,
   orders, bond, sniff radius) and live-game integration (free from snare,
   follows across the island, distract pulls a guard to SUSPICIOUS, combat
   knockout + hold-E revive, save/load persistence, photo mode freezes the
   sim, flies a tethered camera, cycles filters, writes a real PNG).
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif
#include "../src/core/dh_types.h"
#include "../src/core/dh_log.h"
#include "../src/core/settings.h"
#include "../src/ai/dog.h"
#include "../src/rend/rend.h"
#include "../src/game/game.h"
#include "../src/plat/plat.h"
#include "../src/audio/audio.h"
#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke9", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke9", "ok: " __VA_ARGS__); } while (0)
#define DT (1.0f / 60.0f)

static float flat(void *ud, float x, float z) { (void)ud; (void)x; (void)z; return 0.f; }

static void test_pure(void) {
    Dog d; DogWorld w = { v3(0, 0, 0), 0.f, 0, flat, NULL };
    dog_init(&d, v3(5, 0, 5), 1);
    CHECK(d.state == DOG_TRAPPED && !dog_active(&d), "starts trapped");
    for (int i = 0; i < 300; i++) dog_update(&d, &w, DT);
    CHECK(v3_dist(d.pos, v3(5, 0, 5)) < 0.01f, "trapped dog doesn't move");
    dog_free(&d);
    CHECK(d.state == DOG_FOLLOW, "freed -> FOLLOW");
    /* heel: player walks 40 m, dog keeps up */
    for (int i = 0; i < 600; i++) { w.player.z += 4.f * DT; dog_update(&d, &w, DT); }
    CHECK(v3_dist_xz(d.pos, w.player) < 5.f, "follows a walking player (%.1f m)", v3_dist_xz(d.pos, w.player));
    CHECK(d.teleports == 0, "no teleport while keeping up");
    /* lost: player sprints away faster than the dog (vehicle) */
    int tp0 = d.teleports; float lost_for = 0.f, worst = 0.f;
    for (int i = 0; i < 60 * 30; i++) {
        w.player.z += 25.f * DT; dog_update(&d, &w, DT);
        if (v3_dist_xz(d.pos, w.player) > DOG_LOST_DIST) { lost_for += DT; if (lost_for > worst) worst = lost_for; }
        else lost_for = 0.f;
    }
    CHECK(d.teleports > tp0, "lost dog teleported back (%d)", d.teleports - tp0);
    CHECK(worst <= 8.f, "never lost longer than 8 s (worst %.2f s)", worst);
    /* far snap */
    tp0 = d.teleports; w.player = v3(900, 0, 900); dog_update(&d, &w, DT);
    CHECK(d.teleports == tp0 + 1 && v3_dist_xz(d.pos, w.player) < 4.f, "act swap / fast travel snaps instantly");
    /* stay */
    dog_command(&d, DOG_CMD_STAY); Vec3 sp = d.pos;
    for (int i = 0; i < 600; i++) { w.player.x += 5.f * DT; dog_update(&d, &w, DT); }
    CHECK(d.state == DOG_STAY && v3_dist_xz(d.pos, sp) < 1.f, "STAY holds position while player leaves");
    CHECK(dog_cycle_command(&d) == DOG_CMD_HUNT && d.state == DOG_HUNT, "cycle STAY -> HUNT");
    CHECK(dog_cycle_command(&d) == DOG_CMD_FOLLOW, "cycle HUNT -> HEEL");
    /* never dies */
    int ko = dog_damage(&d, 1000.f);
    CHECK(ko && d.state == DOG_KO && d.hp == 0.f, "lethal damage knocks out (hp clamps at 0)");
    CHECK(!dog_damage(&d, 50.f) && d.state == DOG_KO, "no further damage while down");
    w.player = d.pos; w.reviving = 1; int t = 0, ev = 0;
    while (!(ev & DOG_EV_REVIVED) && t < 60 * 10) { ev = dog_update(&d, &w, DT); t++; }
    CHECK((ev & DOG_EV_REVIVED) && t / 60.f > 2.9f && t / 60.f < 3.2f, "hold-revive takes 3 s (%.2f)", t / 60.f);
    CHECK(d.hp > 0.f && dog_active(&d), "revived with hp %.0f", d.hp);
    dog_damage(&d, 1000.f); w.reviving = 0; t = 0; ev = 0;
    while (!(ev & DOG_EV_REVIVED) && t < 60 * 40) { ev = dog_update(&d, &w, DT); t++; }
    CHECK((ev & DOG_EV_REVIVED) && t / 60.f > 29.5f && t / 60.f < 30.5f, "auto-recover at 30 s (%.1f)", t / 60.f);
    /* bond */
    CHECK(dog_bond_level(&d) == 1 && dog_sniff_radius(&d) == 20.f, "bond 1 -> sniff 20 m");
    CHECK(dog_pet(&d) && !dog_pet(&d), "pet grows bond, 20 s cooldown");
    d.bond_xp = 500;
    CHECK(dog_bond_level(&d) == 5 && dog_sniff_radius(&d) == 28.f, "bond caps at 5 (28 m)");
    /* distract */
    CHECK(dog_distract(&d, v3(d.pos.x + 15, 0, d.pos.z)), "distract order accepted");
    int arrived = 0, barks = d.barks;
    for (int i = 0; i < 600; i++) { ev = dog_update(&d, &w, DT); if (ev & DOG_EV_ARRIVED) arrived++; }
    CHECK(arrived == 1 && d.barks > barks, "reaches target once and barks");
    CHECK(d.state == DOG_FOLLOW, "returns to orders after distracting");
}

static void run(Game *g, uint32_t held, uint32_t pressed, float my, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof in);
        in.buttons = held; in.pressed = (i == 0) ? pressed : 0u; in.my = my;
        game_frame(g, &in, DT);
    }
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out);
    dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke9", "════ M9 SOUL SMOKE ════");
    test_pure();
    audio_init();
    CHECK(audio_sfx_len(SFX_BARK) > 1000 && audio_sfx_len(SFX_JINGLE) > 1000 && audio_sfx_len(SFX_SHUTTER) > 100,
          "bark / jingle / shutter synthesised");

    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0xD06u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 0, 0, 0.f, 5);
    CHECK(g.dog.state == DOG_TRAPPED && v3_dist_xz(g.dog.pos, g.spawn) < 12.f, "Poncho snared near the spawn beach");
    /* walk up and free him */
    g.player.pos = v3(g.dog.pos.x + 1.2f, g.dog.pos.y, g.dog.pos.z);
    g.player.vel = v3(0, 0, 0);
    run(&g, BTN_USE, BTN_USE, 0.f, 1); run(&g, 0, 0, 0.f, 30);
    CHECK(g.dog.state == DOG_FOLLOW, "E frees Poncho");
    run(&g, 0, 0, 1.f, 300);                                  /* walk 5 s */
    CHECK(v3_dist_xz(g.dog.pos, g.player.pos) < 8.f, "heels on real terrain (%.1f m)", v3_dist_xz(g.dog.pos, g.player.pos));
    CHECK(g.dog.pos.y > -5.f && g.dog.pos.y < 200.f, "stays on the ground (y %.1f)", g.dog.pos.y);
    {   /* report still: Poncho in front of the camera */
        Dog keep = g.dog; Vec3 f2 = v3(sinf(g.player.yaw), 0, cosf(g.player.yaw));
        g.dog.pos = v3(g.player.pos.x + f2.x * 2.6f, 0, g.player.pos.z + f2.z * 2.6f);
        g.dog.pos.y = terrain_height(&g.terrain, g.dog.pos.x, g.dog.pos.z);
        g.dog.yaw = g.player.yaw + 2.2f; g.player.pitch = -0.35f;
        game_render(&g);
        char pth[256]; snprintf(pth, sizeof pth, "%s/m9_poncho.png", out); rend_save_png(pth);
        g.dog = keep; g.player.pitch = 0.f;
    }
    run(&g, 0, BTN_DOG, 0.f, 1); run(&g, 0, BTN_DOG, 0.f, 1);
    CHECK(g.dog.state == DOG_HUNT, "O cycles orders to HUNT");
    g.pickups.count = 0;
    g.pickups.v[0].pos = v3(g.dog.pos.x + 3.f, g.dog.pos.y, g.dog.pos.z); g.pickups.v[0].live = 1; g.pickups.v[0].kind = PK_AMMO;
    g.pickups.count = 1;
    run(&g, 0, 0, 0.f, 2);
    CHECK(g.dog_scents >= 1, "HUNT sniffs pickups in range (%d scents)", g.dog_scents);
    run(&g, 0, BTN_DOG, 0.f, 1);
    CHECK(g.dog.state == DOG_FOLLOW, "cycled back to HEEL");

    /* distract: one calm guard 18 m ahead under the crosshair */
    g.enemies.count = 0; g.enemies.alive_count = 0;
    Vec3 fw = v3(sinf(g.player.yaw), 0, cosf(g.player.yaw));
    Vec3 ep = v3(g.player.pos.x + fw.x * 18.f, 0, g.player.pos.z + fw.z * 18.f);
    ep.y = terrain_height(&g.terrain, ep.x, ep.z);
    g.player.pitch = 0.f;
    int ei = enemies_spawn(&g.enemies, ep, EN_GRUNT, 0, ep, ep);
    g.enemies.v[ei].yaw = g.player.yaw;                      /* facing away */
    run(&g, 0, 0, 0.f, 1);
    run(&g, BTN_AIM, BTN_AIM | BTN_DOG, 0.f, 1);
    CHECK(g.dog.state == DOG_DISTRACT, "aim + O sends Poncho at the guard");
    g.player.pos = v3(g.player.pos.x - fw.x * 25.f, 0, g.player.pos.z - fw.z * 25.f);   /* hang back out of sight */
    g.player.pos.y = terrain_height(&g.terrain, g.player.pos.x, g.player.pos.z) + 0.05f;
    g.player.vel = v3(0, 0, 0);
    g.enemies.v[ei].state = EN_PATROL; g.enemies.v[ei].meter = 0.f;
    int sus = 0;
    for (int i = 0; i < 60 * 8 && !sus; i++) {
        run(&g, BTN_CROUCH, 0, 0.f, 1);
        if (g.dog.state == DOG_DISTRACT && g.dog.state_t >= 100.f && g.enemies.v[ei].state >= EN_SUSPICIOUS) sus = 1;
    }
    CHECK(sus, "guard pulled to SUSPICIOUS by the barking (%s)", enemy_state_name(g.enemies.v[ei].state));
    CHECK(v3_dist_xz(g.enemies.v[ei].lkp, g.dog.pos) < 6.f, "guard investigates the dog, not the player (lkp %.1f m from dog, %.1f from player, meter %.2f)", v3_dist_xz(g.enemies.v[ei].lkp, g.dog.pos), v3_dist_xz(g.enemies.v[ei].lkp, g.player.pos), g.enemies.v[ei].meter);

    /* combat knockout + hold E revive */
    g.enemies.v[ei].state = EN_COMBAT;
    g.dog.state = DOG_FOLLOW;
    g.dog.pos = g.enemies.v[ei].pos; g.dog.pos.x += 1.f;
    int down = 0;
    for (int i = 0; i < 60 * 10 && !down; i++) {
        g.enemies.v[ei].state = EN_COMBAT; g.enemies.v[ei].pos = v3(g.dog.pos.x - 1.f, g.dog.pos.y, g.dog.pos.z);
        g.player.health = g.player.health_max;
        run(&g, 0, 0, 0.f, 1); if (g.dog.state == DOG_KO) down = 1;
    }
    CHECK(down, "a fighting guard at close range knocks Poncho out");
    g.enemies.count = 0; g.enemies.alive_count = 0;
    g.player.pos = v3(g.dog.pos.x + 1.f, g.dog.pos.y, g.dog.pos.z); g.player.vel = v3(0,0,0);
    run(&g, BTN_USE, BTN_USE, 0.f, 60 * 3 + 20);
    CHECK(dog_active(&g.dog), "holding E beside him revives Poncho");

    /* save / load */
    g.dog.bond_xp = 120;
    CHECK(game_save(&g, 2, 0), "save with Poncho");
    dog_init(&g.dog, g.spawn, 1);
    CHECK(game_load(&g, 2, 0), "load");
    CHECK(g.dog.state != DOG_TRAPPED && g.dog.bond_xp == 120, "freed + bond persist (bond xp %d)", g.dog.bond_xp);

    /* photo mode */
    CHECK(game_photo_filter_count() == 6, "6 photo filters");
    run(&g, 0, 0, 0.f, 10);
    Vec3 pp = g.player.pos; float t0 = g.day_t; int kills = g.kills;
    run(&g, 0, BTN_PHOTO, 0.f, 1);
    CHECK(g.mode == GM_PHOTO, "P enters photo mode");
    Vec3 c0 = g.ph_pos;
    run(&g, BTN_SPRINT, 0, 1.f, 60 * 3);                     /* fly forward fast */
    CHECK(v3_dist(g.player.pos, pp) < 0.05f, "simulation frozen (player didn't move)");
    CHECK(v3_dist(g.ph_pos, c0) > 20.f, "free camera flies (%.0f m)", v3_dist(g.ph_pos, c0));
    run(&g, BTN_SPRINT, 0, 1.f, 60 * 5);
    CHECK(v3_dist(g.ph_pos, v3(pp.x, pp.y + 1.6f, pp.z)) <= 60.5f, "camera tethered to 60 m");
    run(&g, 0, BTN_RELOAD, 0.f, 1); run(&g, 0, BTN_RELOAD, 0.f, 1);
    CHECK(g.ph_filter == 2, "R cycles filter (%s)", game_photo_filter_name(g.ph_filter));
    run(&g, 0, BTN_ROLL, 0.f, 1);
    CHECK(g.day_t != t0, "V advances time of day");
    float f0 = g.ph_fov; run(&g, BTN_SLOT1, 0, 0.f, 30);
    CHECK(g.ph_fov < f0, "1 zooms in");
    run(&g, 0, BTN_USE, 0.f, 1);
    game_render(&g);
    CHECK(g.ph_shots == 1 && dh_fs_exists(g.ph_last), "capture wrote %s", g.ph_last);
    FILE *f = fopen(g.ph_last, "rb"); unsigned char sig[8] = {0};
    if (f) { fread(sig, 1, 8, f); fclose(f); }
    CHECK(sig[0] == 137 && sig[1] == 'P' && sig[2] == 'N' && sig[3] == 'G', "capture is a PNG");
    CHECK(g.kills == kills, "no sim side effects in photo mode");
    run(&g, 0, BTN_PHOTO, 0.f, 1);
    CHECK(g.mode == GM_PLAY, "P exits photo mode");
    game_render(&g);
    CHECK(rend()->draw_calls > 0, "renders normally after photo mode");

    DH_INFO("smoke9", "──── %d checks, %d failed ────", g_checks, g_failed);
    printf("M9 SOUL SMOKE %s: %d/%d checks\n", g_failed ? "FAILED" : "PASSED", g_checks - g_failed, g_checks);
    game_free(&g);
    return g_failed ? 1 : 0;
}
