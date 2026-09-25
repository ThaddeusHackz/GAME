/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — SaveManager (Spec 10.5, 6.11, 37.4)
   100% local JSON in user://, never in the install dir. 3 auto + 10 manual
   slots. Atomic writes. Corrupt-save auto-repair. No telemetry, ever.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_SAVE_H
#define DH_SAVE_H

#include "dh_types.h"

#define SAVE_AUTO_SLOTS   3
#define SAVE_MANUAL_SLOTS 10
#define SAVE_TOTAL_SLOTS  (SAVE_AUTO_SLOTS + SAVE_MANUAL_SLOTS)

#define SAVE_MAX_FLAGS        512   /* quest / story flags            */
#define SAVE_MAX_MISSIONS      64   /* 11 story + 30 side + bosses    */
#define SAVE_MAX_OUTPOSTS      15   /* 12 outposts + 3 strongholds    */
#define SAVE_MAX_WEAPONS       12
#define SAVE_MAX_INVENTORY     40
#define SAVE_MAX_COLLECT       64   /* relic / tag / photo / letter   */
#define SAVE_MAX_VEHICLES      16   /* owned/garaged                  */
#define SAVE_MAX_COMPANIONS     6
#define NAME_LEN               48
#define PLAYTHROUGH_ID_LEN     24

typedef enum {
    PS_DEAD = 0, PS_ALIVE, PS_BLEEDOUT, PS_DOWNED, PS_ARRESTED, PS_CAPTURED
} PlayerState;

typedef enum {
    MISSION_LOCKED = 0, MISSION_AVAILABLE, MISSION_ACTIVE, MISSION_COMPLETE, MISSION_FAILED
} MissionStatus;

typedef struct {
    char  id[32];
    int   status;         /* MissionStatus */
    int   phase;          /* sub-objective index */
    int   rank;           /* 0 none, 1..3 stars */
    float time_best;
    int   attempts;
    int   stealth_complete;
} SaveMission;

typedef struct {
    char  id[24];
    int   captured;        /* 0 hostile, 1 captured, 2 lost */
    int   alarm_level;     /* 0..3 (Spec 24.6) */
    int   cleared_stealth;
    float capture_time;
    int   commander_alive;
} SaveOutpost;

typedef struct {
    char id[32];
    int  value;
} SaveFlag;

typedef struct {
    char  id[24];
    int   count;
    int   total;
} SaveCollect;

typedef struct {
    int   slot;                 /* -1 = none */
    int   is_auto;
    char  name[NAME_LEN];
    char  playthrough_id[PLAYTHROUGH_ID_LEN];
    char  timestamp[32];        /* ISO-8601 UTC */
    double real_timestamp;      /* epoch seconds for sorting */
    float playtime_sec;
    int   version_major, version_minor, version_patch;
    int   difficulty;
    int   mutators[8];
    int   ng_plus;
    int   ng_plus_allowed;

    /* world */
    int   map;                  /* 0 island, 1 city */
    float time_of_day;          /* 0..1 */
    float day_count;
    int   weather;
    float weather_intensity;
    float island_heat;          /* Spec 13.6 */
    float city_heat;            /* Spec 14.6 */
    int   wanted_level;
    float wanted_decay_timer;
    int   district_flags[16];

    /* player */
    Vec3  pos; Vec3 vel;
    float yaw, pitch, roll;
    int   state;                /* PlayerState */
    float health, health_max;
    float armor, armor_max;
    float stamina, stamina_max;
    float breath;
    int   money;
    int   level, xp;
    int   skill_points;
    unsigned int skills_unlocked;
    int   stealth_rank;
    int   in_vehicle;
    char  vehicle_id[24];
    Vec3  checkpoint;           /* Spec 37.4: death → nearest checkpoint */
    int   companions_active;
    int   companion_ids[SAVE_MAX_COMPANIONS];
    float companion_trust[SAVE_MAX_COMPANIONS];

    /* inventory */
    int   weapon_ids[SAVE_MAX_WEAPONS];
    int   weapon_ammo[SAVE_MAX_WEAPONS];
    int   weapon_reserve[SAVE_MAX_WEAPONS];
    int   weapon_unlocked[SAVE_MAX_WEAPONS];
    int   weapon_count;
    int   equipped_slot;
    char  inv_ids[SAVE_MAX_INVENTORY][24];
    int   inv_counts[SAVE_MAX_INVENTORY];
    int   inv_count;
    int   grenades[4];

    /* progress tables */
    SaveMission   missions[SAVE_MAX_MISSIONS];   int mission_count;
    SaveOutpost   outposts[SAVE_MAX_OUTPOSTS];   int outpost_count;
    SaveFlag      flags[SAVE_MAX_FLAGS];         int flag_count;
    SaveCollect   collect[SAVE_MAX_COLLECT];     int collect_count;
    char          vehicles[SAVE_MAX_VEHICLES][24]; int vehicle_count;

    /* stats (Spec 10.10 stats screen — no telemetry, local only) */
    struct {
        long long enemies_defeated, stealth_takedowns, headshots, melee_kills;
        long long distance_foot_m, distance_vehicle_m, distance_air_m, distance_sea_m;
        long long photos_taken, relics_found, tags_done, letters_read;
        long long vehicles_stolen, vehicles_destroyed, jumps_landed;
        long long poncho_pets, poncho_saves, dog_fetches;
        long long missions_complete, side_complete, outposts_captured;
        long long deaths, arrests, saves_loaded;
        long long longest_stealth_chain, highest_wanted, biggest_jump_m;
        long long money_earned, money_spent;
        float     best_drift_m, longest_glide_s;
    } stats;

    /* accessibility + settings snapshot so a save is self-describing */
    int   photosensitivity;
    int   subtitles;
    int   colorblind_mode;
    int   language;
    int   difficulty_labels_shown;
} SaveGame;

void save_init(void);
void save_shutdown(void);

SaveGame *save_active(void);                 /* the live working save */
void      save_new_game(SaveGame *s, int difficulty, int map);
int       save_write_slot(int slot, int is_auto, const SaveGame *s);
int       save_read_slot(int slot, int is_auto, SaveGame *out);
int       save_delete_slot(int slot, int is_auto);
/* Slot index → absolute slot number (autos are 0..2, manual 3..12). */
int       save_slot_index(int slot, int is_auto);
int       save_slot_exists(int abs_slot);
/* Fill a lightweight header list for the Load/Save UI without parsing bodies. */
typedef struct {
    int   abs_slot, exists, is_auto;
    char  name[NAME_LEN];
    char  timestamp[32];
    char  map_name[24];
    float playtime_sec;
    int   difficulty;
    int   mutators_on;
    int   ng_plus;
    int   corrupt;
} SaveHeader;
int       save_list_headers(SaveHeader *out, int max);

void      save_set_flag(SaveGame *s, const char *id, int value);
int       save_get_flag(const SaveGame *s, const char *id, int dflt);
int       save_has_flag(const SaveGame *s, const char *id);
SaveMission *save_mission(SaveGame *s, const char *id);
SaveOutpost *save_outpost(SaveGame *s, const char *id);
SaveCollect *save_collect(SaveGame *s, const char *id);
void      save_add_stat_ll(long long *field, long long delta);
void      save_autosave_tick(float dt);       /* called from game loop */
void      save_autosave(const char *reason);  /* Spec 6.11: checkpoints, outpost capture, mission end */
char     *save_serialize(const SaveGame *s);  /* owned JSON text (or NULL) */
int       save_deserialize(SaveGame *out, const char *json_text);
void      save_playtime_add(float dt);
int       save_is_ng_plus_blocked(void);

#endif /* DH_SAVE_H */
