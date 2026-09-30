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
    if (!(cond)) { g_failed++; DH_ERROR("smoke4", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke4", "ok: " __VA_ARGS__); \
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

/* ═══════════════════════ §69: the vehicle table is law ═══════════════════ */
static void test_vehicle_table(Game *g) {
    City *c = &g->city;
    CHECK(c->def_count >= 5, "vehicles.json: %d defs loaded (>= 5)", c->def_count);
    int i01 = vidx(g, "V01"), i02 = vidx(g, "V02"), i06 = vidx(g, "V06"),
        i07 = vidx(g, "V07");
    CHECK(i01 >= 0 && i02 >= 0 && i06 >= 0 && i07 >= 0, "V01/V02/V06/V07 all present");
    if (i01 < 0 || i02 < 0 || i06 < 0 || i07 < 0) return;
    CHECK(NEAR(c->defs[i01].top_ms, 145.0f / 3.6f, 0.05f),
          "V01 Coralina top speed is the §69 law: 145 km/h (%.2f m/s)",
          (double)c->defs[i01].top_ms);
    CHECK(NEAR(c->defs[i02].top_ms, 195.0f / 3.6f, 0.05f), "V02 Tiburon GT top = 195 km/h");
    CHECK(NEAR(c->defs[i06].top_ms, 180.0f / 3.6f, 0.05f), "V06 Sirena top = 180 km/h");
    CHECK(c->defs[i06].steal_heat == 2, "V06 Sirena steal is a 2-evidence crime");
    CHECK(c->defs[i07].radio == 0 && c->defs[i01].radio == 1,
          "motorcycle has no radio; Coralina does (data-driven)");
    CHECK(c->defs[i02].accel_ms2 > c->defs[i01].accel_ms2,
          "muscle out-accelerates the compact (derived from 0-100 law)");

    /* pure handling sim: no world, flat virtual ground */
    Vehicle v; memset(&v, 0, sizeof v);
    v.def = i01; v.state = VS_DRIVEN; v.occupant = -1;
    float t100 = -1.0f;
    for (float t = 0.0f; t < 70.0f; t += DT) {
        vehicle_step(&v, c->defs, DT, 1.0f, 0.0f, 0.0f, 0);
        if (t100 < 0.0f && v.speed >= 27.78f) t100 = t;
    }
    CHECK(t100 > 0.0f && t100 < 8.0f * 1.35f,
          "V01 hits 100 km/h in %.2f s (table 8.0 s ±35%%, drag-corrected)", (double)t100);
    CHECK(NEAR(v.speed, c->defs[i01].top_ms, 1.0f),
          "V01 settles at its LAW top speed (%.2f of %.2f m/s)",
          (double)v.speed, (double)c->defs[i01].top_ms);

    memset(&v, 0, sizeof v); v.def = i01; v.state = VS_DRIVEN;
    v.speed = 20.0f;
    for (int i = 0; i < 180; i++) vehicle_step(&v, c->defs, DT, 0.0f, 1.0f, 0.0f, 0);
    CHECK(v.speed < 0.5f, "service brake stops 20 m/s within 3 s (%.2f m/s left)",
          (double)v.speed);
    for (int i = 0; i < 240; i++) vehicle_step(&v, c->defs, DT, 0.0f, 1.0f, 0.0f, 0);
    CHECK(v.speed < -5.0f, "holding brake from standstill reverses (%.2f m/s)",
          (double)v.speed);

    memset(&v, 0, sizeof v); v.def = i01; v.state = VS_DRIVEN; v.speed = 10.0f;
    for (int i = 0; i < 60; i++) vehicle_step(&v, c->defs, DT, 0.4f, 0.0f, 1.0f, 0);
    CHECK(v.yaw > 0.6f, "steering right at 10 m/s turns %.2f rad in 1 s", (double)v.yaw);
    for (int i = 0; i < 30; i++) vehicle_step(&v, c->defs, DT, 0.4f, 0.0f, 1.0f, 1);
    CHECK(v.slip > 0.05f, "handbrake builds drift slip (%.3f)", (double)v.slip);

    /* grip is law: steer authority scales with it */
    VehHandling h1, h2;
    vehicle_derive(&c->defs[i01], &h1);
    vehicle_derive(&c->defs[i07], &h2);
    CHECK(h2.steer_rate > h1.steer_rate,
          "V07 grip 0.90 steers harder than V01 grip 0.85 (law, derived)");
    CHECK(NEAR(h1.drag_k * c->defs[i01].top_ms * c->defs[i01].top_ms, h1.accel_ms2, 0.01f),
          "quadratic drag is derived so v_top == the table value");

    /* damage law: crash → wreck at the table HP */
    memset(&v, 0, sizeof v); v.def = i01; v.state = VS_DRIVEN; v.speed = 30.0f;
    float hp = c->defs[i01].hp;
    int impacts = 0;
    while (!vehicle_wrecked(&v, c->defs) && impacts < 100) {
        vehicle_impact(&v, 30.0f);
        v.speed = 30.0f;
        impacts++;
    }
    CHECK(vehicle_wrecked(&v, c->defs) && NEAR(v.damage, hp, hp * 0.5f),
          "V01 wrecks after absorbing its %0.f hp (damage %.0f)", (double)hp, (double)v.damage);
    CHECK(impacts <= 4, "one 30 m/s major crash plus a couple more ends the car (%d impacts)",
          impacts);
}

/* ═══════════════════════ world swap + layout ═════════════════════════════ */
static void test_city_world(Game *g) {
    City *c = &g->city;
    CHECK(g->act == 1, "act II loaded (act == 1)");
    CHECK(c->built == 1, "city_init built Meridian City");
    int distinct = 1;
    for (int i = 0; i < CITY_BLOCKS && distinct; i++)
        for (int j = i + 1; j < CITY_BLOCKS; j++)
            if (fabsf(c->blocks[i].x - c->blocks[j].x) < 1.0f &&
                fabsf(c->blocks[i].y - c->blocks[j].y) < 1.0f) { distinct = 0; break; }
    CHECK(distinct, "8 distinct city blocks");
    int inb = 1;
    for (int i = 0; i < CITY_BLOCKS; i++)
        if (c->blocks[i].x < 370 || c->blocks[i].x > 630 ||
            c->blocks[i].y < 370 || c->blocks[i].y > 630) inb = 0;
    CHECK(inb, "every block sits inside the road grid bounds");

    CHECK(city_feature_at(c, 380.0f, 500.0f) == 1, "avenue cell maps to ROAD");
    CHECK(city_feature_at(c, 413.0f, 440.0f) == 2, "block interior maps to BLOCK");
    CHECK(city_feature_at(c, 500.0f, 700.0f) == 3, "south strip maps to HARBOR");
    CHECK(city_feature_at(c, 200.0f, 200.0f) == 0, "outside the city maps to NOTHING");

    float hmid = terrain_height(&g->terrain, 500.0f, 500.0f);
    float hharbor = terrain_height(&g->terrain, 500.0f, 716.0f);
    float hbay = terrain_height(&g->terrain, 200.0f, 200.0f);
    CHECK(NEAR(hmid, CITY_GROUND_Y, 0.15f), "city plate is flat at 4.0 m (%.2f)", (double)hmid);
    CHECK(hharbor < 0.0f, "harbor channel is below sea level (%.2f)", (double)hharbor);
    CHECK(hbay < -1.0f, "the rest of the map was flattened into the bay (%.2f)", (double)hbay);
    CHECK(terrain_height(&g->terrain, c->player_spawn.x, c->player_spawn.z) > 3.0f,
          "player spawn (harbor promenade) is on dry land");

    CHECK(g->prop_count >= 40 && g->prop_count < GAME_MAX_PROPS,
          "%d props: 32+ buildings, curbs, docks, stand, board (< cap %d)",
          g->prop_count, GAME_MAX_PROPS);
    CHECK(g->obs.count >= 32, "%d collision volumes (buildings + walls)", g->obs.count);
    CHECK(g->enemies.count == 0 && g->critter_count == 0 && !g->outpost.built,
          "act-I content was freed by the world swap");
    CHECK(g->zips.count == 0, "island ziplines don't hang over the streets");

    /* minimap repainted: road cell dark gray, block cell brown, harbor blue */
    float cell = 1000.0f / (float)MAPW;
    int rc = (int)(500.0f / cell), rr = (int)(380.0f / cell);
    uint32_t roadc = g->map_cell[rr * MAPW + rc];
    CHECK((roadc & 0xFF) < 90 && ((roadc >> 16) & 0xFF) < 90,
          "minimap: avenue cell painted asphalt-dark (%08X)", roadc);

    CHECK(c->veh_count == 27, "12 parked + 15 traffic = 27 vehicles (%d)", c->veh_count);
    CHECK(c->traffic_n == 15, "traffic fleet is exactly 15 cars");
    CHECK(c->ped_count == 30, "30 pedestrians on the sidewalks");
    int loops_seen = 0;
    for (int l = 0; l < 3; l++) {
        int n = 0;
        for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++)
            if (c->veh[i].loop == l) n++;
        if (n >= 4) loops_seen++;
    }
    CHECK(loops_seen == 3, "traffic spread across all 3 loops (>= 4 cars each)");
    int police_parked = parked_at(g, "V06", 490.5f, 512.0f);
    CHECK(police_parked >= 0, "a Sirena cruiser is parked by the hot-dog corner (stealable)");
    CHECK(parked_at(g, "V02", 389.5f, 545.0f) >= 0 && parked_at(g, "V07", 509.5f, 560.0f) >= 0,
          "the 3 drivable classes are parked curbside (muscle, compact, moto)");
}

/* ═══════════════════════ §38: arcade driving in the world ════════════════ */
static void test_enter_exit_drive(Game *g) {
    City *c = &g->city;
    int vi = parked_at(g, "V01", 389.5f, 420.0f);
    CHECK(vi >= 0, "found the parked Coralina on avenue 1");
    if (vi < 0) return;

    /* enter with E (the shipped binding) */
    tp(g, c->veh[vi].pos.x - 2.5f, 4.05f, c->veh[vi].pos.z);
    g->player.yaw = 1.57f;
    step_frames(g, 0, BTN_USE, 1);
    CHECK(g->in_vehicle == vi, "[E] next to a parked car ENTERS it (DoD verb 1)");
    CHECK(c->veh[vi].state == VS_DRIVEN, "entered car switches to DRIVEN");
    CHECK(c->heat.stars >= 1, "stealing a parked car is a crime: >= 1 star (%d)",
          c->heat.stars);
    heat_reset(g);   /* isolate the driving physics from the response ladder */
    c->veh[vi].state = VS_DRIVEN;

    /* throttle: accelerate north up the avenue */
    float z0 = c->veh[vi].pos.z;
    step_frames(g, 0, 0, 180);                      /* 3 s of W (my=+1 below) */
    PlatInput in; memset(&in, 0, sizeof in);
    for (int i = 0; i < 180; i++) { in.my = 1.0f; game_frame(g, &in, DT); }
    CHECK(c->veh[vi].speed > 8.0f, "3 s of throttle passes 8 m/s (%.1f m/s)",
          (double)c->veh[vi].speed);
    CHECK(c->veh[vi].pos.z - z0 > 20.0f, "the car actually travelled (+%.0f m north)",
          (double)(c->veh[vi].pos.z - z0));
    CHECK(NEAR(g->player.pos.z, c->veh[vi].pos.z, 0.01f),
          "player body rides with the car (camera follows)");

    /* steering changes heading */
    float yaw0 = c->veh[vi].yaw;
    for (int i = 0; i < 60; i++) { in.my = 0.5f; in.mx = 1.0f; game_frame(g, &in, DT); }
    in.mx = 0.0f;
    CHECK(fabsf(c->veh[vi].yaw - yaw0) > 0.3f, "steering input turns the car (%.2f rad)",
          (double)(c->veh[vi].yaw - yaw0));

    /* brake: 1.5 s from ~10 m/s must nearly stop the car... */
    for (int i = 0; i < 90; i++) { in.my = -1.0f; game_frame(g, &in, DT); }
    CHECK(fabsf(c->veh[vi].speed) < 3.5f, "1.5 s of brake from ~10 m/s nearly stops it (%.2f)",
          (double)c->veh[vi].speed);
    /* ...and holding S keeps backing up */
    for (int i = 0; i < 240; i++) { in.my = -1.0f; game_frame(g, &in, DT); }
    CHECK(c->veh[vi].speed < -0.5f, "holding S reverses (%.2f m/s)", (double)c->veh[vi].speed);

    /* leaving at speed is refused; at rest [E] exits */
    for (int i = 0; i < 300; i++) { in.my = 1.0f; game_frame(g, &in, DT); }
    CHECK(c->veh[vi].speed > 3.0f, "back up to speed for the exit-refusal check (%.1f m/s)",
          (double)c->veh[vi].speed);
    step_frames(g, 0, BTN_USE, 1);
    CHECK(g->in_vehicle == vi, "stepping out above 3 m/s is refused (arcade safety)");
    memset(&in, 0, sizeof in);
    c->veh[vi].speed = 0.0f;               /* deterministic standstill */
    step_frames(g, 0, 0, 30);              /* let the E-repeat guard expire */
    c->veh[vi].speed = 0.0f;
    step_frames(g, 0, BTN_USE, 3);
    CHECK(g->in_vehicle == -1, "[E] at rest exits the car (player on foot)");
    CHECK(dist2d(g->player.pos, c->veh[vi].pos) < 5.0f,
          "exit places the player beside the car (%.1f m)",
          (double)dist2d(g->player.pos, c->veh[vi].pos));
    CHECK(c->veh[vi].state == VS_PARKED, "abandoned car returns to PARKED");
    heat_reset(g);

    /* wreck law: crash a wounded car into the boundary wall */
    int vj = parked_at(g, "V02", 389.5f, 545.0f);
    CHECK(vj >= 0, "found the parked Tiburon GT for the wreck test");
    if (vj >= 0) {
        Vehicle *w = &c->veh[vj];
        w->damage = c->defs[w->def].hp * 0.95f;     /* one big crash from scrap */
        city_enter_vehicle(c, g, vj);
        w->pos = v3(500.0f, 4.0f, 392.0f);
        w->yaw = 3.14159f;                          /* nose at the north wall */
        w->speed = 0.0f;
        PlatInput inn; memset(&inn, 0, sizeof inn);
        for (int i = 0; i < 240 && g->in_vehicle == vj; i++) {
            inn.my = 1.0f; game_frame(g, &inn, DT);
        }
        CHECK(w->state == VS_WRECK, "crashing a wounded muscle car into a wall wrecks it");
        CHECK(g->in_vehicle == -1, "wreck ejects the driver");
        CHECK(g->player.health < 100.0f, "bailing out of a wreck costs health (%.0f)",
              (double)g->player.health);
        CHECK(c->cars_wrecked >= 1, "wreck counter tracks the loss (%d)", c->cars_wrecked);
        g->player.health = 100.0f;
    }
    heat_reset(g);

    /* the harbor is a wall, not a submarine simulator */
    int vk = parked_at(g, "V01", 610.5f, 450.0f);
    if (vk >= 0) {
        city_enter_vehicle(c, g, vk);
        Vehicle *w = &c->veh[vk];
        w->pos = v3(500.0f, 4.0f, 600.0f); w->yaw = 0.0f; w->speed = 12.0f;
        PlatInput inn; memset(&inn, 0, sizeof inn);
        for (int i = 0; i < 300; i++) { inn.my = 1.0f; game_frame(g, &inn, DT); }
        CHECK(w->pos.z < 655.0f && w->pos.y > 0.0f,
              "driving south into the harbor stops at the water line (z=%.1f)",
              (double)w->pos.z);
        city_exit_vehicle(c, g);
    }
    heat_reset(g);
}

/* ═══════════════════════ traffic AI (15 cars, gap rule) ══════════════════ */
static void test_traffic(Game *g) {
    City *c = &g->city;
    tp(g, 500.0f, 4.05f, 640.0f);          /* out of the traffic's way */
    int finite = 1, inb = 1;
    for (int f = 0; f < 600; f++) {
        PlatInput in; memset(&in, 0, sizeof in);
        game_frame(g, &in, DT);
        for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++) {
            const Vehicle *v = &c->veh[i];
            if (!v3_valid(v->pos) || !isfinite(v->speed)) finite = 0;
            if (v->pos.x < 360 || v->pos.x > 640 || v->pos.z < 360 || v->pos.z > 645) inb = 0;
        }
    }
    CHECK(finite, "600-frame traffic soak: no NaN positions or speeds");
    CHECK(inb, "traffic stays inside the road grid for 10 simulated seconds");
    int moving = 0;
    for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++)
        if (c->veh[i].speed > 1.0f) moving++;
    CHECK(moving >= 12, "%d/15 traffic cars are rolling after the soak", moving);

    /* gap rule: cars on the same loop never bunch into each other */
    float worst = 1e9f;
    for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++)
        for (int j = i + 1; j < c->traffic0 + c->traffic_n; j++) {
            if (c->veh[i].loop != c->veh[j].loop) continue;
            float d = dist2d(c->veh[i].pos, c->veh[j].pos);
            if (d < worst) worst = d;
        }
    CHECK(worst > 2.0f, "same-loop spacing holds (min pair distance %.1f m)", (double)worst);

    /* gunfire panics nearby traffic */
    Vehicle *t = &c->veh[c->traffic0];
    tp(g, t->pos.x + 10.0f, 4.05f, t->pos.z);
    step_frames(g, BTN_FIRE, BTN_FIRE, 2);
    int panicked = 0;
    for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++)
        if (c->veh[i].panic_t > 0.0f) panicked++;
    CHECK(panicked >= 1, "gunfire scatters traffic (%d cars panicking)", panicked);
    heat_reset(g);
}

/* ═══════════════════════ peds: wander, panic, survive ════════════════════ */
static void test_peds(Game *g) {
    City *c = &g->city;
    tp(g, 500.0f, 4.05f, 640.0f);
    for (int i = 0; i < c->ped_count; i++) c->peds[i].state = 0;  /* un-panic */
    step_frames(g, 0, 0, 2);               /* walk state snaps peds to their loops */
    int onwalk = 1;
    for (int i = 0; i < c->ped_count; i++) {
        const Ped *p = &c->peds[i];
        if (p->block < 0 || p->block >= CITY_BLOCKS) { onwalk = 0; continue; }
        Vec2 b = c->blocks[p->block];
        if (fabsf(p->pos.x - b.x) > c->block_rx + 8.0f ||
            fabsf(p->pos.z - b.y) > c->block_rz + 8.0f) onwalk = 0;
    }
    CHECK(onwalk, "all 30 peds belong to a block sidewalk loop");

    Vec3 p0 = c->peds[0].pos;
    float walked = 0.0f;
    int finite = 1;
    for (int f = 0; f < 600; f++) {
        PlatInput in; memset(&in, 0, sizeof in);
        Vec3 prev = c->peds[0].pos;
        game_frame(g, &in, DT);
        walked += dist2d(prev, c->peds[0].pos);
        for (int i = 0; i < c->ped_count; i++)
            if (!v3_valid(c->peds[i].pos)) finite = 0;
    }
    CHECK(finite, "600-frame ped soak: no NaN positions");
    CHECK(walked > 8.0f, "peds actually walk their sidewalks (%.1f m travelled)",
          (double)walked);
    (void)p0;

    /* gunfire panic */
    c->peds[0].state = 0;
    tp(g, c->peds[0].pos.x + 5.0f, 4.05f, c->peds[0].pos.z);
    step_frames(g, BTN_FIRE, BTN_FIRE, 2);
    CHECK(c->peds[0].state == 1, "a ped 5 m from gunfire flees (state 1)");
    heat_reset(g);

    /* witnesses: a ped with LOS reports the crime (open road = guaranteed LOS) */
    heat_init(&c->heat);
    tp(g, 383.0f, 4.05f, 480.0f);
    c->peds[1].state = 0;
    c->peds[1].pos = v3(383.0f, 4.0f, 488.0f);
    city_crime(c, g, g->player.pos, 1.0f, "TEST CRIME");
    CHECK(c->heat.evidence >= 2.0f,
          "crime + visible witness = >= 2 evidence (got %.1f)", (double)c->heat.evidence);
    CHECK(c->peds[1].state == 1, "the witnessing ped panics and runs");
    heat_reset(g);

    /* hit-and-run: peds get knocked down and SURVIVE (honest M-rating slice) */
    int vi = parked_at(g, "V05", 509.5f, 420.0f);
    CHECK(vi >= 0, "found a car for the hit-and-run test");
    if (vi >= 0) {
        Ped *target = &c->peds[2];
        target->state = 2; target->state_t = 5.0f;   /* cowering = stationary */
        target->pos = v3(383.0f, 4.0f, 470.0f);      /* jaywalker on avenue 1 */
        Vehicle *v = &c->veh[vi];
        v->pos = v3(383.0f, 4.0f, 464.0f);
        v->yaw = 0.0f; v->speed = 6.0f; v->damage = 0.0f; v->state = VS_PARKED;
        city_enter_vehicle(c, g, vi);
        heat_reset(g);
        g->in_vehicle = vi; c->veh[vi].state = VS_DRIVEN;
        PlatInput in; memset(&in, 0, sizeof in);
        for (int i = 0; i < 90; i++) { in.my = 1.0f; game_frame(g, &in, DT); }
        CHECK(target->state == 3, "driving into a ped knocks them down (state 3)");
        CHECK(c->peds_hit >= 1, "hit-and-run counter increments (%d)", c->peds_hit);
        CHECK(c->heat.stars >= 1, "hit-and-run draws heat (%d stars)", c->heat.stars);
        city_exit_vehicle(c, g);
        target->state = 0; target->state_t = 0.0f;
        heat_reset(g);
    }
}

/* ═══════════════════════ §29: heat system, unit level ════════════════════ */
static void test_heat_unit(void) {
    Heat h; heat_init(&h);
    CHECK(h.stars == 0 && h.evidence == 0.0f, "heat starts clean");
    heat_add_evidence(&h, 1.0f); CHECK(h.stars == 1, "1 evidence = 1 star");
    heat_add_evidence(&h, 2.0f); CHECK(h.stars == 2, "3 evidence = 2 stars");
    heat_add_evidence(&h, 3.0f); CHECK(h.stars == 3, "6 evidence = 3 stars");
    heat_add_evidence(&h, 4.0f); CHECK(h.stars == 4, "10 evidence = 4 stars");
    heat_add_evidence(&h, 5.0f); CHECK(h.stars == 5, "15 evidence = 5 stars (cap)");
    heat_add_evidence(&h, 50.0f); CHECK(h.stars == 5, "stars cap at 5");
    CHECK(heat_response_cruisers(&h) == 6, "5-star response fields 6 cruisers");
    CHECK(heat_has_heli(&h) == 1, "4+ stars call the heli");

    /* escape clock: 20 clean seconds per star, evidence snaps down */
    heat_init(&h);
    heat_add_evidence(&h, 6.0f);                    /* 3 stars */
    for (float t = 0; t < 19.5f; t += DT) heat_tick(&h, DT, 200.0f, 0);
    CHECK(h.stars == 3, "19.5 s clean is NOT enough to drop a star");
    for (float t = 0; t < 1.0f; t += DT) heat_tick(&h, DT, 200.0f, 0);
    CHECK(h.stars == 2, "20.5 s clean drops 3->2 stars");
    CHECK(NEAR(h.evidence, 3.0f, 0.01f), "evidence snaps to the new threshold (3.0)");
    CHECK(h.searching == 1, "escape clock shows as SEARCHING on the HUD");

    /* LOS freezes the clock */
    for (float t = 0; t < 40.0f; t += DT) heat_tick(&h, DT, 200.0f, 1);
    CHECK(h.stars == 2, "a cop with line of sight freezes the escape clock");
    /* proximity freezes it too */
    for (float t = 0; t < 40.0f; t += DT) heat_tick(&h, DT, 50.0f, 0);
    CHECK(h.stars == 2, "cops within 100 m freeze the escape clock");
    /* all the way down to clean */
    for (float t = 0; t < 41.0f; t += DT) heat_tick(&h, DT, 200.0f, 0);
    CHECK(h.stars == 0 && h.evidence == 0.0f && h.searching == 0,
          "two more clean streaks take 2->0 and clear the record");
    /* a fresh crime mid-escape resets the streak */
    heat_add_evidence(&h, 3.0f);
    for (float t = 0; t < 10.0f; t += DT) heat_tick(&h, DT, 200.0f, 0);
    heat_add_evidence(&h, 1.0f);
    CHECK(NEAR(h.escape_t, 0.0f, 0.01f), "committing a crime resets the escape streak");
    CHECK(heat_response_cruisers(&h) == h.stars + 1,
          "response ladder = stars + 1 cruisers");
}

/* ═══════════════════════ heat, live: response + escape ═══════════════════ */
static void test_heat_live(Game *g) {
    City *c = &g->city;
    heat_reset(g);
    g->cash = 0;

    /* carjack an occupied traffic car (DoD verb 1, the loud way) */
    int ti = c->traffic0;
    Vehicle *t = &c->veh[ti];
    tp(g, t->pos.x + 1.5f, 4.05f, t->pos.z);
    int peds_before = c->ped_count;
    CHECK(city_enter_vehicle(c, g, ti) == 1, "carjack an occupied traffic car");
    CHECK(g->in_vehicle == ti, "the player is now driving the hijacked car");
    CHECK(t->occupant == -1, "the driver was thrown out");
    CHECK(c->ped_count == peds_before + 1 && c->peds[c->ped_count - 1].state == 1,
          "the ejected driver spawns as a fleeing ped");
    CHECK(g->cash >= 25 && c->wallets_looted == 1,
          "the driver dropped a wallet: $%d (STOLEN CASH for the DoD)", g->cash);
    CHECK(c->heat.stars >= 1, "carjacking raises heat to >= 1 star (%d)", c->heat.stars);
    CHECK(c->cars_stolen >= 1, "cars_stolen counter ticks (%d)", c->cars_stolen);

    /* response ladder fields cruisers that actually chase */
    step_frames(g, 0, 0, 120);                       /* 2 s */
    CHECK(count_chase(g) >= 2, "1-2 stars field >= 2 cruisers (%d spawned)", count_chase(g));
    float d0 = 1e9f;
    for (int i = c->chase0; i < c->veh_count; i++)
        if (c->veh[i].state == VS_CHASE)
            d0 = fminf(d0, dist2d(c->veh[i].pos, g->player.pos));
    /* sit still 8 s: the nearest cruiser must close distance */
    for (int i = 0; i < 480; i++) {
        PlatInput in; memset(&in, 0, sizeof in);
        game_frame(g, &in, DT);
    }
    float d1 = 1e9f;
    for (int i = c->chase0; i < c->veh_count; i++)
        if (c->veh[i].state == VS_CHASE)
            d1 = fminf(d1, dist2d(c->veh[i].pos, g->player.pos));
    CHECK(d1 < d0, "cruisers pursue: nearest went %.0f m -> %.0f m in 8 s",
          (double)d0, (double)d1);
    CHECK(g->player.health <= 100.0f, "player still alive after the pursuit so far (%.0f hp)",
          (double)g->player.health);

    /* escape: with units shaken (no LOS, >100 m) the clock winds down in-world */
    heat_init(&c->heat);
    heat_add_evidence(&c->heat, 6.0f);                /* exactly 3 stars */
    city_exit_vehicle(c, g);
    tp(g, 500.0f, 4.05f, 500.0f);
    for (int f = 0; f < 3960 && c->heat.stars > 0; f++) {   /* up to 66 s */
        despawn_cruisers(g);
        PlatInput in; memset(&in, 0, sizeof in);
        game_frame(g, &in, DT);
    }
    CHECK(c->heat.stars == 0, "3 stars decay to 0 after 60+ clean seconds in-world");
    CHECK(c->heat.searching == 0, "searching flag clears when the record is clean");
    CHECK(c->escape_peak_stars >= 3, "escape_peak_stars remembers the 3-star run (%d)",
          c->escape_peak_stars);
    g->player.health = 100.0f;
    heat_reset(g);
}

/* ═══════════════════════ odd job + hot-dog economy ═══════════════════════ */
static void test_job_hotdog(Game *g) {
    City *c = &g->city;
    heat_reset(g);
    g->cash = 0;

    /* board → pickup → dropoff */
    tp(g, c->job_board.x, 4.05f, c->job_board.z + 2.0f);
    CHECK(city_job_interact(c, g) == 1, "[E] on the harbor job board accepts the job");
    CHECK(c->job.active == 1 && NEAR(c->job.timer, 90.0f, 0.5f),
          "job armed with a 90 s clock (%.0f s)", (double)c->job.timer);
    tp(g, c->job.pickup.x, 4.05f, c->job.pickup.z);
    CHECK(city_job_interact(c, g) == 2, "[E] at the crate marker picks it up");
    tp(g, c->job.dropoff.x, 4.05f, c->job.dropoff.z + 2.0f);
    CHECK(city_job_interact(c, g) == 3, "[E] at the stand delivers it");
    CHECK(g->cash == 400 && c->jobs_done == 1, "delivery pays $400 (cash %d)", g->cash);

    /* fail-forward: the clock runs out, the board still offers work */
    tp(g, c->job_board.x, 4.05f, c->job_board.z + 2.0f);
    city_job_interact(c, g);
    CHECK(c->job.active == 1, "job is re-acceptable after delivery");
    step_frames(g, 0, 0, 60 * 92);                    /* let the clock expire */
    CHECK(c->job.active == 0, "job fails when the 90 s clock expires");
    CHECK(city_job_interact(c, g) == 1, "the board still has work after a failure");
    c->job.active = 0;

    /* hot dog: refused without cash, served with it */
    tp(g, c->hotdog_stand.x, 4.05f, c->hotdog_stand.z + 2.5f);
    g->cash = 2;
    CHECK(city_buy_hotdog(c, g) == 0 && g->cash == 2,
          "no cash, no hot dog (vendor refuses, wallet untouched)");
    g->cash = 100;
    g->player.health = 80.0f;
    CHECK(city_buy_hotdog(c, g) == 1, "[E] at the stand buys the hot dog (DoD verb 3)");
    CHECK(g->cash == 95, "the hot dog cost exactly $5 (cash %d)", g->cash);
    CHECK(NEAR(g->player.health, 95.0f, 0.01f), "the hot dog healed +15 hp (%.0f)",
          (double)g->player.health);
    CHECK(c->hotdogs_bought == 1, "hot-dog counter ticks (%d)", c->hotdogs_bought);
    tp(g, 500.0f, 4.05f, 500.0f);
    CHECK(city_buy_hotdog(c, g) == -1, "the vendor doesn't teleport (out of range = -1)");
    g->player.health = 100.0f;
}

/* ═══════════════════════ THE SPEC-16 DoD CHAIN ═══════════════════════════ */
static void test_dod_chain(Game *g) {
    City *c = &g->city;
    heat_reset(g);
    g->cash = 0;
    int stolen_before = c->cars_stolen;
    int dogs_before = c->hotdogs_bought;

    /* 1 ── STEAL A CAR (occupied → the cash is stolen cash) */
    int ti = c->traffic0 + 3;
    Vehicle *t = &c->veh[ti];
    tp(g, t->pos.x + 1.5f, 4.05f, t->pos.z);
    CHECK(city_enter_vehicle(c, g, ti) == 1, "DoD 1: steal a car (carjack traffic)");
    int stolen_cash = g->cash;
    CHECK(stolen_cash >= 25, "DoD 1: the stolen cash is in hand ($%d)", stolen_cash);

    /* 2 ── EVADE 3 STARS (escalate with drive-by gunfire, then shake them) */
    c->veh[ti].speed = 2.0f;               /* kill the coast so the hide spot holds */
    int shots = 0;
    while (c->heat.stars < 3 && shots < 16) {
        step_frames(g, BTN_FIRE, BTN_FIRE, 1);
        step_frames(g, 0, 0, 10);          /* long enough for fire_cd + noise decay */
        shots++;
    }
    CHECK(c->heat.stars >= 3, "DoD 2: gunfire escalation reaches 3 stars (%d, %d shots)",
          c->heat.stars, shots);
    CHECK(c->escape_peak_stars >= 3, "DoD 2: the run peaked at >= 3 stars");
    /* hide: slow and cold at the center intersection, units shaken */
    if (fabsf(c->veh[ti].speed) > 3.0f) c->veh[ti].speed = 0.0f;
    int frames = 0;
    while (c->heat.stars > 0 && frames < 4200) {      /* up to 70 s */
        despawn_cruisers(g);
        c->veh[ti].speed = 0.0f;
        PlatInput in; memset(&in, 0, sizeof in);
        game_frame(g, &in, DT);
        frames++;
    }
    CHECK(c->heat.stars == 0, "DoD 2: evaded — heat is back to zero (%.1f s of hiding)",
          frames / 60.0);

    /* 3 ── BUY A HOT DOG WITH THE STOLEN CASH */
    city_exit_vehicle(c, g);
    tp(g, c->hotdog_stand.x, 4.05f, c->hotdog_stand.z + 2.5f);
    int cash_before = g->cash;
    CHECK(cash_before >= 5, "DoD 3: still holding stolen cash ($%d)", cash_before);
    step_frames(g, 0, BTN_USE, 3);
    CHECK(c->hotdogs_bought == dogs_before + 1,
          "DoD 3: hot dog bought with stolen cash (dogs %d)", c->hotdogs_bought);
    CHECK(g->cash == cash_before - 5, "DoD 3: $5 left the wallet (%d -> %d)",
          cash_before, g->cash);
    CHECK(c->cars_stolen == stolen_before + 1, "DoD 1 (audit): exactly one car stolen");
    CHECK(g->player.health > 0.0f, "DoD: survived the whole chain (%.0f hp)",
          (double)g->player.health);
    DH_INFO("smoke4", "DoD CHAIN COMPLETE: steal -> evade 3 stars -> hot dog ($%d left)",
            g->cash);
    heat_reset(g);
}

/* ═══════════════════════ radio stub, render, perf, soak ══════════════════ */
static void test_radio_render_perf(Game *g) {
    City *c = &g->city;
    heat_reset(g);
    g->player.health = 100.0f;

    /* radio: [Q] cycles the 5 stations while driving (stub: HUD text, M7 audio) */
    int vi = -1;
    for (int i = 0; i < c->veh_count; i++)
        if (c->veh[i].state == VS_PARKED && i < c->traffic0 && c->defs[c->veh[i].def].radio) { vi = i; break; }
    CHECK(vi >= 0, "radio test car found");
    if (vi >= 0) {
        c->veh[vi].state = VS_PARKED;
        tp(g, c->veh[vi].pos.x - 2.5f, 4.05f, c->veh[vi].pos.z);
        c->radio_station = 0;
        c->use_cd = 0.0f;
        step_frames(g, 0, BTN_USE, 1);
        CHECK(g->in_vehicle == vi, "in the car for the radio test");
        for (int i = 0; i < 36; i++) step_frames(g, 0, 0, 1);   /* clear use_cd */
        step_frames(g, 0, BTN_RADIO, 1);
        CHECK(c->radio_station == 1, "[Q] cycles the radio (0 -> 1)");
        for (int i = 0; i < 4; i++) {
            step_frames(g, 0, 0, 2);
            step_frames(g, 0, BTN_RADIO, 1);
        }
        CHECK(c->radio_station == 0, "[Q] wraps around the station list (5 -> 0)");
        CHECK(!strcmp(city_radio_name(4), "RADIO OFF"), "station 5 is RADIO OFF (honest stub)");
        city_exit_vehicle(c, g);
    }
    heat_reset(g);

    /* the city renders: vehicles, peds, markers, and the heli when wanted */
    tp(g, 500.0f, 4.05f, 500.0f);
    g->player.yaw = 0.0f; g->player.pitch = 0.0f;
    game_render(g);
    RendState *r = rend();
    CHECK(r && r->items > 0, "city renders (%d items submitted)", r ? r->items : -1);
    c->heli_active = 1;
    game_render(g);
    CHECK(r && r->items > 0, "city renders with the 4-star heli overhead");
    c->heli_active = 0;

    /* perf gate — Spec 15.3 LOW: < 900 draw calls shipping, < 900 world-only */
    settings_apply_preset(settings(), QUALITY_LOW);
    game_apply_settings(g);
    int worst_ship = 0, worst_items = 0, worst_world = 0;
    double worst_ms = 0.0;
    int finite = 1;
    g->show_debug = 0;
    for (int f = 0; f < 120; f++) {
        PlatInput in; memset(&in, 0, sizeof in);
        g->player.yaw = (float)f * 0.55f;
        g->player.pitch = (float)((f % 20) - 10) * 0.02f;
        game_frame(g, &in, DT);
        game_render(g);
        if (g->sim_ms > worst_ms) worst_ms = g->sim_ms;
        if (r->draw_calls > worst_ship) { worst_ship = r->draw_calls; worst_items = r->items; }
        if (!isfinite(g->player.pos.x) || !isfinite(g->day_t)) finite = 0;
        for (int i = 0; i < c->veh_count && finite; i++)
            if (!v3_valid(c->veh[i].pos)) finite = 0;
        for (int i = 0; i < c->ped_count && finite; i++)
            if (!v3_valid(c->peds[i].pos)) finite = 0;
    }
    CHECK(finite, "120-frame viewpoint sweep stays finite (no NaN drift)");
    CHECK(worst_ms < 6.0, "city sim frame under 6 ms (worst %.2f ms)", worst_ms);
    g->show_hud = 0;
    for (int f = 0; f < 120; f++) {
        PlatInput in; memset(&in, 0, sizeof in);
        g->player.yaw = (float)f * 0.55f;
        game_frame(g, &in, DT);
        game_render(g);
        if (r->draw_calls > worst_world) worst_world = r->draw_calls;
    }
    g->show_hud = 1;
    CHECK(worst_world < 900, "Spec 15.3 LOW: world draws %d < 900", worst_world);
    CHECK(worst_ship < 900, "Spec 15.3 LOW: shipping frame draws %d < 900", worst_ship);
    CHECK(worst_items < 1600, "item submissions %d < 1600 with 23 cars + 30 peds live",
          worst_items);
    CHECK(g->prop_count < 300, "static prop count %d < 300", g->prop_count);
    DH_INFO("smoke4", "perf: world=%d ship=%d items=%d sim=%.2fms props=%d",
            worst_world, worst_ship, worst_items, worst_ms, g->prop_count);

    /* reloading act II is idempotent (the world swap is repeatable) */
    int props1 = g->prop_count, veh1 = c->traffic0 + c->traffic_n;
    game_load_city(g);
    CHECK(g->prop_count == props1, "second city load rebuilds the same %d props", props1);
    CHECK(g->city.veh_count == veh1, "second city load rebuilds the same %d vehicles", veh1);
    CHECK(g->in_vehicle == -1 && g->act == 1, "reload leaves the player on foot in act II");

    /* Beto's ferry: refused while wanted, then a full round trip */
    heat_add_evidence(&g->city.heat, 1.0f);
    step_frames(g, 0, BTN_FERRY, 1);
    CHECK(g->act == 1, "[K] ferry refuses to sail while you're wanted");
    heat_reset(g);
    int cash = g->cash = 77;
    step_frames(g, 0, BTN_FERRY, 1);
    CHECK(g->act == 0 && g->outpost.built, "[K] ferry returns to Isla Sombra (outpost rebuilt)");
    CHECK(g->cash == cash, "cash rides the ferry ($%d)", g->cash);
    CHECK(terrain_height(&g->terrain, g->player.pos.x, g->player.pos.z) > 0.0f,
          "ferry drop point is dry land");
    step_frames(g, 0, 0, 120);
    CHECK(v3_valid(g->player.pos) && !player_is_down(&g->player), "island sim runs after the trip");
    step_frames(g, 0, BTN_FERRY, 1);
    CHECK(g->act == 1 && g->city.built, "[K] ferry back to Meridian works too");
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out);
    dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke4", "════ M4 CITY VERTICAL-SLICE SMOKE ════");

    /* Build under the LOW preset so the perf gate measures the true Spec 15.3
       Low shipping path, then hand the game a loadout via the M3 beach drop
       (the world swap keeps inventory — act II starts armed). */
    settings_defaults(settings());
    settings_load(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    settings()->fov = 85.0f;
    settings_save(settings());

    Game g;
    memset(&g, 0, sizeof g);
    if (!game_init(&g, 320, 180, REND_SOFT, 0xC0FFEEu)) {
        CHECK(0, "game_init for the city slice");
        DH_INFO("smoke4", "──── %d checks, %d failed ────", g_checks, g_failed);
        dh_log_shutdown();
        return 1;
    }
    game_start_play(&g);
    game_outpost_start(&g);            /* grants the loadout on the island */
    game_load_city(&g);                /* act II world swap */

    test_vehicle_table(&g);
    test_city_world(&g);
    test_enter_exit_drive(&g);
    test_traffic(&g);
    test_peds(&g);
    test_heat_unit();
    test_heat_live(&g);
    test_job_hotdog(&g);
    test_dod_chain(&g);
    test_radio_render_perf(&g);

    game_free(&g);

    /* leave the shared settings file on a normal preset for the other smokes */
    settings_apply_preset(settings(), QUALITY_MEDIUM);
    settings_save(settings());

    DH_INFO("smoke4", "──── %d checks, %d failed ────", g_checks, g_failed);
    if (g_failed == 0) printf("M4 CITY SMOKE PASSED: %d/%d checks\n", g_checks, g_checks);
    else printf("M4 CITY SMOKE FAILED: %d/%d checks failed\n", g_failed, g_checks);
    dh_log_shutdown();
    return g_failed ? 1 : 0;
}
