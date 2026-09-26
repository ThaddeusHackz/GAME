/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — Player traversal controller
   "Traversal is joy" (P1). Capsule controller with the Spec 6.1 feel kit:
     coyote time 0.12s · jump buffer 0.15s · variable jump height
     step 1.2m auto · vault 2.2m · ledge grab 4.0m (M1b)
     slope slide > 0.62 steep · fall damage > 4m with 0.35s roll-cancel
     swim/tread/dive with breath 30s base (G04 upgrade → 55s)
   Gravity is deliberately snappier than real (24 m/s²): responsive beats
   realistic for an action game, and it keeps air-time readable.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_PLAYER_H
#define DH_PLAYER_H

#include "../core/dh_types.h"
#include "../world/terrain.h"
#include "../world/collision.h"

/* ---- tuning (Spec 6.1, Section 14 accessibility-adjustable later) ---- */
#define PL_GROUND_ACCEL     46.0f
#define PL_AIR_ACCEL        13.0f
#define PL_GROUND_FRICTION  11.0f
#define PL_GRAVITY          24.0f
#define PL_TERMINAL_VEL     55.0f
#define PL_JUMP_HEIGHT      1.35f
#define PL_JUMP_VEL         8.05f     /* sqrt(2 * g * h) */
#define PL_JUMP_CUT         0.45f     /* release early → shorter hop */
#define PL_COYOTE           0.12f
#define PL_JUMP_BUFFER      0.15f
#define PL_STEP_HEIGHT      1.20f
#define PL_VAULT_HEIGHT     2.20f
#define PL_LEDGE_HEIGHT     4.00f     /* M1b: ledge grab + shimmy */
#define PL_SLIDE_STEEP      0.62f
#define PL_FALL_SAFE_M      4.0f
#define PL_FALL_DMG_PER_M   12.0f
#define PL_ROLL_CANCEL      0.35f
#define PL_RADIUS           0.34f
#define PL_HEIGHT           1.80f
#define PL_CROUCH_HEIGHT    1.05f
#define PL_EYE_STAND        1.66f
#define PL_EYE_CROUCH       0.95f
#define PL_EYE_SWIM         0.35f
#define PL_BREATH_BASE      30.0f
#define PL_BREATH_G04       55.0f
#define PL_STAMINA_MAX      100.0f
#define PL_STAMINA_DRAIN    15.0f
#define PL_STAMINA_REGEN    13.0f
#define PL_STAMINA_DELAY    0.70f

typedef enum {
    PL_ST_GROUND = 0,
    PL_ST_AIR,
    PL_ST_SWIM,
    PL_ST_MANTLE,     /* vaulting/climbing over an obstacle */
    PL_ST_SLIDE,      /* steep slope slide */
    PL_ST_ZIP,        /* riding a zipline cable (6.1) */
    PL_ST_DOWN        /* health 0 — respawn handled by game layer */
} PlayerStance;

typedef struct {
    float mx, my;          /* move axes: mx = strafe, my = forward (-1..1) */
    float look_dx, look_dy;/* raw look delta (already sensitivity-scaled) */
    int   jump;            /* held */
    int   jump_pressed;    /* edge this frame */
    int   sprint, crouch, roll;
    int   use_pressed;     /* edge: interact / mount zipline */
    int   walk_only;       /* accessibility / stealth preference */
    int   reload_unused;
} PlayerInput;

typedef struct {
    /* transform */
    Vec3  pos;             /* FEET position */
    Vec3  vel;
    float yaw, pitch;      /* radians */
    float eye;             /* smoothed eye height above feet */

    /* state */
    PlayerStance stance;
    int   crouched;
    int   grounded;
    float ground_y;        /* resolved support height under feet */
    Vec3  ground_normal;
    float steepness;
    int   in_water;        /* body submerged enough to swim */

    /* feel timers */
    float coyote;          /* time left we may still jump after leaving ground */
    float jump_buffer;     /* buffered press before landing */
    float fall_apex;       /* highest point since leaving the ground (fall dmg) */
    int   blocked;         /* last frame: horizontal push-out happened */
    float block_top;       /* top of the volume that blocked us */
    float mantle_t, mantle_dur;
    /* zipline ride state (6.1) */
    int   zip_i;           /* cable index in `zips` */
    float zip_s;           /* arc length along cable (m) */
    float zip_speed;       /* m/s along cable (+ = a→b) */
    float zip_sway, zip_sway_v;   /* lateral pendulum (m, m/s) */
    Vec3  mantle_from, mantle_to;
    float slide_t;
    float stamina, stamina_idle;
    float breath, breath_max;
    int   has_g04;         /* deep-lungs upgrade */

    /* combat-agnostic vitals for M1 (M2 extends) */
    float health;

    /* instrumentation — tests assert on these */
    struct {
        int   jumps, mantles, vaults, slides, landings, zip_rides;
        float max_fall_m, fall_damage_taken, distance_m, zip_m;
        int   swim_time_s, breathouts;
        float last_jump_height;
    } stats;

    /* world refs */
    const Terrain     *terrain;
    const ObstacleSet *obs;
    const ZiplineSet  *zips;
} Player;

void  player_init(Player *p, const Terrain *t, const ObstacleSet *obs, Vec3 spawn);
void  player_set_spawn(Player *p, Vec3 spawn);
void  player_input(Player *p, const PlayerInput *in, float dt);
float player_eye_y(const Player *p);
Vec3  player_forward(const Player *p);
void  player_camera(const Player *p, Vec3 *out_pos, Vec3 *out_dir);
/* Is the player dead / needs respawn? */
int   player_is_down(const Player *p);
void  player_respawn(Player *p, Vec3 spawn);
const char *player_stance_name(PlayerStance s);

#endif /* DH_PLAYER_H */
