/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — story missions (M6, Spec §73)
   Data-driven: game/data/missions.json (embedded copy as fallback) lists the
   11 main missions as ordered objectives built ONLY from mechanics that
   already exist (traversal, outpost capture, mast sync, hunting, crafting,
   vendors, safehouses, ferry, car theft, heat). Pure state machine: no Game
   dependency — game.c fills a MissionCtx each frame and reacts to events.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_MISSION_H
#define DH_MISSION_H

#include "../core/dh_types.h"

typedef enum {
    OBJ_CARD = 0,   /* dialogue / text card — modal, E advances            */
    OBJ_CHOICE,     /* card with two options (1 / 2)                         */
    OBJ_GOTO,       /* reach anchor/xz within r                              */
    OBJ_HERBS,      /* collect n herbs                                       */
    OBJ_HUNT,       /* skin n animals                                        */
    OBJ_CRAFT,      /* craft n items                                         */
    OBJ_BUY,        /* buy n items at any vendor                             */
    OBJ_REST,       /* rest at a safehouse                                   */
    OBJ_KILLS,      /* n kills                                               */
    OBJ_CAPTURE,    /* liberate the outpost                                  */
    OBJ_MAST,       /* sync the signal mast                                  */
    OBJ_FERRY,      /* arrive in act `act` by ferry                          */
    OBJ_DRIVE,      /* be driving a vehicle (optionally within r of target)  */
    OBJ_HEAT,       /* reach n stars                                         */
    OBJ_LOSE_HEAT,  /* get back to 0 stars                                   */
    OBJ_SURVIVE,    /* stay free for n seconds with ≥1 star                  */
    OBJ_JOB,        /* finish n odd jobs                                     */
    OBJ_ASSAULT,    /* arrive → game spawns n cult hostiles; kill them all   */
    OBJ_KIND_N
} ObjKind;

typedef enum {
    ANC_XZ = 0, ANC_BEACH, ANC_SAFEHOUSE_I, ANC_VENDOR_I, ANC_OUTPOST, ANC_MAST,
    ANC_LIGHTHOUSE, ANC_PIER, ANC_SAFEHOUSE_C, ANC_VENDOR_C, ANC_JOB_BOARD,
    ANC_N
} Anchor;

#define MIS_MAX        12
#define MIS_OBJ_MAX    14
#define MIS_LINES_MAX  6
#define MIS_LINE_LEN   200

typedef struct {
    int   kind, act;              /* act -1 = either */
    int   anchor;  float x, z, r;
    int   n;
    char  text[80];               /* tracker line */
    char  speaker[32];
    char  lines[MIS_LINES_MAX][MIS_LINE_LEN]; int line_n;
    char  opt[2][48];             /* OBJ_CHOICE */
    char  result[2][MIS_LINE_LEN];/* OBJ_CHOICE: line shown after picking */
    int   sets_ending;            /* OBJ_CHOICE: pick → ending 1 / 2 */
} Objective;

typedef struct {
    char id[8], title[48], logline[120];
    Objective obj[MIS_OBJ_MAX]; int obj_n;
    int  cash, xp;
} MissionDef;

typedef struct {
    MissionDef m[MIS_MAX]; int n;
    int from_file;
} MissionSet;

/* What the game observes this frame (counters are cumulative totals). */
typedef struct {
    int   act;
    Vec3  pos;
    Vec3  anchor[ANC_N];
    int   herbs, hunts, crafts, buys, rests, kills, jobs, ferries;
    int   outpost_captured, mast_synced;
    int   stars, driving;
    int   assault_left;           /* -1 when no assault is running */
} MissionCtx;

typedef enum {
    MEV_NONE = 0, MEV_STARTED, MEV_OBJ_DONE, MEV_CARD, MEV_ASSAULT,
    MEV_MISSION_DONE, MEV_ALL_DONE
} MissionEvent;

typedef struct {
    int   cur;          /* mission index, n = campaign finished */
    int   obj;          /* objective index inside mission */
    int   active;       /* 0 = waiting between missions */
    float wait_t;       /* delay before the next mission opens */
    int   base;         /* counter snapshot at objective start */
    float timer;        /* OBJ_SURVIVE */
    int   assault_started;
    int   card_open;    /* game must show the current card */
    int   choices[MIS_MAX];   /* 0 none, 1/2 = option picked */
    int   ending;       /* 0 none, 1 = Tobias's tape, 2 = the Ledger */
    int   done_mask;
    int   pending_assault;    /* n hostiles the game should spawn */
} MissionState;

int   missions_load(MissionSet *s);                 /* 1 data file · 0 embedded */
int   missions_parse(MissionSet *s, const char *json);   /* count parsed */
const char *obj_kind_name(int k);

void  mission_reset(MissionState *st);
MissionEvent mission_tick(MissionState *st, const MissionSet *s, const MissionCtx *c, float dt);
MissionEvent mission_card_done(MissionState *st, const MissionSet *s, const MissionCtx *c, int choice);
void  mission_resume(MissionState *st, const MissionSet *s, const MissionCtx *c);  /* after load */
void  mission_on_death(MissionState *st, const MissionSet *s, const MissionCtx *c);
const Objective *mission_objective(const MissionState *st, const MissionSet *s);
const MissionDef *mission_current(const MissionState *st, const MissionSet *s);
/* Tracker text for the HUD (includes live counters / ferry hints). */
void  mission_tracker(const MissionState *st, const MissionSet *s, const MissionCtx *c,
                      char *out, int cap);
/* Waypoint (xz) for the current objective; returns 0 if none / wrong act. */
int   mission_waypoint(const MissionState *st, const MissionSet *s, const MissionCtx *c, Vec3 *out);

#endif
