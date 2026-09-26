/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — weapons & ballistics (M2, Spec 6.2 / 68)
   Pure ballistics + state-machine math: no world access, no rendering, so the
   whole table is unit-testable headless (Spec 18.4 "no NaN damage bugs").
   Balance numbers come from game/data/weapons.json (Spec 10.4); an embedded
   copy of the same four M2 weapons is the honest fallback if the file is
   missing (logged, never silent).
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_WEAPON_H
#define DH_WEAPON_H

#include "../core/dh_types.h"

#define WPN_MAX      16
#define WPN_SLOT_MAX 3            /* 2 primaries + sidearm (6.2) */

typedef enum { AMMO_9MM = 0, AMMO_556, AMMO_12G, AMMO_338, AMMO_ARROW, AMMO_N } AmmoType;

typedef enum { ZONE_HEAD = 0, ZONE_TORSO, ZONE_LIMB } HitZone;

typedef struct {
    char  id[8];
    char  name[24];
    char  cls[10];                /* pistol / smg / rifle / shotgun / ... */
    float dmg;                    /* per bullet (per pellet for shotguns) */
    int   rpm;
    int   mag;
    float reload_s;
    float recoil_v, recoil_h;     /* degrees of view kick */
    float spread_deg;             /* hipfire cone half-angle */
    float ads_s;                  /* seconds to full aim */
    float zone_mul[3];            /* head / torso / limb */
    float fall_a, fall_b, fall_pct;
    int   pellets;
    int   ammo;                   /* AmmoType */
    int   price;
    char  audio[24];
} WeaponDef;

/* Load table from JSON. Returns weapon count (>=0). On missing/broken file
   falls back to the embedded M2 table and logs a WARN (Honesty Contract). */
int  weapons_load(const char *json_path);
int  weapons_count(void);
const WeaponDef *weapons_get(int i);
const WeaponDef *weapon_by_id(const char *id);

/* ── ballistics ─────────────────────────────────────────────────────────── */
float weapon_falloff(const WeaponDef *w, float dist);          /* 0..1 */
float weapon_damage(const WeaponDef *w, int zone, float dist); /* per bullet/pellet */
float weapon_spread_rad(const WeaponDef *w, float ads_k, int moving, int crouched);
Vec3  weapon_recoil(const WeaponDef *w, float ads_k, int shot_index); /* deg kick */
float weapon_shot_interval(const WeaponDef *w);                /* seconds */
int   weapon_valid(const WeaponDef *w);                        /* NaN guard */
int   ammo_from_str(const char *s);
const char *ammo_name(int a);

#endif /* DH_WEAPON_H */
