/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M7 polish smoke test (headless, deterministic)
   Proves: procedural SFX bank synthesises; mixer is silent when idle, audible
   when a voice plays, obeys volume + ducking, steals voices at the cap; the
   adaptive score eases toward its target; in the live game gunfire / steps /
   jumps raise cues, flocks exist and scatter from gunfire, camera shake is
   gated by the camera_shake + photosensitivity settings, and difficulty
   scales incoming damage.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif
#include "../src/core/dh_types.h"
#include "../src/core/dh_log.h"
#include "../src/core/settings.h"
#include "../src/world/terrain.h"
#include "../src/player/player.h"
#include "../src/ai/enemy.h"
#include "../src/rend/rend.h"
#include "../src/city/heat.h"
#include "../src/city/city.h"
#include "../src/meta/progress.h"
#include "../src/meta/mission.h"
#include "../src/game/game.h"
#include "../src/plat/plat.h"
#include "../src/audio/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke7", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke7", "ok: " __VA_ARGS__); } while (0)
#define DT (1.0f / 60.0f)

static int peak_of(int frames) {
    int pk = 0;
    for (int i = 0; i < frames; i++) { int p = plat_audio_pump(); if (p > pk) pk = p; }
    return pk;
}
static void run(Game *g, uint32_t held, float my, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof in);
        in.buttons = held; in.pressed = (i == 0) ? held : 0u; in.my = my;
        game_frame(g, &in, DT);
        plat_audio_pump();
    }
}

static void test_mixer(void) {
    CHECK(audio_init(), "procedural SFX bank synthesised");
    int all = 1;
    for (int i = 0; i < SFX_N; i++) if (audio_sfx_len((SfxId)i) < 200) all = 0;
    CHECK(all, "all %d SFX have real sample data", SFX_N);
    AudioMix m = { 1.f, 1.f, 1.f, 0.f };
    audio_set_mix(&m);
    audio_set_music_on(0);
    peak_of(120);                                   /* drain anything */
    CHECK(peak_of(10) == 0, "mixer is silent with no voices and music off");
    audio_play(SFX_SHOT_RIFLE, 1.f, 1.f);
    int loud = peak_of(3);
    CHECK(loud > 3000, "a rifle shot is clearly audible (peak %d)", loud);
    peak_of(120);
    m.sfx = 0.f; audio_set_mix(&m);
    audio_play(SFX_SHOT_RIFLE, 1.f, 1.f);
    CHECK(peak_of(3) == 0, "SFX volume 0 silences effects");
    m.sfx = 1.f; m.duck = 1.f; audio_set_mix(&m);
    audio_play(SFX_SHOT_RIFLE, 1.f, 1.f);
    int ducked = peak_of(3);
    CHECK(ducked > 0 && ducked < loud, "ducking attenuates (%d < %d)", ducked, loud);
    m.duck = 0.f; audio_set_mix(&m);
    for (int i = 0; i < 40; i++) audio_play(SFX_SIREN, 0.2f, 1.f);
    CHECK(audio_active_voices() == AUDIO_VOICES, "voice cap %d respected (stealing)", AUDIO_VOICES);
    peak_of(200);
    CHECK(audio_active_voices() == 0, "voices retire after their sample ends");
    audio_set_music_on(1);
    audio_set_intensity(1.f);
    int mpk = peak_of(240);
    CHECK(mpk > 200, "procedural score produces output (peak %d)", mpk);
    CHECK(audio_intensity() > 0.5f, "intensity eases toward target (%.2f)", audio_intensity());
    audio_set_intensity(0.f);
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out);
    dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke7", "════ M7 POLISH SMOKE ════");
    test_mixer();

    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 320, 180, REND_SOFT, 0xC0FFEEu)) { CHECK(0, "game_init"); return 1; }
    settings()->camera_shake = 1; settings()->photosensitivity = 0;   /* other suites share settings.json */
    settings()->hit_marker = 1; settings()->difficulty = DIFF_NORMAL;
    settings()->enemy_damage = 1.f; settings()->adaptive_music = 1;
    settings()->vol_master = settings()->vol_sfx = settings()->vol_music = 1.f;
    game_start_play(&g);
    run(&g, 0, 0.f, 5);
    CHECK(g.pol.inited, "polish layer initialised on first frame");
    int birds = 0;
    for (int f = 0; f < BIRD_FLOCKS; f++) if (g.pol.flock[f].home.y > 20.f) birds++;
    CHECK(birds == BIRD_FLOCKS, "%d bird flocks placed above the island", birds);

    uint64_t p0 = audio_plays_total();
    run(&g, 0, 1.f, 150);                           /* walk forward 2.5 s */
    CHECK(audio_plays_total() > p0 + 2, "walking plays footsteps (%llu cues)",
          (unsigned long long)(audio_plays_total() - p0));
    int j0 = g.player.stats.jumps; p0 = audio_plays_total();
    run(&g, BTN_JUMP, 0.f, 1); run(&g, 0, 0.f, 60);
    CHECK(g.player.stats.jumps > j0 && audio_plays_total() > p0, "jump/land cues fire");

    /* gunfire: shot cue, shake, flock scatter (arena loadout arms the player) */
    game_arena_start(&g, 1);
    g.enemies.count = 0;
    run(&g, 0, 0.f, 30);
    {                                                   /* park a flock overhead */
        g.pol.flock[0].home = v3(g.player.pos.x + 20.f, g.player.pos.y + 25.f, g.player.pos.z);
        g.pol.flock[0].off = v3(0, 0, 0);
    }
    int mag0 = g.wpn_mag[g.wpn_slot]; int sc0 = g.pol.scatters; p0 = audio_plays_total();
    run(&g, BTN_FIRE, 0.f, 20);
    CHECK(g.wpn_mag[g.wpn_slot] < mag0, "weapon fired");
    CHECK(audio_plays_total() > p0, "gunfire raises a shot cue");
    CHECK(g.pol.scatters > sc0, "gunfire scatters the nearby flock");
    CHECK(g.pol.shake > 0.f, "shot adds camera shake (%.3f)", g.pol.shake);
    settings()->photosensitivity = 1;
    run(&g, 0, 0.f, 2); run(&g, BTN_FIRE, 0.f, 20);
    CHECK(g.pol.shake == 0.f, "photosensitivity mode kills camera shake");
    settings()->photosensitivity = 0; settings()->camera_shake = 0;
    run(&g, 0, 0.f, 2); run(&g, BTN_FIRE, 0.f, 20);
    CHECK(g.pol.shake == 0.f, "camera_shake=off kills camera shake");
    settings()->camera_shake = 1;
    run(&g, 0, 0.f, 600);
    CHECK(g.pol.flock[0].scatter_t <= 0.f, "flock settles back after scatter");

    /* difficulty */
    settings()->difficulty = DIFF_EXPLORER;
    float lo = pol_diff_damage_pub();
    settings()->difficulty = DIFF_NORMAL;
    float mid = pol_diff_damage_pub();
    settings()->difficulty = DIFF_HARDCORE;
    float hi = pol_diff_damage_pub();
    settings()->difficulty = DIFF_NORMAL;
    CHECK(lo < mid && mid < hi && mid == 1.f, "difficulty scales damage %.2f / %.2f / %.2f", lo, mid, hi);

    /* adaptive music reacts to combat */
    game_arena_start(&g, 4);
    for (int i = 0; i < g.enemies.count; i++) g.enemies.v[i].state = EN_COMBAT,
        g.enemies.v[i].pos = v3(g.player.pos.x + 30.f + i, g.player.pos.y, g.player.pos.z);
    run(&g, 0, 0.f, 1);
    CHECK(g.pol.intensity > 0.4f, "combat raises music intensity target (%.2f)", g.pol.intensity);

    DH_INFO("smoke7", "──── %d checks, %d failed ────", g_checks, g_failed);
    game_free(&g);
    if (g_failed) { printf("M7 POLISH SMOKE FAILED: %d/%d\n", g_failed, g_checks); return 1; }
    printf("M7 POLISH SMOKE OK: %d checks\n", g_checks);
    return 0;
}
