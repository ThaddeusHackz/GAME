/* DIVIDED HORIZON — progression & economy (M5). See progress.h. */
#include "progress.h"
#include "../core/dh_json.h"
#include "../core/dh_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ══════════════════════════════ XP curve ══════════════════════════════ */
int prog_xp_to_next(int level) {
    if (level < 1) level = 1;
    return 100 + 50 * (level - 1);
}

/* ══════════════════════════════ skills ═══════════════════════════════ */
static const SkillDef SKILLS[SKILL_N] = {
    /* SURVIVOR */
    { "tough_hide",   "TOUGH HIDE",    "+25 max health",                         0, 1, 1, 1 },
    { "deep_lungs",   "DEEP LUNGS",    "+50% breath underwater",                 0, 1, 1, 1 },
    { "second_wind",  "SECOND WIND",   "+40% sprint stamina",                    0, 2, 2, 3 },
    { "field_medic",  "FIELD MEDIC",   "Medkits and bandages heal +50%",         0, 2, 2, 3 },
    { "iron_stomach", "IRON STOMACH",  "Street food heals double",               0, 3, 3, 6 },
    { "last_stand",   "LAST STAND",    "Survive a lethal hit at 1 HP (120 s cd)",0, 3, 3, 6 },
    /* HUNTER */
    { "steady_hands", "STEADY HANDS",  "-25% recoil",                            1, 1, 1, 1 },
    { "quick_hands",  "QUICK HANDS",   "-30% reload time",                       1, 1, 1, 1 },
    { "skinner",      "SKINNER",       "Double hides from every animal",         1, 2, 2, 3 },
    { "eagle_eye",    "EAGLE EYE",     "+50% binocular tagging range",      1, 2, 2, 3 },
    { "deadeye",      "DEADEYE",       "+25% headshot damage",                   1, 3, 3, 6 },
    { "pack_mule",    "PACK MULE",     "+50% reserve ammo capacity",             1, 3, 3, 6 },
    /* GHOST */
    { "soft_step",    "SOFT STEP",     "Enemies spot you 35% slower",  2, 1, 1, 1 },
    { "chain_takedown","CHAIN TAKEDOWN","Takedowns reset melee cooldown",        2, 1, 1, 1 },
    { "wire_cutter",  "WIRE CUTTER",   "Alarm boxes take double damage",         2, 2, 2, 3 },
    { "night_owl",    "NIGHT OWL",     "Island alert rises 40% slower",          2, 2, 2, 3 },
    { "ghost_protocol","GHOST PROTOCOL","-30% outpost alarm radius",             2, 3, 3, 6 },
    { "smooth_talker","SMOOTH TALKER", "-15% vendor prices",                     2, 3, 3, 6 },
    /* DRIVER */
    { "wheelman",     "WHEELMAN",      "+15% vehicle top speed",                 3, 1, 1, 1 },
    { "road_armor",   "ROAD ARMOR",    "-40% crash damage to your car",          3, 1, 1, 1 },
    { "jacker",       "JACKER",        "Carjack wallets pay double",             3, 2, 2, 3 },
    { "low_profile",  "LOW PROFILE",   "-30% time to lose each wanted star",     3, 2, 2, 3 },
    { "fixer",        "FIXER",         "Jobs pay +25%",                          3, 3, 3, 6 },
    { "launderer",    "LAUNDERER",     "Sell materials for +30%",                3, 3, 3, 6 },
};
static const char *TREE_NAMES[TREE_N] = { "SURVIVOR", "HUNTER", "GHOST", "DRIVER" };

const SkillDef *skill_def(int i) { return (i >= 0 && i < SKILL_N) ? &SKILLS[i] : NULL; }
const char *skill_tree_name(int t) { return (t >= 0 && t < TREE_N) ? TREE_NAMES[t] : "?"; }

static const char *MAT_NAMES[MAT_N] = { "DEER HIDE", "BOAR HIDE", "HERB", "SCRAP" };
static const char *ITEM_NAMES[IT_N] = { "MEDKIT", "BANDAGE", "ARMOR PLATE" };
const char *material_name(int m) { return (m >= 0 && m < MAT_N) ? MAT_NAMES[m] : "?"; }
const char *item_name(int it)    { return (it >= 0 && it < IT_N) ? ITEM_NAMES[it] : "?"; }

static const FastTravelDef FT_DEFS[FT_N] = {
    { "WEST BEACH SAFEHOUSE", 0, { 144.f, 0.f, 626.f } },
    { "OUTPOST",              0, { 0.f, 0.f, 0.f } },        /* filled from world */
    { "SIGNAL MAST",          0, { 0.f, 0.f, 0.f } },
    { "HARBOR SAFEHOUSE",     1, { 486.f, 0.f, 640.f } },
    { "JOB BOARD",            1, { 500.f, 0.f, 644.f } },
};

void prog_init(Progress *p) {
    memset(p, 0, sizeof *p);
    p->level = 1;
    for (int i = 0; i < FT_N; i++) p->ft[i] = FT_DEFS[i];
    p->ft_unlocked[FT_ISLAND_SAFEHOUSE] = 1;
}

int prog_add_xp(Progress *p, int xp) {
    if (xp <= 0 || p->level >= PROG_LEVEL_CAP) return 0;
    p->xp += xp; p->xp_total += xp;
    int gained = 0;
    while (p->level < PROG_LEVEL_CAP && p->xp >= prog_xp_to_next(p->level)) {
        p->xp -= prog_xp_to_next(p->level);
        p->level++; p->skill_points++; gained++;
    }
    if (p->level >= PROG_LEVEL_CAP) p->xp = 0;
    return gained;
}

static const int EV_XP[EV_N] = {
    /* KILL */ 10, /* HEADSHOT */ 5, /* TAKEDOWN */ 25, /* OUTPOST_LOUD */ 250,
    /* OUTPOST_STEALTH */ 375, /* MAST */ 100, /* JOB */ 60, /* HUNT */ 15,
    /* CRAFT */ 5, /* LOUD_SHOT */ 0, /* ALARM */ 0
};

static void alert_add(Progress *p, float amt) {
    if (amt > 0.f) amt *= prog_mod(p, MOD_ALERT_GAIN);
    p->alert = dh_clampf(p->alert + amt, 0.f, 100.f);
    if (amt > 0.f) p->alert_quiet_t = 0.f;
}

int prog_event(Progress *p, ProgEvent ev, int arg) {
    (void)arg;
    if ((int)ev < 0 || ev >= EV_N) return 0;
    switch (ev) {
    case EV_LOUD_SHOT:       alert_add(p, 0.6f); break;
    case EV_ALARM:           alert_add(p, 30.f); break;
    case EV_KILL:            alert_add(p, 2.f);  break;
    case EV_OUTPOST_STEALTH: alert_add(p, -25.f); break;
    case EV_OUTPOST_LOUD:    alert_add(p, -10.f); break;
    default: break;
    }
    int xp = EV_XP[ev];
    prog_add_xp(p, xp);
    return xp;
}

int prog_has(const Progress *p, int s) {
    return (s >= 0 && s < SKILL_N) && (p->skills & (1u << s));
}

int prog_can_unlock(const Progress *p, int s) {
    const SkillDef *d = skill_def(s);
    if (!d) return -1;
    if (prog_has(p, s)) return -2;
    if (p->level < d->min_level) return -4;
    if (d->tier > 1) {            /* needs any skill of the previous tier in-tree */
        int ok = 0;
        for (int i = 0; i < SKILL_N; i++)
            if (SKILLS[i].tree == d->tree && SKILLS[i].tier == d->tier - 1 && prog_has(p, i)) ok = 1;
        if (!ok) return -5;
    }
    if (p->skill_points < d->cost) return -3;
    return 0;
}

int prog_unlock(Progress *p, int s) {
    int r = prog_can_unlock(p, s);
    if (r) return r;
    p->skill_points -= SKILLS[s].cost;
    p->skills |= 1u << s;
    return 0;
}

const char *prog_unlock_error(int c) {
    switch (c) {
    case 0: return "UNLOCKED";
    case -2: return "ALREADY OWNED";
    case -3: return "NOT ENOUGH SKILL POINTS";
    case -4: return "LEVEL TOO LOW";
    case -5: return "NEEDS A LOWER-TIER SKILL IN THIS TREE";
    default: return "INVALID";
    }
}

float prog_mod(const Progress *p, SkillMod m) {
#define H(s) prog_has(p, s)
    switch (m) {
    case MOD_MAX_HP:       return H(SK_TOUGH_HIDE) ? 1.25f : 1.f;
    case MOD_BREATH:       return H(SK_DEEP_LUNGS) ? 1.5f : 1.f;
    case MOD_STAMINA:      return H(SK_SECOND_WIND) ? 1.4f : 1.f;
    case MOD_HEAL:         return H(SK_FIELD_MEDIC) ? 1.5f : 1.f;
    case MOD_FOOD:         return H(SK_IRON_STOMACH) ? 2.f : 1.f;
    case MOD_RECOIL:       return H(SK_STEADY_HANDS) ? 0.75f : 1.f;
    case MOD_RELOAD:       return H(SK_QUICK_HANDS) ? 0.7f : 1.f;
    case MOD_HIDES:        return H(SK_SKINNER) ? 2.f : 1.f;
    case MOD_TAG_TIME:     return H(SK_EAGLE_EYE) ? 1.5f : 1.f;
    case MOD_HEADSHOT:     return H(SK_DEADEYE) ? 1.25f : 1.f;
    case MOD_RESERVE:      return (H(SK_PACK_MULE) ? 1.5f : 1.f) + 0.25f * (float)p->pouches;
    case MOD_NOISE:        return H(SK_SOFT_STEP) ? 0.65f : 1.f;
    case MOD_ALARM_RADIUS: return H(SK_GHOST_PROTOCOL) ? 0.7f : 1.f;
    case MOD_PRICE:        return H(SK_SMOOTH_TALKER) ? 0.85f : 1.f;
    case MOD_ALERT_GAIN:   return H(SK_NIGHT_OWL) ? 0.6f : 1.f;
    case MOD_TOP_SPEED:    return H(SK_WHEELMAN) ? 1.15f : 1.f;
    case MOD_VEH_DAMAGE:   return H(SK_ROAD_ARMOR) ? 0.6f : 1.f;
    case MOD_WALLET:       return H(SK_JACKER) ? 2.f : 1.f;
    case MOD_HEAT_ESCAPE:  return H(SK_LOW_PROFILE) ? 0.7f : 1.f;
    case MOD_JOB_PAY:        return H(SK_FIXER) ? 1.25f : 1.f;     /* job payout */
    case MOD_EXCHANGE:     return H(SK_LAUNDERER) ? 1.3f : 1.f;
    default: return 1.f;
    }
#undef H
}

/* ══════════════════════════════ alert ════════════════════════════════ */
int prog_alert_level(const Progress *p) {
    return p->alert >= 75.f ? 3 : p->alert >= 45.f ? 2 : p->alert >= 20.f ? 1 : 0;
}
const char *prog_alert_name(int l) {
    static const char *n[4] = { "CALM", "WARY", "ALERT", "LOCKDOWN" };
    return n[l < 0 ? 0 : l > 3 ? 3 : l];
}
void prog_alert_tick(Progress *p, float dt) {
    p->alert_quiet_t += dt;
    if (p->alert_quiet_t > 30.f) p->alert = dh_maxf(0.f, p->alert - 1.0f * dt);
    if (p->last_stand_cd > 0.f) p->last_stand_cd -= dt;
}
int prog_reinforce_size(const Progress *p) { return 3 + prog_alert_level(p); }

/* ══════════════════════════════ economy ══════════════════════════════ */
int prog_price(const Progress *p, int base) {
    float f = (float)base * prog_mod(p, MOD_PRICE);
    int v = (int)(f + 0.5f);
    return v < 1 ? 1 : v;
}

int prog_buy(Progress *p, const Offer *o, int *cash) {
    if (o->kind == OFFER_SELL_MAT) return prog_sell_mat(p, o->value, o->price, cash) > 0 ? 0 : -3;
    int price = prog_price(p, o->price);
    if (o->kind == OFFER_UPGRADE && p->pouches >= 3) return -2;
    if (o->kind == OFFER_ITEM && o->value >= 0 && o->value < IT_N && p->items[o->value] >= 9) return -2;
    if (*cash < price) return -1;
    *cash -= price;
    p->money_spent += price;
    switch (o->kind) {
    case OFFER_ITEM:    if (o->value >= 0 && o->value < IT_N) p->items[o->value] += o->amount; break;
    case OFFER_UPGRADE: p->pouches++; break;
    default: break;     /* AMMO / MAP are applied by the caller (needs Game) */
    }
    return 0;
}

int prog_sell_mat(Progress *p, int mat, int price_each, int *cash) {
    if (mat < 0 || mat >= MAT_N || p->mat[mat] <= 0) return 0;
    int each = (int)((float)price_each * prog_mod(p, MOD_EXCHANGE) + 0.5f);
    int earned = each * p->mat[mat];
    p->mat[mat] = 0;
    *cash += earned;
    p->money_earned += earned;
    return earned;
}

int prog_can_craft(const Progress *p, const Recipe *r) {
    for (int m = 0; m < MAT_N; m++) if (p->mat[m] < r->mat[m]) return 0;
    if (r->out_kind == 1 && p->pouches >= 3) return 0;
    if (r->out_kind == 0 && r->out_value >= 0 && r->out_value < IT_N && p->items[r->out_value] >= 9) return 0;
    return 1;
}
int prog_craft(Progress *p, const Recipe *r) {
    if (!prog_can_craft(p, r)) return -1;
    for (int m = 0; m < MAT_N; m++) p->mat[m] -= r->mat[m];
    if (r->out_kind == 1) p->pouches++;
    else if (r->out_value >= 0 && r->out_value < IT_N) p->items[r->out_value] += r->out_amount;
    prog_event(p, EV_CRAFT, 0);
    return 0;
}
int prog_use_item(Progress *p, int it) {
    if (it < 0 || it >= IT_N || p->items[it] <= 0) return 0;
    p->items[it]--;
    return 1;
}

/* ══════════════════════════════ vendors: data ════════════════════════ */
static int mat_from(const char *s) {
    if (!strcmp(s, "deer_hide")) return MAT_DEER_HIDE;
    if (!strcmp(s, "boar_hide")) return MAT_BOAR_HIDE;
    if (!strcmp(s, "herb"))      return MAT_HERB;
    if (!strcmp(s, "scrap"))     return MAT_SCRAP;
    return -1;
}
static int item_from(const char *s) {
    if (!strcmp(s, "medkit"))  return IT_MEDKIT;
    if (!strcmp(s, "bandage")) return IT_BANDAGE;
    if (!strcmp(s, "armor"))   return IT_ARMOR_PLATE;
    return -1;
}
static int ammo_from(const char *s) {
    if (!strcmp(s, "9mm")) return 0;
    if (!strcmp(s, "556")) return 1;
    if (!strcmp(s, "12g")) return 2;
    if (!strcmp(s, "338")) return 3;
    return -1;
}

static void offer(Vendor *v, const char *name, int kind, int value, int amount, int price) {
    if (v->offer_n >= OFFER_MAX) return;
    Offer *o = &v->offers[v->offer_n++];
    snprintf(o->name, sizeof o->name, "%s", name);
    o->kind = kind; o->value = value; o->amount = amount; o->price = price;
}

/* Embedded fallback — identical to game/data/economy.json (Honesty Contract:
   a missing data file degrades to the shipped defaults, never to nothing). */
static void economy_builtin(Economy *e) {
    memset(e, 0, sizeof *e);
    Vendor *r = &e->vendors[e->vendor_n++];
    snprintf(r->id, sizeof r->id, "rosa"); snprintf(r->name, sizeof r->name, "ROSA'S TRADING POST");
    r->act = 0; r->pos = v3(158.f, 0.f, 612.f);
    offer(r, "Medkit", OFFER_ITEM, IT_MEDKIT, 1, 60);
    offer(r, "Bandage", OFFER_ITEM, IT_BANDAGE, 1, 20);
    offer(r, "9mm x30", OFFER_AMMO, 0, 30, 30);
    offer(r, "5.56 x60", OFFER_AMMO, 1, 60, 45);
    offer(r, "12g x16", OFFER_AMMO, 2, 16, 40);
    offer(r, "Chart of the isle", OFFER_MAP, 0, 1, 150);
    offer(r, "Sell deer hides", OFFER_SELL_MAT, MAT_DEER_HIDE, 0, 35);
    offer(r, "Sell boar hides", OFFER_SELL_MAT, MAT_BOAR_HIDE, 0, 50);
    Vendor *v = &e->vendors[e->vendor_n++];
    snprintf(v->id, sizeof v->id, "vargas"); snprintf(v->name, sizeof v->name, "VARGAS PAWN & ARMS");
    v->act = 1; v->pos = v3(528.f, 0.f, 640.f);
    offer(v, "Medkit", OFFER_ITEM, IT_MEDKIT, 1, 75);
    offer(v, "Armor plate", OFFER_ITEM, IT_ARMOR_PLATE, 1, 120);
    offer(v, "9mm x30", OFFER_AMMO, 0, 30, 35);
    offer(v, "5.56 x60", OFFER_AMMO, 1, 60, 55);
    offer(v, "12g x16", OFFER_AMMO, 2, 16, 45);
    offer(v, "Ammo pouch", OFFER_UPGRADE, 0, 1, 400);
    offer(v, "Sell scrap", OFFER_SELL_MAT, MAT_SCRAP, 0, 15);
    offer(v, "Sell deer hides", OFFER_SELL_MAT, MAT_DEER_HIDE, 0, 45);
    offer(v, "Sell boar hides", OFFER_SELL_MAT, MAT_BOAR_HIDE, 0, 65);
    Recipe *c;
    c = &e->recipes[e->recipe_n++]; snprintf(c->name, sizeof c->name, "Field medkit");
    c->mat[MAT_HERB] = 2; c->out_kind = 0; c->out_value = IT_MEDKIT; c->out_amount = 1;
    c = &e->recipes[e->recipe_n++]; snprintf(c->name, sizeof c->name, "Bandage x2");
    c->mat[MAT_HERB] = 1; c->out_kind = 0; c->out_value = IT_BANDAGE; c->out_amount = 2;
    c = &e->recipes[e->recipe_n++]; snprintf(c->name, sizeof c->name, "Boar-hide plate");
    c->mat[MAT_BOAR_HIDE] = 2; c->mat[MAT_SCRAP] = 1; c->out_kind = 0; c->out_value = IT_ARMOR_PLATE; c->out_amount = 1;
    c = &e->recipes[e->recipe_n++]; snprintf(c->name, sizeof c->name, "Deer-hide pouch");
    c->mat[MAT_DEER_HIDE] = 3; c->out_kind = 1; c->out_value = 0; c->out_amount = 1;
}

int economy_load(Economy *e) {
    char pathbuf[512];
    snprintf(pathbuf, sizeof pathbuf, "%s/economy.json", dh_fs_data_dir());
    const char *path = pathbuf;
    if (!dh_fs_exists(path)) path = "game/data/economy.json";
    size_t len = 0;
    char *text = dh_fs_read_text(path, &len);
    if (!text) { DH_WARN("econ", "cannot read economy.json - embedded fallback"); economy_builtin(e); return 0; }
    const char *err = NULL;
    JsonValue *root = json_parse(text, &err);
    free(text);
    if (!root) { DH_WARN("econ", "economy.json parse error (%s) - embedded fallback", err ? err : "?");
                 economy_builtin(e); return 0; }
    memset(e, 0, sizeof *e);
    JsonValue *va = json_obj_get(root, "vendors");
    for (int i = 0; va && i < json_arr_len(va) && e->vendor_n < VENDOR_MAX; i++) {
        JsonValue *o = json_arr_get(va, i);
        Vendor *v = &e->vendors[e->vendor_n];
        memset(v, 0, sizeof *v);
        snprintf(v->id, sizeof v->id, "%s", json_get_str(o, "id", ""));
        snprintf(v->name, sizeof v->name, "%s", json_get_str(o, "name", "VENDOR"));
        v->act = json_get_int(o, "act", 0);
        v->pos = v3(json_get_flt(o, "x", 0), 0.f, json_get_flt(o, "z", 0));
        JsonValue *oa = json_obj_get(o, "offers");
        for (int k = 0; oa && k < json_arr_len(oa); k++) {
            JsonValue *f = json_arr_get(oa, k);
            const char *kind = json_get_str(f, "kind", "");
            const char *val = json_get_str(f, "value", "");
            int amount = json_get_int(f, "amount", 1), price = json_get_int(f, "price", 0);
            const char *nm = json_get_str(f, "name", "?");
            int kv = -1, kk = -1;
            if (!strcmp(kind, "item"))    { kk = OFFER_ITEM; kv = item_from(val); }
            else if (!strcmp(kind, "ammo")) { kk = OFFER_AMMO; kv = ammo_from(val); }
            else if (!strcmp(kind, "map"))  { kk = OFFER_MAP; kv = 0; }
            else if (!strcmp(kind, "sell")) { kk = OFFER_SELL_MAT; kv = mat_from(val); }
            else if (!strcmp(kind, "upgrade")) { kk = OFFER_UPGRADE; kv = 0; }
            if (kk < 0 || kv < 0 || price <= 0) { DH_WARN("econ", "skipping bad offer '%s'", nm); continue; }
            offer(v, nm, kk, kv, amount, price);
        }
        if (v->offer_n > 0) e->vendor_n++;
    }
    JsonValue *ra = json_obj_get(root, "recipes");
    for (int i = 0; ra && i < json_arr_len(ra) && e->recipe_n < RECIPE_MAX; i++) {
        JsonValue *o = json_arr_get(ra, i);
        Recipe *c = &e->recipes[e->recipe_n];
        memset(c, 0, sizeof *c);
        snprintf(c->name, sizeof c->name, "%s", json_get_str(o, "name", "?"));
        c->mat[MAT_DEER_HIDE] = json_get_int(o, "deer_hide", 0);
        c->mat[MAT_BOAR_HIDE] = json_get_int(o, "boar_hide", 0);
        c->mat[MAT_HERB]      = json_get_int(o, "herb", 0);
        c->mat[MAT_SCRAP]     = json_get_int(o, "scrap", 0);
        const char *out = json_get_str(o, "out", "");
        c->out_amount = json_get_int(o, "amount", 1);
        if (!strcmp(out, "pouch")) { c->out_kind = 1; c->out_value = 0; }
        else { c->out_kind = 0; c->out_value = item_from(out); if (c->out_value < 0) continue; }
        e->recipe_n++;
    }
    json_free(root);
    if (e->vendor_n == 0) { DH_WARN("econ", "economy.json had no vendors - embedded fallback"); economy_builtin(e); return 0; }
    e->from_file = 1;
    return 1;
}

int economy_vendor_near(const Economy *e, int act, Vec3 p, float r) {
    for (int i = 0; i < e->vendor_n; i++) {
        const Vendor *v = &e->vendors[i];
        if (v->act != act) continue;
        float dx = v->pos.x - p.x, dz = v->pos.z - p.z;
        if (dx * dx + dz * dz < r * r) return i;
    }
    return -1;
}

/* ══════════════════════════════ fast travel ══════════════════════════ */
int prog_fast_travel_check(const Progress *p, int node, int act, int wanted, int in_combat) {
    if (node < 0 || node >= FT_N || !p->ft_unlocked[node]) return -1;
    if (p->ft[node].act != act) return -2;
    if (wanted) return -3;
    if (in_combat) return -4;
    return 0;
}

void prog_fog_store(Progress *p, const uint8_t *fog, int n) {
    memset(p->fog_bits, 0, sizeof p->fog_bits);
    if (n > 64 * 64) n = 64 * 64;
    for (int i = 0; i < n; i++) if (fog[i]) p->fog_bits[i >> 5] |= 1u << (i & 31);
}
void prog_fog_restore(const Progress *p, uint8_t *fog, int n) {
    if (n > 64 * 64) n = 64 * 64;
    for (int i = 0; i < n; i++)
        if ((p->fog_bits[i >> 5] >> (i & 31)) & 1u) { if (!fog[i]) fog[i] = 1; }
}
