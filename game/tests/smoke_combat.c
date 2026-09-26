/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M2 combat smoke test (headless, deterministic)
   "The kill loop is satisfying" starts as "the kill loop is CORRECT":
     §68 weapon table · zones ×2.5/×1.0/×0.7 · falloff curves · TTK math
     §82 detection meter + FSM PATROL→SUSPICIOUS→SEARCH→COMBAT→DEAD
     §18.4 no NaN damage · hitscan vs world+enemies · reload FSM
     DoD: 30-enemy arena boots, player can kill everything, dead stay dead,
     death→respawn keeps the loadout, arena-clear fires, sim stays fast.
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
#include "../src/game/game.h"
#include "../src/plat/plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke2", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke2", "ok: " __VA_ARGS__); \
} while (0)

#define DT (1.0f / 60.0f)
#define NEAR(a, b, eps) (fabsf((a) - (b)) <= (eps))

/* ═══════════════════════ 1. weapons & ballistics (§68) ══════════════════ */
static void test_weapons(void) {
    int n = weapons_load("game/data/weapons.json");
    CHECK(n >= 4, "weapons.json loaded %d weapons (need >= 4)", n);

    const WeaponDef *w01 = weapon_by_id("W01");
    const WeaponDef *w03 = weapon_by_id("W03");
    const WeaponDef *w05 = weapon_by_id("W05");
    const WeaponDef *w08 = weapon_by_id("W08");
    CHECK(w01 && w03 && w05 && w08, "M2 set present: W01 W03 W05 W08");
    if (!w01 || !w03 || !w05 || !w08) return;
    CHECK(weapon_valid(w01) && weapon_valid(w03) && weapon_valid(w05) && weapon_valid(w08),
          "all four defs pass validation (no NaN balance)");

    /* §68 prose numbers are law */
    CHECK(w01->dmg == 24.f && w01->rpm == 350 && w01->mag == 12 && w01->pellets == 1
          && w01->ammo == AMMO_9MM && w01->price == 400 && NEAR(w01->reload_s, 1.4f, 1e-4f),
          "W01 Culebra: 24dmg 350rpm mag12 reload1.4 9mm $400");
    CHECK(w03->dmg == 16.f && w03->rpm == 900 && w03->mag == 32,
          "W03 Chispa: 16dmg 900rpm mag32");
    CHECK(w05->dmg == 28.f && w05->rpm == 600 && w05->mag == 30
          && w05->fall_a == 40.f && w05->fall_b == 90.f && NEAR(w05->fall_pct, 0.75f, 1e-4f),
          "W05 Libertad: 28dmg 600rpm mag30 falloff 40->90 @75%%");
    CHECK(w08->dmg == 11.f && w08->pellets == 9 && w08->rpm == 70 && w08->mag == 6
          && w08->fall_a == 8.f && w08->fall_b == 18.f && NEAR(w08->fall_pct, 0.35f, 1e-4f),
          "W08 Trueno: 9x11 pellets 70rpm mag6 falloff 8->18 @35%%");

    /* zones (§6.3): head ×2.5, torso ×1.0, limb ×0.7 */
    CHECK(NEAR(weapon_damage(w01, ZONE_HEAD, 10.f), 60.f, 0.01f), "W01 head @10m = 60");
    CHECK(NEAR(weapon_damage(w01, ZONE_TORSO, 10.f), 24.f, 0.01f), "W01 torso @10m = 24");
    CHECK(NEAR(weapon_damage(w01, ZONE_LIMB, 10.f), 16.8f, 0.01f), "W01 limb @10m = 16.8");

    /* falloff: 100% <25m, linear to 70% @50m, flat beyond (§68 prose — the
       §92 matrix rows B-0005/6 say 18/12 and are stale; prose wins per §95) */
    CHECK(NEAR(weapon_falloff(w01, 10.f), 1.0f, 1e-4f), "W01 falloff 100%% inside 25m");
    CHECK(NEAR(weapon_damage(w01, ZONE_TORSO, 37.5f), 20.4f, 0.01f),
          "W01 torso @37.5m = 20.4 (mid-ramp)");
    CHECK(NEAR(weapon_damage(w01, ZONE_TORSO, 50.f), 16.8f, 0.01f), "W01 torso @50m = 16.8");
    CHECK(NEAR(weapon_damage(w01, ZONE_TORSO, 200.f), 16.8f, 0.01f), "W01 falloff floors @70%%");
    CHECK(weapon_falloff(w01, 10.f) >= weapon_falloff(w01, 30.f) &&
          weapon_falloff(w01, 30.f) >= weapon_falloff(w01, 60.f) &&
          weapon_falloff(w01, 60.f) >= weapon_falloff(w01, 300.f),
          "falloff is monotonic non-increasing");

    /* TTK (§68: grunt torso = 5 shots; 4×24=96 < 100) */
    CHECK(4.f * weapon_damage(w01, ZONE_TORSO, 5.f) < 100.f &&
          5.f * weapon_damage(w01, ZONE_TORSO, 5.f) >= 100.f,
          "grunt TTK with W01 torso = 5 shots");
    CHECK(2.f * weapon_damage(w01, ZONE_HEAD, 5.f) >= 100.f,
          "grunt TTK with W01 headshots = 2 shots");

    /* W05 @90m = 21; W08 close = 99 total (just under one-shot — by design,
       the pump ritual is the point), @18m+ = 3.85/pellet */
    CHECK(NEAR(weapon_damage(w05, ZONE_TORSO, 90.f), 21.f, 0.01f), "W05 torso @90m = 21");
    CHECK(NEAR(weapon_damage(w08, ZONE_TORSO, 4.f) * 9.f, 99.f, 0.01f),
          "W08 all pellets torso @4m = 99");
    CHECK(NEAR(weapon_damage(w08, ZONE_TORSO, 30.f), 11.f * 0.35f, 0.01f),
          "W08 pellet beyond 18m = 3.85");

    /* cadence */
    CHECK(NEAR(weapon_shot_interval(w01), 60.f / 350.f, 1e-4f), "W01 interval = 60/350 s");
    CHECK(NEAR(weapon_shot_interval(w03), 60.f / 900.f, 1e-4f), "W03 interval = 60/900 s");

    /* spread posture math: ADS < hip, crouch < stand, move > stand */
    CHECK(weapon_spread_rad(w01, 1.f, 0, 0) < weapon_spread_rad(w01, 0.f, 0, 0),
          "ADS tightens the cone");
    CHECK(weapon_spread_rad(w01, 0.f, 0, 1) < weapon_spread_rad(w01, 0.f, 0, 0),
          "crouch tightens hipfire");
    CHECK(weapon_spread_rad(w01, 0.f, 1, 0) > weapon_spread_rad(w01, 0.f, 0, 0),
          "movement widens hipfire");

    /* recoil: deterministic pattern, vertical kick positive, shotgun > pistol */
    Vec3 r1 = weapon_recoil(w01, 0.f, 3);
    Vec3 r2 = weapon_recoil(w01, 0.f, 3);
    CHECK(r1.y > 0.f && r1.x == r2.x && r1.y == r2.y, "recoil deterministic, kicks up");
    CHECK(weapon_recoil(w08, 0.f, 0).y > weapon_recoil(w01, 0.f, 0).y,
          "Trueno kicks harder than Culebra");
    CHECK(weapon_recoil(w01, 1.f, 0).y < weapon_recoil(w01, 0.f, 0).y, "ADS dampens recoil");

    /* NaN guards (§18.4: no NaN damage bugs, ever) */
    float nan = (float)NAN;
    CHECK(isfinite(weapon_damage(w01, ZONE_TORSO, nan)) &&
          weapon_damage(w01, ZONE_TORSO, nan) == 0.f, "NaN distance deals 0, stays finite");
    CHECK(weapon_falloff(w01, nan) == 0.f, "NaN falloff = 0");
    CHECK(weapon_damage(NULL, ZONE_HEAD, 5.f) == 0.f, "NULL weapon deals 0");
    WeaponDef bad = *w01; bad.dmg = nan;
    CHECK(!weapon_valid(&bad), "validation rejects NaN damage");
    WeaponDef bad2 = *w01; bad2.zone_mul[ZONE_HEAD] = 0.5f;
    CHECK(!weapon_valid(&bad2), "validation rejects head < torso multiplier");

    /* ammo helpers + honest fallback */
    CHECK(ammo_from_str("556") == AMMO_556 && ammo_from_str("nope") == AMMO_N &&
          !strcmp(ammo_name(AMMO_12G), "12g"), "ammo string helpers round-trip");
    int nf = weapons_load("/nonexistent/path/weapons.json");
    const WeaponDef *fb = weapon_by_id("W01");
    CHECK(nf >= 4 && fb && fb->dmg == 24.f, "missing file falls back to embedded table");
    weapons_load("game/data/weapons.json");   /* restore for the integration pass */
}

/* ═══════════════════════ 2. rays vs the world ═══════════════════════════ */
static void test_rays(void) {
    Terrain t; ObstacleSet ob;
    terrain_init(&t, 65, 256.0f, 0xC0FFEEu);
    terrain_flatten(&t, v3(128.f, 0.f, 128.f), 110.f, 110.f, 0.f, 6.f);
    obstacles_init(&ob);
    obstacles_add(&ob, v3(10.f, 0.f, 10.f), v3(12.f, 2.f, 12.f), 0);   /* solid */
    obstacles_add(&ob, v3(20.f, 0.f, 10.f), v3(22.f, 2.f, 12.f), 1);   /* ladder: bullets pass */

    float th; Vec3 nh;
    CHECK(obstacles_ray_hit(&ob, v3(11.f, 1.f, 0.f), v3(0.f, 0.f, 1.f), 50.f, &th, &nh)
          && NEAR(th, 10.f, 0.01f) && nh.z < -0.5f,
          "ray hits box face at t=10 with -z normal");
    CHECK(!obstacles_ray_hit(&ob, v3(11.f, 1.f, 0.f), v3(0.f, 0.f, -1.f), 50.f, &th, &nh),
          "ray away from the box misses");
    CHECK(!obstacles_ray_hit(&ob, v3(11.f, 1.f, 0.f), v3(0.f, 0.f, 1.f), 5.f, &th, &nh),
          "tmax shorter than the box = miss");
    CHECK(obstacles_ray_hit(&ob, v3(11.f, 1.f, 11.f), v3(0.f, 0.f, 1.f), 50.f, &th, &nh)
          && th == 0.f, "ray starting inside a box hits at t=0");
    CHECK(!obstacles_ray_hit(&ob, v3(21.f, 1.f, 0.f), v3(0.f, 0.f, 1.f), 50.f, &th, &nh),
          "ladder volumes do not stop bullets");
    CHECK(!obstacles_segment_clear(&ob, v3(11.f, 1.f, 0.f), v3(11.f, 1.f, 20.f)),
          "segment through the box is blocked");
    CHECK(obstacles_segment_clear(&ob, v3(11.f, 3.f, 0.f), v3(11.f, 3.f, 20.f)),
          "segment over the box is clear");
    terrain_free(&t);
}

/* ═══════════════════════ 3. enemy AI (§82 / §65) ════════════════════════ */
static Terrain  a_t;
static ObstacleSet a_ob;
static int ai_world_ready = 0;
static void ai_world(void) {
    if (ai_world_ready) return;
    terrain_init(&a_t, 65, 256.0f, 0xC0FFEEu);
    terrain_flatten(&a_t, v3(128.f, 0.f, 128.f), 110.f, 110.f, 0.f, 6.f);
    ai_world_ready = 1;
}
static void ai_reset(void) { obstacles_init(&a_ob); }

static void test_ai(void) {
    ai_world(); ai_reset();
    EnemySet s; enemies_init(&s);

    /* §65 health table */
    int ig = enemies_spawn(&s, v3(128.f, 0.f, 120.f), EN_GRUNT, 0,
                           v3(128.f, 0.f, 120.f), v3(128.f, 0.f, 120.f));
    int ib = enemies_spawn(&s, v3(140.f, 0.f, 120.f), EN_BRUISER, 0,
                           v3(140.f, 0.f, 120.f), v3(140.f, 0.f, 120.f));
    int io = enemies_spawn(&s, v3(150.f, 0.f, 120.f), EN_OFFICER, 0,
                           v3(150.f, 0.f, 120.f), v3(150.f, 0.f, 120.f));
    CHECK(ig == 0 && ib == 1 && io == 2 && s.count == 3, "spawn returns indices");
    CHECK(s.v[ig].health == 100.f && s.v[ib].health == 160.f && s.v[io].health == 130.f,
          "grunt 100 / bruiser 160 / officer 130 HP (§65)");
    CHECK(enemies_spawn(&s, v3(0.f, 0.f, 0.f), 7, 0, v3(0.f,0.f,0.f), v3(0.f,0.f,0.f)) < 0,
          "invalid archetype rejected");

    /* detection rate physics: inverse-square, stance & light multipliers */
    EnemyView pv; memset(&pv, 0, sizeof pv);
    pv.eye = v3(128.f, 1.7f, 100.f); pv.alive = 1; pv.light = 1.f;
    Enemy probe = s.v[ig];
    probe.pos = v3(128.f, 0.f, 120.f);
    float r20 = enemy_detect_rate(&probe, &pv);
    probe.pos = v3(128.f, 0.f, 140.f);
    float r40 = enemy_detect_rate(&probe, &pv);
    CHECK(r20 > 3.4f * r40 && r20 < 4.6f * r40, "detection follows inverse-square (~4x)");
    probe.pos = v3(128.f, 0.f, 120.f);
    EnemyView pc = pv; pc.crouched = 1;
    CHECK(enemy_detect_rate(&probe, &pc) < 0.5f * r20, "crouching halves detection rate");
    EnemyView pm = pv; pm.moving = 1;
    CHECK(enemy_detect_rate(&probe, &pm) > r20, "moving raises detection rate");
    EnemyView po = pv; po.eye = v3(150.f, 1.7f, 100.f);
    CHECK(enemy_detect_rate(&s.v[io], &po) > enemy_detect_rate(&s.v[ig], &pv) * 0.9f,
          "officers spot you at least as fast as grunts at equal range");

    /* meter → FSM: PATROL → SUSPICIOUS (0.35) → COMBAT (0.8) */
    enemies_init(&s);
    enemies_spawn(&s, v3(128.f, 0.f, 120.f), EN_GRUNT, 0,
                  v3(128.f, 0.f, 120.f), v3(132.f, 0.f, 120.f));
    pv.eye = v3(128.f, 1.7f, 100.f);
    for (int i = 0; i < 15; i++) enemies_update(&s, &pv, DT, &a_t, &a_ob);
    CHECK(s.v[0].state == EN_SUSPICIOUS, "meter >= 0.35 -> SUSPICIOUS (got %s)",
          enemy_state_name(s.v[0].state));
    for (int i = 0; i < 60; i++) enemies_update(&s, &pv, DT, &a_t, &a_ob);
    CHECK(s.v[0].state == EN_COMBAT, "meter >= 0.8 -> COMBAT (got %s)",
          enemy_state_name(s.v[0].state));

    /* COMBAT enemies shoot, with bounded accuracy */
    int saw_shot = 0; float acc_seen = 1.f;
    for (int i = 0; i < 180; i++) {
        enemies_update(&s, &pv, DT, &a_t, &a_ob);
        if (s.v[0].shot_this_frame) { saw_shot = 1; acc_seen = s.v[0].shot_acc; }
    }
    CHECK(saw_shot, "COMBAT enemy with LOS opens fire");
    CHECK(acc_seen >= 0.f && acc_seen <= 0.95f, "enemy accuracy bounded [0, 0.95]");

    /* team share ≤ 8 m (§82): squad-mate WITHOUT its own LOS still wakes up
       when the enemy beside it transitions to COMBAT */
    ai_reset();
    obstacles_add(&a_ob, v3(128.6f, 0.f, 108.f), v3(133.f, 5.f, 112.f), 0);
    enemies_init(&s);
    enemies_spawn(&s, v3(128.f, 0.f, 120.f), EN_GRUNT, 0,     /* clear LOS down x=128 */
                  v3(128.f, 0.f, 120.f), v3(128.f, 0.f, 120.f));
    enemies_spawn(&s, v3(131.f, 0.f, 120.f), EN_GRUNT, 0,     /* wall blocks its view */
                  v3(131.f, 0.f, 120.f), v3(131.f, 0.f, 120.f));
    CHECK(!obstacles_segment_clear(&a_ob, v3(131.f, 1.55f, 120.f), pv.eye),
          "test rig: squad-mate really has no LOS");
    for (int i = 0; i < 90 && s.v[1].state != EN_SEARCH && s.v[1].state != EN_COMBAT; i++)
        enemies_update(&s, &pv, DT, &a_t, &a_ob);
    CHECK(s.v[0].state == EN_COMBAT, "sharer reached COMBAT first");
    CHECK(s.v[1].state == EN_SEARCH || s.v[1].state == EN_COMBAT,
          "squad-mate within 8m shares detection (got %s)", enemy_state_name(s.v[1].state));
    ai_reset();

    /* LOS blocked → meter decays, stays PATROL (§82 decay 0.2/s) */
    ai_reset();
    obstacles_add(&a_ob, v3(124.f, 0.f, 109.f), v3(132.f, 5.f, 111.f), 0); /* wall */
    enemies_init(&s);
    enemies_spawn(&s, v3(128.f, 0.f, 120.f), EN_GRUNT, 0,
                  v3(128.f, 0.f, 120.f), v3(128.f, 0.f, 120.f));
    for (int i = 0; i < 180; i++) enemies_update(&s, &pv, DT, &a_t, &a_ob);
    CHECK(s.v[0].state == EN_PATROL && s.v[0].meter < 0.2f,
          "no LOS -> stays PATROL, meter decays (got %s, %.2f)",
          enemy_state_name(s.v[0].state), (double)s.v[0].meter);

    /* gunshot noise alerts through walls inside 40 m */
    EnemyView pn = pv; pn.noise = 1.f;
    enemies_update(&s, &pn, DT, &a_t, &a_ob);
    CHECK(s.v[0].state == EN_SEARCH, "gunshot noise -> SEARCH even without LOS");
    ai_reset();

    /* damage & death: 5 torso hits of 24 kill a grunt; dead stay dead */
    enemies_init(&s);
    enemies_spawn(&s, v3(128.f, 0.f, 120.f), EN_GRUNT, 0,
                  v3(128.f, 0.f, 120.f), v3(128.f, 0.f, 120.f));
    Enemy *e = &s.v[0];
    for (int i = 0; i < 4; i++)
        CHECK(enemy_apply_damage(e, 24.f, ZONE_TORSO) == 0, "hit %d: grunt still up (hp %.0f)",
              i + 1, (double)e->health);
    CHECK(enemy_apply_damage(e, 24.f, ZONE_TORSO) == 1, "5th torso hit kills (TTK=5)");
    CHECK(e->health == 0.f && e->state == EN_DEAD, "corpse at 0 hp in DEAD state");
    int alive0 = 0;
    for (int i = 0; i < 30; i++) { enemies_update(&s, &pv, DT, &a_t, &a_ob); }
    alive0 = s.alive_count;
    CHECK(e->state == EN_DEAD && alive0 == 0, "dead stay dead across updates");
    CHECK(enemy_apply_damage(e, 50.f, ZONE_HEAD) == 0, "shooting a corpse does nothing");
    CHECK(enemy_apply_damage(e, (float)NAN, ZONE_HEAD) == 0 && e->health == 0.f,
          "NaN damage is rejected (18.4)");

    /* accuracy falls with distance; crouching helps you survive */
    EnemyView q = pv;
    float a10 = enemy_accuracy(e, &q, 10.f);
    float a50 = enemy_accuracy(e, &q, 50.f);
    EnemyView qc = q; qc.crouched = 1;
    CHECK(a10 > a50, "enemy accuracy falls with distance");
    CHECK(enemy_accuracy(e, &qc, 10.f) < a10, "crouching lowers enemy accuracy");

    CHECK(enemy_shot_damage(EN_GRUNT) == 12.f && enemy_shot_damage(EN_BRUISER) == 22.f &&
          enemy_shot_damage(EN_OFFICER) == 16.f, "enemy damage per shot: 12/22/16");

    /* determinism: same inputs → same positions */
    EnemySet d1, d2;
    enemies_init(&d1); enemies_init(&d2);
    for (int k = 0; k < 4; k++) {
        enemies_spawn(&d1, v3(120.f + 6.f*k, 0.f, 120.f), k % 3, 0,
                      v3(120.f + 6.f*k, 0.f, 120.f), v3(124.f + 6.f*k, 0.f, 126.f));
        enemies_spawn(&d2, v3(120.f + 6.f*k, 0.f, 120.f), k % 3, 0,
                      v3(120.f + 6.f*k, 0.f, 120.f), v3(124.f + 6.f*k, 0.f, 126.f));
    }
    for (int i = 0; i < 200; i++) {
        enemies_update(&d1, &pv, DT, &a_t, &a_ob);
        enemies_update(&d2, &pv, DT, &a_t, &a_ob);
    }
    int same = 1;
    for (int k = 0; k < 4; k++)
        if (!NEAR(d1.v[k].pos.x, d2.v[k].pos.x, 1e-5f) ||
            !NEAR(d1.v[k].pos.z, d2.v[k].pos.z, 1e-5f)) same = 0;
    CHECK(same, "AI is deterministic (identical runs, identical positions)");
}

/* ═══════════════════════ 4. integration: the arena DoD ══════════════════ */
static void step_frames(Game *g, uint32_t buttons, uint32_t pressed, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        in.buttons = buttons; in.pressed = (i == 0) ? pressed : 0u;
        game_frame(g, &in, DT);
    }
}

static void test_integration(void) {
    Game g;
    if (!game_init(&g, 320, 180, REND_SOFT, 0xC0FFEEu)) {
        CHECK(0, "game_init for combat integration");
        return;
    }
    CHECK(weapons_count() >= 4, "game_init loaded the weapon table (%d)", weapons_count());

    /* ── DoD: a 30-enemy arena boots ── */
    game_arena_start(&g, 30);
    CHECK(g.enemies.count == 30 && g.arena_total == 30 && g.arena_active == 1,
          "arena spawns 30 hostiles");
    CHECK(g.mode == GM_PLAY && g.player.health == 100.f, "arena drops the player in PLAY at full hp");
    CHECK(g.wpn_def[0] >= 0 && g.wpn_def[1] >= 0 && g.wpn_def[2] >= 0,
          "loadout granted: 3 slots (W01/W05/W08)");
    CHECK(g.wpn_mag[0] == 12 && g.ammo[AMMO_9MM] >= 48, "W01 starts loaded: mag 12, reserve >= 48");
    int bruisers = 0, officers = 0;
    for (int i = 0; i < g.enemies.count; i++) {
        if (g.enemies.v[i].arch == EN_BRUISER) bruisers++;
        if (g.enemies.v[i].arch == EN_OFFICER) officers++;
    }
    CHECK(bruisers > 0 && officers > 0, "arena mixes archetypes (%d bruisers, %d officers)",
          bruisers, officers);
    CHECK(g.player.pos.z < g.arena_center.z - 40.f, "player starts at the south gate");

    /* ── duel: trim to 3 grunts, auto-aim bot guns them down ── */
    for (int i = 3; i < g.enemies.count; i++)
        enemy_apply_damage(&g.enemies.v[i], 999.f, ZONE_TORSO);
    const Vec3 A = g.arena_center;
    for (int k = 0; k < 3; k++) {
        Enemy *e = &g.enemies.v[k];
        e->pos = v3(A.x + 8.f + 3.f * k, 0.f, A.z - 26.f + 3.f * k);
        e->pos.y = terrain_height(&g.terrain, e->pos.x, e->pos.z);
        e->state = EN_PATROL; e->meter = 0.f;
        e->patrol_a = e->patrol_b = e->pos;
        e->burst_left = 4; e->fire_cd = 0.f;
    }
    int saw_tracer = 0, saw_hitmark = 0, saw_fx = 0, saw_hit = 0;
    for (int f = 0; f < 480 && g.kills < 3; f++) {
        /* aim at the nearest living duelist (upper torso) */
        int best = -1; float bd = 1e9f;
        for (int k = 0; k < 3; k++)
            if (g.enemies.v[k].state != EN_DEAD) {
                float d = v3_dist(g.player.pos, g.enemies.v[k].pos);
                if (d < bd) { bd = d; best = k; }
            }
        if (best >= 0) {
            const Enemy *e = &g.enemies.v[best];
            g.player.yaw = atan2f(e->pos.x - g.player.pos.x, e->pos.z - g.player.pos.z);
            float eye = g.player.pos.y + g.player.eye;
            float dxz = dh_maxf(v3_dist_xz(g.player.pos, e->pos), 0.1f);
            g.player.pitch = atan2f((e->pos.y + 1.55f) - eye, dxz);
        }
        PlatInput in; memset(&in, 0, sizeof(in));
        in.buttons = BTN_FIRE | BTN_CROUCH;
        game_frame(&g, &in, DT);
        for (int i = 0; i < g.tracers.count; i++)
            if (g.tracers.v[i].life > 0.f) saw_tracer = 1;
        if (g.hitmark_t > 0.f) saw_hitmark = 1;
        if (g.fx_count > 0) saw_fx = 1;
        for (int k = 0; k < 3; k++)
            if (g.enemies.v[k].health < g.enemies.v[k].health_max ||
                g.enemies.v[k].state == EN_DEAD) saw_hit = 1;
    }
    CHECK(g.kills == 3, "player killed all 3 duelists (kills=%d)", g.kills);
    CHECK(saw_hit, "hits registered on enemies (hp dropped)");
    CHECK(saw_tracer, "tracers rendered for shots");
    CHECK(saw_hitmark, "hitmarker fired on hit");
    CHECK(saw_fx, "impact particles spawned");
    CHECK(g.player.health > 0.f, "player survived the duel (hp %.0f)", (double)g.player.health);

    /* arena clear */
    step_frames(&g, 0, 0, 3);
    CHECK(g.arena_active == 0, "arena-clear fires when the last hostile dies");
    CHECK(g.enemies.alive_count == 0, "alive_count tracks the wipe");
    int all_dead = 1;
    for (int i = 0; i < g.enemies.count; i++)
        if (g.enemies.v[i].state != EN_DEAD) all_dead = 0;
    step_frames(&g, 0, 0, 120);
    int still_dead = 1;
    for (int i = 0; i < g.enemies.count; i++)
        if (g.enemies.v[i].state != EN_DEAD) still_dead = 0;
    CHECK(all_dead && still_dead, "corpses stay dead (no respawn cheat)");

    /* ── reload FSM ── */
    step_frames(&g, 0, 0, 120);                     /* let any reload finish */
    int mag_before_total = g.wpn_mag[0] + g.ammo[AMMO_9MM];
    g.wpn_mag[0] = 3;
    g.ammo[AMMO_9MM] = mag_before_total - 3;
    int r0 = g.ammo[AMMO_9MM];
    int saw_reloading = 0;
    PlatInput rin; memset(&rin, 0, sizeof rin);
    rin.pressed = BTN_RELOAD;
    game_frame(&g, &rin, DT);
    for (int i = 0; i < 100; i++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        game_frame(&g, &in, DT);
        if (g.reloading) saw_reloading = 1;
    }
    CHECK(saw_reloading, "R starts the reload (reloading flag set)");
    CHECK(g.reloading == 0 && g.wpn_mag[0] == 12, "reload completes: mag back to 12 (got %d)",
          g.wpn_mag[0]);
    CHECK(g.ammo[AMMO_9MM] == r0 - 9, "reserve paid exactly 9 rounds (got %d, want %d)",
          g.ammo[AMMO_9MM], r0 - 9);
    CHECK(g.wpn_mag[0] + g.ammo[AMMO_9MM] == mag_before_total, "ammo is conserved (no dupe)");

    /* ── slot switching ── */
    PlatInput sin2; memset(&sin2, 0, sizeof sin2); sin2.pressed = BTN_SLOT2;
    game_frame(&g, &sin2, DT);
    CHECK(g.wpn_slot == 1, "key 2 -> slot 2 (Libertad)");
    PlatInput sin3; memset(&sin3, 0, sizeof sin3); sin3.pressed = BTN_SLOT3;
    game_frame(&g, &sin3, DT);
    CHECK(g.wpn_slot == 2, "key 3 -> slot 3 (Trueno)");
    PlatInput sin1; memset(&sin1, 0, sizeof sin1); sin1.pressed = BTN_SLOT1;
    game_frame(&g, &sin1, DT);
    CHECK(g.wpn_slot == 0, "key 1 -> slot 1 (Culebra)");

    /* ── dry fire with no ammo anywhere: no crash, no NaN ── */
    g.wpn_mag[0] = 0;
    int keep9 = g.ammo[AMMO_9MM]; g.ammo[AMMO_9MM] = 0;
    step_frames(&g, BTN_FIRE, 0, 60);
    CHECK(g.wpn_mag[0] == 0 && g.ammo[AMMO_9MM] == 0 && !g.reloading,
          "dry fire with empty reserve: no phantom ammo, no stuck reload");
    CHECK(v3_valid(g.player.pos) && isfinite(g.player.health), "player state stays finite");
    g.ammo[AMMO_9MM] = keep9;

    /* ── enemies hurt the player; death → respawn keeps the loadout ── */
    game_arena_start(&g, 6);
    int inv0 = g.wpn_mag[0], inv1 = g.wpn_mag[1], inv2 = g.wpn_mag[2];
    int res9 = g.ammo[AMMO_9MM];
    Enemy *e0 = &g.enemies.v[0];
    e0->pos = v3(g.player.pos.x + 2.f, 0.f, g.player.pos.z + 8.f);
    e0->pos.y = terrain_height(&g.terrain, e0->pos.x, e0->pos.z);
    e0->state = EN_COMBAT; e0->meter = 1.2f; e0->burst_left = 4; e0->fire_cd = 0.f;
    e0->lkp = v3(g.player.pos.x, g.player.pos.y + 1.7f, g.player.pos.z);
    int saw_hurt = 0, saw_flash = 0, frames_to_down = -1;
    for (int f = 0; f < 900; f++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        game_frame(&g, &in, DT);
        if (g.player.health < 100.f) saw_hurt = 1;
        if (g.damage_flash > 0.f) saw_flash = 1;
        if (player_is_down(&g.player)) { frames_to_down = f; break; }
    }
    CHECK(saw_hurt, "enemy fire damages the player");
    CHECK(saw_flash, "damage flash feedback fired");
    CHECK(frames_to_down >= 0, "player can actually die (down at frame %d)", frames_to_down);
    int kills_at_death = g.kills;
    int alive_at_death = g.enemies.alive_count;
    CHECK(g.respawn_t > 0.f, "death starts the respawn timer");
    int respawned_at = -1;
    for (int f = 0; f < 300; f++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        game_frame(&g, &in, DT);
        if (!player_is_down(&g.player)) { respawned_at = f; break; }
    }
    CHECK(respawned_at >= 0 && g.player.health == 100.f, "respawn at checkpoint with full hp");
    CHECK(g.wpn_mag[0] == inv0 && g.wpn_mag[1] == inv1 && g.wpn_mag[2] == inv2 &&
          g.ammo[AMMO_9MM] == res9,
          "respawn keeps the loadout (mags + reserve untouched)");
    CHECK(g.kills == kills_at_death, "kill count persists through death");
    CHECK(g.enemies.alive_count == alive_at_death, "dead enemies stay dead through respawn");
    CHECK(fabsf(g.player.pos.z - (g.arena_center.z - 46.f)) < 3.f,
          "respawn point is the arena checkpoint");

    /* ── pickups ── */
    int ammo_pk = -1, health_pk = -1;
    for (int i = 0; i < g.pickups.count; i++) {
        if (!g.pickups.v[i].live) continue;
        if (g.pickups.v[i].kind == PK_AMMO && ammo_pk < 0) ammo_pk = i;
        if (g.pickups.v[i].kind == PK_HEALTH && health_pk < 0) health_pk = i;
    }
    CHECK(ammo_pk >= 0 && health_pk >= 0, "arena carries health + ammo pickups");
    if (ammo_pk >= 0) {
        int before = g.ammo[AMMO_556];
        g.player.pos = g.pickups.v[ammo_pk].pos;
        g.player.pos.y = terrain_height(&g.terrain, g.player.pos.x, g.player.pos.z) + 0.05f;
        step_frames(&g, 0, 0, 2);
        CHECK(g.ammo[AMMO_556] == before + 60 && !g.pickups.v[ammo_pk].live,
              "ammo pickup grants exactly +60 5.56 and despawns");
    }
    if (health_pk >= 0) {
        g.player.health = 60.f;
        g.player.pos = g.pickups.v[health_pk].pos;
        g.player.pos.y = terrain_height(&g.terrain, g.player.pos.x, g.player.pos.z) + 0.05f;
        step_frames(&g, 0, 0, 2);
        CHECK(g.player.health == 100.f && !g.pickups.v[health_pk].live,
              "medkit heals 60 -> 100 and despawns");
    }

    /* ── perf sanity: full 30-enemy arena, sim-only frame cost ── */
    game_arena_start(&g, 30);
    Vec3 first[4];                       /* fresh spawn positions, for determinism */
    for (int i = 0; i < 4; i++) first[i] = g.enemies.v[i].pos;
    clock_t t0 = clock();
    step_frames(&g, 0, 0, 300);
    double sim_ms = 1000.0 * (double)(clock() - t0) / (double)CLOCKS_PER_SEC / 300.0;
    CHECK(sim_ms < 8.0, "sim with 30 enemies costs %.2f ms/frame (budget: a slice of 16.6)",
          sim_ms);

    /* ── render smoke: enemies/pickups/tracers submit without crashing ── */
    step_frames(&g, BTN_FIRE, 0, 10);
    game_render(&g);
    RendState *rs = rend();
    CHECK(rs && rs->items > 0 && rs->draw_calls > 0, "renderer submitted %d items / %d draws",
          rs ? rs->items : -1, rs ? rs->draw_calls : -1);

    /* ── NaN sweep over everything the combat layer touched (§18.4) ── */
    int finite = v3_valid(g.player.pos) && v3_valid(g.player.vel) &&
                 isfinite(g.player.health) && isfinite(g.player.pitch) && isfinite(g.player.yaw) &&
                 isfinite(g.ads_k) && g.ads_k >= 0.f && g.ads_k <= 1.f;
    for (int i = 0; i < g.enemies.count && finite; i++)
        finite = v3_valid(g.enemies.v[i].pos) && isfinite(g.enemies.v[i].health) &&
                 isfinite(g.enemies.v[i].meter);
    for (int i = 0; i < WPN_SLOT_MAX && finite; i++)
        finite = g.wpn_mag[i] >= 0 && g.wpn_mag[i] <= 32;
    CHECK(finite, "post-combat NaN sweep: every value finite");

    /* ── determinism: same seed → same arena (positions captured pre-sim) ── */
    game_free(&g);
    if (!game_init(&g, 320, 180, REND_SOFT, 0xC0FFEEu)) { CHECK(0, "re-init"); return; }
    game_arena_start(&g, 30);
    int det = 1;
    for (int i = 0; i < 4; i++)
        if (!NEAR(first[i].x, g.enemies.v[i].pos.x, 1e-4f) ||
            !NEAR(first[i].z, g.enemies.v[i].pos.z, 1e-4f)) det = 0;
    CHECK(det, "arena layout is reproducible from the seed");

    game_free(&g);
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out);
    dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke2", "── M2 combat smoke test ──");

    test_weapons();
    test_rays();
    test_ai();
    test_integration();

    DH_INFO("smoke2", "── %d checks, %d failed ──", g_checks, g_failed);
    if (g_failed == 0) printf("M2 COMBAT SMOKE PASSED: %d/%d checks\n", g_checks, g_checks);
    else printf("M2 COMBAT SMOKE FAILED: %d/%d checks failed\n", g_failed, g_checks);
    dh_log_shutdown();
    return g_failed ? 1 : 0;
}
