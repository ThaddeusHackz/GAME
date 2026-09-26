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

#define GAME_MAX_PROPS 640

typedef enum { GM_MENU = 0, GM_PLAY, GM_PAUSE, GM_PHOTO } GameMode;

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

/* ── combat pickups + transient FX (M2) ── */
#define PICKUP_MAX 48
typedef enum { PK_NONE = 0, PK_HEALTH, PK_AMMO } PickupKind;
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

typedef struct {
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

    /* ── perf / config ── */
    float        fps_smooth;
    float        sim_ms, render_ms;
    int          backend;
    uint32_t     seed;
    int          ready;
    int          quit;
} Game;

int   game_init(Game *g, int w, int h, int backend, uint32_t seed);
void  game_free(Game *g);
void  game_start_play(Game *g);
/* M2 combat-arena test/spawn API: teleport the player to the arena, grant the
   starting loadout, and populate `n` hostiles. Used by smoke_combat + demo. */
void  game_arena_start(Game *g, int n_enemies);
void  game_frame(Game *g, const PlatInput *in, float dt);   /* simulate only */
void  game_render(Game *g);                                 /* submit + draw */
void  game_message(Game *g, const char *fmt, ...);
void  game_apply_settings(Game *g);
const char *game_mode_name(GameMode m);
/* Traversal-course element table (also printed in the M1 report). */
int   game_course_element_count(void);
const char *game_course_element_name(int i);
const char *game_course_element_tests(int i);

#endif /* DH_GAME_H */
