/* DIVIDED HORIZON — M12 smoke: Feats poster wall (Spec 53).
   Predicates stamp once, XP reward, one toast per tick, secret masking,
   clean-getaway tracker, poster UI pauses sim, render + save round-trip. */
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
    if (!(cond)) { g_failed++; DH_ERROR("smoke12", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke12", "ok: " __VA_ARGS__); } while (0)
#define DT (1.0f / 60.0f)
static void run(Game *g, uint32_t pressed, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof in);
        in.pressed = (i == 0) ? pressed : 0u;
        game_frame(g, &in, DT);
    }
}
static int has(Game *g, const char *name) {
    for (int i = 0; i < game_feat_count(); i++)
        if (!strcmp(game_feat_name(i), name)) return (g->feats.got >> i) & 1;
    return -1;
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out); dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke12", "════ M12 FEATS SMOKE ════");
    audio_init();
    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    if (!game_init(&g, 640, 360, REND_SOFT, 0x12F0u)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    run(&g, 0, 40);
    CHECK(game_feat_count() == 28, "28 feats defined");
    int secrets = 0; for (int i = 0; i < game_feat_count(); i++) secrets += game_feat_secret(i);
    CHECK(secrets == 2, "2 secret feats");
    CHECK(game_feats_stamped(&g) == 0, "fresh game: nothing stamped (got=%x)", g.feats.got);

    int xp0 = g.prog.xp, lv0 = g.prog.level;
    g.prog.ev_count[EV_OUTPOST_STEALTH] = 1; g.prog.ev_count[EV_MAST] = 1;
    run(&g, 0, 31);
    CHECK(game_feats_stamped(&g) == 1, "one stamp per tick (toasts don't stack)");
    CHECK(g.feats.stamp_t > 0.f, "stamp toast showing");
    CHECK(g.prog.xp != xp0 || g.prog.level != lv0, "stamp grants XP");
    run(&g, 0, 31);
    CHECK(has(&g, "GHOST") == 1 && has(&g, "SIGNAL BOOST") == 1, "GHOST + SIGNAL BOOST stamped");
    int n = game_feats_stamped(&g);
    CHECK(!game_feat_unlock(&g, 4), "re-unlock is a no-op");
    run(&g, 0, 120);
    CHECK(game_feats_stamped(&g) == n, "no duplicate stamps");

    game_render(&g);
    CHECK(rend()->draw_calls > 0 && rend()->draw_calls < 900, "toast renders");

    /* poster UI */
    run(&g, BTN_FEATS, 1);
    CHECK(g.ui == UI_FEATS, "J opens poster wall");
    run(&g, BTN_RIGHT, 1); run(&g, BTN_DOWN, 1);
    CHECK(g.ui_sel == 5, "grid navigation (%d)", g.ui_sel);
    float t0 = g.time; run(&g, 0, 30);
    CHECK(g.time >= t0, "sim paused on the wall");
    game_render(&g);
    CHECK(rend()->draw_calls > 0 && rend()->draw_calls < 900, "wall renders (%d draws)", rend()->draw_calls);
    { char p[256]; snprintf(p, sizeof p, "%s/feats_wall.png", out); rend_save_png(p); }
    run(&g, BTN_FEATS, 1);
    CHECK(g.ui == UI_NONE, "J closes");

    /* secret */
    g.dog.distracts = 5; run(&g, 0, 31);
    CHECK(has(&g, "THE DOG JUDGES YOU") == 1, "secret feat stamps");

    /* save / load */
    uint32_t got = g.feats.got;
    CHECK(game_save(&g, 3, 0), "save");
    g.feats.got = 0;
    CHECK(game_load(&g, 3, 0), "load");
    CHECK(g.feats.got == got, "stamps persist (%x)", g.feats.got);

    /* city: crossing + clean getaway */
    game_load_city(&g);
    run(&g, 0, 62);
    CHECK(has(&g, "CROSSING") == 1, "CROSSING stamped in Meridian");
    g.city.heat.stars = 3; run(&g, 0, 2);
    CHECK(g.feats.peak_stars >= 3, "peak stars tracked");
    g.city.heat.stars = 0; g.city.heat.evidence = 0; run(&g, 0, 62);
    CHECK(has(&g, "CLEAN GETAWAY") == 1, "CLEAN GETAWAY after losing 3 stars");
    CHECK(has(&g, "MOST WANTED") == 0, "MOST WANTED not given for 3 stars");

    DH_INFO("smoke12", "════ %d/%d checks passed ════", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
