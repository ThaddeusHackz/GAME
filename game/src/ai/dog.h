/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — Poncho, the companion dog (M9, Spec companion section)
   Pure simulation (no rendering / audio / Game access) so it is unit-tested
   headless. The game layer feeds a DogWorld snapshot, consumes DogEvents.
   Guarantees (tested): never dies (0 hp → knocked out), a lost dog is
   teleported back within DOG_LOST_TELEPORT s (< 8 s), knockout recovers with
   a 3 s revive hold or a 30 s auto-recover.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_DOG_H
#define DH_DOG_H
#include "../core/dh_types.h"

typedef enum { DOG_TRAPPED = 0, DOG_FOLLOW, DOG_STAY, DOG_HUNT, DOG_DISTRACT, DOG_KO } DogState;
typedef enum { DOG_CMD_FOLLOW = 0, DOG_CMD_STAY, DOG_CMD_HUNT, DOG_CMD_N } DogCmd;

#define DOG_LOST_DIST      28.f   /* farther than this counts as "lost" */
#define DOG_LOST_TELEPORT  6.f    /* seconds lost before a safety teleport */
#define DOG_FAR_SNAP       120.f  /* act change / fast travel: snap at once */
#define DOG_REVIVE_HOLD    3.f
#define DOG_AUTO_RECOVER   30.f
#define DOG_HP_MAX         100.f

enum { DOG_EV_BARK = 1, DOG_EV_JINGLE = 2, DOG_EV_TELEPORT = 4, DOG_EV_KO = 8,
       DOG_EV_REVIVED = 16, DOG_EV_ARRIVED = 32, DOG_EV_FREED = 64 };

typedef float (*DogGroundFn)(void *ud, float x, float z);

typedef struct {
    Vec3  player;           /* feet */
    float player_yaw;
    int   reviving;         /* player is holding USE next to a KO'd dog */
    DogGroundFn ground; void *ud;
} DogWorld;

typedef struct {
    Vec3  pos, vel;
    float yaw;
    int   state, cmd;
    float hp;
    float state_t;
    float lost_t;           /* consecutive seconds lost */
    float revive_t;         /* revive hold progress */
    Vec3  stay_pos, goal;   /* STAY anchor / DISTRACT target */
    float bark_cd, step_acc, gait;
    float pet_cd;
    int   bond_xp;
    /* stats (tests / save) */
    int   teleports, knockouts, revives, barks, pets, distracts;
} Dog;

void  dog_init(Dog *d, Vec3 pos, int trapped);
int   dog_update(Dog *d, const DogWorld *w, float dt);   /* returns DOG_EV_* mask */
void  dog_free(Dog *d);                  /* TRAPPED → FOLLOW */
void  dog_command(Dog *d, int cmd);
int   dog_cycle_command(Dog *d);         /* follow → stay → hunt; returns new cmd */
int   dog_distract(Dog *d, Vec3 target); /* 1 if sent */
int   dog_damage(Dog *d, float amt);     /* 1 if this knocked him out */
int   dog_pet(Dog *d);                   /* 1 if bond grew (20 s cooldown) */
int   dog_bond_level(const Dog *d);      /* 1..5 */
float dog_sniff_radius(const Dog *d);    /* 20 m + 2 m per bond level above 1 */
int   dog_active(const Dog *d);          /* freed and not KO */
const char *dog_state_name(int st);
const char *dog_cmd_name(int cmd);
#endif
