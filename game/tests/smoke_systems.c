/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M5 systems & progression smoke test (headless, determ.)
   Proves:
     §71  XP curve 100+50(n-1), 24 skills in 4 trees, tier/level/point gates,
          skill effects reach the live game (max HP, recoil, prices …)
     §72  vendors from data/economy.json, buy / sell / caps, crafting
     §70  hunting → skinning → hides; herbs → medkits; H heals
     §13.6 island alert level rises with alarms and shrinks after stealth
     fast travel (locks, wanted/combat gates), map fog persistence
     island state survives Beto's ferry; saves round-trip (incl. wanted stars)
   Exits non-zero on any failure so tools/build.sh can gate on it.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif

#include "../src/core/dh_types.h"
#include "../src/core/dh_log.h"
#include "../src/core/settings.h"
#include "../src/core/save.h"
#include "../src/world/terrain.h"
#include "../src/player/player.h"
#include "../src/combat/weapon.h"
#include "../src/ai/enemy.h"
#include "../src/rend/rend.h"
#include "../src/city/heat.h"
#include "../src/city/city.h"
#include "../src/meta/progress.h"
#include "../src/game/game.h"
#include "../src/plat/plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke5", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke5", "ok: " __VA_ARGS__); \
} while (0)

#define DT (1.0f / 60.0f)

static void step_frames(Game *g, uint32_t buttons, uint32_t pressed, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        in.buttons = buttons; in.pressed = (i == 0) ? pressed : 0u;
        game_frame(g, &in, DT);
    }
}
static void press(Game *g, uint32_t b) { step_frames(g, 0, b, 1); }
static void tp(Game *g, float x, float z) {
    g->player.pos = v3(x, terrain_height(&g->terrain, x, z) + 0.05f, z);
    g->player.vel = v3(0, 0, 0);
}

/* ─────────────────────────── pure rules ─────────────────────────── */
static void test_xp_and_skills(void) {
    CHECK(prog_xp_to_next(1) == 100 && prog_xp_to_next(2) == 150 && prog_xp_to_next(10) == 550,
          "XP curve 100 + 50(n-1): L1 100, L2 150, L10 550");
    Progress p; prog_init(&p);
    CHECK(p.level == 1 && p.skill_points == 0, "new profile: level 1, no points");
    int lv = prog_add_xp(&p, 99);
    CHECK(lv == 0 && p.level == 1 && p.xp == 99, "99 xp does not level");
    lv = prog_add_xp(&p, 1 + 150 + 10);
    CHECK(lv == 2 && p.level == 3 && p.xp == 10 && p.skill_points == 2,
          "carry-over: +161 xp = two levels, 10 xp into L3, 2 points");
    int per_tree[TREE_N] = {0}, ids_ok = 1;
    for (int i = 0; i < SKILL_N; i++) {
        const SkillDef *d = skill_def(i);
        if (!d || !d->name || !d->desc || d->tier < 1 || d->tier > 3) ids_ok = 0;
        else per_tree[d->tree]++;
    }
    CHECK(ids_ok && per_tree[0] == 6 && per_tree[1] == 6 && per_tree[2] == 6 && per_tree[3] == 6,
          "24 skills, 6 per tree (SURVIVOR/HUNTER/GHOST/DRIVER)");
    CHECK(prog_can_unlock(&p, SK_SECOND_WIND) == -5, "tier-2 needs a tier-1 in the same tree");
    CHECK(prog_unlock(&p, SK_TOUGH_HIDE) == 0 && prog_has(&p, SK_TOUGH_HIDE) && p.skill_points == 1,
          "unlock TOUGH HIDE (1 SP)");
    CHECK(prog_unlock(&p, SK_TOUGH_HIDE) == -2, "cannot buy a skill twice");
    CHECK(prog_unlock(&p, SK_SECOND_WIND) == -3, "tier-2 costs 2 SP - refused with 1");
    CHECK(prog_can_unlock(&p, SK_LAST_STAND) == -4, "tier-3 is level-gated (LV6)");
    CHECK(prog_mod(&p, MOD_MAX_HP) == 1.25f && prog_mod(&p, MOD_RECOIL) == 1.0f,
          "mods: TOUGH HIDE x1.25 hp, recoil untouched");
    Progress q; prog_init(&q);
    q.skill_points = 3; q.level = 3;
    prog_unlock(&q, SK_STEADY_HANDS);
    CHECK(prog_mod(&q, MOD_RECOIL) == 0.75f, "STEADY HANDS: recoil x0.75");
    CHECK(prog_unlock(&q, SK_SKINNER) == 0 && prog_mod(&q, MOD_HIDES) == 2.f,
          "tier-2 SKINNER after a HUNTER tier-1: double hides");
    Progress cap; prog_init(&cap);
    prog_add_xp(&cap, 1000000);
    CHECK(cap.level == PROG_LEVEL_CAP && cap.xp == 0 && cap.skill_points == PROG_LEVEL_CAP - 1,
          "level cap %d holds (points %d)", PROG_LEVEL_CAP, cap.skill_points);
    Progress e; prog_init(&e);
    int x = prog_event(&e, EV_OUTPOST_STEALTH, 0);
    CHECK(x == 375 && prog_event(&e, EV_OUTPOST_LOUD, 0) == 250,
          "stealth liberation pays 1.5x a loud one (375 vs 250)");
}

static void test_economy_rules(void) {
    Economy ec;
    int ff = economy_load(&ec);
    CHECK(ff == 1 && ec.from_file, "economy.json loads from disk");
    CHECK(ec.vendor_n == 2 && ec.recipe_n == 4, "2 vendors (island + city), 4 recipes");
    int rosa = economy_vendor_near(&ec, 0, v3(158.f, 0.f, 613.f), 3.f);
    CHECK(rosa >= 0 && ec.vendors[rosa].act == 0, "Rosa trades on the island");
    CHECK(economy_vendor_near(&ec, 1, v3(158.f, 0.f, 613.f), 3.f) < 0, "vendors are act-scoped");

    Progress p; prog_init(&p);
    int cash = 50;
    Offer med = { "Medkit", OFFER_ITEM, IT_MEDKIT, 1, 60 };
    CHECK(prog_buy(&p, &med, &cash) == -1 && cash == 50 && p.items[IT_MEDKIT] == 0,
          "can't afford $60 with $50 - nothing changes");
    cash = 200;
    CHECK(prog_buy(&p, &med, &cash) == 0 && cash == 140 && p.items[IT_MEDKIT] == 1,
          "buy medkit: $200 -> $140");
    p.skills |= 1u << SK_SMOOTH_TALKER;
    CHECK(prog_price(&p, 60) == 51, "SMOOTH TALKER: $60 -> $51");
    p.mat[MAT_BOAR_HIDE] = 3;
    int earned = prog_sell_mat(&p, MAT_BOAR_HIDE, 50, &cash);
    CHECK(earned == 150 && p.mat[MAT_BOAR_HIDE] == 0 && cash == 290, "sell 3 boar hides @ $50");
    p.skills |= 1u << SK_LAUNDERER;
    p.mat[MAT_DEER_HIDE] = 2;
    CHECK(prog_sell_mat(&p, MAT_DEER_HIDE, 35, &cash) == 92, "LAUNDERER: +30%% (2 x $46)");
    Offer pouch = { "Pouch", OFFER_UPGRADE, 0, 1, 1 };
    cash = 100;
    for (int i = 0; i < 3; i++) prog_buy(&p, &pouch, &cash);
    CHECK(p.pouches == 3 && prog_buy(&p, &pouch, &cash) == -2, "ammo pouches max at 3");

    Progress c; prog_init(&c);
    const Recipe *medr = &ec.recipes[0];
    CHECK(!prog_can_craft(&c, medr) && prog_craft(&c, medr) == -1, "no herbs, no medkit");
    c.mat[MAT_HERB] = 5;
    CHECK(prog_craft(&c, medr) == 0 && c.mat[MAT_HERB] == 3 && c.items[IT_MEDKIT] == 1,
          "craft '%s': 2 herbs -> 1 medkit", medr->name);

    Progress a; prog_init(&a);
    CHECK(prog_alert_level(&a) == 0 && prog_reinforce_size(&a) == 3, "island calm: waves of 3");
    prog_event(&a, EV_ALARM, 0);
    CHECK(prog_alert_level(&a) == 1 && prog_reinforce_size(&a) == 4, "an alarm makes the island WARY (waves of 4)");
    prog_event(&a, EV_ALARM, 0); prog_event(&a, EV_ALARM, 0);
    CHECK(prog_alert_level(&a) == 3, "three alarms: LOCKDOWN");
    float before = a.alert;
    for (int i = 0; i < 60 * 29; i++) prog_alert_tick(&a, DT);
    CHECK(a.alert == before, "alert holds for 30 s after an incident");
    for (int i = 0; i < 60 * 40; i++) prog_alert_tick(&a, DT);
    CHECK(a.alert < before - 30.f, "then decays (%.0f -> %.0f)", before, a.alert);
    Progress n; prog_init(&n); n.skills |= 1u << SK_NIGHT_OWL;
    prog_event(&n, EV_ALARM, 0);
    CHECK(fabsf(n.alert - 18.f) < 0.01f, "NIGHT OWL: alarm adds 18 not 30");

    Progress f; prog_init(&f);
    CHECK(prog_fast_travel_check(&f, FT_ISLAND_SAFEHOUSE, 0, 0, 0) == 0, "island safehouse unlocked from the start");
    CHECK(prog_fast_travel_check(&f, FT_OUTPOST, 0, 0, 0) == -1, "outpost node locked until liberated");
    CHECK(prog_fast_travel_check(&f, FT_ISLAND_SAFEHOUSE, 1, 0, 0) == -2, "no cross-act fast travel (ferry only)");
    f.ft_unlocked[FT_CITY_SAFEHOUSE] = 1;
    CHECK(prog_fast_travel_check(&f, FT_CITY_SAFEHOUSE, 1, 1, 0) == -3, "wanted blocks fast travel");
    CHECK(prog_fast_travel_check(&f, FT_ISLAND_SAFEHOUSE, 0, 0, 1) == -4, "combat blocks fast travel");

    uint8_t fog[64 * 64], fog2[64 * 64];
    for (int i = 0; i < 64 * 64; i++) fog[i] = (uint8_t)((i * 7) % 3 == 0);
    memset(fog2, 0, sizeof fog2);
    prog_fog_store(&f, fog, 64 * 64);
    prog_fog_restore(&f, fog2, 64 * 64);
    int same = 1;
    for (int i = 0; i < 64 * 64; i++) if ((fog[i] != 0) != (fog2[i] != 0)) same = 0;
    CHECK(same, "fog of war bit-packs and restores exactly");
}

/* ─────────────────────────── live game ─────────────────────────── */
static int nearest_critter(Game *g, Vec3 from) {
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < g->critter_count; i++) {
        if (g->critters[i].state >= 3) continue;
        float dx = g->critters[i].pos.x - from.x, dz = g->critters[i].pos.z - from.z;
        float d = dx * dx + dz * dz;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void test_hunt_and_heal(Game *g) {
    int ci = nearest_critter(g, g->player.pos);
    CHECK(ci >= 0, "island has live wildlife to hunt (%d)", g->critter_count);
    if (ci < 0) return;
    Critter *c = &g->critters[ci];
    int species = c->species;
    /* stand 5 m south, face it, fire once */
    tp(g, c->pos.x, c->pos.z - 5.f);
    g->player.crouched = 1;
    Vec3 eye; player_camera(&g->player, &eye, NULL);
    Vec3 tgt = v3(c->pos.x, c->pos.y + 0.6f, c->pos.z);
    Vec3 to = v3_sub(tgt, eye);
    g->player.yaw = atan2f(to.x, to.z);
    g->player.pitch = atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z));
    g->fire_cd = 0.f; g->reloading = 0; g->rec_pitch = g->rec_yaw = 0.f;
    step_frames(g, BTN_FIRE, BTN_FIRE, 1);
    CHECK(c->state == 3, "a clean shot drops the %s (state %d)", species ? "boar" : "deer", c->state);
    if (c->state != 3) { c->state = 3; c->vel = v3(0, 0, 0); }
    step_frames(g, 0, 0, 30);
    tp(g, c->pos.x + 1.f, c->pos.z);
    int hides0 = g->prog.mat[MAT_DEER_HIDE] + g->prog.mat[MAT_BOAR_HIDE];
    int xp0 = g->prog.xp_total;
    press(g, BTN_USE);
    int hides1 = g->prog.mat[MAT_DEER_HIDE] + g->prog.mat[MAT_BOAR_HIDE];
    CHECK(hides1 == hides0 + 1 && c->state == 4, "[E] skins the carcass: +1 hide, carcass gone");
    CHECK(g->prog.xp_total >= xp0 + 15, "hunting pays XP");

    /* herbs */
    int hk = -1;
    for (int i = 0; i < g->pickups.count; i++)
        if (g->pickups.v[i].live && g->pickups.v[i].kind == PK_HERB) { hk = i; break; }
    int herbs_total = 0;
    for (int i = 0; i < g->pickups.count; i++)
        if (g->pickups.v[i].live && g->pickups.v[i].kind == PK_HERB) herbs_total++;
    CHECK(herbs_total >= 10, "island has herb patches (%d)", herbs_total);
    if (hk >= 0) {
        g->player.pos = g->pickups.v[hk].pos;
        g->player.pos.y += 0.05f;
        int h0 = g->prog.mat[MAT_HERB];
        step_frames(g, 0, 0, 2);
        CHECK(g->prog.mat[MAT_HERB] == h0 + 1 && !g->pickups.v[hk].live, "walk over a herb: +1 HERB");
    }
    g->prog.mat[MAT_HERB] = 2;
    /* craft through the character screen: I, LEFT (-> crafting tab), E */
    press(g, BTN_CHAR);
    CHECK(g->ui == UI_CHAR, "[I] opens the character screen");
    float t0 = g->player.pos.x; Vec3 v0 = g->player.pos; (void)t0;
    step_frames(g, BTN_UP, 0, 20);
    CHECK(g->player.pos.x == v0.x && g->player.pos.z == v0.z, "modal screens pause the world");
    press(g, BTN_LEFT);
    CHECK(g->ui_tab == 1 && g->ui_sel == 0, "LEFT from SURVIVOR switches to the crafting tab");
    int m0 = g->prog.items[IT_MEDKIT];
    press(g, BTN_USE);
    CHECK(g->prog.items[IT_MEDKIT] == m0 + 1 && g->prog.mat[MAT_HERB] == 0, "crafted a field medkit from 2 herbs");
    press(g, BTN_CHAR);
    CHECK(g->ui == UI_NONE, "[I] closes it again");

    g->player.health = 30.f;
    press(g, BTN_HEAL);
    CHECK(g->player.health >= 89.f && g->prog.items[IT_MEDKIT] == m0, "[H] medkit heals +60 (%.0f)", g->player.health);
}

static void test_skills_live(Game *g) {
    g->prog.skill_points = 1;
    press(g, BTN_CHAR);
    g->ui_sel = SK_TOUGH_HIDE;
    press(g, BTN_USE);
    press(g, BTN_MENU);
    CHECK(prog_has(&g->prog, SK_TOUGH_HIDE) && g->player.health_max == 125.f,
          "unlocking TOUGH HIDE in the UI lifts max HP to 125");
    CHECK(g->ui == UI_NONE, "ESC closes any screen");
}

static void test_shop_live(Game *g) {
    int rosa = economy_vendor_near(&g->econ, 0, v3(158.f, 0.f, 612.f), 2.f);
    CHECK(rosa >= 0, "Rosa's stall exists in the world");
    if (rosa < 0) return;
    Vendor *v = &g->econ.vendors[rosa];
    tp(g, v->pos.x, v->pos.z + 1.6f);
    step_frames(g, 0, 0, 25);    /* let use_cd etc. settle */
    tp(g, v->pos.x, v->pos.z + 1.6f);
    g->cash = 100;
    press(g, BTN_USE);
    CHECK(g->ui == UI_SHOP && g->ui_vendor == rosa, "[E] at the stall opens the shop");
    int m0 = g->prog.items[IT_MEDKIT];
    press(g, BTN_USE);                      /* offer 0 = Medkit $60 */
    CHECK(g->cash == 40 && g->prog.items[IT_MEDKIT] == m0 + 1, "bought a medkit ($100 -> $%d)", g->cash);
    press(g, BTN_USE);
    CHECK(g->cash == 40 && g->prog.items[IT_MEDKIT] == m0 + 1, "second medkit refused: not enough cash");
    /* sell hides: navigate to the last offer (UP wraps) */
    g->prog.mat[MAT_BOAR_HIDE] = 2;
    press(g, BTN_UP);
    CHECK(g->ui_sel == v->offer_n - 1, "UP wraps to the bottom of the list");
    press(g, BTN_USE);
    CHECK(g->cash == 140 && g->prog.mat[MAT_BOAR_HIDE] == 0, "sold 2 boar hides for $100 (cash $%d)", g->cash);
    /* ammo respects the reserve cap */
    int sel9 = -1;
    for (int i = 0; i < v->offer_n; i++) if (v->offers[i].kind == OFFER_AMMO && v->offers[i].value == AMMO_9MM) sel9 = i;
    if (sel9 >= 0) {
        g->ui_sel = sel9;
        g->ammo[AMMO_9MM] = game_ammo_cap(g, AMMO_9MM);
        int c0 = g->cash;
        press(g, BTN_USE);
        CHECK(g->cash == c0, "ammo at cap: purchase refused, no charge");
    }
    press(g, BTN_MENU);
    CHECK(g->ui == UI_NONE, "ESC leaves the shop");
}

static void test_outpost_ferry_travel_save(Game *g) {
    Outpost *o = &g->outpost;
    /* alarm raises island alert */
    float a0 = g->prog.alert;
    g->outpost.alarm = 0;
    /* liberate: remove the garrison, hold the yard 3.5 s */
    for (int i = 0; i < g->enemies.count; i++)
        if (g->enemies.v[i].faction == 2) enemy_apply_damage(&g->enemies.v[i], 5000.f, ZONE_HEAD);
    int xp0 = g->prog.xp_total;
    tp(g, o->center.x, o->center.z);
    step_frames(g, 0, 0, 60 * 4);
    CHECK(o->captured, "outpost liberated");
    CHECK(g->prog.xp_total - xp0 >= 375, "undetected liberation pays the stealth bonus (+%d)", g->prog.xp_total - xp0);
    CHECK(g->prog.ft_unlocked[FT_OUTPOST], "liberating unlocks the outpost fast-travel point");
    CHECK(g->prog.alert <= a0, "stealth capture does not raise island alert");
    int reveal0 = game_map_reveal_pct(g);

    /* ferry to the city and back: the island remembers */
    press(g, BTN_FERRY);
    CHECK(g->act == 1, "ferry to Meridian");
    step_frames(g, 0, 0, 30);
    CHECK(g->prog.ft_unlocked[FT_CITY_SAFEHOUSE], "arriving at the harbor discovers the city safehouse");
    press(g, BTN_FERRY);
    CHECK(g->act == 0 && g->outpost.captured && g->outpost.flag_raise >= 1.f,
          "back on the island: outpost is STILL liberated");
    int alive2 = 0;
    for (int i = 0; i < g->enemies.count; i++)
        if (g->enemies.v[i].faction == 2 && g->enemies.v[i].state != EN_DEAD) alive2++;
    CHECK(alive2 == 0, "no garrison respawned (%d alive)", alive2);
    CHECK(game_map_reveal_pct(g) >= reveal0, "explored map survives the ferry (%d%% >= %d%%)",
          game_map_reveal_pct(g), reveal0);

    /* fast travel via the map screen */
    press(g, BTN_MAP);
    CHECK(g->ui == UI_MAP, "[TAB] opens the map");
    press(g, BTN_DOWN);                  /* sel 1 = OUTPOST */
    press(g, BTN_USE);
    CHECK(g->ui == UI_NONE && v3_dist_xz(g->player.pos, o->flag_pos) < 8.f,
          "fast travel lands at the liberated outpost (%.1f m)", v3_dist_xz(g->player.pos, o->flag_pos));
    press(g, BTN_MAP);
    g->ui_sel = FT_MAST;
    press(g, BTN_USE);
    CHECK(g->ui == UI_MAP, "locked node refuses travel (mast not synced)");
    press(g, BTN_MAP);
    CHECK(g->ui == UI_NONE, "[TAB] closes the map");

    /* safehouse rest = heal + autosave */
    Vec3 sh = g->safehouse[0];
    tp(g, sh.x + 1.0f, sh.z);
    step_frames(g, 0, 0, 2);
    tp(g, sh.x + 1.0f, sh.z);
    g->player.health = 40.f;
    int sw = g->saves_written;
    press(g, BTN_USE);
    CHECK(g->saves_written == sw + 1 && g->player.health == g->player.health_max,
          "safehouse [E]: healed to full and autosaved");
    CHECK(save_slot_exists(save_slot_index(0, 1)), "autosave file is on disk");

    /* manual save round-trip */
    g->cash = 1234;
    g->prog.mat[MAT_SCRAP] = 7;
    g->prog.items[IT_BANDAGE] = 3;
    g->armor = 50.f;
    int lvl = g->prog.level, skills = (int)g->prog.skills;
    Vec3 pos = g->player.pos;
    CHECK(game_save(g, 0, 0), "manual save slot 1 written");
    g->cash = 0; g->prog.mat[MAT_SCRAP] = 0; g->prog.items[IT_BANDAGE] = 0; g->armor = 0.f;
    g->prog.skills = 0; g->prog.level = 1;
    CHECK(game_load(g, 0, 0), "manual save loads");
    CHECK(g->cash == 1234 && g->prog.mat[MAT_SCRAP] == 7 && g->prog.items[IT_BANDAGE] == 3 &&
          g->armor == 50.f, "cash, materials, items, armor restored");
    CHECK(g->prog.level == lvl && (int)g->prog.skills == skills && g->player.health_max == 125.f,
          "level %d + skills restored (max HP 125 re-applied)", g->prog.level);
    CHECK(g->act == 0 && g->outpost.captured && g->prog.ft_unlocked[FT_OUTPOST],
          "island progress restored from the save");
    CHECK(v3_dist_xz(g->player.pos, pos) < 1.0f, "position restored");

    /* wanted persistence: save in the city with 2 stars */
    press(g, BTN_FERRY);
    CHECK(g->act == 1, "ferry to the city for the wanted-save test");
    heat_add_evidence(&g->city.heat, 3.f);
    CHECK(g->city.heat.stars == 2, "2 stars");
    CHECK(game_fast_travel(g, FT_CITY_SAFEHOUSE) == -3, "wanted: fast travel refused");
    CHECK(game_save(g, 1, 0), "manual save slot 2 (city, 2 stars)");
    heat_init(&g->city.heat);
    CHECK(game_load(g, 1, 0) && g->act == 1 && g->city.heat.stars == 2,
          "loading brings back the city AND the 2-star heat");

    /* title-screen continue */
    g->mode = GM_MENU;
    press(g, BTN_LOAD);
    CHECK(g->mode == GM_PLAY && g->act == 0, "title [L] continues from the safehouse autosave (island)");
}

static void test_alert_live(Game *g) {
    /* fresh island outpost: alarm raises alert */
    Progress *p = &g->prog;
    p->alert = 0.f;
    g->outpost.captured = 0; g->outpost.alarm = 0;
    prog_event(p, EV_ALARM, 0);
    CHECK(prog_alert_level(p) >= 1 && prog_reinforce_size(p) >= 4, "alarm -> WARY, bigger waves");
    p->alert = 0.f;
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out);
    dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke5", "════ M5 SYSTEMS & PROGRESSION SMOKE ════");
    for (int i = 0; i < SAVE_TOTAL_SLOTS; i++) {
        if (save_slot_exists(i)) save_delete_slot(i < SAVE_AUTO_SLOTS ? i : i - SAVE_AUTO_SLOTS, i < SAVE_AUTO_SLOTS);
    }

    test_xp_and_skills();
    test_economy_rules();

    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    memset(&g, 0, sizeof g);
    if (!game_init(&g, 320, 180, REND_SOFT, 0xC0FFEEu)) {
        CHECK(0, "game_init");
        return 1;
    }
    CHECK(g.econ.vendor_n == 2 && g.player.health_max == 100.f, "game boots with economy + base 100 HP");
    game_start_play(&g);
    game_outpost_start(&g);             /* loadout + fresh garrison */
    CHECK(g.safehouse[0].x > 0.f, "island safehouse built");

    test_hunt_and_heal(&g);
    test_skills_live(&g);
    test_shop_live(&g);
    test_alert_live(&g);
    test_outpost_ferry_travel_save(&g);

    /* world-swap leak guard: 5 more ferry round trips must not grow the mesh
       registry (it used to leak every terrain chunk per trip) */
    {
        press(&g, BTN_FERRY); press(&g, BTN_FERRY);
        int m0 = mesh_count();
        for (int i = 0; i < 5; i++) { press(&g, BTN_FERRY); press(&g, BTN_FERRY); }
        CHECK(g.act == 0 && mesh_count() <= m0 && g.terrain.chunk_count > 0,
              "10 ferry crossings: mesh registry flat (%d -> %d), terrain still meshed (%d chunks)",
              m0, mesh_count(), g.terrain.chunk_count);
    }

    /* stability: 20 s of island sim after everything, no NaN */
    step_frames(&g, 0, 0, 60 * 20);
    CHECK(v3_valid(g.player.pos), "20 s post-test sim: player position finite");

    game_free(&g);
    DH_INFO("smoke5", "──── %d checks, %d failed ────", g_checks, g_failed);
    if (g_failed) printf("M5 SYSTEMS SMOKE FAILED: %d/%d checks failed\n", g_failed, g_checks);
    else printf("M5 SYSTEMS SMOKE PASSED: %d/%d checks\n", g_checks, g_checks);
    dh_log_shutdown();
    return g_failed ? 1 : 0;
}
