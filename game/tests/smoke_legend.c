/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M11 smoke: Stature (hidden slider) + the Herald.
   First edition on arrival, deeds move stature and print tone-matched front
   pages, the five reactions (price, heat escape, barks, poll, epilogue),
   scrapbook UI, save/load round-trip, rendering the page, draw budget.
   ══════════════════════════════════════════════════════════════════════════ */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke11", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke11", "ok: " __VA_ARGS__); } while (0)
#define DT (1.0f / 60.0f)

static void run(Game *g, uint32_t pressed, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof in);
        in.pressed = (i == 0) ? pressed : 0u;
        game_frame(g, &in, DT);
    }
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out);
    dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke11", "════ M11 LEGEND SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x11E6u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 0, 3);
    char h[96];
    CHECK(g.leg.n == 1 && g.leg.pages[0].tmpl == 0, "first edition printed on arrival");
    CHECK(game_herald_headline(&g, 0, h, sizeof h) > 5, "headline: %s", h);
    CHECK(game_stature_tier(&g) == 0 && strcmp(game_stature_name(&g), "UNKNOWN QUANTITY") == 0, "starts neutral");
    int base = 100;
    CHECK(prog_price(&g.prog, base) == prog_price(&g.prog, base), "price stable");
    int p_neutral = prog_price(&g.prog, 100);

    /* stealth capture: hero-ward deed + quiet front page */
    int n0 = g.leg.n;
    g.prog.ev_count[EV_OUTPOST_STEALTH]++;
    run(&g, 0, 1);
    CHECK(g.leg.n == n0 + 1 && g.leg.pages[g.leg.n - 1].tmpl == 1, "stealth capture prints a front page");
    CHECK(g.leg.stature > 5.f, "stealth capture raises stature (%.1f)", g.leg.stature);
    CHECK(g.leg.banner_t > 0.f, "EXTRA! banner shows");
    /* kill streak: renegade-ward */
    float s0 = g.leg.stature;
    g.prog.ev_count[EV_KILL] += 20; g.kills += 20;
    run(&g, 0, 1);
    CHECK(g.leg.stature < s0, "killing lowers stature (%.1f -> %.1f)", s0, g.leg.stature);
    game_herald_headline(&g, g.leg.n - 1, h, sizeof h);
    CHECK(g.leg.pages[g.leg.n - 1].tmpl == 8 && strstr(h, "10") != NULL, "body-count milestone headline: %s", h);
    CHECK(g.leg.stature >= -100.f && g.leg.stature <= 100.f, "stature clamped");

    /* tiers + reactions */
    game_stature_add(&g, 200.f); run(&g, 0, 1);
    CHECK(game_stature_tier(&g) == 2 && g.leg.stature == 100.f, "clamp at +100 = FOLK HERO");
    int p_hero = prog_price(&g.prog, 100);
    CHECK(p_hero < p_neutral, "reaction 1: hero discount ($%d vs $%d)", p_hero, p_neutral);
    g.prog.ev_count[EV_MAST]++; run(&g, 0, 1);
    CHECK(g.leg.pages[g.leg.n - 1].tone == 2, "reaction 4: hero-tone headline");
    game_stature_add(&g, -300.f); run(&g, 0, 1);
    CHECK(game_stature_tier(&g) == -2, "clamp at -100 = PUBLIC MENACE");
    int p_ren = prog_price(&g.prog, 100);
    CHECK(p_ren > p_neutral, "reaction 1: menace markup ($%d)", p_ren);
    g.prog.ev_count[EV_ALARM]++; run(&g, 0, 1);
    CHECK(g.leg.pages[g.leg.n - 1].tone == 0, "renegade-tone headline");
    char hh[96], hr[96];
    game_herald_headline(&g, g.leg.n - 2, hh, sizeof hh);
    game_herald_headline(&g, g.leg.n - 1, hr, sizeof hr);
    CHECK(strcmp(hh, hr) != 0, "tone changes the words");

    /* ending: epilogue front page written by stature */
    g.ms.ending = 2; run(&g, 0, 1);
    game_herald_headline(&g, g.leg.n - 1, h, sizeof h);
    CHECK(g.leg.pages[g.leg.n - 1].tmpl == 12 && g.leg.pages[g.leg.n - 1].tone == 0, "reaction 5: menace epilogue: %s", h);
    g.ms.ending = 0;

    /* scrapbook UI */
    run(&g, BTN_HERALD, 1);
    CHECK(g.ui == UI_HERALD && g.leg.view == g.leg.n - 1, "N opens the scrapbook at the newest page");
    float t0 = g.time; int k0 = g.kills;
    run(&g, BTN_LEFT, 1);
    CHECK(g.leg.view == g.leg.n - 2, "LEFT turns back a page");
    run(&g, 0, 30);
    CHECK(g.kills == k0 && g.time >= t0, "sim paused while reading");
    game_render(&g);
    CHECK(rend()->draw_calls > 0 && rend()->draw_calls < 900, "front page renders (%d draws)", rend()->draw_calls);
    { char pth[256]; snprintf(pth, sizeof pth, "%s/herald_page.png", out); rend_save_png(pth); }
    run(&g, BTN_HERALD, 1);
    CHECK(g.ui == UI_NONE, "N closes");

    /* overflow cap */
    for (int i = 0; i < 40; i++) game_herald_print(&g, 3, 0);
    CHECK(g.leg.n == HERALD_MAX, "scrapbook caps at %d pages", HERALD_MAX);

    /* save / load */
    game_stature_add(&g, 130.f);   /* -100 -> +30 */
    run(&g, 0, 1);
    float st = g.leg.stature; int n = g.leg.n;
    char first[96]; game_herald_headline(&g, 5, first, sizeof first);
    CHECK(game_save(&g, 3, 0), "save");
    g.leg.stature = 0.f; g.leg.n = 0;
    CHECK(game_load(&g, 3, 0), "load");
    char again[96]; game_herald_headline(&g, 5, again, sizeof again);
    CHECK(g.leg.n == n && (int)(g.leg.stature * 10) == (int)(st * 10), "stature + %d pages persist", n);
    CHECK(strcmp(first, again) == 0, "page text identical after load");
    int n1 = g.leg.n;
    run(&g, 0, 5);
    CHECK(g.leg.n == n1, "load does not re-print old deeds");

    /* city: heat escape bonus + street barks */
    game_load_city(&g);
    run(&g, 0, 2);
    CHECK(g.leg.pages[g.leg.n - 1].tmpl == 11, "ferry arrival edition");
    game_stature_add(&g, 100.f); run(&g, 0, 1);
    g.city.heat.searching = 1; float e0 = g.city.heat.escape_t;
    run(&g, 0, 1);
    CHECK(g.city.heat.escape_t >= e0 || g.city.heat.just_dropped || !g.city.heat.searching, "reaction 2 path runs");
    {   /* isolate the bonus: stars>0 & searching; compare hero vs neutral */
        Heat hsave = g.city.heat;
        g.city.heat.stars = 2; g.city.heat.evidence = 3.f; g.city.heat.searching = 1; g.city.heat.escape_t = 0.f;
        g.leg.bark_cd = 99.f;
        run(&g, 0, 1); float hero_t = g.city.heat.escape_t;
        g.city.heat.escape_t = 0.f; g.leg.stature = 0.f; g.city.heat.searching = 1;
        run(&g, 0, 1); float neut_t = g.city.heat.escape_t;
        CHECK(hero_t > neut_t, "reaction 2: heroes shake cops faster (%.4f vs %.4f)", hero_t, neut_t);
        g.city.heat = hsave;
    }
    g.leg.stature = 100.f; g.leg.bark_cd = 0.f; g.message_t = 0.f; g.city.heat.stars = 0;
    int pi = -1;
    for (int i = 0; i < g.city.ped_count; i++) if (g.city.peds[i].state == 0) { pi = i; break; }
    if (pi >= 0) { g.player.pos = g.city.peds[pi].pos; g.player.pos.x += 1.f; }
    run(&g, 0, 1);
    CHECK(pi >= 0 && g.leg.bark_cd > 0.f && g.message[0] == '"', "reaction 3: street bark: %s", g.message);

    DH_INFO("smoke11", "──── %d checks, %d failed ────", g_checks, g_failed);
    printf("M11 LEGEND SMOKE %s: %d/%d checks\n", g_failed ? "FAILED" : "PASSED", g_checks - g_failed, g_checks);
    game_free(&g);
    return g_failed ? 1 : 0;
}
