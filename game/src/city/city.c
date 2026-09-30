/* ============================================================================
   DIVIDED HORIZON — Horizon Engine
   src/city/city.c — Meridian City act (M4).
   8 blocks + harbor stub · 15 traffic cars on 3 loops · 30 peds · heat 0→5
   with pursuing cruisers (heli at 4★) · 1 odd job · hot-dog stand economy.
   Static geometry (buildings, docks, walls, minimap paint) is built by
   game.c from the layout this file computes — city.c owns the living city.
   ========================================================================== */
#include "../city/city.h"
#include "../game/game.h"
#include "../core/dh_log.h"
#include "../rend/rend.h"
#include "../rend/proc_tex.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define CITY_PI 3.14159265358979f
#define CITY_LANE 3.4f          /* right-hand lane offset from road centerline */
#define CITY_BOUNDS_MIN 368.0f
#define CITY_BOUNDS_MAX 632.0f

/* ── radio (audio is M7; this is the honest stub: station names on the HUD) ── */
static const char *STATIONS[CITY_STATIONS] = {
    "COSTERA 98.5", "ISLA SON 101.3", "MERIDIAN TALK 1240", "DUB DOCK 88.1", "RADIO OFF"
};
const char *city_radio_name(int s) {
    s %= CITY_STATIONS;
    if (s < 0) s += CITY_STATIONS;
    return STATIONS[s];
}

static const uint32_t CIVIL_COL[6] = {
    0xFF8A6E4Au, 0xFF4A4A8Au, 0xFF4A7A5Au, 0xFFD8D0B8u, 0xFF5A4040u, 0xFF9A7A3Au
};
static const uint32_t SHIRT_COL[6] = {
    0xFF5068A0u, 0xFF408060u, 0xFFA05050u, 0xFFC8B870u, 0xFF7050A0u, 0xFFB0B0B0u
};

static float dist2d(Vec3 a, Vec3 b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}
static float ang_diff(float a, float b) {
    float d = fmodf(a - b + CITY_PI, 2.0f * CITY_PI);
    if (d < 0.0f) d += 2.0f * CITY_PI;
    return d - CITY_PI;
}
static int def_find(const City *c, const char *id) {
    for (int i = 0; i < c->def_count; i++)
        if (!strcmp(c->defs[i].id, id)) return i;
    return 0;
}
static float rect_len(const CityRect *r) {
    return 2.0f * ((r->x1 - r->x0) + (r->z1 - r->z0));
}
/* Position + heading at arc length s on a rectangular loop (CW from x0,z0).
   Engine yaw convention: forward = (sin yaw, cos yaw). */
static void rect_at(const CityRect *r, float s, Vec3 *pos, float *yaw) {
    float per = rect_len(r);
    s = fmodf(s, per);
    if (s < 0.0f) s += per;
    float w = r->x1 - r->x0, d = r->z1 - r->z0;
    if (s < w)              { pos->x = r->x0 + s;              pos->z = r->z0; *yaw = CITY_PI * 0.5f; }
    else if (s < w + d)     { pos->x = r->x1;                  pos->z = r->z0 + (s - w);           *yaw = 0.0f; }
    else if (s < 2*w + d)   { pos->x = r->x1 - (s - w - d);    pos->z = r->z1; *yaw = -CITY_PI * 0.5f; }
    else                    { pos->x = r->x0;                  pos->z = r->z1 - (s - 2*w - d);     *yaw = CITY_PI; }
    pos->y = CITY_GROUND_Y;
}
/* Right-hand lane offset for a heading. */
static void lane_offset(float yaw, float off, Vec3 *pos) {
    pos->x += cosf(yaw) * off;
    pos->z += -sinf(yaw) * off;
}

/* ══════════════════════════════ layout ══════════════════════════════════ */

void city_init(City *c, struct Game *g) {
    memset(c, 0, sizeof *c);
    c->built = 1;
    c->def_count = vehicle_defs_load(c->defs, NULL);
    if (c->def_count < 3) DH_WARN("city", "fewer than 3 vehicle defs - slice DoD needs V01/V02/V06");

    c->avenues[0] = 380.0f; c->avenues[1] = 500.0f; c->avenues[2] = 620.0f;
    c->streets[0] = 380.0f; c->streets[1] = 500.0f; c->streets[2] = 620.0f;
    c->block_rx = 25.0f; c->block_rz = 53.0f;

    /* 4 superblocks split by alleys at x=440/560 → 8 blocks */
    {
        int bi = 0;
        const float cx[4] = { 440.0f, 560.0f, 440.0f, 560.0f };
        const float cz[4] = { 440.0f, 440.0f, 560.0f, 560.0f };
        for (int i = 0; i < 4; i++) {
            c->blocks[bi++] = v2(cx[i] - 27.0f, cz[i]);
            c->blocks[bi++] = v2(cx[i] + 27.0f, cz[i]);
        }
    }
    for (int i = 0; i < CITY_BLOCKS; i++) {
        c->sidewalk[i].x0 = c->blocks[i].x - c->block_rx - 2.5f;
        c->sidewalk[i].x1 = c->blocks[i].x + c->block_rx + 2.5f;
        c->sidewalk[i].z0 = c->blocks[i].y - c->block_rz - 2.5f;
        c->sidewalk[i].z1 = c->blocks[i].y + c->block_rz + 2.5f;
    }

    /* traffic loops: outer ring + NW square + SE square (opposite traffic on
       avenue 500 separates via the lane offset) */
    c->loops[0] = (CityRect){ 380.0f, 380.0f, 620.0f, 620.0f };
    c->loops[1] = (CityRect){ 380.0f, 380.0f, 500.0f, 500.0f };
    c->loops[2] = (CityRect){ 500.0f, 500.0f, 620.0f, 620.0f };
    for (int i = 0; i < 3; i++) c->loop_len[i] = rect_len(&c->loops[i]);

    heat_init(&c->heat);

    c->job_board     = v3(500.0f, CITY_GROUND_Y, 648.0f);
    c->hotdog_stand  = v3(472.0f, CITY_GROUND_Y, 511.0f);
    c->player_spawn  = v3(500.0f, CITY_GROUND_Y, 636.0f);
    c->job.pickup    = v3(606.0f, CITY_GROUND_Y, 394.0f);
    c->job.dropoff   = c->hotdog_stand;
    c->job.reward    = 400;
    c->job.active    = 0;

    /* ── parked cars (8, curbside; one Sirena so the +2★ steal is playable) ── */
    {
        struct { const char *id; float x, z, yaw; } P[12] = {
            {"V01", 389.5f, 420.0f, 0.0f},
            {"V02", 389.5f, 545.0f, 0.0f},
            {"V05", 509.5f, 420.0f, 0.0f},
            {"V07", 509.5f, 560.0f, CITY_PI},
            {"V01", 610.5f, 450.0f, CITY_PI},
            {"V05", 610.5f, 580.0f, CITY_PI},
            {"V06", 490.5f, 512.0f, CITY_PI},   /* cruiser by the hot-dog corner */
            {"V02", 442.0f, 629.5f, CITY_PI * 0.5f},
            /* M22: van + pickup curbside, Limpio armored van at the harbor lot,
               a cult buggy the Cosecha smuggled across (steal-only classes) */
            {"V03", 389.5f, 470.0f, 0.0f},
            {"V04", 610.5f, 520.0f, CITY_PI},
            {"V11", 509.5f, 470.0f, 0.0f},
            {"V12", 558.0f, 629.5f, CITY_PI * 0.5f},
        };
        for (int i = 0; i < 12 && c->veh_count < VEHICLES_MAX; i++) {
            Vehicle *v = &c->veh[c->veh_count++];
            memset(v, 0, sizeof *v);
            v->def = def_find(c, P[i].id);
            v->pos = v3(P[i].x, CITY_GROUND_Y, P[i].z);
            v->yaw = P[i].yaw;
            v->state = VS_PARKED;
            v->occupant = -1;
            v->loop = -1;
        }
    }

    /* ── traffic (15 cars: 7 outer ring, 4 NW, 4 SE) ── */
    c->traffic0 = c->veh_count;
    {
        const int loop_of[15] = { 0,0,0,0,0,0,0, 1,1,1,1, 2,2,2,2 };
        const int per_loop[3] = { 7, 4, 4 };
        int seen[3] = { 0, 0, 0 };
        for (int i = 0; i < 15 && c->veh_count < VEHICLES_MAX; i++) {
            Vehicle *v = &c->veh[c->veh_count++];
            memset(v, 0, sizeof *v);
            v->def = def_find(c, (i % 2) ? "V05" : "V01");
            v->state = VS_TRAFFIC;
            v->occupant = 1;                 /* anonymous driver (flees on carjack) */
            v->loop = loop_of[i];
            int li = v->loop;
            v->loop_s = c->loop_len[li] * (float)seen[li] / (float)per_loop[li];
            seen[li]++;
            Vec3 p; float yaw;
            rect_at(&c->loops[li], v->loop_s, &p, &yaw);
            lane_offset(yaw, CITY_LANE, &p);
            v->pos = p; v->yaw = yaw;
            v->speed = c->defs[v->def].top_ms * 0.30f;
        }
        c->traffic_n = c->veh_count - c->traffic0;
    }
    c->chase0 = c->veh_count;

    /* ── peds (30 on the 8 sidewalk loops) ── */
    for (int i = 0; i < 30 && c->ped_count < CITY_PEDS_MAX; i++) {
        Ped *p = &c->peds[c->ped_count++];
        memset(p, 0, sizeof *p);
        p->block = i % CITY_BLOCKS;
        const CityRect *r = &c->sidewalk[p->block];
        p->loop_s = rng_f(&g->rng) * rect_len(r);
        p->dir = (rng_f(&g->rng) < 0.5f) ? 1 : -1;
        p->state = 0;
        p->shirt = SHIRT_COL[i % 6];
        Vec3 pp; float yaw;
        rect_at(r, p->loop_s, &pp, &yaw);
        p->pos = pp; p->yaw = yaw;
    }
    DH_INFO("city", "Meridian City: %d blocks, %d vehicles (%d traffic), %d peds",
            CITY_BLOCKS, c->veh_count, c->traffic_n, c->ped_count);
}

int city_feature_at(const City *c, float x, float z) {
    if (x < CITY_BOUNDS_MIN - 8 || x > CITY_BOUNDS_MAX + 8 ||
        z < CITY_BOUNDS_MIN - 8) return 0;
    if (z > 655.0f) return 3;                       /* harbor water */
    if (z > CITY_BOUNDS_MAX + 8) return 0;
    for (int i = 0; i < 3; i++) {
        if (fabsf(x - c->avenues[i]) < CITY_ROAD_HW + 1.5f) return 1;
        if (fabsf(z - c->streets[i]) < CITY_ROAD_HW + 1.5f) return 1;
    }
    if (fabsf(x - 440.0f) < CITY_ALLEY_HW + 1.0f || fabsf(x - 560.0f) < CITY_ALLEY_HW + 1.0f)
        return 1;
    for (int i = 0; i < CITY_BLOCKS; i++)
        if (fabsf(x - c->blocks[i].x) < c->block_rx + 3.0f &&
            fabsf(z - c->blocks[i].y) < c->block_rz + 3.0f) return 2;
    return 1;
}

/* ══════════════════════════════ crimes & heat ═══════════════════════════ */

void city_crime(City *c, struct Game *g, Vec3 pos, float evidence, const char *what) {
    float bonus = 0.0f;
    int witnesses = 0;
    for (int i = 0; i < c->ped_count; i++) {
        Ped *p = &c->peds[i];
        if (p->state == 2) continue;                    /* hiding peds see nothing */
        if (dist2d(p->pos, pos) > 30.0f) continue;
        Vec3 a = v3(pos.x, pos.y + 1.5f, pos.z);
        Vec3 b = v3(p->pos.x, p->pos.y + 1.5f, p->pos.z);
        if (!obstacles_segment_clear(&g->obs, a, b)) continue;
        if (witnesses < 3) bonus += 1.0f;
        witnesses++;
        if (p->state == 0) {                            /* crime → panic */
            p->state = 1; p->state_t = 8.0f;
            float dx = p->pos.x - pos.x, dz = p->pos.z - pos.z;
            float l = sqrtf(dx*dx + dz*dz) + 0.001f;
            p->panic_x = dx / l; p->panic_z = dz / l;
        }
    }
    heat_add_evidence(&c->heat, evidence + bonus);
    if (c->heat.stars > c->escape_peak_stars) c->escape_peak_stars = c->heat.stars;
    game_message(g, "%s%s  [%d STAR%s]", what,
                 witnesses > 0 ? " - WITNESSES CALLED IT IN" : "",
                 c->heat.stars, c->heat.stars == 1 ? "" : "S");
    g->message_t = 3.0f;
    DH_INFO("city", "crime '%s' evidence %.1f (+%.1f witnesses) -> %d stars",
            what, (double)evidence, (double)bonus, c->heat.stars);
}

/* ══════════════════════════════ vehicles ════════════════════════════════ */

static void vehicle_explode(City *c, struct Game *g, Vehicle *v) {
    v->state = VS_WRECK;
    v->speed = 0.0f;
    c->cars_wrecked++;
    game_fx(g, v->pos, v3(0, 1, 0), 18, 0xFF3060F0u, 5.0f, 0.7f, 1);
    game_fx(g, v->pos, v3(0, 1, 0), 12, 0xFF404040u, 3.0f, 1.2f, 0);
    if (g->in_vehicle >= 0 && &c->veh[g->in_vehicle] == v) {
        g->in_vehicle = -1;
        Vec3 right = v3(cosf(v->yaw), 0.0f, -sinf(v->yaw));
        g->player.pos = v3(v->pos.x - right.x * 2.4f, v->pos.y + 1.8f,
                           v->pos.z - right.z * 2.4f);
        g->player.vel = v3(0, 0, 0);
        g->player.health = dh_clampf(g->player.health - 35.0f, 0.0f, g->player.health_max);
        g->damage_flash = 1.0f;
        game_message(g, "YOUR RIDE IS A WRECK - you bailed out for 35 damage");
        g->message_t = 3.0f;
    }
}

/* Terrain clamp + building collision + wreck check for any AI/player car. */
static void vehicle_clamp(City *c, struct Game *g, Vehicle *v) {
    const VehicleDef *d = &c->defs[v->def];
    float px = v->pos.x, pz = v->pos.z;
    float rad = d->width * 0.5f + 0.35f;
    int pushed = obstacles_resolve(&g->obs, &v->pos, rad, d->height, NULL);
    float th = terrain_height(&g->terrain, v->pos.x, v->pos.z);
    if (th < 0.5f) {                    /* harbor/boundary water is a wall */
        v->pos.x = px; v->pos.z = pz;
        th = terrain_height(&g->terrain, v->pos.x, v->pos.z);
        pushed = 1;
    }
    v->pos.x = dh_clampf(v->pos.x, CITY_BOUNDS_MIN - 14.0f, CITY_BOUNDS_MAX + 14.0f);
    v->pos.z = dh_clampf(v->pos.z, CITY_BOUNDS_MIN - 14.0f, 652.0f);
    v->pos.y = dh_maxf(th, 0.5f);
    if (pushed && v->state != VS_WRECK) {
        int mine = (g->in_vehicle >= 0 && v == &c->veh[g->in_vehicle]);
        vehicle_impact(v, fabsf(v->speed) * (mine ? prog_mod(&g->prog, MOD_VEH_DAMAGE) : 1.0f));
        if (fabsf(v->speed) > 5.0f)
            game_fx(g, v->pos, v3(0, 1, 0), 5, 0xFFB0B0B0u, 2.5f, 0.35f, 0);
        if (vehicle_wrecked(v, c->defs)) vehicle_explode(c, g, v);
    }
}

int city_nearest_vehicle(City *c, Vec3 pos, float reach, int include_wrecks) {
    int best = -1; float bd = reach;
    for (int i = 0; i < c->veh_count; i++) {
        const Vehicle *v = &c->veh[i];
        if (v->state == VS_WRECK && !include_wrecks) continue;
        float d = dist2d(v->pos, pos);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

int city_enter_vehicle(City *c, struct Game *g, int vi) {
    if (vi < 0 || vi >= c->veh_count || g->in_vehicle >= 0) return 0;
    Vehicle *v = &c->veh[vi];
    const VehicleDef *d = &c->defs[v->def];
    if (v->state == VS_WRECK) {
        game_message(g, "THAT %s IS SCRAP", d->name);
        g->message_t = 2.0f;
        return 0;
    }
    if (v->occupant > 0) {
        /* carjack: driver bails, drops a wallet (this is the "stolen cash") */
        v->occupant = -1;
        if (c->ped_count < CITY_PEDS_MAX) {
            Ped *p = &c->peds[c->ped_count++];
            memset(p, 0, sizeof *p);
            p->block = -1; p->state = 1; p->state_t = 8.0f;
            p->pos = v->pos; p->yaw = v->yaw;
            p->shirt = SHIRT_COL[c->ped_count % 6];
            float dx = v->pos.x - g->player.pos.x, dz = v->pos.z - g->player.pos.z;
            float l = sqrtf(dx*dx + dz*dz) + 0.001f;
            p->panic_x = dx / l; p->panic_z = dz / l;
        }
        int wallet = (25 + (int)(rng_f(&g->rng) * 36.0f)) * (int)prog_mod(&g->prog, MOD_WALLET);
        g->cash += wallet;
        c->wallets_looted++;
        char what[64];
        snprintf(what, sizeof what, "CARJACKED %s - DRIVER DROPPED $%d", d->name, wallet);
        city_crime(c, g, v->pos, 1.0f + (float)d->steal_heat, what);
    } else {
        char what[64];
        snprintf(what, sizeof what, "GRAND THEFT AUTO - %s", d->name);
        city_crime(c, g, v->pos, (float)d->steal_heat, what);
    }
    v->state = VS_DRIVEN;
    g->in_vehicle = vi;
    c->cars_stolen++;
    g->player.vel = v3(0, 0, 0);
    return 1;
}

void city_exit_vehicle(City *c, struct Game *g) {
    if (g->in_vehicle < 0) return;
    Vehicle *v = &c->veh[g->in_vehicle];
    Vec3 right = v3(cosf(v->yaw), 0.0f, -sinf(v->yaw));
    Vec3 p = v3(v->pos.x - right.x * 2.3f, v->pos.y + 1.8f, v->pos.z - right.z * 2.3f);
    p.x = dh_clampf(p.x, CITY_BOUNDS_MIN, CITY_BOUNDS_MAX);
    p.z = dh_clampf(p.z, CITY_BOUNDS_MIN, 650.0f);
    p.y = dh_maxf(terrain_height(&g->terrain, p.x, p.z), CITY_GROUND_Y) + 0.05f;
    obstacles_resolve(&g->obs, &p, 0.34f, 1.8f, NULL);
    g->player.pos = p;
    g->player.vel = v3(0, 0, 0);
    if (v->state == VS_DRIVEN)
        v->state = (v->occupant > 0) ? VS_TRAFFIC : VS_PARKED;   /* abandoned */
    g->in_vehicle = -1;
    game_message(g, "ON FOOT");
    g->message_t = 1.2f;
}

/* ══════════════════════════════ economy & jobs ══════════════════════════ */

int city_buy_hotdog(City *c, struct Game *g) {
    if (dist2d(g->player.pos, c->hotdog_stand) > 3.5f) return -1;
    if (g->in_vehicle >= 0) {
        game_message(g, "THE VENDOR WON'T SERVE A CAR - STEP OUT");
        g->message_t = 2.0f;
        return 0;
    }
    if (g->cash < 5) {
        game_message(g, "HOT DOGS ARE $5 - YOU HAVE $%d", g->cash);
        g->message_t = 2.5f;
        return 0;
    }
    g->cash -= 5;
    c->hotdogs_bought++;
    float before = g->player.health;
    g->player.health = dh_clampf(g->player.health + 15.0f * prog_mod(&g->prog, MOD_FOOD), 0.0f, g->player.health_max);
    game_message(g, "HOT DOG - $5. MUSTARD IS FREE. (+%.0f HP)",
                 g->player.health - before);
    g->message_t = 3.0f;
    DH_INFO("city", "hot dog #%d bought (cash now $%d)", c->hotdogs_bought, g->cash);
    return 1;
}

int city_job_interact(City *c, struct Game *g) {
    Vec3 pp = g->player.pos;
    OddJob *j = &c->job;
    if (j->active == 0 || j->active == 3) {
        if (dist2d(pp, c->job_board) < 3.5f) {
            j->active = 1;
            j->timer = 90.0f;
            game_message(g, "HOT DELIVERY - GRAB THE CRATE, GET IT TO THE STAND. 90s. $%d",
                         j->reward);
            g->message_t = 4.0f;
            return 1;
        }
        return 0;
    }
    if (j->active == 1 && dist2d(pp, j->pickup) < 3.0f) {
        j->active = 2;
        game_message(g, "CRATE ACQUIRED - DELIVER IT TO THE HOT-DOG STAND");
        g->message_t = 3.5f;
        return 2;
    }
    if (j->active == 2 && dist2d(pp, j->dropoff) < 3.0f) {
        j->active = 3;
        int pay = (int)((float)j->reward * prog_mod(&g->prog, MOD_JOB_PAY) + 0.5f);
        g->cash += pay;
        g->prog.money_earned += pay;
        prog_event(&g->prog, EV_JOB, 0);
        c->jobs_done++;
        game_message(g, "DELIVERED - +$%d (cash $%d)", pay, g->cash);
        g->message_t = 4.0f;
        DH_INFO("city", "odd job #%d delivered (+$%d)", c->jobs_done, pay);
        return 3;
    }
    return 0;
}

/* ══════════════════════════════ per-frame ═══════════════════════════════ */

static void traffic_update(City *c, struct Game *g, float dt) {
    for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++) {
        Vehicle *v = &c->veh[i];
        if (v->state != VS_TRAFFIC) continue;
        const VehicleDef *d = &c->defs[v->def];
        if (v->panic_t > 0.0f) v->panic_t -= dt;

        /* gap rule: another car ahead within 11 m on the same loop → slow */
        float want = d->top_ms * 0.30f * (v->panic_t > 0.0f ? 1.45f : 1.0f);
        for (int j = c->traffic0; j < c->traffic0 + c->traffic_n; j++) {
            if (j == i) continue;
            const Vehicle *o = &c->veh[j];
            if (o->state != VS_TRAFFIC || o->loop != v->loop) continue;
            float ds = fmodf(o->loop_s - v->loop_s + c->loop_len[v->loop],
                             c->loop_len[v->loop]);
            if (ds > 0.1f && ds < 11.0f) { want *= 0.35f; break; }
        }
        /* player's car blocking the lane → brake too */
        if (g->in_vehicle >= 0) {
            const Vehicle *pv = &c->veh[g->in_vehicle];
            if (pv->loop == v->loop && pv->state == VS_DRIVEN) {
                float ds = fmodf(pv->loop_s - v->loop_s + c->loop_len[v->loop],
                                 c->loop_len[v->loop]);
                if (ds > 0.1f && ds < 12.0f) want *= 0.2f;
            }
        }
        float dv = want - v->speed;
        v->speed += dh_clampf(dv, -9.0f * dt, 5.0f * dt);
        v->speed = dh_maxf(0.0f, v->speed);
        v->loop_s += v->speed * dt;
        Vec3 p; float yaw;
        rect_at(&c->loops[v->loop], v->loop_s, &p, &yaw);
        lane_offset(yaw, CITY_LANE, &p);
        p.y = dh_maxf(terrain_height(&g->terrain, p.x, p.z), CITY_GROUND_Y);
        v->pos = p; v->yaw = yaw;
    }
}

static void peds_update(City *c, struct Game *g, float dt) {
    for (int i = 0; i < c->ped_count; i++) {
        Ped *p = &c->peds[i];
        p->state_t -= dt;
        switch (p->state) {
        case 0: {   /* sidewalk wander */
            if (p->block < 0) { p->block = i % CITY_BLOCKS; p->loop_s = 0.0f; }
            const CityRect *r = &c->sidewalk[p->block];
            p->loop_s += 1.25f * dt * (float)p->dir;
            Vec3 pp; float yaw;
            rect_at(r, p->loop_s, &pp, &yaw);
            if (p->dir < 0) yaw += CITY_PI;
            p->pos = pp; p->yaw = yaw;
            break; }
        case 1: {   /* flee */
            p->pos.x += p->panic_x * 4.6f * dt;
            p->pos.z += p->panic_z * 4.6f * dt;
            p->pos.x = dh_clampf(p->pos.x, CITY_BOUNDS_MIN, CITY_BOUNDS_MAX);
            p->pos.z = dh_clampf(p->pos.z, CITY_BOUNDS_MIN, 650.0f);
            p->yaw = atan2f(p->panic_x, p->panic_z);
            obstacles_resolve(&g->obs, &p->pos, 0.3f, 1.7f, NULL);
            if (p->state_t <= 0.0f) { p->state = 2; p->state_t = 10.0f; }
            break; }
        case 2:     /* hide */
            if (p->state_t <= 0.0f) p->state = 0;
            break;
        case 3:     /* knocked down (survives - the slice keeps its M-rating honest) */
            if (p->state_t <= 0.0f) { p->state = 1; p->state_t = 6.0f;
                                      p->panic_x = 0.0f; p->panic_z = -1.0f; }
            break;
        }
        p->pos.y = dh_maxf(terrain_height(&g->terrain, p->pos.x, p->pos.z), CITY_GROUND_Y);
    }
}

static const Vec3 CHASE_SPAWN[4] = {
    { 380.0f, CITY_GROUND_Y, 348.0f }, { 620.0f, CITY_GROUND_Y, 348.0f },
    { 380.0f, CITY_GROUND_Y, 652.0f }, { 620.0f, CITY_GROUND_Y, 652.0f },
};

static void chase_spawn(City *c, struct Game *g) {
    static int next_spawn = 0;
    int vi = -1;
    /* reuse a deactivated cruiser far from the player, else append */
    for (int i = c->chase0; i < c->veh_count; i++) {
        if (c->veh[i].state == VS_PARKED &&
            (c->veh[i].def == def_find(c, "V06") || c->veh[i].def == def_find(c, "V11")) &&
            dist2d(c->veh[i].pos, g->player.pos) > 130.0f) { vi = i; break; }
    }
    if (vi < 0) {
        if (c->veh_count >= VEHICLES_MAX) return;
        vi = c->veh_count++;
        memset(&c->veh[vi], 0, sizeof(Vehicle));
        c->veh[vi].def = def_find(c, "V06");
        c->veh[vi].occupant = 1;
        c->veh[vi].loop = -1;
    }
    Vehicle *v = &c->veh[vi];
    Vec3 sp = CHASE_SPAWN[next_spawn % 4];
    next_spawn++;
    v->pos = sp;
    v->yaw = (sp.z < 500.0f) ? 0.0f : CITY_PI;
    v->speed = 8.0f;
    /* M22 (§6.5/§69): from 4★ every other response vehicle is a Limpio Obelisco */
    v->def = (c->heat.stars >= 4 && (next_spawn & 1)) ? def_find(c, "V11") : def_find(c, "V06");
    v->damage = (c->heat.stars >= 5 && c->defs[v->def].cls == VC_POLICE) ? -1200.0f : 0.0f;
    v->slip = 0.0f;
    v->state = VS_CHASE;
    v->panic_t = 0.0f;
    c->cop_fire_cd[vi] = 1.5f;
}

static void chase_update(City *c, struct Game *g, float dt) {
    int want = heat_response_cruisers(&c->heat);
    int have = 0;
    for (int i = c->chase0; i < c->veh_count; i++)
        if (c->veh[i].state == VS_CHASE) have++;
    while (have < want) { chase_spawn(c, g); have++; }
    if (have > want) {   /* stand down the farthest cruiser */
        int far = -1; float fd = -1.0f;
        for (int i = c->chase0; i < c->veh_count; i++) {
            if (c->veh[i].state != VS_CHASE) continue;
            float d = dist2d(c->veh[i].pos, g->player.pos);
            if (d > fd) { fd = d; far = i; }
        }
        if (far >= 0 && fd > 120.0f) c->veh[far].state = VS_PARKED;
    }

    Vec3 eye = v3(g->player.pos.x, g->player.pos.y + 1.4f, g->player.pos.z);
    for (int i = c->chase0; i < c->veh_count; i++) {
        Vehicle *v = &c->veh[i];
        if (v->state != VS_CHASE) continue;
        float dx = g->player.pos.x - v->pos.x, dz = g->player.pos.z - v->pos.z;
        float dist = sqrtf(dx*dx + dz*dz) + 0.001f;
        float want_yaw = atan2f(dx, dz);
        float diff = ang_diff(want_yaw, v->yaw);
        float steer = dh_clampf(diff * 1.6f, -1.0f, 1.0f);
        float throttle = (fabsf(diff) > 1.2f && v->speed > 9.0f) ? 0.15f : 1.0f;
        int hb = (fabsf(diff) > 2.2f && v->speed > 10.0f);
        vehicle_step(v, c->defs, dt, throttle, 0.0f, steer, hb);
        vehicle_clamp(c, g, v);

        /* ram the player on foot */
        if (g->in_vehicle < 0 && dist < 2.4f && fabsf(v->speed) > 6.0f) {
            g->player.health = dh_clampf(g->player.health - 25.0f, 0.0f, g->player.health_max);
            g->damage_flash = 1.0f;
            g->player.pos.x += dx / dist * 2.5f;
            g->player.pos.z += dz / dist * 2.5f;
            game_message(g, "RUN DOWN BY A CRUISER -25 HP");
            g->message_t = 2.0f;
        }
        /* pit the player's car */
        if (g->in_vehicle >= 0 && dist < 3.6f && fabsf(v->speed) > 4.0f) {
            vehicle_impact(&c->veh[g->in_vehicle], fabsf(v->speed) * 0.6f * prog_mod(&g->prog, MOD_VEH_DAMAGE));
            vehicle_impact(v, fabsf(v->speed) * 0.4f);
            if (vehicle_wrecked(&c->veh[g->in_vehicle], c->defs))
                vehicle_explode(c, g, &c->veh[g->in_vehicle]);
            if (vehicle_wrecked(v, c->defs)) vehicle_explode(c, g, v);
        }
        /* drive-by from the cruiser */
        c->cop_fire_cd[i] -= dt;
        if (dist < 45.0f && c->cop_fire_cd[i] <= 0.0f) {
            Vec3 muzzle = v3(v->pos.x, v->pos.y + 1.3f, v->pos.z);
            int los = obstacles_segment_clear(&g->obs, muzzle, eye);
            if (los) {
                float acc = 0.26f + 0.05f * (float)c->heat.stars;
                game_tracer(g, muzzle, eye, 0xFF60A0FFu);
                if (rng_f(&g->rng) < acc) {
                    if (g->in_vehicle >= 0) {
                        Vehicle *pv = &c->veh[g->in_vehicle];
                        pv->damage += 9.0f;
                        if (vehicle_wrecked(pv, c->defs)) vehicle_explode(c, g, pv);
                    } else {
                        g->player.health = dh_clampf(g->player.health -
                                                     (5.0f + (float)c->heat.stars), 0.0f, g->player.health_max);
                        g->damage_flash = 0.8f;
                    }
                }
                c->cop_fire_cd[i] = 1.4f - 0.1f * (float)c->heat.stars;
            } else {
                c->cop_fire_cd[i] = 0.3f;
            }
        }
    }
}

static void heli_update(City *c, struct Game *g, float dt) {
    int want = heat_has_heli(&c->heat);
    if (want != c->heli_active) {
        c->heli_active = want;
        if (want) {
            game_message(g, "AIR SUPPORT INBOUND - 4 STARS");
            g->message_t = 3.0f;
            c->heli_fire_cd = 2.0f;
        }
    }
    if (!c->heli_active) return;
    c->heli_ang += dt * 0.55f;
    c->heli_fire_cd -= dt;
    if (c->heli_fire_cd <= 0.0f) {
        c->heli_fire_cd = 2.2f;
        Vec3 hp = v3(g->player.pos.x + cosf(c->heli_ang) * 18.0f,
                     g->player.pos.y + 26.0f,
                     g->player.pos.z + sinf(c->heli_ang) * 18.0f);
        Vec3 eye = v3(g->player.pos.x, g->player.pos.y + 1.4f, g->player.pos.z);
        game_tracer(g, hp, eye, 0xFF60A0FFu);
        if (rng_f(&g->rng) < 0.55f) {
            if (g->in_vehicle >= 0) {
                Vehicle *pv = &c->veh[g->in_vehicle];
                pv->damage += 12.0f;
                if (vehicle_wrecked(pv, c->defs)) vehicle_explode(c, g, pv);
            } else {
                g->player.health = dh_clampf(g->player.health - 7.0f, 0.0f, g->player.health_max);
                g->damage_flash = 0.8f;
            }
        }
    }
}

/* Nearest pursuing cruiser distance/LOS for the escape clock. */
static void heat_sensor(City *c, struct Game *g, float *dist_out, int *los_out) {
    float best = 1e9f; int los = 0;
    Vec3 eye = v3(g->player.pos.x, g->player.pos.y + 1.4f, g->player.pos.z);
    for (int i = c->chase0; i < c->veh_count; i++) {
        const Vehicle *v = &c->veh[i];
        if (v->state != VS_CHASE) continue;
        float d = dist2d(v->pos, g->player.pos);
        if (d < best) {
            best = d;
            Vec3 m = v3(v->pos.x, v->pos.y + 1.3f, v->pos.z);
            los = obstacles_segment_clear(&g->obs, m, eye);
        }
    }
    if (c->heli_active) {
        /* The heli keeps a visual only while you're moving fast or shooting
           ("going slow and cold shakes the spotlight") — otherwise 4★ would
           make the escape clock unreachable by design. */
        float sp = 0.0f;
        if (g->in_vehicle >= 0) sp = fabsf(c->veh[g->in_vehicle].speed);
        else sp = sqrtf(g->player.vel.x * g->player.vel.x +
                        g->player.vel.z * g->player.vel.z);
        if (sp > 6.0f || g->noise_t > 0.0f) { los = 1; if (30.0f < best) best = 30.0f; }
    }
    *dist_out = best; *los_out = los;
}

void city_frame(City *c, struct Game *g, const PlatInput *in, float dt) {
    if (!c->built || dt <= 0.0f) return;
    c->use_cd = dh_maxf(0.0f, c->use_cd - dt);

    /* radio (stub: HUD text; adaptive audio arrives in M7) */
    if (g->in_vehicle >= 0 && (in->pressed & BTN_RADIO)) {
        const Vehicle *v = &c->veh[g->in_vehicle];
        if (c->defs[v->def].radio) {
            c->radio_station = (c->radio_station + 1) % CITY_STATIONS;
            game_message(g, "RADIO: %s", city_radio_name(c->radio_station));
            g->message_t = 2.0f;
        }
    }

    /* gunfire edge → panic + a crime report (rising edge of Game.noise_t) */
    if (g->noise_t > c->prev_noise_t + 0.05f) {
        city_crime(c, g, g->player.pos, 1.0f, "DISCHARGING A FIREARM");
        for (int i = 0; i < c->ped_count; i++) {
            Ped *p = &c->peds[i];
            if (p->state != 0 || dist2d(p->pos, g->player.pos) > 45.0f) continue;
            p->state = 1; p->state_t = 8.0f;
            float dx = p->pos.x - g->player.pos.x, dz = p->pos.z - g->player.pos.z;
            float l = sqrtf(dx*dx + dz*dz) + 0.001f;
            p->panic_x = dx / l; p->panic_z = dz / l;
        }
        for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++)
            if (dist2d(c->veh[i].pos, g->player.pos) < 40.0f) c->veh[i].panic_t = 5.0f;
    }
    c->prev_noise_t = g->noise_t;

    /* player vehicle */
    if (g->in_vehicle >= 0) {
        Vehicle *v = &c->veh[g->in_vehicle];
        const VehicleDef *d = &c->defs[v->def];
        if (v->state == VS_WRECK) {
            city_exit_vehicle(c, g);
        } else {
            float thr = in->my > 0.0f ? dh_clampf(in->my, 0.0f, 1.0f) : 0.0f;
            float brk = in->my < 0.0f ? dh_clampf(-in->my, 0.0f, 1.0f) : 0.0f;
            int hb = (in->buttons & BTN_JUMP) ? 1 : 0;
            {   /* M5 WHEELMAN: player's car gets +15% top speed */
                float keep = c->defs[v->def].top_ms;
                c->defs[v->def].top_ms = keep * prog_mod(&g->prog, MOD_TOP_SPEED);
                vehicle_step(v, c->defs, dt, thr, brk, dh_clampf(in->mx, -1.0f, 1.0f), hb);
                c->defs[v->def].top_ms = keep;
            }
            vehicle_clamp(c, g, v);
            g->player.pos = v3(v->pos.x, v->pos.y + d->height * 0.5f + 0.75f, v->pos.z);
            g->player.vel = v3(0, 0, 0);
            /* run over peds (they survive; knocked down) */
            if (fabsf(v->speed) > 3.0f) {
                for (int i = 0; i < c->ped_count; i++) {
                    Ped *p = &c->peds[i];
                    if (p->state == 3) continue;
                    if (dist2d(p->pos, v->pos) < 1.7f) {
                        p->state = 3; p->state_t = 15.0f;
                        c->peds_hit++;
                        city_crime(c, g, v->pos, 2.0f, "HIT AND RUN");
                    }
                }
            }
            /* bump traffic */
            for (int i = c->traffic0; i < c->traffic0 + c->traffic_n; i++) {
                Vehicle *o = &c->veh[i];
                if (o->state != VS_TRAFFIC) continue;
                if (dist2d(o->pos, v->pos) < 3.2f) {
                    float imp = fabsf(v->speed - o->speed) + 2.0f;
                    vehicle_impact(o, imp);
                    vehicle_impact(v, imp * 0.5f * prog_mod(&g->prog, MOD_VEH_DAMAGE));
                    if (vehicle_wrecked(o, c->defs)) vehicle_explode(c, g, o);
                    if (vehicle_wrecked(v, c->defs)) vehicle_explode(c, g, v);
                }
            }
            if (v->state != VS_WRECK && (in->pressed & BTN_USE) && c->use_cd <= 0.0f) {
                c->use_cd = 0.35f;
                if (fabsf(v->speed) < 3.0f) city_exit_vehicle(c, g);
                else { game_message(g, "TOO FAST TO STEP OUT"); g->message_t = 1.5f; }
            }
        }
    } else if ((in->pressed & BTN_USE) && c->use_cd <= 0.0f) {
        /* on foot: prioritize economy/quest interactables, then vehicles */
        c->use_cd = 0.35f;
        int done = 0;
        if (dist2d(g->player.pos, c->hotdog_stand) < 3.5f) { city_buy_hotdog(c, g); done = 1; }
        else if (city_job_interact(c, g)) done = 1;
        if (!done) {
            int vi = city_nearest_vehicle(c, g->player.pos, 3.5f, 0);
            if (vi >= 0) city_enter_vehicle(c, g, vi);
            else { game_message(g, "NOTHING TO USE HERE"); g->message_t = 1.0f; }
        }
    }

    traffic_update(c, g, dt);
    peds_update(c, g, dt);
    chase_update(c, g, dt);
    heli_update(c, g, dt);

    /* odd-job clock */
    if (c->job.active == 1 || c->job.active == 2) {
        c->job.timer -= dt;
        if (c->job.timer <= 0.0f) {
            c->job.active = 0;
            game_message(g, "JOB FAILED - THE BOARD STILL HAS WORK");
            g->message_t = 3.5f;
        }
    }

    /* heat escape clock */
    {
        float d; int los;
        heat_sensor(c, g, &d, &los);
        heat_tick(&c->heat, dt / prog_mod(&g->prog, MOD_HEAT_ESCAPE), d, los);   /* LOW PROFILE */
        if (c->heat.stars > c->escape_peak_stars) c->escape_peak_stars = c->heat.stars;
        if (c->heat.just_dropped) {
            if (c->heat.stars == 0) {
                game_message(g, "YOU'RE CLEAN - THE SIRENS FADE OUT");
                g->message_t = 3.5f;
            } else {
                game_message(g, "LOST A STAR - %d REMAIN", c->heat.stars);
                g->message_t = 2.5f;
            }
        }
    }

    /* wreck smoke */
    for (int i = 0; i < c->veh_count; i++) {
        const Vehicle *v = &c->veh[i];
        if (v->state == VS_WRECK && (g->frame_index % 12) == (i % 12) &&
            dist2d(v->pos, g->player.pos) < 80.0f)
            game_fx(g, v3(v->pos.x, v->pos.y + 0.8f, v->pos.z), v3(0, 1, 0),
                    1, 0xFF505050u, 0.8f, 1.6f, 0);
    }
}

/* ══════════════════════════════ rendering ═══════════════════════════════ */

static void draw_vehicle(City *c, struct Game *g, const Vehicle *v, int idx) {
    if (!rend_should_draw(&v->pos, 4.5f)) return;
    const VehicleDef *d = &c->defs[v->def];
    int wreck = (v->state == VS_WRECK);
    uint32_t col;
    if (wreck) col = 0xFF262626u;
    else if (d->cls == VC_POLICE || d->cls == VC_ARMORED) col = 0xFFEEEAE0u;
    else if (d->cls == VC_BUGGY) col = 0xFF3C78A8u;   /* cult ochre */
    else col = CIVIL_COL[idx % 6];

    Mat4 body = m4_trs(v3(v->pos.x, v->pos.y + d->height * 0.45f, v->pos.z),
                       v->yaw, 0.0f, 0.0f,
                       v3(d->width, d->height * 0.85f, d->length));
    rend_mesh_lit(g->mesh_box, &body, -1, col, 1.0f, 1, 1, 1, 0, 0);
    if (d->cls != VC_MOTO && !wreck) {
        Mat4 cab = m4_trs(v3(v->pos.x, v->pos.y + d->height * 0.92f, v->pos.z),
                          v->yaw, 0.0f, 0.0f,
                          v3(d->width * 0.78f, d->height * 0.5f, d->length * 0.52f));
        rend_mesh_lit(g->mesh_box, &cab, -1, 0xFF2A2E33u, 1.0f, 1, 1, 1, 0, 0);
    }
    if (d->cls == VC_PICKUP && !wreck) {   /* open bed: lower rear box */
        Mat4 bed = m4_trs(v3(v->pos.x - sinf(v->yaw) * d->length * 0.28f, v->pos.y + d->height * 0.7f,
                             v->pos.z - cosf(v->yaw) * d->length * 0.28f), v->yaw, 0, 0,
                          v3(d->width * 0.9f, 0.25f, d->length * 0.4f));
        rend_mesh_lit(g->mesh_box, &bed, -1, 0xFF30343Au, 1.0f, 1, 1, 1, 0, 0);
    }
    if ((d->cls == VC_POLICE || d->cls == VC_ARMORED) && !wreck) {
        int phase = ((int)(g->time * 5.0f) + idx) % 2;
        Vec3 lp = v3(v->pos.x, v->pos.y + d->height * 1.22f, v->pos.z);
        Mat4 l1 = m4_trs(v3(lp.x + 0.3f, lp.y, lp.z), v->yaw, 0, 0, v3(0.3f, 0.16f, 0.3f));
        Mat4 l2 = m4_trs(v3(lp.x - 0.3f, lp.y, lp.z), v->yaw, 0, 0, v3(0.3f, 0.16f, 0.3f));
        rend_mesh_lit(g->mesh_box, &l1, proc_tex.glow, phase ? 0xFF2020D0u : 0xFF404040u,
                      1.0f, 0, 1, 1, 0, 1);
        rend_mesh_lit(g->mesh_box, &l2, proc_tex.glow, phase ? 0xFF404040u : 0xFFD02020u,
                      1.0f, 0, 1, 1, 0, 1);
    }
}

static void draw_ped(struct Game *g, const Ped *p) {
    if (!rend_should_draw(&p->pos, 2.2f)) return;
    if (p->state == 3) {  /* knocked down: flat slab */
        Mat4 m = m4_trs(v3(p->pos.x, p->pos.y + 0.18f, p->pos.z), p->yaw, 0, 0,
                        v3(0.5f, 0.36f, 1.6f));
        rend_mesh_lit(g->mesh_box, &m, -1, p->shirt, 1.0f, 1, 1, 1, 0, 0);
        return;
    }
    float bob = (p->state == 0) ? sinf(p->loop_s * 2.0f) * 0.03f : 0.0f;
    Mat4 m = m4_trs(v3(p->pos.x, p->pos.y + 0.85f + bob, p->pos.z), p->yaw, 0, 0,
                    v3(0.44f, 1.7f, 0.34f));
    rend_mesh_lit(g->mesh_box, &m, -1, p->shirt, 1.0f, 1, 1, 1, 0, 0);
}

static void draw_marker(struct Game *g, Vec3 pos, uint32_t col, int wood) {
    if (!rend_should_draw(&pos, 6.0f)) return;
    if (wood) {
        Mat4 m = m4_trs(v3(pos.x, pos.y + 0.45f, pos.z), 0.4f, 0, 0, v3(0.9f, 0.9f, 0.9f));
        rend_mesh_lit(g->mesh_box, &m, proc_tex.wood, 0xFFFFFFFFu, 1.0f, 1, 1, 1, 0, 0);
    }
    float fy = pos.y + 2.6f + sinf(g->time * 2.4f) * 0.18f;
    Mat4 gm = m4_trs(v3(pos.x, fy, pos.z), g->time * 1.2f, 0, 0, v3(1.4f, 1.4f, 0.05f));
    rend_mesh_lit(g->mesh_box, &gm, proc_tex.glow, col, 0.85f, 0, 1, 1, 0, 1);
}

void city_render(City *c, struct Game *g) {
    if (!c->built || g->act != 1) return;
    for (int i = 0; i < c->veh_count; i++) draw_vehicle(c, g, &c->veh[i], i);
    for (int i = 0; i < c->ped_count; i++) draw_ped(g, &c->peds[i]);

    /* job markers */
    if (c->job.active == 1) draw_marker(g, c->job.pickup, 0xFF30C0F0u, 1);
    if (c->job.active == 2) draw_marker(g, c->job.dropoff, 0xFF30F060u, 0);
    if (c->job.active == 0 || c->job.active == 3) draw_marker(g, c->job_board, 0xFF30C0F0u, 0);

    /* 4★+ heli: orbiting dark body + rotor cross */
    if (c->heli_active) {
        Vec3 hp = v3(g->player.pos.x + cosf(c->heli_ang) * 18.0f,
                     g->player.pos.y + 26.0f,
                     g->player.pos.z + sinf(c->heli_ang) * 18.0f);
        Mat4 hb = m4_trs(hp, c->heli_ang + CITY_PI * 0.5f, 0, 0, v3(2.6f, 1.4f, 6.0f));
        rend_mesh_lit(g->mesh_box, &hb, -1, 0xFF30343Au, 1.0f, 1, 1, 1, 0, 0);
        float rot = g->time * 26.0f;
        Mat4 r1 = m4_trs(v3(hp.x, hp.y + 1.0f, hp.z), rot, 0, 0, v3(9.0f, 0.08f, 0.6f));
        Mat4 r2 = m4_trs(v3(hp.x, hp.y + 1.0f, hp.z), rot + CITY_PI * 0.5f, 0, 0,
                         v3(9.0f, 0.08f, 0.6f));
        rend_mesh_lit(g->mesh_box, &r1, -1, 0xFF202020u, 0.9f, 0, 1, 1, 0, 1);
        rend_mesh_lit(g->mesh_box, &r2, -1, 0xFF202020u, 0.9f, 0, 1, 1, 0, 1);
    }
}
