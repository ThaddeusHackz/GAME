/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — weapons & ballistics implementation (M2)
   Every number here traces to MASTER_PROMPT §68 (prose wins, §95) — see the
   M2 report's balance-conflict log for the §92 matrix rows we overrode.
   ══════════════════════════════════════════════════════════════════════════ */
#include "weapon.h"
#include "../core/dh_json.h"
#include "../core/dh_log.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static WeaponDef g_wpn[WPN_MAX];
static int       g_wpn_count = 0;

/* ── embedded fallback table (identical to game/data/weapons.json) ───────── */
static void weapons_builtin(void)
{
    static const struct {
        const char *id, *name, *cls;
        float dmg; int rpm, mag; float reload, rv, rh, spr, ads;
        float fa, fb, fp; int pellets, ammo, price;
    } t[] = {
        { "W01", "Culebra",  "pistol",  24.f, 350, 12, 1.4f, 0.6f, 0.2f, 0.4f, 0.180f, 25.f, 50.f, 0.70f, 1, AMMO_9MM,  400 },
        { "W03", "Chispa",   "smg",     16.f, 900, 32, 1.9f, 0.5f, 0.9f, 1.2f, 0.140f, 15.f, 35.f, 0.55f, 1, AMMO_9MM, 1500 },
        { "W05", "Libertad", "rifle",   28.f, 600, 30, 2.2f, 0.9f, 0.4f, 0.5f, 0.200f, 40.f, 90.f, 0.75f, 1, AMMO_556, 4800 },
        { "W08", "Trueno",   "shotgun", 11.f,  70,  6, 4.0f, 3.0f, 0.3f, 6.0f, 0.250f,  8.f, 18.f, 0.35f, 9, AMMO_12G, 3900 },
    };
    g_wpn_count = 0;
    for (size_t i = 0; i < sizeof t / sizeof t[0] && g_wpn_count < WPN_MAX; i++) {
        WeaponDef *w = &g_wpn[g_wpn_count++];
        memset(w, 0, sizeof *w);
        snprintf(w->id, sizeof w->id, "%s", t[i].id);
        snprintf(w->name, sizeof w->name, "%s", t[i].name);
        snprintf(w->cls, sizeof w->cls, "%s", t[i].cls);
        w->dmg = t[i].dmg; w->rpm = t[i].rpm; w->mag = t[i].mag;
        w->reload_s = t[i].reload; w->recoil_v = t[i].rv; w->recoil_h = t[i].rh;
        w->spread_deg = t[i].spr; w->ads_s = t[i].ads;
        w->zone_mul[ZONE_HEAD] = 2.5f; w->zone_mul[ZONE_TORSO] = 1.0f;
        w->zone_mul[ZONE_LIMB] = 0.7f;
        w->fall_a = t[i].fa; w->fall_b = t[i].fb; w->fall_pct = t[i].fp;
        w->pellets = t[i].pellets; w->ammo = t[i].ammo; w->price = t[i].price;
        snprintf(w->audio, sizeof w->audio, "sfx.wpn.%s", t[i].id);
    }
}

int ammo_from_str(const char *s)
{
    if (!s) return AMMO_N;
    if (!strcmp(s, "9mm"))  return AMMO_9MM;
    if (!strcmp(s, "556"))  return AMMO_556;
    if (!strcmp(s, "12g"))  return AMMO_12G;
    if (!strcmp(s, "338"))  return AMMO_338;
    if (!strcmp(s, "arrow"))return AMMO_ARROW;
    return AMMO_N;
}
const char *ammo_name(int a)
{
    switch (a) {
    case AMMO_9MM:   return "9mm";
    case AMMO_556:   return "5.56";
    case AMMO_12G:   return "12g";
    case AMMO_338:   return ".338";
    case AMMO_ARROW: return "arrow";
    default:         return "?";
    }
}

static void wpn_from_json(WeaponDef *w, JsonValue *o)
{
    memset(w, 0, sizeof *w);
    snprintf(w->id,   sizeof w->id,   "%s", json_get_str(o, "id", "W??"));
    snprintf(w->name, sizeof w->name, "%s", json_get_str(o, "name", "?"));
    snprintf(w->cls,  sizeof w->cls,  "%s", json_get_str(o, "class", "?"));
    w->dmg       = json_get_flt(o, "dmg", 20.f);
    w->rpm       = json_get_int(o, "rpm", 300);
    w->mag       = json_get_int(o, "mag", 12);
    w->reload_s  = json_get_flt(o, "reload", 1.5f);
    w->recoil_v  = json_get_flt(o, "recoil_v", 0.5f);
    w->recoil_h  = json_get_flt(o, "recoil_h", 0.2f);
    w->spread_deg= json_get_flt(o, "spread", 1.0f);
    w->ads_s     = json_get_flt(o, "ads", 0.18f);
    w->pellets   = json_get_int(o, "pellets", 1);
    w->price     = json_get_int(o, "price", 0);
    w->ammo      = ammo_from_str(json_get_str(o, "ammo", "9mm"));
    JsonValue *z = json_obj_get(o, "zones");
    w->zone_mul[ZONE_HEAD]  = z ? json_get_flt(z, "head", 2.5f)  : 2.5f;
    w->zone_mul[ZONE_TORSO] = z ? json_get_flt(z, "torso", 1.0f) : 1.0f;
    w->zone_mul[ZONE_LIMB]  = z ? json_get_flt(z, "limb", 0.7f)  : 0.7f;
    JsonValue *f = json_obj_get(o, "falloff");
    w->fall_a   = f ? json_get_flt(f, "a", 25.f)   : 25.f;
    w->fall_b   = f ? json_get_flt(f, "b", 50.f)   : 50.f;
    w->fall_pct = f ? json_get_flt(f, "pct", 0.7f) : 0.7f;
    const char *audio = json_get_str(o, "audio", NULL);
    snprintf(w->audio, sizeof w->audio, "%s", audio ? audio : "sfx.wpn.generic");
}

int weapons_load(const char *json_path)
{
    char pathbuf[512];
    const char *path = json_path;
    if (!path || !*path) {
        snprintf(pathbuf, sizeof pathbuf, "%s/weapons.json", dh_fs_data_dir());
        path = pathbuf;
        if (!dh_fs_exists(path)) path = "game/data/weapons.json";
    }
    size_t len = 0;
    char *text = dh_fs_read_text(path, &len);
    if (!text) {
        DH_WARN("weapon", "cannot read %s — using embedded fallback table (Honesty Contract)", path);
        weapons_builtin();
        return g_wpn_count;
    }
    const char *err = NULL;
    JsonValue *root = json_parse(text, &err);
    free(text);
    if (!root) {
        DH_WARN("weapon", "weapons.json parse error (%s) — embedded fallback", err ? err : "?");
        weapons_builtin();
        return g_wpn_count;
    }
    JsonValue *arr = json_obj_get(root, "weapons");
    int n = arr ? json_arr_len(arr) : 0;
    if (n <= 0) {
        DH_WARN("weapon", "weapons.json has no weapons array — embedded fallback");
        json_free(root);
        weapons_builtin();
        return g_wpn_count;
    }
    g_wpn_count = 0;
    for (int i = 0; i < n && g_wpn_count < WPN_MAX; i++) {
        JsonValue *o = json_arr_get(arr, i);
        if (!o) continue;
        wpn_from_json(&g_wpn[g_wpn_count], o);
        if (!weapon_valid(&g_wpn[g_wpn_count])) {
            DH_WARN("weapon", "%s failed validation — row skipped (no NaN balance, 18.4)",
                    g_wpn[g_wpn_count].id);
            continue;
        }
        g_wpn_count++;
    }
    json_free(root);
    if (g_wpn_count == 0) {
        DH_WARN("weapon", "no valid rows in weapons.json — embedded fallback");
        weapons_builtin();
    }
    DH_INFO("weapon", "loaded %d weapons from %s", g_wpn_count, path);
    return g_wpn_count;
}

int  weapons_count(void) { return g_wpn_count; }
const WeaponDef *weapons_get(int i) { return (i >= 0 && i < g_wpn_count) ? &g_wpn[i] : NULL; }
const WeaponDef *weapon_by_id(const char *id)
{
    if (!id) return NULL;
    for (int i = 0; i < g_wpn_count; i++)
        if (!strcmp(g_wpn[i].id, id)) return &g_wpn[i];
    return NULL;
}

int weapon_valid(const WeaponDef *w)
{
    if (!w) return 0;
    float f[] = { w->dmg, w->reload_s, w->recoil_v, w->recoil_h, w->spread_deg,
                  w->ads_s, w->zone_mul[0], w->zone_mul[1], w->zone_mul[2],
                  w->fall_a, w->fall_b, w->fall_pct };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++)
        if (!isfinite(f[i])) return 0;
    if (w->dmg <= 0.f || w->rpm <= 0 || w->mag <= 0) return 0;
    if (w->pellets < 1 || w->pellets > 16) return 0;
    if (w->fall_b <= w->fall_a || w->fall_pct <= 0.f || w->fall_pct > 1.f) return 0;
    if (w->zone_mul[ZONE_HEAD] < w->zone_mul[ZONE_TORSO]) return 0;
    if (w->zone_mul[ZONE_LIMB] > w->zone_mul[ZONE_TORSO]) return 0;
    if (w->reload_s < 0.2f || w->ads_s < 0.02f) return 0;
    return w->id[0] != '\0';
}

float weapon_falloff(const WeaponDef *w, float dist)
{
    if (!w || !isfinite(dist)) return 0.f;
    if (dist <= w->fall_a) return 1.f;
    if (dist >= w->fall_b) return w->fall_pct;
    float t = (dist - w->fall_a) / (w->fall_b - w->fall_a);
    return 1.f + (w->fall_pct - 1.f) * t;
}

float weapon_damage(const WeaponDef *w, int zone, float dist)
{
    if (!w || zone < 0 || zone > 2) return 0.f;
    return w->dmg * w->zone_mul[zone] * weapon_falloff(w, dist);
}

float weapon_spread_rad(const WeaponDef *w, float ads_k, int moving, int crouched)
{
    if (!w) return 0.f;
    float hip = w->spread_deg * DH_DEG2RAD;
    /* ADS shrinks the cone to 25% of hipfire (tuned in M2 playtests); crouch
       tightens a further 15%, movement widens 60% — stance matters (§82). */
    float s = hip * (1.f - 0.75f * ads_k);
    if (crouched) s *= 0.85f;
    if (moving && ads_k < 0.5f) s *= 1.6f;
    return s;
}

Vec3 weapon_recoil(const WeaponDef *w, float ads_k, int shot_index)
{
    /* Deterministic pattern (seeded by shot index, not RNG): vertical kick
       scaled down by ADS, horizontal kick alternates like a real bolt — the
       player can learn the pattern, which is the whole point of §68 "feel". */
    Vec3 r = v3(0.f, 0.f, 0.f);
    if (!w) return r;
    float ads_scale = 1.f - 0.45f * ads_k;
    r.y = w->recoil_v * ads_scale * DH_DEG2RAD;                    /* pitch up */
    float ph = (float)shot_index * 1.7f;
    r.x = w->recoil_h * ads_scale * DH_DEG2RAD * sinf(ph);         /* yaw wobble */
    return r;
}

float weapon_shot_interval(const WeaponDef *w)
{
    if (!w || w->rpm <= 0) return 1.f;
    return 60.f / (float)w->rpm;
}
