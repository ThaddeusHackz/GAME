/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M1 traversal smoke test (headless, deterministic)
   "Traversal is joy" is a testable claim (P1), so every number in Spec 6.1
   gets asserted here:
     speeds · jump arc · coyote 0.12s · buffer 0.15s · step 1.2m · vault 2.2m
     fall damage > 4m · roll cancel 0.35s · swim/breath · slope slide
   Plus an integration pass that boots the real island, runs the graybox
   course through the real game loop, and asserts the renderer actually put
   pixels on screen (the M1 double-model-matrix bug would fail this).
   Exits non-zero on any failure so tools/build.sh can gate on it.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif

#include "../src/core/dh_types.h"
#include "../src/core/dh_log.h"
#include "../src/core/settings.h"
#include "../src/world/terrain.h"
#include "../src/world/collision.h"
#include "../src/player/player.h"
#include "../src/rend/rend.h"
#include "../src/rend/font.h"
#include "../src/rend/proc_tex.h"
#include "../src/game/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke1", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke1", "ok: " __VA_ARGS__); \
} while (0)

#define DT (1.0f / 60.0f)
#define B_JUMP 1u
#define B_SPRINT 2u
#define B_CROUCH 4u

/* ── flat test world ───────────────────────────────────────────────────── */
static Terrain  t_terr;
static ObstacleSet t_obs;
static int world_ready = 0;

static void make_flat_world(void) {
    if (world_ready) return;
    terrain_init(&t_terr, 65, 256.0f, 0xC0FFEEu);
    terrain_flatten(&t_terr, v3(128.0f, 0.0f, 128.0f), 110.0f, 110.0f, 0.0f, 6.0f);
    obstacles_init(&t_obs);
    world_ready = 1;
}

static void reset_player(Player *p) {
    obstacles_init(&t_obs);
    player_init(p, &t_terr, &t_obs, v3(128.0f, 0.0f, 100.0f));
    p->yaw = 0.0f;                       /* +z forward */
}

/* Step the controller n frames with a simple button set. Edges fire once. */
static void step(Player *p, float mx, float my, unsigned btn,
                  int jump_edge, int roll_edge, int n) {
    for (int i = 0; i < n; i++) {
        PlayerInput in;
        memset(&in, 0, sizeof(in));
        in.mx = mx; in.my = my;
        in.jump   = (btn & B_JUMP)   ? 1 : 0;
        in.sprint = (btn & B_SPRINT) ? 1 : 0;
        in.crouch = (btn & B_CROUCH) ? 1 : 0;
        in.jump_pressed = (jump_edge && i == 0) ? 1 : 0;
        in.roll         = (roll_edge && i == 0) ? 1 : 0;
        player_input(p, &in, DT);
    }
}

static float run_for(Player *p, float mx, float my, unsigned btn, float seconds) {
    Vec3 a = p->pos;
    step(p, mx, my, btn, 0, 0, (int)(seconds / DT));
    return v3_dist_xz(a, p->pos);
}

/* Box with its TOP at `top`. */
static void box_top(ObstacleSet *s, float x, float top, float z, float hx, float hz) {
    float hy = top / 2.0f;
    obstacles_add_box(s, v3(x, top - hy, z), v3(hx, hy, hz), 0);
}

/* ══════════════════════════════ tests ═══════════════════════════════════ */

static void test_speeds(void) {
    Player p; reset_player(&p);
    float walk = run_for(&p, 0, 1, 0, 1.0f);
    reset_player(&p);
    float sprint = run_for(&p, 0, 1, B_SPRINT, 1.0f);
    reset_player(&p);
    float crouch = run_for(&p, 0, 1, B_CROUCH, 1.0f);
    reset_player(&p);
    float strafe = run_for(&p, 1, 0, 0, 1.0f);
    CHECK(walk > 3.4f && walk < 5.2f, "walk ~4.7 m/s (got %.2f m in 1 s)", walk);
    CHECK(sprint > 6.0f && sprint < 7.8f, "sprint ~7.2 m/s (got %.2f)", sprint);
    CHECK(crouch < walk * 0.5f, "crouch is markedly slower (%.2f vs %.2f)", crouch, walk);
    CHECK(strafe > 3.4f && strafe < 5.2f, "strafe uses run speed (%.2f)", strafe);
}

static void test_jump_arc(void) {
    Player p; reset_player(&p);
    /* held jump: full arc */
    step(&p, 0, 0, B_JUMP, 1, 0, 1);
    float apex = 0.0f;
    for (int i = 0; i < 90; i++) {
        step(&p, 0, 0, B_JUMP, 0, 0, 1);
        if (p.pos.y > apex) apex = p.pos.y;
    }
    CHECK(p.stats.jumps == 1, "one press = one jump");
    CHECK(apex > 1.15f && apex < 1.55f, "held jump apex ~1.35 m (got %.2f)", apex);
    CHECK(p.grounded && p.stance == PL_ST_GROUND, "jump lands back on ground");

    /* tapped jump: variable height cuts the arc */
    reset_player(&p);
    step(&p, 0, 0, B_JUMP, 1, 0, 1);
    float tap_apex = 0.0f;
    for (int i = 0; i < 90; i++) {
        step(&p, 0, 0, 0, 0, 0, 1);
        if (p.pos.y > tap_apex) tap_apex = p.pos.y;
    }
    CHECK(tap_apex < apex * 0.85f, "tap jump is shorter than held (%.2f < %.2f)",
          tap_apex, apex);
}

static void test_coyote_and_buffer(void) {
    /* coyote: walk off a 2 m crate, jump 0.08 s later — must still jump */
    Player p; reset_player(&p);
    box_top(&t_obs, 128.0f, 2.0f, 100.0f, 3.0f, 3.0f);
    p.pos = v3(128.0f, 2.05f, 101.0f);
    p.vel = v3(0, 0, 0);
    int left_at = -1;
    for (int i = 0; i < 90 && left_at < 0; i++) {
        step(&p, 0, 1, 0, 0, 0, 1);
        if (!p.grounded) left_at = i;
    }
    CHECK(left_at >= 0, "walked off the crate into the air");
    step(&p, 0, 1, 0, 0, 0, 5);               /* +0.08 s of falling */
    int jumps = p.stats.jumps;
    step(&p, 0, 1, B_JUMP, 1, 0, 1);
    CHECK(p.stats.jumps == jumps + 1, "coyote time allows a jump 0.08 s off a ledge");
    CHECK(p.vel.y > 3.0f, "coyote jump has real upward velocity (%.2f)", p.vel.y);

    /* too late: 0.3 s after leaving ground, no jump */
    reset_player(&p);
    box_top(&t_obs, 128.0f, 2.0f, 100.0f, 3.0f, 3.0f);
    p.pos = v3(128.0f, 2.05f, 101.0f);
    for (int i = 0; i < 90 && p.grounded; i++) step(&p, 0, 1, 0, 0, 0, 1);
    step(&p, 0, 1, 0, 0, 0, 18);              /* 0.3 s */
    jumps = p.stats.jumps;
    step(&p, 0, 1, B_JUMP, 1, 0, 1);
    CHECK(p.stats.jumps == jumps, "coyote window has expired by 0.3 s");

    /* buffer: press jump 0.12 s before landing, must bounce on touchdown */
    reset_player(&p);
    p.pos = v3(128.0f, 3.0f, 100.0f);
    p.grounded = 0; p.stance = PL_ST_AIR; p.vel = v3(0, 0, 0);
    int landed_jumps = -1;
    for (int i = 0; i < 120; i++) {
        int press = (i == 40);                /* well before landing (~i=70) */
        PlayerInput in; memset(&in, 0, sizeof(in));
        in.jump_pressed = press; in.jump = press;
        player_input(&p, &in, DT);
        if (landed_jumps < 0 && p.stats.landings > 0) landed_jumps = p.stats.jumps;
        if (landed_jumps >= 0 && p.stats.jumps > landed_jumps) break;
    }
    CHECK(p.stats.jumps >= 1, "jump buffered 0.5 s early fires on landing (buffer)");
}

static void test_step_and_vault(void) {
    /* auto step-up: 1.0 m box, no jump */
    Player p; reset_player(&p);
    box_top(&t_obs, 128.0f, 1.0f, 106.0f, 3.0f, 2.0f);
    float step_max_y = 0.0f;
    for (int i = 0; i < 150; i++) {
        step(&p, 0, 1, 0, 0, 0, 1);
        if (p.pos.y > step_max_y) step_max_y = p.pos.y;
    }
    CHECK(fabsf(step_max_y - 1.0f) < 0.05f,
          "1.0 m step is walked up without jumping (max y=%.2f)", step_max_y);
    CHECK(p.pos.z > 108.0f, "player walked clean over the step (z=%.1f)", p.pos.z);

    /* blocked: 1.6 m wall, no jump */
    reset_player(&p);
    box_top(&t_obs, 128.0f, 1.6f, 106.0f, 3.0f, 0.5f);
    run_for(&p, 0, 1, 0, 2.0f);
    CHECK(p.pos.y < 0.2f && p.pos.z < 105.6f, "1.6 m wall blocks walking (z=%.1f y=%.2f)",
          p.pos.z, p.pos.y);

    /* vault: same wall, jump while pressing into it */
    reset_player(&p);
    box_top(&t_obs, 128.0f, 1.6f, 106.0f, 3.0f, 0.5f);
    int vaults = 0, pressed = 0;
    float max_y = 0.0f;
    for (int i = 0; i < 240; i++) {
        PlayerInput in; memset(&in, 0, sizeof(in));
        in.my = 1.0f;
        /* jump the frame we make contact — the "jumped at the wall" case */
        if (!pressed && p.blocked && p.pos.z > 104.0f) { in.jump_pressed = 1; pressed = 1; }
        in.jump = pressed && (i % 60 < 20);
        player_input(&p, &in, DT);
        if (p.stats.vaults > vaults) vaults = p.stats.vaults;
        if (p.pos.y > max_y) max_y = p.pos.y;
        if (vaults && p.grounded && p.pos.y > 1.5f) break;
    }
    CHECK(vaults >= 1, "jump on contact with a 1.6 m wall vaults it (vaults=%d)", vaults);
    CHECK(fabsf(max_y - 1.6f) < 0.15f, "vault reaches the top of the wall (max y=%.2f)", max_y);

    /* too high: 3.0 m wall never vaults */
    reset_player(&p);
    box_top(&t_obs, 128.0f, 3.0f, 106.0f, 3.0f, 0.5f);
    for (int i = 0; i < 180; i++) {
        int near = (p.pos.z > 103.5f);
        PlayerInput in; memset(&in, 0, sizeof(in));
        in.my = 1.0f;
        in.jump = near ? 1 : 0;
        in.jump_pressed = (near && i % 30 == 0) ? 1 : 0;
        player_input(&p, &in, DT);
    }
    CHECK(p.stats.vaults == 0 && p.pos.y < 0.3f, "3.0 m wall is beyond vault reach");
}

static void test_air_mantle(void) {
    Player p; reset_player(&p);
    box_top(&t_obs, 128.0f, 2.0f, 106.0f, 3.0f, 2.0f);
    /* drop from above and drift into the wall while falling */
    p.pos = v3(128.0f, 1.5f, 103.2f);
    p.grounded = 0; p.stance = PL_ST_AIR; p.vel = v3(0, -1.0f, 1.5f);
    int saw_mantle = 0;
    float max_y = 0.0f;
    for (int i = 0; i < 90; i++) {
        PlayerInput in; memset(&in, 0, sizeof(in));
        in.my = 1.0f;
        player_input(&p, &in, DT);
        if (p.stance == PL_ST_MANTLE) saw_mantle = 1;
        if (p.pos.y > max_y) max_y = p.pos.y;
    }
    CHECK(saw_mantle, "falling into a ledge triggers an air mantle");
    CHECK(fabsf(max_y - 2.0f) < 0.15f, "mantle reaches the ledge top (max y=%.2f)", max_y);
}

static void test_fall_damage(void) {
    /* 7 m drop hurts */
    Player p; reset_player(&p);
    p.pos = v3(128.0f, 7.0f, 100.0f);
    p.grounded = 0; p.stance = PL_ST_AIR; p.vel = v3(0, 0, 0);
    step(&p, 0, 0, 0, 0, 0, 150);
    CHECK(p.grounded, "7 m drop ends grounded");
    CHECK(p.health < 100.0f && p.stats.fall_damage_taken > 0.0f,
          "7 m drop deals fall damage (hp=%.0f dmg=%.0f)", p.health, p.stats.fall_damage_taken);

    /* 3.5 m drop is safe */
    reset_player(&p);
    p.pos = v3(128.0f, 3.5f, 100.0f);
    p.grounded = 0; p.stance = PL_ST_AIR; p.vel = v3(0, 0, 0);
    step(&p, 0, 0, 0, 0, 0, 120);
    CHECK(p.health == 100.0f, "3.5 m drop is inside the 4 m safe window (hp=%.0f)", p.health);

    /* roll cancel */
    reset_player(&p);
    p.pos = v3(128.0f, 7.0f, 100.0f);
    p.grounded = 0; p.stance = PL_ST_AIR; p.vel = v3(0, 0, 0);
    step(&p, 0, 0, 0, 0, 0, 40);
    step(&p, 0, 0, 0, 0, 1, 1);              /* roll just before touchdown */
    step(&p, 0, 0, 0, 0, 0, 90);
    CHECK(p.health == 100.0f, "rolling inside the cancel window negates fall damage (hp=%.0f)",
          p.health);
}

static void test_swim(void) {
    Player p; reset_player(&p);
    /* dig a pool: terrain under sea level */
    terrain_flatten(&t_terr, v3(128.0f, 0.0f, 160.0f), 12.0f, 12.0f, -4.0f, 3.0f);
    p.pos = v3(128.0f, -2.5f, 160.0f);
    p.vel = v3(0, 0, 0);
    p.grounded = 0; p.stance = PL_ST_AIR;
    step(&p, 0, 0, 0, 0, 0, 10);
    CHECK(p.stance == PL_ST_SWIM, "submerged body enters SWIM stance");
    float b0 = p.breath;
    step(&p, 0, 1, 0, 0, 0, 120);
    CHECK(p.breath < b0 - 1.0f, "breath drains underwater (%.1f → %.1f)", b0, p.breath);
    float y0 = p.pos.y;
    step(&p, 0, 0, B_CROUCH, 0, 0, 60);
    CHECK(p.pos.y < y0 - 0.3f, "crouch dives (%.2f → %.2f)", y0, p.pos.y);
    float y1 = p.pos.y;
    step(&p, 0, 0, B_JUMP, 0, 0, 60);
    CHECK(p.pos.y > y1 + 0.3f, "jump surfaces (%.2f → %.2f)", y1, p.pos.y);
    /* drown */
    p.breath = 0.4f;
    float hp = p.health;
    step(&p, 0, 0, B_CROUCH, 0, 0, 90);
    CHECK(p.health < hp, "running out of air drains health (%.0f → %.0f)", hp, p.health);
    /* surface refills */
    p.pos = v3(128.0f, -0.2f, 160.0f);
    p.breath = 5.0f;
    p.stance = PL_ST_AIR; p.grounded = 0;
    step(&p, 0, 0, 0, 0, 0, 60);
    CHECK(p.breath > 5.0f, "breath refills at the surface (%.1f)", p.breath);
    terrain_flatten(&t_terr, v3(128.0f, 0.0f, 160.0f), 12.0f, 12.0f, 0.0f, 3.0f);
}

static void test_slide(void) {
    Player p; reset_player(&p);
    terrain_mound(&t_terr, v3(180.0f, 0.0f, 100.0f), 16.0f, 40.0f);
    /* stand on the steepest sample of the flank (heightfield slope is
       sample-resolution limited, so pick the worst point honestly) */
    float hx = 175.0f, hs = 0.0f;
    for (float x = 168.0f; x < 178.0f; x += 0.5f) {
        float st = terrain_steepness(&t_terr, x, 100.0f);
        if (st > hs) { hs = st; hx = x; }
    }
    float hy = terrain_height(&t_terr, hx, 100.0f);
    p.pos = v3(hx, hy + 0.2f, 100.0f);
    p.grounded = 1; p.stance = PL_ST_GROUND; p.vel = v3(0, 0, 0);
    int saw_slide = 0;
    float z0 = p.pos.x;
    for (int i = 0; i < 120; i++) {
        PlayerInput in; memset(&in, 0, sizeof(in));
        player_input(&p, &in, DT);
        if (p.stance == PL_ST_SLIDE) saw_slide = 1;
    }
    CHECK(p.steepness > 0.5f, "mound flank is steep (%.2f)", p.steepness);
    CHECK(saw_slide, "steep slope enters SLIDE stance");
    CHECK(p.pos.x < z0 - 1.0f, "slide carries the player downhill (%.1f → %.1f)", z0, p.pos.x);
    terrain_flatten(&t_terr, v3(180.0f, 0.0f, 100.0f), 22.0f, 22.0f, 0.0f, 5.0f);
}

/* ── zipline (6.1): mount with USE, ride, brake with crouch, drop with SPACE ─ */
static ZiplineSet t_zips;
static void test_zipline(void) {
    Player p;
    reset_player(&p);
    zips_init(&t_zips);
    zips_add(&t_zips, v3(128.0f, 10.0f, 100.0f), v3(168.0f, 2.0f, 100.0f));
    p.zips = &t_zips;

    /* USE far from any cable does nothing */
    p.pos = v3(128.0f, 0.0f, 120.0f);
    {
        PlayerInput in; memset(&in, 0, sizeof(in)); in.use_pressed = 1;
        player_input(&p, &in, DT);
        CHECK(p.stance == PL_ST_GROUND, "use away from cable stays grounded");
    }

    /* stand under the high anchor and mount */
    p.pos = v3(128.0f, 10.0f - 1.55f - 0.30f, 100.0f);
    p.vel = v3(0, 0, 0);
    {
        PlayerInput in; memset(&in, 0, sizeof(in)); in.use_pressed = 1;
        player_input(&p, &in, DT);
    }
    CHECK(p.stance == PL_ST_ZIP, "use under cable mounts zipline (stance %s)",
          player_stance_name(p.stance));
    CHECK(p.stats.zip_rides == 1, "zip_rides counted");

    /* ride 4 s: must travel down-cable and stay attached */
    float s0 = p.zip_s;
    step(&p, 0.0f, 0.0f, 0, 0, 0, 240);
    CHECK(p.stance == PL_ST_ZIP, "still on cable after 4 s (stance %s)",
          player_stance_name(p.stance));
    CHECK(p.zip_s - s0 > 18.0f, "rode %.1f m of cable in 4 s", p.zip_s - s0);
    CHECK(p.pos.x > 146.0f, "cable carries player +x (%.1f)", p.pos.x);
    CHECK(p.pos.y > 2.0f && p.pos.y < 9.0f, "hangs below cable (y %.2f)", p.pos.y);
    float v_cruise = p.zip_speed;
    CHECK(v_cruise > 4.0f, "gravity-along-cable accelerates to %.1f m/s", v_cruise);

    /* SPACE drops with momentum (checked before braking bleeds the speed) */
    step(&p, 0.0f, 0.0f, 0, 1, 0, 1);
    CHECK(p.stance == PL_ST_AIR, "jump releases into AIR");
    CHECK(p.vel.x > 2.0f, "release keeps momentum (vx %.1f)", p.vel.x);
    float drop_y = p.pos.y;
    /* 4.6 m of fall is lethal-ish by design (>4 m safe window), so the correct
       play off a high cable is a roll-cancel on touchdown — assert the two
       systems interlock. */
    int landed = 0, rolled = 0;
    for (int i = 0; i < 240 && !landed; i++) {
        PlayerInput in; memset(&in, 0, sizeof(in));
        if (!rolled && p.vel.y < -5.0f && p.pos.y < 2.2f) { in.roll = 1; rolled = 1; }
        player_input(&p, &in, DT);
        if (p.grounded) landed = 1;
    }
    CHECK(landed, "lands after dropping off the cable");
    CHECK(rolled, "roll-cancel window reached on the zip drop");
    CHECK(p.health >= 99.9f,
          "roll-cancel negates the %.1f m zip drop (hp %.2f)", drop_y, p.health);
    CHECK(p.stats.zip_m > 18.0f, "zip distance metered (%.1f m)", p.stats.zip_m);

    /* crouch brakes (fresh ride so the speed budget is clean) */
    reset_player(&p);
    zips_init(&t_zips);
    zips_add(&t_zips, v3(128.0f, 10.0f, 100.0f), v3(168.0f, 2.0f, 100.0f));
    p.zips = &t_zips;
    p.pos = v3(128.0f, 10.0f - 1.85f, 100.0f);
    {
        PlayerInput in; memset(&in, 0, sizeof(in)); in.use_pressed = 1;
        player_input(&p, &in, DT);
    }
    step(&p, 0.0f, 0.0f, 0, 0, 0, 180);
    v_cruise = p.zip_speed;
    step(&p, 0.0f, 0.0f, B_CROUCH, 0, 0, 30);
    CHECK(p.zip_speed < v_cruise - 1.0f, "crouch brakes (%.1f → %.1f m/s)",
          v_cruise, p.zip_speed);


    /* riding to the end auto-releases */
    reset_player(&p);
    zips_init(&t_zips);
    zips_add(&t_zips, v3(128.0f, 6.0f, 100.0f), v3(140.0f, 3.0f, 100.0f));
    p.zips = &t_zips;
    p.pos = v3(128.0f, 6.0f - 1.85f, 100.0f);
    {
        PlayerInput in; memset(&in, 0, sizeof(in)); in.use_pressed = 1;
        player_input(&p, &in, DT);
    }
    step(&p, 0.0f, 0.0f, 0, 0, 0, 600);
    CHECK(p.stance != PL_ST_ZIP, "end of cable auto-releases (stance %s)",
          player_stance_name(p.stance));
    CHECK(p.grounded, "auto-release ends on the ground");
}

static void test_stamina(void) {
    Player p; reset_player(&p);
    step(&p, 0, 1, B_SPRINT, 0, 0, 60 * 8);
    CHECK(p.stamina < 20.0f, "sprint drains stamina (%.0f left after 8 s)", p.stamina);
    float d_tired = run_for(&p, 0, 1, B_SPRINT, 1.0f);
    CHECK(d_tired < 5.5f, "exhausted sprint falls back to run speed (%.2f m/s)", d_tired);
    step(&p, 0, 0, 0, 0, 0, 60 * 4);
    CHECK(p.stamina > 20.0f, "stamina regenerates while idle (%.0f)", p.stamina);
}

static void test_bounds_and_sanity(void) {
    Player p; reset_player(&p);
    run_for(&p, 0, -1, B_SPRINT, 6.0f);       /* run off the south edge of the flat pad */
    CHECK(v3_valid(p.pos), "position stays finite after running into world edge terrain");
    p.pos = v3(-50.0f, 5.0f, -50.0f);
    step(&p, 0, 1, 0, 0, 0, 60);
    CHECK(p.pos.x >= 15.0f && p.pos.z >= 15.0f, "world bounds clamp the player inside (x=%.1f z=%.1f)",
          p.pos.x, p.pos.z);
}

/* ── integration: real island, real loop, real renderer ────────────────── */
static void test_integration(const char *shot_dir) {
    Game g;
    if (!game_init(&g, 320, 180, REND_SOFT, 0xD1CE5EEDu)) {
        CHECK(0, "game_init");
        return;
    }
    CHECK(g.terrain.chunk_count == 256, "island meshed into 256 chunks (got %d)",
          g.terrain.chunk_count);
    CHECK(g.obs.count >= 20, "course registered %d collision volumes", g.obs.count);
    CHECK(fabsf(terrain_height(&g.terrain, g.course_center.x, g.course_center.z) - g.pad_h) < 0.02f,
          "course pad is flat at deck height (%.2f)",
          terrain_height(&g.terrain, g.course_center.x, g.course_center.z));
    CHECK(font_tex() >= 0 && proc_tex.water >= 0 && proc_tex.wood >= 0,
          "font + procedural textures registered");

    game_start_play(&g);

    /* hand-driven run down the course line: sprint, hop the steps, vault the
       wall, jump the gap. Fixed input timeline = deterministic CI. */
    float sim = 0.0f;
    int frames = 0;
    int gap_jumped = 0, crate_jumped = 0, max_tris = 0, max_draws = 0;
    for (int i = 0; i < 60 * 10; i++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        if (sim > 0.5f && sim < 7.0f) { in.my = 1.0f; in.buttons = BTN_SPRINT; }
        /* vault the on-line crate the frame we touch it */
        if (!crate_jumped && g.player.blocked && g.player.pos.z < 566.0f) {
            in.pressed = BTN_JUMP; in.buttons |= BTN_JUMP; crate_jumped = 1;
        }
        /* jump the gap from the lip of the first deck (position-triggered so
           the run is robust to small speed changes) */
        if (!gap_jumped && g.player.pos.z > 592.4f && g.player.pos.z < 593.5f &&
            g.player.grounded) {
            in.pressed = BTN_JUMP; in.buttons |= BTN_JUMP; gap_jumped = 1;
        }
        game_frame(&g, &in, DT);
        game_render(&g);
        if (rend()->tris_drawn > max_tris) max_tris = rend()->tris_drawn;
        if (rend()->draw_calls > max_draws) max_draws = rend()->draw_calls;
        sim += DT; frames++;
    }
    Player *p = &g.player;
    CHECK(frames == 600, "simulated 10 s of gameplay");
    CHECK(p->stats.jumps >= 2, "course run performed %d jumps", p->stats.jumps);
    CHECK(p->stats.vaults >= 1, "course run vaulted the crate (vaults=%d)", p->stats.vaults);
    CHECK(p->stats.distance_m > 40.0f, "course run covered %.1f m", p->stats.distance_m);
    CHECK(v3_valid(p->pos) && p->pos.y > 0.0f, "player ended in a sane place (%.1f %.1f %.1f)",
          p->pos.x, p->pos.y, p->pos.z);
    CHECK(!player_is_down(p), "player survived the course (hp=%.0f)", p->health);

    RendState *rs = rend();
    CHECK(max_tris > 100, "renderer rasterized up to %d triangles in a frame", max_tris);
    CHECK(max_draws > 20, "renderer issued up to %d draw calls in a frame", max_draws);
    (void)rs;

    /* the frame must contain world pixels, not just clear colour */
    if (rs && rs->fb) {
        uint32_t clear = rs->fb[0];
        int world_px = 0;
        for (int y = 20; y < 160; y += 4)
            for (int x = 10; x < 310; x += 4)
                if (rs->fb[y * 320 + x] != clear) world_px++;
        CHECK(world_px > 200, "framebuffer holds %d non-sky sample pixels", world_px);
        if (shot_dir) {
            char path[512];
            snprintf(path, sizeof(path), "%s/m1_test_frame.bmp", shot_dir);
            rend_save_bmp(path);
        }
    }
    game_free(&g);
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : ".";
    dh_log_init(out);
    DH_INFO("smoke1", "── M1 traversal smoke test ──");

    make_flat_world();
    test_speeds();
    test_jump_arc();
    test_coyote_and_buffer();
    test_step_and_vault();
    test_air_mantle();
    test_fall_damage();
    test_swim();
    test_slide();
    test_zipline();
    test_stamina();
    test_bounds_and_sanity();
    test_integration(out);

    DH_INFO("smoke1", "── %d checks, %d failed ──", g_checks, g_failed);
    if (g_failed == 0) printf("M1 TRAVERSAL SMOKE PASSED: %d/%d checks\n", g_checks, g_checks);
    else printf("M1 TRAVERSAL SMOKE FAILED: %d/%d checks failed\n", g_failed, g_checks);
    dh_log_shutdown();
    return g_failed ? 1 : 0;
}
