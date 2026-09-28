/* DIVIDED HORIZON — story mission state machine (M6). See mission.h. */
#include "mission.h"
#include "../core/dh_json.h"
#include "../core/dh_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "missions_embed.h"   /* MISSIONS_EMBED: generated from data/missions.json */

static const char *KIND_NAMES[OBJ_KIND_N] = {
    "card", "choice", "goto", "herbs", "hunt", "craft", "buy", "rest", "kills",
    "capture", "mast", "ferry", "drive", "heat", "lose_heat", "survive", "job", "assault"
};
static const char *ANC_NAMES[ANC_N] = {
    "", "beach", "safehouse_i", "vendor_i", "outpost", "mast",
    "lighthouse", "pier", "safehouse_c", "vendor_c", "job_board"
};

const char *obj_kind_name(int k) { return (k >= 0 && k < OBJ_KIND_N) ? KIND_NAMES[k] : "?"; }

static int lookup(const char *s, const char **tab, int n) {
    for (int i = 0; i < n; i++) if (tab[i][0] && strcmp(s, tab[i]) == 0) return i;
    return -1;
}

static void copy_str_arr(JsonValue *a, char out[][MIS_LINE_LEN], int max, int *n) {
    *n = 0;
    for (int i = 0; a && i < json_arr_len(a) && *n < max; i++) {
        JsonValue *v = json_arr_get(a, i);
        if (v && v->type == JSON_STR) snprintf(out[(*n)++], MIS_LINE_LEN, "%s", v->str);
    }
}

int missions_parse(MissionSet *s, const char *json) {
    memset(s, 0, sizeof *s);
    const char *err = NULL;
    JsonValue *root = json_parse(json, &err);
    if (!root) { DH_WARN("mission", "parse error: %s", err ? err : "?"); return 0; }
    JsonValue *ma = json_obj_get(root, "missions");
    for (int i = 0; ma && i < json_arr_len(ma) && s->n < MIS_MAX; i++) {
        JsonValue *mo = json_arr_get(ma, i);
        MissionDef *m = &s->m[s->n];
        snprintf(m->id, sizeof m->id, "%s", json_get_str(mo, "id", "M??"));
        snprintf(m->title, sizeof m->title, "%s", json_get_str(mo, "title", "UNTITLED"));
        snprintf(m->logline, sizeof m->logline, "%s", json_get_str(mo, "logline", ""));
        m->cash = json_get_int(mo, "cash", 0);
        m->xp = json_get_int(mo, "xp", 0);
        JsonValue *oa = json_obj_get(mo, "objectives");
        for (int k = 0; oa && k < json_arr_len(oa) && m->obj_n < MIS_OBJ_MAX; k++) {
            JsonValue *o = json_arr_get(oa, k);
            Objective *ob = &m->obj[m->obj_n];
            int kind = lookup(json_get_str(o, "kind", ""), KIND_NAMES, OBJ_KIND_N);
            if (kind < 0) { DH_WARN("mission", "%s: unknown objective kind '%s' skipped",
                                    m->id, json_get_str(o, "kind", "")); continue; }
            ob->kind = kind;
            ob->act = json_get_int(o, "act", -1);
            const char *an = json_get_str(o, "anchor", "");
            ob->anchor = an[0] ? lookup(an, ANC_NAMES, ANC_N) : ANC_XZ;
            if (ob->anchor < 0) ob->anchor = ANC_XZ;
            ob->x = json_get_flt(o, "x", 0.f);
            ob->z = json_get_flt(o, "z", 0.f);
            ob->r = json_get_flt(o, "r", 12.f);
            ob->n = json_get_int(o, "n", 1);
            ob->sets_ending = json_get_int(o, "ending", 0);
            snprintf(ob->text, sizeof ob->text, "%s", json_get_str(o, "text", ""));
            snprintf(ob->speaker, sizeof ob->speaker, "%s", json_get_str(o, "speaker", ""));
            copy_str_arr(json_obj_get(o, "lines"), ob->lines, MIS_LINES_MAX, &ob->line_n);
            int n2 = 0;
            char tmp[2][MIS_LINE_LEN];
            copy_str_arr(json_obj_get(o, "options"), tmp, 2, &n2);
            for (int q = 0; q < n2; q++) snprintf(ob->opt[q], sizeof ob->opt[q], "%.47s", tmp[q]);
            copy_str_arr(json_obj_get(o, "results"), ob->result, 2, &n2);
            if (kind == OBJ_CHOICE && (!ob->opt[0][0] || !ob->opt[1][0])) {
                DH_WARN("mission", "%s: choice without 2 options -> card", m->id);
                ob->kind = OBJ_CARD;
            }
            m->obj_n++;
        }
        if (m->obj_n > 0) s->n++;
    }
    json_free(root);
    return s->n;
}

int missions_load(MissionSet *s) {
    char path[512];
    snprintf(path, sizeof path, "%s/missions.json", dh_fs_data_dir());
    const char *p = path;
    if (!dh_fs_exists(p)) p = "game/data/missions.json";
    size_t len = 0;
    char *text = dh_fs_read_text(p, &len);
    if (text) {
        int n = missions_parse(s, text);
        free(text);
        if (n > 0) { s->from_file = 1; return 1; }
        DH_WARN("mission", "missions.json unusable - embedded campaign");
    }
    missions_parse(s, MISSIONS_EMBED);
    s->from_file = 0;
    return 0;
}

/* ── state machine ── */
void mission_reset(MissionState *st) {
    memset(st, 0, sizeof *st);
    st->wait_t = 2.0f;
}

const MissionDef *mission_current(const MissionState *st, const MissionSet *s) {
    return (st->cur >= 0 && st->cur < s->n) ? &s->m[st->cur] : NULL;
}
const Objective *mission_objective(const MissionState *st, const MissionSet *s) {
    const MissionDef *m = mission_current(st, s);
    if (!m || !st->active || st->obj < 0 || st->obj >= m->obj_n) return NULL;
    return &m->obj[st->obj];
}

static int counter_of(const Objective *o, const MissionCtx *c) {
    switch (o->kind) {
    case OBJ_HERBS: return c->herbs;
    case OBJ_HUNT:  return c->hunts;
    case OBJ_CRAFT: return c->crafts;
    case OBJ_BUY:   return c->buys;
    case OBJ_REST:  return c->rests;
    case OBJ_KILLS: return c->kills;
    case OBJ_JOB:   return c->jobs;
    case OBJ_FERRY: return c->ferries;
    default: return 0;
    }
}

static Vec3 target_of(const Objective *o, const MissionCtx *c) {
    if (o->anchor > ANC_XZ && o->anchor < ANC_N) return c->anchor[o->anchor];
    return v3(o->x, 0.f, o->z);
}

static void begin_objective(MissionState *st, const MissionSet *s, const MissionCtx *c) {
    const Objective *o = mission_objective(st, s);
    st->timer = 0.f;
    st->assault_started = 0;
    st->pending_assault = 0;
    st->card_open = 0;
    if (!o) return;
    st->base = counter_of(o, c);
    if (o->kind == OBJ_CARD || o->kind == OBJ_CHOICE) st->card_open = 1;
}

static int act_ok(const Objective *o, const MissionCtx *c) {
    return o->act < 0 || o->act == c->act;
}

static int objective_met(MissionState *st, const Objective *o, const MissionCtx *c, float dt) {
    int ok = act_ok(o, c);
    switch (o->kind) {
    case OBJ_CARD: case OBJ_CHOICE: return 0;           /* closed by mission_card_done */
    case OBJ_GOTO: {
        if (!ok) return 0;
        return v3_dist_xz(c->pos, target_of(o, c)) <= o->r;
    }
    case OBJ_HERBS: case OBJ_HUNT: case OBJ_CRAFT: case OBJ_BUY: case OBJ_REST:
    case OBJ_KILLS: case OBJ_JOB:
        return ok && counter_of(o, c) - st->base >= o->n;
    case OBJ_FERRY:   return c->act == o->act;          /* already there counts too */
    case OBJ_CAPTURE: return c->outpost_captured;
    case OBJ_MAST:    return c->mast_synced;
    case OBJ_DRIVE:
        if (!ok || !c->driving) return 0;
        if (o->anchor == ANC_XZ && o->x == 0.f && o->z == 0.f) return 1;
        return v3_dist_xz(c->pos, target_of(o, c)) <= o->r;
    case OBJ_HEAT:      return ok && c->stars >= o->n;
    case OBJ_LOSE_HEAT: return ok && c->stars == 0;
    case OBJ_SURVIVE:
        if (!ok) return 0;
        if (c->stars > 0) st->timer += dt;               /* clock only runs while hunted */
        return st->timer >= (float)o->n;
    case OBJ_ASSAULT:
        if (!ok) return 0;
        if (!st->assault_started) {
            if (v3_dist_xz(c->pos, target_of(o, c)) <= o->r) {
                st->assault_started = 1;
                st->pending_assault = o->n;
            }
            return 0;
        }
        return c->assault_left == 0 && st->pending_assault == 0;
    default: return 0;
    }
}

static MissionEvent advance(MissionState *st, const MissionSet *s, const MissionCtx *c) {
    const MissionDef *m = mission_current(st, s);
    st->obj++;
    if (m && st->obj < m->obj_n) { begin_objective(st, s, c); return MEV_OBJ_DONE; }
    st->done_mask |= 1 << st->cur;
    st->cur++;
    st->active = 0;
    st->obj = 0;
    st->card_open = 0;
    st->wait_t = 4.0f;
    return st->cur >= s->n ? MEV_ALL_DONE : MEV_MISSION_DONE;
}

void mission_resume(MissionState *st, const MissionSet *s, const MissionCtx *c) {
    begin_objective(st, s, c);
}

MissionEvent mission_tick(MissionState *st, const MissionSet *s, const MissionCtx *c, float dt) {
    if (st->cur >= s->n) return MEV_NONE;
    if (!st->active) {
        st->wait_t -= dt;
        if (st->wait_t > 0.f) return MEV_NONE;
        st->active = 1;
        st->obj = 0;
        begin_objective(st, s, c);
        return MEV_STARTED;
    }
    const Objective *o = mission_objective(st, s);
    if (!o) return advance(st, s, c);
    if (st->card_open) return MEV_CARD;
    if (st->pending_assault) return MEV_ASSAULT;
    if (objective_met(st, o, c, dt)) return advance(st, s, c);
    if (st->pending_assault) return MEV_ASSAULT;
    return MEV_NONE;
}

MissionEvent mission_card_done(MissionState *st, const MissionSet *s, const MissionCtx *c, int choice) {
    const Objective *o = mission_objective(st, s);
    if (!o || !st->card_open) return MEV_NONE;
    if (o->kind == OBJ_CHOICE) {
        if (choice != 1 && choice != 2) return MEV_NONE;    /* must pick */
        st->choices[st->cur] = choice;
        if (o->sets_ending) st->ending = choice;
    }
    st->card_open = 0;
    return advance(st, s, c);
}

void mission_on_death(MissionState *st, const MissionSet *s, const MissionCtx *c) {
    const Objective *o = mission_objective(st, s);
    if (!o) return;
    /* checkpoint = start of the current objective; counters rebased, survive
       clock restarts. An assault in progress keeps its dead (world persists). */
    if (o->kind == OBJ_SURVIVE) st->timer = 0.f;
    if (o->kind != OBJ_ASSAULT) st->base = counter_of(o, c);
}

void mission_tracker(const MissionState *st, const MissionSet *s, const MissionCtx *c,
                     char *out, int cap) {
    out[0] = 0;
    const MissionDef *m = mission_current(st, s);
    if (!m) { snprintf(out, cap, "CAMPAIGN COMPLETE - free roam"); return; }
    if (!st->active) { snprintf(out, cap, "NEXT: %s %s", m->id, m->title); return; }
    const Objective *o = mission_objective(st, s);
    if (!o) return;
    if (o->act >= 0 && o->act != c->act && o->kind != OBJ_FERRY) {
        snprintf(out, cap, "Take Beto's ferry (K) to %s", o->act ? "MERIDIAN CITY" : "ISLA SOMBRA");
        return;
    }
    int cnt = counter_of(o, c) - st->base;
    switch (o->kind) {
    case OBJ_HERBS: case OBJ_HUNT: case OBJ_CRAFT: case OBJ_BUY: case OBJ_KILLS: case OBJ_JOB:
        snprintf(out, cap, "%s (%d/%d)", o->text, dh_clampi(cnt, 0, o->n), o->n); break;
    case OBJ_SURVIVE:
        snprintf(out, cap, "%s (%ds)%s", o->text, (int)((float)o->n - st->timer + 0.99f),
                 c->stars > 0 ? "" : " - need heat"); break;
    case OBJ_ASSAULT:
        if (st->assault_started && c->assault_left >= 0)
            snprintf(out, cap, "%s (%d left)", o->text, c->assault_left);
        else snprintf(out, cap, "%s", o->text);
        break;
    default: snprintf(out, cap, "%s", o->text); break;
    }
}

int mission_waypoint(const MissionState *st, const MissionSet *s, const MissionCtx *c, Vec3 *out) {
    const Objective *o = mission_objective(st, s);
    if (!o || !act_ok(o, c)) return 0;
    int has = (o->kind == OBJ_GOTO || o->kind == OBJ_ASSAULT ||
               o->kind == OBJ_CAPTURE || o->kind == OBJ_MAST || o->kind == OBJ_REST ||
               o->kind == OBJ_DRIVE || o->kind == OBJ_BUY || o->kind == OBJ_JOB);
    if (!has) return 0;
    if (o->kind == OBJ_CAPTURE && o->anchor == ANC_XZ) { *out = c->anchor[ANC_OUTPOST]; return 1; }
    if (o->kind == OBJ_MAST && o->anchor == ANC_XZ)    { *out = c->anchor[ANC_MAST]; return 1; }
    if (o->anchor == ANC_XZ && o->x == 0.f && o->z == 0.f) return 0;
    *out = target_of(o, c);
    return 1;
}
