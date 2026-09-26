/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — progression & economy (M5, Spec §70–§72)
   Pure-data layer: XP/levels, the 24-skill tree, wallet + vendors, crafting,
   materials, island alert level, fast-travel nodes and the island state that
   has to survive Beto's ferry. No Game dependency, so the rules are unit
   tested headless (tests/smoke_systems.c) and game.c only calls in.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_PROGRESS_H
#define DH_PROGRESS_H

#include "../core/dh_types.h"

/* ── XP / levels (§71: xp to next = 100 + 50·(n-1)) ── */
#define PROG_LEVEL_CAP 30
int  prog_xp_to_next(int level);           /* xp needed to go level → level+1 */

/* ── skills: 4 trees × 6 = 24 (§71) ── */
typedef enum { TREE_SURVIVOR = 0, TREE_HUNTER, TREE_GHOST, TREE_DRIVER, TREE_N } SkillTree;
#define SKILL_N 24

typedef enum {
    /* SURVIVOR */ SK_TOUGH_HIDE = 0, SK_DEEP_LUNGS, SK_SECOND_WIND, SK_FIELD_MEDIC, SK_IRON_STOMACH, SK_LAST_STAND,
    /* HUNTER   */ SK_STEADY_HANDS, SK_QUICK_HANDS, SK_SKINNER, SK_EAGLE_EYE, SK_DEADEYE, SK_PACK_MULE,
    /* GHOST    */ SK_SOFT_STEP, SK_CHAIN_TAKEDOWN, SK_WIRE_CUTTER, SK_NIGHT_OWL, SK_GHOST_PROTOCOL, SK_SMOOTH_TALKER,
    /* DRIVER   */ SK_WHEELMAN, SK_ROAD_ARMOR, SK_JACKER, SK_LOW_PROFILE, SK_FIXER, SK_LAUNDERER
} SkillId;

typedef struct {
    const char *id, *name, *desc;
    int tree, tier;          /* tier 1..3 */
    int cost;                /* skill points */
    int min_level;
} SkillDef;

const SkillDef *skill_def(int i);
const char     *skill_tree_name(int t);

/* Skill-driven multipliers game systems read (1.0 = no effect). */
typedef enum {
    MOD_MAX_HP = 0,     /* additive hp bonus returned as multiplier of 100 */
    MOD_BREATH, MOD_STAMINA, MOD_HEAL, MOD_FOOD,
    MOD_RECOIL, MOD_RELOAD, MOD_HIDES, MOD_TAG_TIME, MOD_HEADSHOT, MOD_RESERVE,
    MOD_NOISE, MOD_ALARM_RADIUS, MOD_PRICE, MOD_ALERT_GAIN,
    MOD_TOP_SPEED, MOD_VEH_DAMAGE, MOD_WALLET, MOD_HEAT_ESCAPE, MOD_JOB_PAY, MOD_EXCHANGE,
    MOD_N
} SkillMod;

/* ── materials + consumables ── */
typedef enum { MAT_DEER_HIDE = 0, MAT_BOAR_HIDE, MAT_HERB, MAT_SCRAP, MAT_N } Material;
typedef enum { IT_MEDKIT = 0, IT_BANDAGE, IT_ARMOR_PLATE, IT_N } ItemId;
const char *material_name(int m);
const char *item_name(int it);

/* ── vendors (§72) — loaded from data/economy.json, embedded fallback ── */
typedef enum {
    OFFER_ITEM = 0,      /* value = ItemId, amount = count */
    OFFER_AMMO,          /* value = AmmoType, amount = rounds */
    OFFER_MAP,           /* reveal fog (island only) */
    OFFER_SELL_MAT,      /* sell all of material `value` at `price` each */
    OFFER_UPGRADE        /* value = upgrade id (0 = ammo pouch) */
} OfferKind;

#define VENDOR_MAX 6
#define OFFER_MAX 10
typedef struct {
    char name[32];
    int  kind, value, amount, price;
} Offer;
typedef struct {
    char  id[16], name[32];
    int   act;               /* 0 island, 1 city */
    Vec3  pos;
    Offer offers[OFFER_MAX];
    int   offer_n;
} Vendor;

typedef struct {
    char name[32];
    int  mat[MAT_N];         /* material costs */
    int  out_kind;           /* 0 item, 1 upgrade */
    int  out_value, out_amount;
} Recipe;
#define RECIPE_MAX 8

typedef struct {
    Vendor vendors[VENDOR_MAX]; int vendor_n;
    Recipe recipes[RECIPE_MAX]; int recipe_n;
    int    from_file;
} Economy;

int  economy_load(Economy *e);             /* 1 = data file, 0 = embedded */
int  economy_vendor_near(const Economy *e, int act, Vec3 p, float r);

/* ── fast travel ── */
typedef enum { FT_ISLAND_SAFEHOUSE = 0, FT_OUTPOST, FT_MAST, FT_CITY_SAFEHOUSE, FT_JOB_BOARD, FT_N } FastTravelId;
typedef struct { const char *name; int act; Vec3 pos; } FastTravelDef;

/* ── progression XP events ── */
typedef enum {
    EV_KILL = 0, EV_HEADSHOT, EV_TAKEDOWN, EV_OUTPOST_LOUD, EV_OUTPOST_STEALTH,
    EV_MAST, EV_JOB, EV_HUNT, EV_CRAFT, EV_LOUD_SHOT, EV_ALARM, EV_N
} ProgEvent;

typedef struct {
    /* xp */
    int      level, xp, skill_points;
    uint32_t skills;                       /* bitmask over SkillId */
    int      xp_total;
    /* economy */
    int      mat[MAT_N];
    int      items[IT_N];
    int      pouches;                      /* ammo pouch upgrades 0..3 */
    int      money_earned, money_spent;
    /* island alert (§13.6): 0..100, level 0..3 */
    float    alert;
    float    alert_quiet_t;                /* seconds since last incident */
    /* fast travel unlocks */
    int      ft_unlocked[FT_N];
    FastTravelDef ft[FT_N];
    /* island state carried over the ferry / into saves */
    int      island_saved;
    int      outpost_captured, mast_synced, alarm_destroyed;
    uint32_t fog_bits[64 * 64 / 32];
    int      city_stars;                   /* wanted persistence */
    float    last_stand_cd;
} Progress;

void  prog_init(Progress *p);
int   prog_add_xp(Progress *p, int xp);    /* returns levels gained */
int   prog_event(Progress *p, ProgEvent ev, int arg);   /* returns xp awarded */
int   prog_has(const Progress *p, int skill);
/* 0 ok · -1 bad id · -2 owned · -3 points · -4 level · -5 prerequisite */
int   prog_can_unlock(const Progress *p, int skill);
int   prog_unlock(Progress *p, int skill);
const char *prog_unlock_error(int code);
float prog_mod(const Progress *p, SkillMod m);

/* alert */
int   prog_alert_level(const Progress *p);   /* 0 calm · 1 wary · 2 alert · 3 lockdown */
const char *prog_alert_name(int lvl);
void  prog_alert_tick(Progress *p, float dt);
int   prog_reinforce_size(const Progress *p);   /* per wave, 3 + level */

/* economy: price after skills; purchase/sell/craft return 0 ok, <0 error */
int   prog_price(const Progress *p, int base);
int   prog_buy(Progress *p, const Offer *o, int *cash);      /* -1 cash · -2 maxed */
int   prog_sell_mat(Progress *p, int mat, int price_each, int *cash);   /* returns $ earned */
int   prog_can_craft(const Progress *p, const Recipe *r);
int   prog_craft(Progress *p, const Recipe *r);
int   prog_use_item(Progress *p, int item);   /* consumes one, returns 1 on success */

/* fast travel: 0 ok · -1 locked · -2 other act · -3 wanted · -4 combat */
int   prog_fast_travel_check(const Progress *p, int node, int act, int wanted, int in_combat);

/* fog bit helpers (64×64 map cells) */
void  prog_fog_store(Progress *p, const uint8_t *fog, int n);
void  prog_fog_restore(const Progress *p, uint8_t *fog, int n);

#endif /* DH_PROGRESS_H */
