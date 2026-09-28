/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M6 story missions smoke test (headless, deterministic)
   Proves:
     §73  11 missions load from data/missions.json; the embedded fallback is
          the same campaign; broken JSON degrades to the embedded copy
     pure state machine: every objective kind completes / refuses correctly,
          choices must be picked, deaths rebase the checkpoint
     live game: the WHOLE campaign is played through game_frame — text cards
          via E, choices via 1/2, herbs/hunt/craft/buy/rest, mast + outpost,
          Beto's ferry both ways, car theft, heat + survive + lose heat, odd
          job, the Faro Viejo assault — to an ending; saves round-trip the
          campaign mid-way.
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
#include "../src/ai/enemy.h"
#include "../src/rend/rend.h"
#include "../src/city/heat.h"
#include "../src/city/city.h"
#include "../src/meta/progress.h"
#include "../src/meta/mission.h"
#include "../src/game/game.h"
#include "../src/plat/plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke6", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke6", "ok: " __VA_ARGS__); \
} while (0)

#define DT (1.0f / 60.0f)

static void step_frames(Game *g, uint32_t pressed, int n) {
    for (int i = 0; i < n; i++) {
        PlatInput in; memset(&in, 0, sizeof(in));
        in.pressed = (i == 0) ? pressed : 0u;
        game_frame(g, &in, DT);
        g->player.health = g->player.health_max;      /* story walk-through, not a combat test */
    }
}
static void press(Game *g, uint32_t b) { step_frames(g, b, 1); }
static void tp(Game *g, Vec3 p) {
    g->player.pos = v3(p.x, terrain_height(&g->terrain, p.x, p.z) + 0.05f, p.z);
    g->player.vel = v3(0, 0, 0);
}
static const Objective *cur_obj(Game *g) { return mission_objective(&g->ms, &g->missions); }
static int cur_kind(Game *g) { const Objective *o = cur_obj(g); return o ? o->kind : -1; }

/* page through cards; choices take `pick` */
static int clear_cards(Game *g, int pick) {
    int n = 0;
    for (int guard = 0; guard < 40 && g->ui == UI_CARD; guard++) {
        const Objective *o = cur_obj(g);
        if (o && o->kind == OBJ_CHOICE && g->card_page >= o->line_n - 1 && !g->card_result)
            press(g, pick == 2 ? BTN_SLOT2 : BTN_SLOT1);
        else { press(g, BTN_USE); n++; }
    }
    step_frames(g, 0, 2);
    return n;
}
/* run until the mission is active (waits out the between-mission delay) */
static void wait_active(Game *g) {
    for (int i = 0; i < 600 && !g->ms.active && g->ms.cur < g->missions.n; i++) step_frames(g, 0, 1);
}

/* ─────────────────────────── data ─────────────────────────── */
static void test_data(void) {
    MissionSet a, b;
    int f = missions_load(&a);
    CHECK(f == 1 && a.from_file && a.n == 11, "missions.json: %d missions from file", a.n);
    int kinds[OBJ_KIND_N] = {0}, objs = 0, cards = 0, lines_ok = 1;
    for (int i = 0; i < a.n; i++)
        for (int k = 0; k < a.m[i].obj_n; k++) {
            const Objective *o = &a.m[i].obj[k];
            kinds[o->kind]++; objs++;
            if (o->kind == OBJ_CARD || o->kind == OBJ_CHOICE) { cards++; if (o->line_n < 1) lines_ok = 0; }
            if (o->kind != OBJ_CARD && o->kind != OBJ_CHOICE && !o->text[0]) lines_ok = 0;
        }
    int used = 0; for (int k = 0; k < OBJ_KIND_N; k++) if (kinds[k]) used++;
    CHECK(used == OBJ_KIND_N, "all %d objective kinds are used by the campaign (%d used)", OBJ_KIND_N, used);
    CHECK(lines_ok && cards >= 15, "%d objectives, %d text cards, every card has lines / every task has tracker text", objs, cards);
    CHECK(a.m[0].id[0] == 'M' && !strcmp(a.m[10].title, "HIGH LIGHTHOUSE"), "M01..M11 ordered, finale HIGH LIGHTHOUSE");
    int endings = 0;
    for (int k = 0; k < a.m[10].obj_n; k++) if (a.m[10].obj[k].sets_ending) endings++;
    CHECK(endings == 1, "finale carries exactly one ending choice (2 endings)");

    /* embedded fallback == data file */
    extern int missions_parse(MissionSet *, const char *);
    FILE *fp = fopen("game/data/missions.json", "rb");
    CHECK(fp != NULL, "data file readable from repo root");
    if (fp) fclose(fp);
    MissionSet e;
    const char *broken = "{ \"missions\": [ { \"id\": \"X\", ";
    CHECK(missions_parse(&b, broken) == 0, "broken JSON parses to 0 missions (no crash)");
    missions_parse(&e, "{\"missions\":[{\"id\":\"T1\",\"title\":\"T\",\"objectives\":[{\"kind\":\"warp\"},{\"kind\":\"choice\",\"lines\":[\"x\"]}]}]}");
    CHECK(e.n == 1 && e.m[0].obj_n == 1 && e.m[0].obj[0].kind == OBJ_CARD,
          "unknown kinds skipped, a choice without options degrades to a card");
}

/* ─────────────────────────── pure state machine ─────────────────────────── */
static void test_rules(void) {
    MissionSet s;
    int n = missions_parse(&s,
        "{\"missions\":[{\"id\":\"A\",\"title\":\"A\",\"cash\":10,\"xp\":5,\"objectives\":["
        "{\"kind\":\"card\",\"lines\":[\"hi\"]},"
        "{\"kind\":\"goto\",\"act\":0,\"x\":100,\"z\":100,\"r\":5,\"text\":\"go\"},"
        "{\"kind\":\"herbs\",\"n\":2,\"text\":\"h\"},"
        "{\"kind\":\"survive\",\"act\":1,\"n\":3,\"text\":\"s\"},"
        "{\"kind\":\"choice\",\"ending\":1,\"lines\":[\"q\"],\"options\":[\"a\",\"b\"]}]},"
        "{\"id\":\"B\",\"title\":\"B\",\"objectives\":[{\"kind\":\"lose_heat\",\"act\":1,\"text\":\"l\"}]}]}");
    CHECK(n == 2, "inline campaign parsed (2 missions)");
    MissionState st; mission_reset(&st);
    MissionCtx c; memset(&c, 0, sizeof c); c.assault_left = -1;
    MissionEvent ev = mission_tick(&st, &s, &c, 1.0f);
    CHECK(ev == MEV_NONE && !st.active, "missions wait a beat before opening");
    ev = mission_tick(&st, &s, &c, 1.5f);
    CHECK(ev == MEV_STARTED && st.active && st.card_open, "first mission opens on its card");
    CHECK(mission_tick(&st, &s, &c, DT) == MEV_CARD, "card blocks progress until dismissed");
    mission_card_done(&st, &s, &c, 0);
    CHECK(st.obj == 1, "card dismissed -> goto");
    c.pos = v3(100, 0, 110);
    CHECK(mission_tick(&st, &s, &c, DT) == MEV_NONE, "10 m away of a 5 m goto: not yet");
    c.act = 1; c.pos = v3(100, 0, 101);
    CHECK(mission_tick(&st, &s, &c, DT) == MEV_NONE, "right spot, wrong act: not yet");
    char tr[128]; mission_tracker(&st, &s, &c, tr, sizeof tr);
    CHECK(strstr(tr, "ferry") != NULL, "wrong act -> tracker says take the ferry ('%s')", tr);
    c.act = 0; c.herbs = 7;   /* lifetime herbs before the herb objective starts */
    Vec3 wp; CHECK(mission_waypoint(&st, &s, &c, &wp) && wp.x == 100.f, "goto has a waypoint");
    CHECK(mission_tick(&st, &s, &c, DT) == MEV_OBJ_DONE && st.obj == 2, "goto completes in radius");
    mission_tick(&st, &s, &c, DT);
    CHECK(st.obj == 2 && st.base == 7, "herbs counted from objective start, not lifetime");
    c.herbs = 8; mission_tick(&st, &s, &c, DT);
    mission_on_death(&st, &s, &c);
    c.herbs = 9; mission_tick(&st, &s, &c, DT);
    CHECK(st.obj == 2, "death rebases the checkpoint (8 -> 9 is only 1 of 2)");
    c.herbs = 10;
    CHECK(mission_tick(&st, &s, &c, DT) == MEV_OBJ_DONE && st.obj == 3, "2 herbs after checkpoint -> done");
    c.act = 1; c.stars = 0;
    for (int i = 0; i < 300; i++) mission_tick(&st, &s, &c, DT);
    CHECK(st.obj == 3 && st.timer == 0.f, "survive clock does not run without heat");
    c.stars = 2;
    for (int i = 0; i < 200 && st.obj == 3; i++) mission_tick(&st, &s, &c, DT);
    CHECK(st.obj == 4 && st.card_open, "survive 3 s with 2 stars -> choice card");
    CHECK(mission_card_done(&st, &s, &c, 0) == MEV_NONE && st.obj == 4, "choice refuses to close without a pick");
    ev = mission_card_done(&st, &s, &c, 2);
    CHECK(ev == MEV_MISSION_DONE && st.ending == 2 && st.choices[0] == 2 && st.cur == 1,
          "pick 2 -> ending 2 recorded, mission A done");
    for (int i = 0; i < 400 && !st.active; i++) mission_tick(&st, &s, &c, DT);
    CHECK(st.active && st.cur == 1, "mission B opens after the gap");
    CHECK(mission_tick(&st, &s, &c, DT) == MEV_NONE, "lose_heat: still 2 stars");
    c.stars = 0;
    CHECK(mission_tick(&st, &s, &c, DT) == MEV_ALL_DONE && st.cur == 2 && st.done_mask == 3,
          "0 stars -> campaign complete");
    mission_tracker(&st, &s, &c, tr, sizeof tr);
    CHECK(strstr(tr, "COMPLETE") != NULL, "tracker after the campaign: free roam");
}

/* ─────────────────────────── live campaign ─────────────────────────── */
static void park_chasers(Game *g) {
    City *c = &g->city;
    for (int i = c->chase0; i < c->veh_count; i++) {
        if (c->veh[i].state != VS_CHASE) continue;
        c->veh[i].state = VS_PARKED; c->veh[i].speed = 0.f;
        c->veh[i].pos = v3(380.0f + 80.0f * (float)(i % 4), 4.0f, 200.0f);
    }
    c->heli_active = 0;
}
static void clear_heat(Game *g) {
    heat_init(&g->city.heat);
    park_chasers(g);
    step_frames(g, 0, 3);
}

static void play_campaign(Game *g) {
    int start_cash = g->cash, start_lvl = g->prog.level;

    /* ── M01 LANDFALL ── */
    wait_active(g);
    DH_INFO("smoke6", "state: cur %d active %d wait %.2f ui %d mode %d", g->ms.cur, g->ms.active, (double)g->ms.wait_t, g->ui, g->mode);
    CHECK(g->ms.cur == 0 && g->ui == UI_CARD, "M01 opens with Tobias's voicemail card");
    int pages = clear_cards(g, 1);
    CHECK(pages == 3 && cur_kind(g) == OBJ_GOTO, "voicemail pages 1/3..3/3 with E, then GOTO");
    tp(g, v3(g->safehouse[0].x + 2.f, 0, g->safehouse[0].z)); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_REST, "reaching the hut -> rest objective");
    g->ms.wait_t = 0;
    tp(g, v3(g->safehouse[0].x + 1.f, 0, g->safehouse[0].z));
    press(g, BTN_USE); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_HERBS, "E at the hut rests (and saves) -> herbs");
    int herbs = 0;
    for (int i = 0; i < g->pickups.count && herbs < 3; i++) {
        Pickup *k = &g->pickups.v[i];
        if (!k->live || k->kind != PK_HERB) continue;
        tp(g, k->pos); step_frames(g, 0, 3); herbs++;
    }
    CHECK(cur_kind(g) == OBJ_GOTO && g->stat_herbs >= 3, "walked over 3 real herb patches (%d)", g->stat_herbs);
    Vec3 rosa = g->econ.vendors[0].pos;
    tp(g, v3(rosa.x + 1.f, 0, rosa.z)); step_frames(g, 0, 2);
    CHECK(g->ui == UI_CARD, "at Rosa's -> her card");
    clear_cards(g, 1);
    CHECK(g->ms.cur == 1 && (g->ms.done_mask & 1), "M01 complete");
    CHECK(g->cash == start_cash + g->missions.m[0].cash, "M01 reward paid ($%d)", g->missions.m[0].cash);

    /* ── M02 SIGNAL FIRE ── */
    wait_active(g);
    CHECK(g->ms.cur == 1 && cur_kind(g) == OBJ_GOTO, "M02 opens: reach the mast");
    { Vec3 wp; CHECK(game_story_waypoint(g, &wp) && v3_dist_xz(wp, g->outpost.mast_pos) < 1.f, "mast objective has a waypoint on the mast"); }
    tp(g, g->outpost.mast_pos); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_MAST, "at the mast -> sync it");
    g->outpost.mast_synced = 1; step_frames(g, 0, 2);   /* M3 smoke proves the climb itself */
    CHECK(g->ui == UI_CARD, "synced -> sermon tape");
    clear_cards(g, 1);
    CHECK(g->ms.cur == 2, "M02 complete (2 cards)");

    /* ── M03 HIDE AND SEEK ── */
    wait_active(g);
    int ci = -1;
    for (int i = 0; i < g->critter_count; i++) if (g->critters[i].state != 4) { ci = i; break; }
    CHECK(ci >= 0, "a live animal exists to hunt");
    if (ci >= 0) {
        g->critters[ci].state = 3;                          /* shot (M5 smoke proves the kill) */
        tp(g, g->critters[ci].pos);
        step_frames(g, 0, 25);
        press(g, BTN_USE); step_frames(g, 0, 2);
    }
    CHECK(cur_kind(g) == OBJ_CRAFT, "skinned -> craft");
    g->prog.mat[MAT_HERB] += 5; g->prog.mat[MAT_DEER_HIDE] += 5; g->prog.mat[MAT_BOAR_HIDE] += 5;
    int crafted = 0;
    for (int r = 0; r < g->econ.recipe_n && !crafted; r++)
        if (prog_can_craft(&g->prog, &g->econ.recipes[r])) crafted = (prog_craft(&g->prog, &g->econ.recipes[r]) == 0);
    step_frames(g, 0, 2);
    CHECK(crafted && cur_kind(g) == OBJ_BUY, "crafted a recipe -> buy");
    g->cash += 500;
    tp(g, v3(rosa.x + 1.f, 0, rosa.z)); step_frames(g, 0, 25);
    press(g, BTN_USE);
    CHECK(g->ui == UI_SHOP, "E at Rosa opens the shop");
    for (int k = 0; k < 6 && cur_kind(g) == OBJ_BUY; k++) {
        g->ui_sel = k; press(g, BTN_USE);
        g->ui = UI_NONE; step_frames(g, 0, 2);
        if (cur_kind(g) == OBJ_BUY) { press(g, BTN_USE); }
    }
    step_frames(g, 0, 2);
    CHECK(g->ui == UI_CARD || g->ms.cur == 3, "bought something -> Rosa's advice");
    clear_cards(g, 1);
    CHECK(g->ms.cur == 3, "M03 complete");

    /* ── M04 THE SERMON (choice 1) ── */
    wait_active(g);
    tp(g, v3(g->outpost.center.x, 0, g->outpost.center.z - 80.f)); step_frames(g, 0, 2);
    CHECK(g->ui == UI_CARD && cur_kind(g) == OBJ_CHOICE, "scouting the compound -> Sereno's radio choice");
    clear_cards(g, 1);
    CHECK(g->ms.choices[3] == 1 && cur_kind(g) == OBJ_KILLS, "picked 'ask about Tobias' -> thin the garrison");
    for (int k = 0; k < 3; k++) prog_event(&g->prog, EV_KILL, 0);   /* M2/M3 smokes prove real kills */
    step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_CAPTURE, "3 kills -> capture");
    g->outpost.captured = 1; step_frames(g, 0, 2);
    clear_cards(g, 1);
    CHECK(g->ms.cur == 4, "M04 complete (outpost liberated)");

    /* save mid-campaign → reload → same place */
    int saved_cur = g->ms.cur, saved_mask = g->ms.done_mask;
    CHECK(game_save(g, 1, 0), "manual save between M04 and M05");
    g->ms.cur = 0; g->ms.done_mask = 0; g->ms.choices[3] = 0;
    CHECK(game_load(g, 1, 0) && g->ms.cur == saved_cur && g->ms.done_mask == saved_mask &&
          g->ms.choices[3] == 1, "load restores campaign position, done mask and choices");

    /* ── M05 CROSSING ── */
    wait_active(g);
    CHECK(g->ms.cur == 4 && cur_kind(g) == OBJ_FERRY, "M05: ferry objective");
    press(g, BTN_FERRY); step_frames(g, 0, 2);
    CHECK(g->act == 1 && g->ui == UI_CARD, "arrived in Meridian -> Beto's card");
    clear_cards(g, 1);
    tp(g, v3(g->safehouse[1].x + 2.f, 0, g->safehouse[1].z)); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_DRIVE, "harbor safehouse found -> get a car");
    int vi = -1;
    for (int i = 0; i < g->city.veh_count; i++) if (g->city.veh[i].state == VS_PARKED) { vi = i; break; }
    if (vi >= 0) { g->in_vehicle = vi; g->city.veh[vi].state = VS_DRIVEN; }
    step_frames(g, 0, 2);
    CHECK(g->ms.cur == 5, "in a car -> M05 complete");

    /* ── M06 GILT INVITATION ── */
    if (g->in_vehicle >= 0) { g->city.veh[g->in_vehicle].state = VS_PARKED; g->in_vehicle = -1; }
    wait_active(g);
    tp(g, v3(500, 0, 500)); step_frames(g, 0, 2);
    clear_cards(g, 1);
    CHECK(cur_kind(g) == OBJ_HEAT, "downtown call -> draw 2 stars");
    heat_add_evidence(&g->city.heat, 3.5f); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_LOSE_HEAT, "2 stars -> lose them");
    clear_heat(g);
    clear_cards(g, 1);
    CHECK(g->ms.cur == 6, "M06 complete");

    /* ── M07 UNION DUES ── */
    wait_active(g);
    tp(g, g->city.job_board); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_JOB, "at the job board -> odd job");
    prog_event(&g->prog, EV_JOB, 0); step_frames(g, 0, 2);  /* M4 smoke proves the job route */
    clear_cards(g, 1);
    CHECK(cur_kind(g) == OBJ_BUY, "foreman's card -> gear up");
    g->stat_buys++; step_frames(g, 0, 2);
    CHECK(g->ms.cur == 7, "M07 complete");

    /* ── M08 LIMPIO (survive 45 s at 3 stars through the live city) ── */
    wait_active(g);
    clear_cards(g, 1);
    heat_add_evidence(&g->city.heat, 7.f); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_SURVIVE, "3 stars -> survive");
    for (int i = 0; i < 60 * 50 && cur_kind(g) == OBJ_SURVIVE; i++) {
        if (g->city.heat.stars < 1) heat_add_evidence(&g->city.heat, 2.f);
        step_frames(g, 0, 1);
    }
    CHECK(cur_kind(g) == OBJ_LOSE_HEAT, "stayed free 45 s with the city hunting -> shake them");
    clear_heat(g);
    CHECK(cur_kind(g) == OBJ_REST, "clean -> lie low");
    tp(g, v3(g->safehouse[1].x + 1.f, 0, g->safehouse[1].z));
    g->city.use_cd = 0.f; step_frames(g, 0, 1);
    press(g, BTN_USE); step_frames(g, 0, 2);
    CHECK(g->ms.cur == 8, "rested at the harbor safehouse -> M08 complete");

    /* ── M09 BROADCAST (choice 2) ── */
    wait_active(g);
    tp(g, v3(500, 0, 400)); step_frames(g, 0, 2);
    clear_cards(g, 2);
    CHECK(g->ms.choices[8] == 2, "broadcast: told them about Tobias");
    heat_add_evidence(&g->city.heat, 3.5f); step_frames(g, 0, 2);
    clear_heat(g);
    CHECK(g->ms.cur == 9, "M09 complete");

    /* ── M10 MONSOON TRADE ── */
    wait_active(g);
    press(g, BTN_FERRY); step_frames(g, 0, 2);
    CHECK(g->act == 0 && g->ui == UI_CARD, "ferried home -> Tobias tape 8");
    clear_cards(g, 1);
    tp(g, v3(g->safehouse[0].x + 1.f, 0, g->safehouse[0].z)); step_frames(g, 0, 2);
    press(g, BTN_USE); step_frames(g, 0, 2);
    CHECK(cur_kind(g) == OBJ_GOTO, "rested -> head for Faro Viejo");
    tp(g, v3(g->arena_center.x, 0, g->arena_center.z - 60.f)); step_frames(g, 0, 2);
    CHECK(g->ms.cur == 10, "M10 complete");

    /* ── M11 HIGH LIGHTHOUSE ── */
    wait_active(g);
    tp(g, v3(g->arena_center.x, 0, g->arena_center.z - 40.f)); step_frames(g, 0, 3);
    int alive = 0;
    for (int i = 0; i < g->enemies.count; i++)
        if (g->enemies.v[i].faction == 0 && g->enemies.v[i].state != EN_DEAD) alive++;
    CHECK(g->arena_active && alive == 14, "assault: 14 cult hostiles spawned at the lighthouse yard (%d)", alive);
    for (int i = 0; i < g->enemies.count; i++)
        if (g->enemies.v[i].faction == 0) g->enemies.v[i].state = EN_DEAD;
    step_frames(g, 0, 3);
    CHECK(g->ui == UI_CARD, "congregation broken -> Sereno's last words");
    clear_cards(g, 1);
    CHECK(g->ms.cur == 11 && g->ms.ending == 1, "published the ledger -> ENDING 1 (OPEN HORIZON)");
    CHECK(g->ms.done_mask == 0x7FF, "all 11 missions complete (mask 0x%X)", g->ms.done_mask);
    CHECK(g->prog.level > start_lvl, "campaign XP levelled Mara (L%d -> L%d)", start_lvl, g->prog.level);
    step_frames(g, 0, 300);
    CHECK(g->ui == UI_NONE && g->mode == GM_PLAY, "free roam continues after the credits card");
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "shots";
    dh_log_init(out);
    dh_log_set_level(DH_LOG_INFO);
    DH_INFO("smoke6", "════ M6 STORY MISSIONS SMOKE ════");
    for (int i = 0; i < SAVE_TOTAL_SLOTS; i++)
        if (save_slot_exists(i)) save_delete_slot(i < SAVE_AUTO_SLOTS ? i : i - SAVE_AUTO_SLOTS, i < SAVE_AUTO_SLOTS);

    test_data();
    test_rules();

    settings_defaults(settings());
    settings_apply_preset(settings(), QUALITY_LOW);
    static Game g;
    memset(&g, 0, sizeof g);
    if (!game_init(&g, 320, 180, REND_SOFT, 0xC0FFEEu)) { CHECK(0, "game_init"); return 1; }
    CHECK(g.missions.n == 11, "game boots with the 11-mission campaign");
    CHECK(g.story == 0, "story is OFF by default (tests / DoD modes stay sandbox)");
    game_start_play(&g);
    game_outpost_start(&g);
    g.story = 1;
    play_campaign(&g);

    DH_INFO("smoke6", "──── %d checks, %d failed ────", g_checks, g_failed);
    game_free(&g);
    if (g_failed) { printf("M6 MISSIONS SMOKE FAILED: %d/%d\n", g_failed, g_checks); return 1; }
    printf("M6 MISSIONS SMOKE PASSED: %d/%d checks\n", g_checks, g_checks);
    dh_log_shutdown();
    return 0;
}
