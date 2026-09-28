/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — game application layer (M1: Traversal)
   Owns the world (terrain + collision + props), the player, the camera, the
   sun/atmosphere and the HUD. Deliberately backend-agnostic: everything is
   submitted as RenderItems, so the same code runs on the CPU rasterizer
   (headless CI) and on GL11 (the shipping .exe).
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_GAME_H
#define DH_GAME_H

#include "../core/dh_types.h"
#include "../core/settings.h"
#include "../rend/rend.h"
#include "../rend/proc_tex.h"
#include "../rend/font.h"
#include "../world/terrain.h"
#include "../world/collision.h"
#include "../player/player.h"
#include "../plat/plat.h"
#include "../combat/weapon.h"
#include "../ai/enemy.h"
#include "../city/city.h"
#include "../meta/progress.h"
#include "../meta/mission.h"

#define GAME_MAX_PROPS 640

typedef enum { GM_MENU = 0, GM_PLAY, GM_PAUSE, GM_PHOTO } GameMode;
typedef enum { UI_NONE = 0, UI_MAP, UI_CHAR, UI_SHOP, UI_CARD } GameUi;

typedef struct {
    Mat4     model;
    int      tex;
    uint32_t color;
    float    alpha;
    int      two_sided;
    int      alpha_test;
    Vec3     center;         /* world-space, for culling */
    float    radius;
} PropDraw;

/* ── island content (M3, Spec 4.1 / 6.7 / §28 / §52) ── */

/* One authored outpost. Vertical-slice scope: a single difficulty-1 garrison
   (§28.2: 6-10 enemies, alarm radius 40 m, two reinforcement waves of 3). */
#define GARRISON_MAX 12
typedef struct {
    Vec3  pos, patrol_a, patrol_b;
    int   arch;
} GarrisonSlot;

typedef struct {
    int   built;
    char  name[28];
    int   difficulty;            /* 1..5 (§28.2) */
    int   captured;
    float capture_t;             /* channel seconds held at the flag */
    int   alarm;                 /* 0 quiet · 1 raised */
    float alarm_t;               /* seconds since the alarm was raised */
    float combat_seen_t;         /* garrison has been in COMBAT this long */
    int   waves_spawned;         /* reinforcement waves committed (max 2) */
    Vec3  center;
    float radius;                /* capture + alarm-trigger radius */
    float pad_h;                 /* flattened site height */
    Vec3  flag_pos;
    float flag_raise;            /* 0..1 capture flag animation */
    Vec3  alarm_pos;             /* destructible alarm box (§52: 150 hp) */
    float alarm_hp;
    int   alarm_destroyed;
    Vec3  mast_pos;              /* signal mast base (world landmark) */
    float mast_top_y;            /* interact platform height */
    int   mast_synced;           /* climbed + synced → fog of war lifted */
    Vec3  reinforce[2];          /* wave spawn points */
    GarrisonSlot garrison[GARRISON_MAX];
    int   garrison_n;
} Outpost;

/* Ambient wildlife (§4.2: two island species in the vertical slice).
   0 = venado (deer) · 1 = jabalí (boar). Graze → alert → flee FSM. */
#define CRITTER_MAX 20
typedef struct {
    Vec3  pos, vel;
    Vec3  home, goal;            /* wander anchor + current walk target */
    float yaw;
    int   species;
    int   state;                 /* 0 wander/graze · 1 alert · 2 flee */
    float state_t;
    float hop;                   /* gait bob phase */
} Critter;

/* Fog-of-war minimap: coarse world mask (Spec §52 fog of war). */
#define MAPW 64                  /* 64×64 cells over the 1 km island */


/* ── combat pickups + transient FX (M2) ── */
#define PICKUP_MAX 64
typedef enum { PK_NONE = 0, PK_HEALTH, PK_AMMO, PK_HERB, PK_SCRAP } PickupKind;
typedef struct {
    Vec3  pos;
    int   kind;              /* PickupKind */
    int   ammo;              /* AmmoType when kind==PK_AMMO */
    float amount;            /* hp or rounds */
    int   live;
    float bob;               /* idle animation phase */
} Pickup;
typedef struct { Pickup v[PICKUP_MAX]; int count; } PickupSet;

#define TRACER_MAX 64
typedef struct {
    Vec3  a, b;
    float life;              /* seconds remaining */
    uint32_t color;
} Tracer;
typedef struct { Tracer v[TRACER_MAX]; int count; } TracerSet;

/* ── M7 polish: audio cue edge-detection + ambient birds + camera FX ── */
#define BIRD_FLOCKS 3
#define BIRDS_PER   9
typedef struct {
    Vec3  home;          /* orbit centre (world) */
    Vec3  off;           /* current flock offset from home (scatter) */
    Vec3  vel;
    float radius, phase, speed, scatter_t;
} Flock;
typedef struct {
    int   inited;
    int   prev_slot, prev_mag, prev_reload, prev_jumps, prev_land;
    int   prev_card, prev_done, prev_alarm, prev_stars, prev_ui, prev_mode;
    float prev_hit, prev_dmg, step_dist, siren_t, bird_t;
    float shake;         /* camera shake amplitude (m), decays */
    int   cues;          /* total cues fired (tests) */
    float intensity;     /* last music intensity target */
    Flock flock[BIRD_FLOCKS];
    int   scatters;      /* times a flock was spooked (tests) */
} Polish;

typedef struct Game {
    /* ── world ── */
    Terrain      terrain;
    ObstacleSet  obs;
    ZiplineSet   zips;          /* 6.1 zipline cables */
    Vec3         spawn;
    Vec3         course_center;
    float        pad_h;              /* traversal-course deck height */
    Vec3         pool_center;
    float        pool_level, pool_rx, pool_rz;
    float        ocean_tile;

    /* ── player ── */
    Player       player;

    /* ── scene meshes (built once, reused) ── */
    int          mesh_box;           /* unit cube -0.5..0.5 */
    int          mesh_sky;           /* unit hemisphere, vertex-colour gradient */
    int          mesh_quad;          /* unit XZ quad, uv 0..1 */

    PropDraw     props[GAME_MAX_PROPS];
    int          prop_count;

    /* ── frame state ── */
    GameMode     mode;
    float        time;               /* seconds since boot */
    float        day_t;              /* 0..1 day phase (Spec 6.7) */
    int          w, h;
    int          frame_index;
    SceneLight   light;
    Mat4         view, proj;
    Vec3         cam_pos, cam_dir;
    float        cam_fov;

    /* ── hud ── */
    int          show_debug;
    int          show_hud;
    float        message_t;
    char         message[160];
    float        respawn_t;
    int          respawn_keep_inv;   /* M2: death keeps your loadout (§6.2) */

    /* ── combat (M2) ── */
    Rng          rng;
    EnemySet     enemies;
    int          wpn_slot;                 /* active slot 0..WPN_SLOT_MAX-1 */
    int          wpn_def[WPN_SLOT_MAX];    /* weapon table index per slot, -1 empty */
    int          wpn_mag[WPN_SLOT_MAX];    /* rounds in each magazine */
    int          ammo[AMMO_N];             /* reserve pool per ammo type */
    float        fire_cd;                  /* seconds until next shot allowed */
    int          reloading;
    int          reload_slot;
    float        reload_t;
    float        ads_k;                    /* 0..1 aim-down-sights blend */
    int          shot_index;               /* recoil pattern counter */
    float        rec_pitch, rec_yaw;       /* decaying view punch */
    float        noise_t;                  /* player-made noise (gunfire) timer */
    float        hitmark_t;                /* hitmarker flash */
    int          hitmark_kill;             /* last hitmarker was a kill (red X) */
    float        damage_flash;             /* red vignette on taking a hit */
    int          kills;
    int          arena_active;
    int          arena_total;
    Vec3         arena_center;

    /* pickups + transient FX (tracers, impact particles) */
    PickupSet    pickups;
    TracerSet    tracers;
    Particle     fx[256];
    int          fx_count;

    /* ── island content (M3) ── */
    Outpost      outpost;                 /* vertical-slice capture outpost */
    Critter      critters[CRITTER_MAX];   /* ambient wildlife (2 species) */
    int          critter_count;
    uint8_t      fog[MAPW * MAPW];        /* 0 unexplored · 1 seen · 2 synced */
    uint32_t     map_cell[MAPW * MAPW];   /* cached terrain colours (ABGR) */
    int          binocs;                  /* binocular view active this frame */
    float        binoc_k;                 /* 0..1 zoom blend */
    int          tagged_count;            /* enemies currently tagged */
    float        melee_cd;                /* takedown/swing cooldown */

    /* ── city act (M4, Spec 16) ── */
    int          act;            /* 0 = Isla Sombra · 1 = Meridian City */
    City         city;
    int          in_vehicle;     /* index into city.veh, -1 on foot */
    int          cash;           /* $ — jobs, looted wallets; hot dogs cost 5 */
    /* ── systems & progression (M5, §70–§72) ── */
    Progress     prog;           /* xp, skills, materials, alert, fast travel */
    Economy      econ;           /* vendors + recipes (data/economy.json) */
    int          ui;             /* GameUi — modal screens pause the sim */
    int          ui_sel, ui_tab, ui_vendor;
    /* M6: story missions (Spec §73) */
    int          story;          /* 1 = campaign runs (off in tests / DoD modes) */
    MissionSet   missions;
    MissionState ms;
    int          stat_herbs, stat_buys, stat_rests, stat_ferries;
    int          card_page, card_result, card_pick;
    float        mission_banner_t;
    char         mission_banner[96];
    float        armor;          /* 0..100 plate pool, absorbs 60% of damage */
    Vec3         safehouse[2];   /* act 0 / act 1 safehouse doors */
    int          saves_written;

    /* ── perf / config ── */
    float        fps_smooth;
    float        sim_ms, render_ms;
    int          backend;
    uint32_t     seed;
    int          ready;
    int          quit;
    Polish       pol;                     /* M7 */
} Game;

int   game_init(Game *g, int w, int h, int backend, uint32_t seed);
void  game_free(Game *g);
void  game_start_play(Game *g);
/* M2 combat-arena test/spawn API: teleport the player to the arena, grant the
   starting loadout, and populate `n` hostiles. Used by smoke_combat + demo. */
void  game_arena_start(Game *g, int n_enemies);
float pol_diff_damage_pub(void);          /* M7: difficulty damage multiplier */

/* M3 island-slice API. outpost_start teleports the player to the beach south
   of the outpost, grants the loadout, and seeds the garrison — the DoD path
   (scout → plan → capture three ways) runs from there. */
void  game_outpost_start(Game *g);
/* Day/night (Spec 6.7): day_t ∈ [0,1), 0 = 06:00, 0.25 = noon, 0.75 = midnight.
   day_palette is pure (unit-testable); game_set_time applies it to the world. */
void  day_palette(float day_t, SceneLight *L);
void  game_set_time(Game *g, float day_t);
void  game_frame(Game *g, const PlatInput *in, float dt);   /* simulate only */
/* ── M5 systems API (also driven by tests/smoke_systems.c) ── */
void  game_apply_skills(Game *g);                /* push skill mods into player */
int   game_story_waypoint(Game *g, Vec3 *out);  /* M6: current mission waypoint */
int   game_ammo_cap(const Game *g, int ammo);    /* reserve cap incl. pack mule/pouch */
int   game_shop_buy(Game *g, int vendor, int offer);   /* 0 ok, <0 error */
int   game_fast_travel(Game *g, int node);       /* 0 ok, <0 error (see progress.h) */
int   game_use_heal(Game *g);                    /* 1 if a medkit/bandage was used */
int   game_skin_nearest(Game *g);                /* hides gained (0 = nothing in reach) */
int   game_safehouse_rest(Game *g);              /* 1 = healed + saved */
int   game_save(Game *g, int slot, int is_auto); /* 1 ok */
int   game_load(Game *g, int slot, int is_auto); /* 1 ok */
void  game_island_store(Game *g);                /* snapshot island before the ferry */
int   game_map_reveal_pct(const Game *g);
void  game_render(Game *g);                                 /* submit + draw */
void  game_message(Game *g, const char *fmt, ...);
/* M4: act-II world swap (Spec 4.3 — one streaming world per act). Frees the
   island world and rebuilds Meridian City on the same heightfield footprint. */
void  game_load_city(Game *g);
void  game_load_island(Game *g);   /* ferry back to act I */
/* Public wrappers over the static FX helpers (city.c and future acts use them). */
void  game_fx(Game *g, Vec3 pos, Vec3 dir, int n, uint32_t color,
              float speed, float life, int kind);
void  game_tracer(Game *g, Vec3 a, Vec3 b, uint32_t color);
void  game_apply_settings(Game *g);
const char *game_mode_name(GameMode m);
/* Traversal-course element table (also printed in the M1 report). */
int   game_course_element_count(void);
const char *game_course_element_name(int i);
const char *game_course_element_tests(int i);

#endif /* DH_GAME_H */
