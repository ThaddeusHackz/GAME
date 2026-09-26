/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — enemy AI (M2, Spec 6.2 combat / §82 detection model)
   Deterministic FSM: PATROL → SUSPICIOUS → SEARCH → COMBAT → DEAD (+FLEE).
   Detection meter: rate = base / (dist² · light · stance), thresholds 0.35
   SUSPICIOUS / 0.8 COMBAT, decay 0.2/s, team share within 8 m (§82).
   AI never touches rendering; the game layer draws from the public struct.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_ENEMY_H
#define DH_ENEMY_H

#include "../core/dh_types.h"
#include "../world/terrain.h"
#include "../world/collision.h"

#define ENEMY_MAX 64

typedef enum {
    EN_PATROL = 0, EN_SUSPICIOUS, EN_SEARCH, EN_COMBAT, EN_FLEE, EN_DEAD
} EnemyState;

typedef enum { EN_GRUNT = 0, EN_BRUISER, EN_OFFICER } EnemyArch;

typedef struct {
    /* transform */
    Vec3  pos;              /* feet */
    float yaw;              /* facing (rad) */

    /* identity */
    int   arch;             /* EnemyArch */
    int   faction;          /* 0 = cult (fights to death) */
    float health, health_max;

    /* fsm */
    int   state;            /* EnemyState */
    float meter;            /* detection meter 0..1+ */
    Vec3  lkp;              /* last known player position */
    float lkp_age;
    Vec3  patrol_a, patrol_b;
    int   patrol_leg;       /* heading toward b (1) or a (0) */
    float state_t;          /* time in current state */

    /* combat */
    float fire_cd;          /* seconds until next allowed shot */
    int   burst_left;       /* rounds remaining in current burst */
    float burst_pause;
    int   shot_this_frame;  /* set by update, consumed by game layer */
    float shot_acc;         /* 0..1 hit chance for the shot (dist/light based) */
    float noise_t;          /* recent gunshot noise nearby (seconds left) */

    /* movement */
    Vec3  vel;
    float speed;
    int   grounded;
    float stray_t, stray_dir; /* combat sidestep oscillator */

    /* feedback */
    float hit_t;            /* hitmarker flash timer */
    int   dropped;          /* loot-drop event flag (consumed by game) */
    int   alerted_team;     /* already shared detection with squad */
} Enemy;

typedef struct {
    Enemy v[ENEMY_MAX];
    int   count;
    int   kills;
    int   alive_count;
} EnemySet;

/* Player snapshot the AI is allowed to see (keeps modules decoupled). */
typedef struct {
    Vec3  eye;
    int   crouched;
    int   moving;           /* > walk speed */
    int   alive;
    float light;            /* 0 dark .. 1 bright (M2: constant daylight) */
    float noise;            /* 0..1 recent player noise (shots, sprint) */
} EnemyView;

void  enemies_init(EnemySet *s);
int   enemies_spawn(EnemySet *s, Vec3 pos, int arch, int faction,
                    Vec3 patrol_a, Vec3 patrol_b);
void  enemies_update(EnemySet *s, const EnemyView *pv, float dt,
                     const Terrain *t, const ObstacleSet *ob);

/* Perception helpers (pure — unit-testable). */
float enemy_detect_rate(const Enemy *e, const EnemyView *pv);
int   enemy_state_from_meter(float meter);          /* current-state aware */
float enemy_accuracy(const Enemy *e, const EnemyView *pv, float dist);

/* Damage application. Returns 1 if this hit killed. zone: HitZone. */
int   enemy_apply_damage(Enemy *e, float dmg, int zone);

const char *enemy_state_name(int st);
const char *enemy_arch_name(int arch);
float enemy_shot_damage(int arch);        /* per-bullet damage an enemy deals */

#endif /* DH_ENEMY_H */
