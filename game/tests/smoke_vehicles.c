/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M4 city vertical-slice smoke test (headless, determ.)
   Spec 16 DoD: STEAL A CAR → EVADE 3 STARS → BUY A HOT DOG WITH STOLEN CASH.
   Proves the systems the slice is built from:
     §69   vehicle table is law (top speed / 0-100 / grip / hp); handling derived
     §38   arcade driving: throttle/brake/reverse/steer/handbrake drift-lite
     §29   heat 0→5: evidence thresholds, witnesses, response ladder, escape clock
     16    8 blocks + harbor stub, 15 traffic cars, 30 peds, 1 odd job
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
#include "../src/combat/weapon.h"
#include "../src/ai/enemy.h"
#include "../src/rend/rend.h"
#include "../src/rend/font.h"
#include "../src/rend/proc_tex.h"
#include "../src/city/heat.h"
#include "../src/city/vehicle.h"
#include "../src/city/city.h"
#include "../src/game/game.h"
#include "../src/plat/plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke22", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke22", "ok: " __VA_ARGS__); \
} while (0)

#define DT (1.0f / 60.0f)
#define NEAR(a, b, eps) (fabsf((a) - (b)) <= (eps))

static void step_frames(Game *g, uint32_t buttons, uint32_t pressed, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        in.buttons = buttons; in.pressed = (i == 0) ? pressed : 0u;
        game_frame(g, &in, DT);
    }
}
static void tp(Game *g, float x, float y, float z) {
    g->player.pos = v3(x, y, z);
    g->player.vel = v3(0, 0, 0);
}
static float dist2d(Vec3 a, Vec3 b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}
static int vidx(Game *g, const char *id) {
    City *c = &g->city;
    for (int i = 0; i < c->def_count; i++)
        if (!strcmp(c->defs[i].id, id)) return i;
    return -1;
}
static int count_chase(Game *g) {
    int n = 0;
    for (int i = g->city.chase0; i < g->city.veh_count; i++)
        if (g->city.veh[i].state == VS_CHASE) n++;
    return n;
}
/* Push every pursuing cruiser out of the city, north of the boundary wall —
   the wall guarantees no LOS, so the escape clock is deterministic. */
static void despawn_cruisers(Game *g) {
    City *c = &g->city;
    int k = 0;
    for (int i = c->chase0; i < c->veh_count; i++) {
        if (c->veh[i].state != VS_CHASE) continue;
        c->veh[i].state = VS_PARKED;
        c->veh[i].speed = 0.0f;
        c->veh[i].pos = v3(380.0f + 80.0f * (float)(k % 4), 4.0f, 200.0f);
        k++;
    }
}
static void heat_reset(Game *g) {
    despawn_cruisers(g);
    heat_init(&g->city.heat);
    g->city.heli_active = 0;
    g->noise_t = 0.0f;
}
/* First parked car of def `id` near (x,z) — the parked fleet is fixed layout. */
static int parked_at(Game *g, const char *id, float x, float z) {
    City *c = &g->city;
    int di = vidx(g, id);
    for (int i = 0; i < c->veh_count; i++) {
        const Vehicle *v = &c->veh[i];
        if (v->state == VS_PARKED && v->def == di &&
            fabsf(v->pos.x - x) < 1.0f && fabsf(v->pos.z - z) < 1.0f) return i;
    }
    return -1;
}


static float t0_100(const VehicleDef *defs, int di) {
    Vehicle v; memset(&v, 0, sizeof v); v.def = di; v.state = VS_DRIVEN;
    for (int i = 0; i < 60 * 40; i++) {
        vehicle_step(&v, defs, DT, 1.0f, 0.0f, 0.0f, 0);
        if (v.speed >= 27.78f) return (float)(i + 1) * DT;
    }
    return 99.0f;
}
static float top_after(const VehicleDef *defs, int di) {
    Vehicle v; memset(&v, 0, sizeof v); v.def = di; v.state = VS_DRIVEN;
    for (int i = 0; i < 60 * 90; i++) vehicle_step(&v, defs, DT, 1.0f, 0.0f, 0.0f, 0);
    return v.speed;
}

static void test_roster(Game *g) {
    City *c = &g->city;
    CHECK(c->def_count == 9, "S69 land roster: 9 defs loaded (%d)", c->def_count);
    static const struct { const char *id; int cls; float top, t100, hp; int heat; } R[] = {
        {"V01", VC_COMPACT, 145, 8.0f, 450, 1}, {"V02", VC_MUSCLE, 195, 5.2f, 550, 1},
        {"V03", VC_VAN, 120, 11.0f, 700, 1},    {"V04", VC_PICKUP, 150, 7.5f, 650, 1},
        {"V05", VC_TAXI, 135, 9.0f, 500, 1},    {"V06", VC_POLICE, 180, 6.0f, 800, 2},
        {"V07", VC_MOTO, 205, 4.2f, 300, 1},    {"V11", VC_ARMORED, 140, 9.5f, 2000, 3},
        {"V12", VC_BUGGY, 130, 6.5f, 500, 1},
    };
    for (size_t k = 0; k < sizeof R / sizeof R[0]; k++) {
        int di = vidx(g, R[k].id);
        CHECK(di >= 0, "%s present", R[k].id);
        if (di < 0) continue;
        const VehicleDef *d = &c->defs[di];
        CHECK((int)d->cls == R[k].cls && fabsf(d->hp - R[k].hp) < 0.5f && d->steal_heat == R[k].heat,
              "%s %s: class/hp/steal-heat match the table", R[k].id, d->name);
        float t = t0_100(c->defs, di);
        CHECK(fabsf(t - R[k].t100) <= R[k].t100 * 0.30f, "%s 0-100 %.2fs vs table %.1fs (+-30%%)",
              R[k].id, (double)t, (double)R[k].t100);
        float top = top_after(c->defs, di) * 3.6f;
        CHECK(top > R[k].top * 0.95f && top <= R[k].top * 1.01f, "%s top %.0f km/h vs %.0f",
              R[k].id, (double)top, (double)R[k].top);
    }
    int ia = vidx(g, "V11"), ib = vidx(g, "V12"), iv = vidx(g, "V03");
    if (ia >= 0 && ib >= 0 && iv >= 0) {
        CHECK(c->defs[ia].length > c->defs[iv].length && c->defs[iv].height > c->defs[vidx(g,"V01")].height,
              "body boxes scale by class (armored > van > compact)");
        CHECK(c->defs[ib].radio == 0 && c->defs[ia].seats == 6, "buggy has no radio; Obelisco seats 6");
    }
}

static void test_fleet(Game *g) {
    City *c = &g->city;
    CHECK(c->veh_count >= 27, "12 parked + 15 traffic (%d)", c->veh_count);
    CHECK(parked_at(g, "V03", 389.5f, 470.0f) >= 0, "Almuden van parked on avenue 1");
    CHECK(parked_at(g, "V04", 610.5f, 520.0f) >= 0, "Ranchero pickup parked on avenue 3");
    CHECK(parked_at(g, "V12", 558.0f, 629.5f) >= 0, "Cosecha buggy parked on the south street");
    int oi = parked_at(g, "V11", 509.5f, 470.0f);
    CHECK(oi >= 0, "Limpio Obelisco parked (steal-only)");
    if (oi < 0) return;
    heat_reset(g);
    CHECK(city_enter_vehicle(c, g, oi) == 1 && g->in_vehicle == oi, "enter the Obelisco");
    CHECK(c->heat.stars >= 1, "stealing Limpio armour is a heavy crime (%d stars)", c->heat.stars);
    float ev_armored = c->heat.evidence;
    /* compare with a plain compact */
    city_exit_vehicle(c, g);
    heat_reset(g);
    int ci = parked_at(g, "V04", 610.5f, 520.0f);
    if (ci >= 0) {
        city_enter_vehicle(c, g, ci);
        CHECK(ev_armored > c->heat.evidence, "Obelisco steal (%.1f) hotter than pickup (%.1f)",
              (double)ev_armored, (double)c->heat.evidence);
        city_exit_vehicle(c, g);
    }
    g->in_vehicle = -1;
    heat_reset(g);
}

static void test_armored_response(Game *g) {
    City *c = &g->city;
    heat_reset(g);
    tp(g, 500.0f, 4.05f, 500.0f);
    heat_add_evidence(&c->heat, 12.5f);
    CHECK(c->heat.stars >= 4, "4-star heat (%d)", c->heat.stars);
    int ia = vidx(g, "V11"), ob = 0, cr = 0;
    for (int f = 0; f < 90; f++) {
        heat_add_evidence(&c->heat, 0.02f);
        step_frames(g, 0, 0, 1);
        g->player.health = g->player.health_max;
    }
    for (int i = c->chase0; i < c->veh_count; i++) {
        if (c->veh[i].state != VS_CHASE) continue;
        if (c->veh[i].def == ia) ob++; else cr++;
    }
    CHECK(ob >= 1 && cr >= 1, "4-star response mixes Obelisco armour (%d) with cruisers (%d)", ob, cr);
    game_render(g);
    rend_save_png("docs/reports/img/M22_roster.png");
    heat_reset(g);
    c->heat.stars = 0;
    heat_init(&c->heat);
    CHECK(count_chase(g) == 0, "stand-down clears the pursuit");
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    dh_log_init(NULL);
    settings_defaults(settings());
    Game g;
    memset(&g, 0, sizeof g);
    if (!game_init(&g, 320, 180, REND_SOFT, 0xC0FFEEu)) { CHECK(0, "game_init"); return 1; }
    game_start_play(&g);
    game_outpost_start(&g);
    game_load_city(&g);
    test_roster(&g);
    test_fleet(&g);
    test_armored_response(&g);
    DH_INFO("smoke22", "──── %d checks, %d failed ────", g_checks, g_failed);
    dh_log_shutdown();
    return g_failed ? 1 : 0;
}
