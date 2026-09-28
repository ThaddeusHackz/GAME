/* ============================================================================
   DIVIDED HORIZON — Horizon Engine
   src/city/city.h — Meridian City act (M4 vertical slice, Spec 16).
   Scope by the numbers: 8 blocks + harbor stub, 3+ drivable vehicle defs,
   15 traffic cars, 30 pedestrians, heat 0→5 with cruisers, 1 odd job.
   DoD: steal a car → evade 3 stars → buy a hot dog with stolen cash.
   ========================================================================== */
#ifndef DH_CITY_H
#define DH_CITY_H

#include "../core/dh_types.h"
#include "../plat/plat.h"
#include "../city/vehicle.h"
#include "../city/heat.h"

struct Game;   /* game.h names the struct `Game` */

/* ── layout (metres; world origin 500,500 — same heightfield as the island) ──
   3 avenues (N–S) at x = 380/500/620, 3 streets (E–W) at z = 380/500/620,
   road half-width 7; each of the 4 superblocks is split by a 6 m alley into
   2 blocks → 8 city blocks. Harbor stub: flooded strip south of z = 660. */
#define CITY_ROAD_HW   7.0f
#define CITY_ALLEY_HW  3.0f
#define CITY_GROUND_Y  4.0f
#define CITY_BLOCKS    8
#define CITY_PEDS_MAX  48
#define CITY_STATIONS  5

typedef struct { float x0, z0, x1, z1; } CityRect;

/* Pedestrian: sidewalk wander → panic flee → hide → resume (Section 29
   witnesses). state: 0 walk · 1 flee · 2 hide · 3 knocked down (survives). */
typedef struct {
    Vec3  pos;
    float yaw;
    int   state;
    float state_t;
    int   block;         /* sidewalk loop this ped belongs to */
    float loop_s;        /* arc length on that loop */
    int   dir;           /* +1 / -1 around the loop */
    float panic_x, panic_z;  /* flee direction */
    uint32_t shirt;
} Ped;

typedef struct {
    int   active;        /* 0 none · 1 accepted · 2 carrying · 3 delivered */
    float timer;         /* delivery deadline once accepted */
    Vec3  pickup, dropoff;
    int   reward;
} OddJob;

typedef struct {
    int   built;

    /* static layout — filled by city_init, read by game.c prop building */
    Vec2     blocks[CITY_BLOCKS];       /* block centers */
    float    block_rx, block_rz;        /* block half extents (25 × 53) */
    float    avenues[3], streets[3];    /* road centerlines x / z */
    CityRect sidewalk[CITY_BLOCKS];     /* ped loops around each block */

    /* traffic loops (rectangles on the road grid) */
    CityRect loops[3];
    float    loop_len[3];

    /* actors */
    VehicleDef defs[VEH_DEFS_MAX];
    int     def_count;
    Vehicle veh[VEHICLES_MAX];
    int     veh_count;
    int     traffic0, traffic_n;        /* index range of AI traffic */
    int     chase0;                     /* index range of response cruisers */
    Ped     peds[CITY_PEDS_MAX];
    int     ped_count;
    Heat    heat;
    OddJob  job;

    /* landmarks (props are built by game.c at these spots) */
    Vec3    job_board;
    Vec3    hotdog_stand;
    Vec3    player_spawn;

    /* state */
    int     radio_station;
    int     hotdogs_bought;
    int     cars_stolen;
    int     cars_wrecked;
    int     wallets_looted;
    int     peds_hit;
    int     jobs_done;
    float   prev_noise_t;               /* gunfire edge detect on Game.noise_t */
    float   use_cd;                     /* E-repeat guard */
    float   cop_fire_cd[VEHICLES_MAX];  /* per-cruiser shot clock */
    float   heli_ang, heli_fire_cd;
    int     heli_active;
    int     escape_peak_stars;          /* highest stars reached (DoD proof) */
} City;

/* Build the city act into `g`: layout, actors, minimap paint. game.c calls
   this AFTER flattening the terrain plate, then builds static props via
   city_block_rect()/city_feature_at(). */
void city_init(City *c, struct Game *g);

/* Per-frame simulation: driving, traffic, peds, heat response, jobs. */
void city_frame(City *c, struct Game *g, const PlatInput *in, float dt);

/* Dynamic draws (vehicles, peds, markers, cop lights). Called between
   world props and the player. */
void city_render(City *c, struct Game *g);

/* Report a crime: witness bonus, evidence, HUD message. */
void city_crime(City *c, struct Game *g, Vec3 pos, float evidence, const char *what);

/* Interaction helpers (also used by the smoke test to prove the DoD). */
int  city_nearest_vehicle(City *c, Vec3 pos, float reach, int include_wrecks);
int  city_enter_vehicle(City *c, struct Game *g, int vi);
void city_exit_vehicle(City *c, struct Game *g);
int  city_buy_hotdog(City *c, struct Game *g);          /* 1 ok · 0 no cash */
int  city_job_interact(City *c, struct Game *g);        /* board/pickup/drop */
const char *city_radio_name(int station);

/* Minimap support: 0 outside · 1 road · 2 block · 3 harbor water · 4 park */
int  city_feature_at(const City *c, float x, float z);

#endif /* DH_CITY_H */
