/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — entry point
   One binary, two ways to run (Spec 88 "always playable"):

     DividedHorizon.exe                  → windowed, GL11 if available,
                                            otherwise the CPU rasterizer.
     DividedHorizon.exe --headless       → no window, scripted input, writes
                                            frame captures. This is the CI path
                                            and the proof-of-life on machines
                                            with no display or no GPU.

   The loop is a fixed-step simulation at DH_TICK_HZ with render-once-per-tick
   pacing. WHY fixed step: traversal tuning (coyote time, jump buffer, mantle
   arcs) must behave identically on a 30 FPS potato and a 144 Hz machine, and
   it makes headless verification deterministic.
   ══════════════════════════════════════════════════════════════════════════ */
#include "game/game.h"
#include "plat/plat.h"
#include "core/dh_log.h"
#include "core/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int   headless;
    int   menu_first;
    int   w, h;
    int   backend;             /* REND_SOFT / REND_GL11 */
    float duration;
    int   max_frames;
    uint32_t seed;
    const char *script;
    const char *shot_dir;
    int   no_pacing;           /* run flat out (benchmarks / CI) */
    int   verbose;
} LaunchOpts;

static void usage(void) {
    printf(
"DIVIDED HORIZON v" DH_VERSION_STRING " (" DH_BUILD_NAME ")\n"
"Usage: DividedHorizon [options]\n"
"  --headless          run without a window; captures frames to disk\n"
"  --script <file>     headless input script (key/shot/end lines)\n"
"  --shots <dir>       where headless captures are written (default ./shots)\n"
"  --dur <seconds>     headless run length (default 14)\n"
"  --frames <n>        stop after n frames\n"
"  --w <px> --h <px>   framebuffer size (default 1280x720; headless 640x360)\n"
"  --seed <n>          world seed (default 3502469805)\n"
"  --soft              force the CPU rasterizer even if GL11 is available\n"
"  --gl                force the GL11 backend\n"
"  --menu              start on the title screen instead of in play\n"
"  --fast              no frame pacing (benchmark)\n"
"  -v, --verbose       debug logging to stdout\n"
"  -h, --help          this text\n");
}

static int parse_args(int argc, char **argv, LaunchOpts *o) {
    memset(o, 0, sizeof(*o));
    o->w = 0; o->h = 0;
    o->duration = 0.0f;
    o->backend = -1;                 /* -1 = auto */
    o->seed = 0xD1CE5EEDu;
    o->shot_dir = "shots";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        #define NEXT() ((i + 1 < argc) ? argv[++i] : NULL)
        if (!strcmp(a, "--headless")) o->headless = 1;
        else if (!strcmp(a, "--menu")) o->menu_first = 1;
        else if (!strcmp(a, "--fast")) o->no_pacing = 1;
        else if (!strcmp(a, "-v") || !strcmp(a, "--verbose")) o->verbose = 1;
        else if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(); return 0; }
        else if (!strcmp(a, "--soft")) o->backend = REND_SOFT;
        else if (!strcmp(a, "--gl"))   o->backend = REND_GL11;
        else if (!strcmp(a, "--script")) { const char *v = NEXT(); if (!v) return 0; o->script = v; }
        else if (!strcmp(a, "--shots"))  { const char *v = NEXT(); if (!v) return 0; o->shot_dir = v; }
        else if (!strcmp(a, "--dur"))    { const char *v = NEXT(); if (!v) return 0; o->duration = (float)atof(v); }
        else if (!strcmp(a, "--frames")) { const char *v = NEXT(); if (!v) return 0; o->max_frames = atoi(v); }
        else if (!strcmp(a, "--seed"))   { const char *v = NEXT(); if (!v) return 0; o->seed = (uint32_t)strtoul(v, NULL, 0); }
        else if (!strcmp(a, "--w"))      { const char *v = NEXT(); if (!v) return 0; o->w = atoi(v); }
        else if (!strcmp(a, "--h"))      { const char *v = NEXT(); if (!v) return 0; o->h = atoi(v); }
        else { fprintf(stderr, "unknown option: %s\n", a); usage(); return 0; }
        #undef NEXT
    }
    return 1;
}

int main(int argc, char **argv) {
    LaunchOpts o;
    if (!parse_args(argc, argv, &o)) return 1;

    dh_fs_set_dirs(".", ".");
    dh_log_init("logs");
    dh_log_set_level(o.verbose ? DH_LOG_DEBUG : DH_LOG_INFO);
    DH_INFO("main", "DIVIDED HORIZON v%s (%s) starting", DH_VERSION_STRING, DH_BUILD_NAME);

    if (o.w <= 0) o.w = o.headless ? 640 : 1280;
    if (o.h <= 0) o.h = o.headless ? 360 : 720;

    /* ── platform ── */
    Plat *plat = NULL;
#ifdef _WIN32
    if (!o.headless) {
        plat = plat_win32_create(o.w, o.h, "DIVIDED HORIZON");
        if (o.backend < 0) o.backend = plat ? REND_GL11 : REND_SOFT;
        if (!plat) DH_WARN("main", "Win32 window failed — falling back to headless");
    }
#else
    if (!o.headless) {
        /* Honest degradation (Spec 19): the windowed backend is Win32-only, so
           on any other OS we run the identical simulation headless instead of
           pretending a window exists. */
        DH_WARN("main", "windowed mode is Win32-only in this build; running headless");
        o.headless = 1;
    }
#endif
    if (!plat) {
        dh_fs_mkdirs(o.shot_dir);
        plat = plat_headless_create(o.w, o.h, o.script, o.duration, o.shot_dir);
        if (o.backend < 0) o.backend = REND_SOFT;      /* no window → no GL */
    }
    if (!plat) {
        DH_ERROR("main", "platform creation failed — cannot continue");
        dh_log_shutdown();
        return 2;
    }

    /* ── game ── */
    Game game;
    if (!game_init(&game, plat->width, plat->height, o.backend, o.seed)) {
        DH_ERROR("main", "game_init failed");
        plat_destroy(plat);
        dh_log_shutdown();
        return 3;
    }
    if (!o.menu_first && o.headless) game_start_play(&game);
    if (o.menu_first) game.mode = GM_MENU;

    /* ── main loop ── */
    PlatInput in;
    uint64_t t_prev = plat_now_us();
    double acc_ms = 0.0, sim_ms = 0.0, draw_ms = 0.0;
    int frames = 0;
    const uint64_t frame_budget_us = (uint64_t)(1000000.0 / DH_TICK_HZ);

    while (plat->poll(plat, &in)) {
        uint64_t t0 = plat_now_us();

        game_frame(&game, &in, DH_TICK_DT);

        uint64_t t1 = plat_now_us();
        game_render(&game);
        uint64_t t2 = plat_now_us();

        RendState *rs = rend();
        if (rs) {
            rs->frame_index = frames;
            sim_ms   = (double)(t1 - t0) / 1000.0;
            draw_ms  = (double)(t2 - t1) / 1000.0;
            acc_ms   = acc_ms * 0.90 + (sim_ms + draw_ms) * 0.10;
            rs->cpu_ms   = acc_ms;
            rs->frame_ms = acc_ms;
            rs->fps = acc_ms > 0.001 ? (float)(1000.0 / acc_ms) : 0.0f;
            game.fps_smooth = rs->fps;
            game.sim_ms = (float)sim_ms;
            game.render_ms = (float)draw_ms;
        }

        plat->present(plat, rs ? rs->fb : NULL, plat->width, plat->height);
        frames++;

        if (game.quit) break;
        if (o.max_frames > 0 && frames >= o.max_frames) break;

        /* pacing: only when a human is watching. CI/benchmarks run flat out. */
        if (!o.no_pacing && !o.headless) {
            uint64_t used = plat_now_us() - t_prev;
            if (used < frame_budget_us) plat_sleep_ms((int)((frame_budget_us - used) / 1000));
        }
        t_prev = plat_now_us();
    }

    /* ── summary (also the CI assertion surface) ── */
    Player *p = &game.player;
    DH_INFO("main", "run complete: %d frames, %.1f s sim, avg %.2f ms/frame (%.1f fps)",
            frames, game.time, acc_ms, acc_ms > 0.001 ? 1000.0 / acc_ms : 0.0);
    DH_INFO("main", "traversal: jumps %d, mantles %d, vaults %d, landings %d, "
                    "distance %.1f m, max fall %.1f m, fall damage %.0f, swims %d",
            p->stats.jumps, p->stats.mantles, p->stats.vaults, p->stats.landings,
            p->stats.distance_m, p->stats.max_fall_m, p->stats.fall_damage_taken,
            p->stats.swim_time_s);
    DH_INFO("main", "final state: %s at (%.1f, %.1f, %.1f) health %.0f",
            player_stance_name(p->stance), p->pos.x, p->pos.y, p->pos.z, p->health);
    if (o.headless) DH_INFO("main", "captured %d frames to %s",
                            plat_headless_shots_taken(), o.shot_dir);
    {
        RendState *rs2 = rend();
        if (rs2) DH_INFO("main", "last frame: draw_calls %d, items %d, tris sub %d, "
                                 "culled frustum %d / dist %d, tex %d kb",
                         rs2->draw_calls, rs2->items, rs2->tris_submitted,
                         rs2->frustum_culled, rs2->dist_culled, rs2->texture_mem_kb);
        if (rs2) DH_INFO("main", "tris_drawn %d, overdraw_px %d", rs2->tris_drawn, rs2->overdraw_px);
    }

    /* The distance assertion only applies to the built-in demo script: a
       user-supplied script may legitimately stand still and look around. */
    int ok = (frames > 0) && (game.prop_count > 0) && (game.terrain.chunk_count > 0);
    if (!o.script) ok = ok && (p->stats.distance_m > 1.0f);
    DH_INFO("main", "%s", ok ? "SELF-CHECK PASSED" : "SELF-CHECK FAILED");

    game_free(&game);
    plat_destroy(plat);
    dh_log_shutdown();
    return ok ? 0 : 4;
}
