/* tools/diag/course_shot.c — M1 course flythrough + stills (dev tool, not shipped).
   Builds against the real game layer, runs the course headlessly, then renders
   three stills to game/build/shots/.  Usage:  tools/build.sh shots          */
#include "game/game.h"
#include "plat/plat.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static Game g;

static void shoot(const char *name, Vec3 eye, Vec3 dir) {
    Player *pl = &g.player;
    pl->pos   = v3(eye.x, eye.y - pl->eye, eye.z);
    pl->yaw   = atan2f(dir.x, dir.z);
    pl->pitch = asinf(dir.y / v3_len(dir));
    pl->vel   = v3(0, 0, 0);
    game_render(&g);
    char path[192];
    snprintf(path, sizeof(path), "game/build/shots/%s.bmp", name); rend_save_bmp(path);
    snprintf(path, sizeof(path), "game/build/shots/%s.png", name); rend_save_png(path);
    printf("  shot %-18s draws=%d tris=%d\n", name, rend()->draw_calls, rend()->tris_drawn);
}

int main(void) {
    if (!game_init(&g, 960, 540, REND_SOFT, 0xD1CE5EEDu)) { printf("init failed\n"); return 1; }
    game_start_play(&g);
    PlatInput in;
    float sim = 0.0f, max_z = g.player.pos.z;
    double sim_us = 0.0, rend_us = 0.0;
    int crate_jumped = 0, gap_jumped = 0, max_tris = 0, max_draws = 0;
    for (int i = 0; i < 60 * 12; i++) {
        memset(&in, 0, sizeof(in));
        if (sim > 0.5f && sim < 8.0f) { in.my = 1.0f; in.buttons = BTN_SPRINT; }
        /* vault the on-line crate the frame we are blocked by it */
        if (!crate_jumped && g.player.blocked && g.player.pos.z < g.course_center.z + 10.0f) {
            in.pressed = BTN_JUMP; in.buttons |= BTN_JUMP; crate_jumped = 1;
        }
        /* jump the gap off the lip of the mantle deck */
        if (!gap_jumped && g.player.grounded &&
            g.player.pos.z > g.course_center.z + 32.0f &&
            g.player.pos.z < g.course_center.z + 38.0f) {
            in.pressed = BTN_JUMP; in.buttons |= BTN_JUMP; gap_jumped = 1;
        }
        uint64_t t0 = plat_now_us();
        game_frame(&g, &in, DH_TICK_DT);
        uint64_t t1 = plat_now_us();
        game_render(&g);
        uint64_t t2 = plat_now_us();
        sim_us += (double)(t1 - t0); rend_us += (double)(t2 - t1);
        if (g.player.pos.z > max_z) max_z = g.player.pos.z;
        if (rend()->tris_drawn > max_tris) max_tris = rend()->tris_drawn;
        if (rend()->draw_calls > max_draws) max_draws = rend()->draw_calls;
        sim += DH_TICK_DT;
    }
    Player *p = &g.player;
    printf("course flythrough (%.1f s, %d frames)\n", sim, (int)(sim / DH_TICK_DT));
    printf("  distance along course : %.1f m\n", max_z - g.spawn.z);
    printf("  stance/health         : %s / %.0f\n", player_stance_name(p->stance), p->health);
    printf("  jumps=%d vaults=%d mantles=%d slides=%d landings=%d total=%.1fm\n",
           p->stats.jumps, p->stats.vaults, p->stats.mantles, p->stats.slides,
           p->stats.landings, p->stats.distance_m);
    printf("  max_fall=%.2fm fall_dmg=%.0f last_jump_h=%.2fm\n",
           p->stats.max_fall_m, p->stats.fall_damage_taken, p->stats.last_jump_height);
    int n = (int)(sim / DH_TICK_DT);
    printf("  perf: max tris/frame=%d  max draws/frame=%d\n", max_tris, max_draws);
    printf("  cpu : sim %.2f ms/frame, software raster %.2f ms/frame (%.0f fps cpu-bound)\n",
           sim_us / 1000.0 / n, rend_us / 1000.0 / n, 1000.0 / (rend_us / 1000.0 / n));

    /* ── phase 2: ride the tower→pad zipline and capture a mid-ride still ── */
    Vec3 C = g.course_center;
    Player *pl = &g.player;
    pl->pos = v3(C.x - 14.0f, g.pad_h + 8.9f - 1.85f, C.z - 2.0f);
    pl->vel = v3(0, 0, 0); pl->stance = PL_ST_GROUND; pl->grounded = 1;
    memset(&in, 0, sizeof(in));
    in.pressed = BTN_USE;
    game_frame(&g, &in, DH_TICK_DT);
    printf("  dbg mount frame: stance=%s zip_rides=%d zip_s=%.2f pos=(%.1f,%.2f,%.1f)\n",
           player_stance_name(pl->stance), pl->stats.zip_rides, pl->zip_s,
           pl->pos.x, pl->pos.y, pl->pos.z);
    int rode = 0;
    for (int i = 0; i < 420 && pl->stance == PL_ST_ZIP; i++) {
        memset(&in, 0, sizeof(in));
        game_frame(&g, &in, DH_TICK_DT);
        game_render(&g);
        rode++;
        if (i == 110) {
            Vec3 eye = v3(pl->pos.x, pl->pos.y + pl->eye, pl->pos.z);
            Vec3 dir = v3_norm(v3_sub(g.zips.v[0].b, g.zips.v[0].a));
            shoot("m1_course_zip", eye, dir);
        }
    }
    printf("zipline ride: %d frames (%.1f m of cable), release stance %s\n",
           rode, pl->stats.zip_m, player_stance_name(pl->stance));

    shoot("m1_course_front", v3(C.x - 6.0f, g.pad_h + 5.5f, C.z + 14.0f), v3(0.22f, -0.10f, 0.97f));
    shoot("m1_course_side",  v3(C.x + 26.0f, g.pad_h + 4.5f, C.z + 20.0f), v3(-0.72f, -0.06f, 0.69f));
    shoot("m1_course_gap",   v3(C.x - 2.0f, g.pad_h + 3.0f, C.z + 30.0f), v3(0.05f, -0.05f, 1.0f));
    game_free(&g);
    return 0;
}
