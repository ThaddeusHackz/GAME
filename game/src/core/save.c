/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — SaveManager implementation
   ══════════════════════════════════════════════════════════════════════════ */
/* gmtime_r is POSIX; see the same note in dh_log.c. Must precede all headers. */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif

#include "save.h"
#include "dh_log.h"
#include "dh_json.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SaveGame g_active;
static float    g_autosave_timer = 0.0f;
#define AUTOSAVE_INTERVAL 90.0f     /* Spec 6.11: autosave at checkpoints, never mid-combat */

int save_slot_index(int slot, int is_auto) {
    if (is_auto) return dh_clampi(slot, 0, SAVE_AUTO_SLOTS - 1);
    return SAVE_AUTO_SLOTS + dh_clampi(slot, 0, SAVE_MANUAL_SLOTS - 1);
}

static void slot_path(char *out, int outsz, int abs_slot) {
    char name[64];
    if (abs_slot < SAVE_AUTO_SLOTS) snprintf(name, sizeof(name), "save_auto_%d.json", abs_slot);
    else snprintf(name, sizeof(name), "save_%02d.json", abs_slot - SAVE_AUTO_SLOTS + 1);
    dh_fs_join(out, outsz, dh_fs_user_dir(), name);
}

static void make_timestamp(char *out, int outsz, double *epoch_out) {
    time_t t = time(NULL);
    if (epoch_out) *epoch_out = (double)t;
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(out, (size_t)outsz, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

void save_init(void) {
    dh_fs_mkdirs(dh_fs_user_dir());
    memset(&g_active, 0, sizeof(g_active));
    g_active.slot = -1;
    DH_INFO("save", "SaveManager ready — user dir: %s", dh_fs_user_dir());
}
void save_shutdown(void) { }
SaveGame *save_active(void) { return &g_active; }

/* ── flags / tables ────────────────────────────────────────────────────── */
static SaveFlag *find_flag(SaveGame *s, const char *id) {
    for (int i = 0; i < s->flag_count; i++) if (strcmp(s->flags[i].id, id) == 0) return &s->flags[i];
    return NULL;
}
void save_set_flag(SaveGame *s, const char *id, int value) {
    if (!s || !id) return;
    SaveFlag *f = find_flag(s, id);
    if (f) { f->value = value; return; }
    if (s->flag_count >= SAVE_MAX_FLAGS) { DH_WARN("save","flag table full"); return; }
    f = &s->flags[s->flag_count++];
    dh_strcpy_safe(f->id, sizeof(f->id), id);
    f->value = value;
}
int save_get_flag(const SaveGame *s, const char *id, int dflt) {
    if (!s || !id) return dflt;
    SaveFlag *f = find_flag((SaveGame*)s, id);
    return f ? f->value : dflt;
}
int save_has_flag(const SaveGame *s, const char *id) { return save_get_flag(s, id, 0) != 0; }

SaveMission *save_mission(SaveGame *s, const char *id) {
    if (!s || !id) return NULL;
    for (int i = 0; i < s->mission_count; i++) if (strcmp(s->missions[i].id, id) == 0) return &s->missions[i];
    if (s->mission_count >= SAVE_MAX_MISSIONS) return NULL;
    SaveMission *m = &s->missions[s->mission_count++];
    memset(m, 0, sizeof(*m));
    dh_strcpy_safe(m->id, sizeof(m->id), id);
    return m;
}
SaveOutpost *save_outpost(SaveGame *s, const char *id) {
    if (!s || !id) return NULL;
    for (int i = 0; i < s->outpost_count; i++) if (strcmp(s->outposts[i].id, id) == 0) return &s->outposts[i];
    if (s->outpost_count >= SAVE_MAX_OUTPOSTS) return NULL;
    SaveOutpost *o = &s->outposts[s->outpost_count++];
    memset(o, 0, sizeof(*o));
    dh_strcpy_safe(o->id, sizeof(o->id), id);
    return o;
}
SaveCollect *save_collect(SaveGame *s, const char *id) {
    if (!s || !id) return NULL;
    for (int i = 0; i < s->collect_count; i++) if (strcmp(s->collect[i].id, id) == 0) return &s->collect[i];
    if (s->collect_count >= SAVE_MAX_COLLECT) return NULL;
    SaveCollect *c = &s->collect[s->collect_count++];
    memset(c, 0, sizeof(*c));
    dh_strcpy_safe(c->id, sizeof(c->id), id);
    return c;
}
void save_add_stat_ll(long long *field, long long delta) { if (field) *field += delta; }
void save_playtime_add(float dt) { if (dt > 0 && dt < 5.0f) g_active.playtime_sec += dt; }

/* ── new game ──────────────────────────────────────────────────────────── */
void save_new_game(SaveGame *s, int difficulty, int map) {
    if (!s) s = &g_active;
    memset(s, 0, sizeof(*s));
    s->slot = -1;
    s->difficulty = dh_clampi(difficulty, 0, 3);
    Settings *st = settings();
    s->mutators[0] = st->mutators[0]; /* copy all 8 */
    for (int i = 0; i < 8 && i < MUT_COUNT; i++) s->mutators[i] = st->mutators[i];
    s->ng_plus = 0;
    s->ng_plus_allowed = settings_mutator_count() == 0;

    s->map = dh_clampi(map, 0, 1);
    s->time_of_day = 0.30f;                 /* morning — Spec 6.7 */
    s->day_count = 1.0f;
    s->weather = 0; s->weather_intensity = 0.0f;
    s->island_heat = 0.0f; s->city_heat = 0.0f;
    s->wanted_level = 0; s->wanted_decay_timer = 0.0f;

    s->state = PS_ALIVE;
    s->health = 100.0f; s->health_max = 100.0f;
    s->armor = 0.0f;    s->armor_max = 100.0f;
    s->stamina = 100.0f;s->stamina_max = 100.0f;
    s->breath = 1.0f;
    s->money = 250;                         /* Spec 20: enough to start, never trivial */
    s->level = 1; s->xp = 0; s->skill_points = 1;
    s->stealth_rank = 0;
    s->in_vehicle = 0;
    s->companions_active = 0;
    s->equipped_slot = 0; s->weapon_count = 1;
    s->weapon_ids[0] = 1;                   /* machete / starter */
    s->weapon_ammo[0] = 0; s->weapon_reserve[0] = 0; s->weapon_unlocked[0] = 1;

    /* Accessibility snapshot — a save must replay the player's comfort choices. */
    s->photosensitivity = st->photosensitivity;
    s->subtitles = st->subtitles;
    s->colorblind_mode = st->colorblind_mode;
    s->language = st->language;
    s->difficulty_labels_shown = 1;

    /* Spec 10.5: unique playthrough id, persists across loads */
    char pid[PLAYTHROUGH_ID_LEN];
    double e = 0; make_timestamp(pid, sizeof(pid), &e);
    static Rng g_pid_rng; static int g_pid_seeded = 0;
    if (!g_pid_seeded) { rng_seed(&g_pid_rng, (uint32_t)(e * 1000.0)); g_pid_seeded = 1; }
    snprintf(s->playthrough_id, sizeof(s->playthrough_id), "PT-%08X-%04X",
             (unsigned)(e * 1000.0) & 0xFFFFFFFFu, (unsigned)(rng_f(&g_pid_rng) * 65535.0f));
    make_timestamp(s->timestamp, sizeof(s->timestamp), &s->real_timestamp);
    dh_strcpy_safe(s->name, sizeof(s->name), "New Game");
    s->version_major = DH_VERSION_MAJOR; s->version_minor = DH_VERSION_MINOR; s->version_patch = DH_VERSION_PATCH;
    DH_INFO("save", "new game: difficulty=%d map=%s id=%s", difficulty,
            map == 0 ? "Isla Sombra" : "Meridian City", s->playthrough_id);
}

/* ── serialize ─────────────────────────────────────────────────────────── */
#define SN(k,v) jw_num(&w,(k),(double)(v))
#define SI(k,v) jw_int(&w,(k),(long long)(v))
#define SB(k,v) jw_bool(&w,(k),(v))
#define SS(k,v) jw_str(&w,(k),(v))

char *save_serialize(const SaveGame *s) {
    if (!s) return NULL;
    JsonWriter w; jw_init(&w);
    if (w.err) return NULL;
    jw_obj_begin(&w, NULL);
    SI("schema", 1);
    SI("vmaj", s->version_major); SI("vmin", s->version_minor); SI("vpat", s->version_patch);
    SS("name", s->name);
    SS("playthrough_id", s->playthrough_id);
    SS("timestamp", s->timestamp);
    SN("real_timestamp", s->real_timestamp);
    SN("playtime", s->playtime_sec);
    SI("difficulty", s->difficulty);
    SI("ng_plus", s->ng_plus);
    SB("ng_plus_allowed", s->ng_plus_allowed);
    jw_arr_begin(&w, "mutators");
      for (int i = 0; i < 8; i++) jw_int(&w, NULL, s->mutators[i]);
    jw_arr_end(&w);

    jw_obj_begin(&w, "world");
      SI("map", s->map); SN("time_of_day", s->time_of_day); SN("day_count", s->day_count);
      SI("weather", s->weather); SN("weather_intensity", s->weather_intensity);
      SN("island_heat", s->island_heat); SN("city_heat", s->city_heat);
      SI("wanted_level", s->wanted_level); SN("wanted_decay", s->wanted_decay_timer);
      jw_arr_begin(&w, "districts");
        for (int i = 0; i < 16; i++) jw_int(&w, NULL, s->district_flags[i]);
      jw_arr_end(&w);
    jw_obj_end(&w);

    jw_obj_begin(&w, "player");
      jw_vec3(&w, "pos", s->pos); jw_vec3(&w, "vel", s->vel);
      SN("yaw", s->yaw); SN("pitch", s->pitch); SN("roll", s->roll);
      SI("state", s->state);
      SN("health", s->health); SN("health_max", s->health_max);
      SN("armor", s->armor); SN("armor_max", s->armor_max);
      SN("stamina", s->stamina); SN("stamina_max", s->stamina_max);
      SN("breath", s->breath);
      SI("money", s->money); SI("level", s->level); SI("xp", s->xp);
      SI("skill_points", s->skill_points); SI("skills", (int)s->skills_unlocked);
      SI("stealth_rank", s->stealth_rank);
      SB("in_vehicle", s->in_vehicle); SS("vehicle_id", s->vehicle_id);
      jw_vec3(&w, "checkpoint", s->checkpoint);
      SI("companions_active", s->companions_active);
      jw_arr_begin(&w, "companion_ids");
        for (int i = 0; i < SAVE_MAX_COMPANIONS; i++) jw_int(&w, NULL, s->companion_ids[i]);
      jw_arr_end(&w);
      jw_arr_begin(&w, "companion_trust");
        for (int i = 0; i < SAVE_MAX_COMPANIONS; i++) jw_num(&w, NULL, s->companion_trust[i]);
      jw_arr_end(&w);
    jw_obj_end(&w);

    jw_obj_begin(&w, "inventory");
      jw_arr_begin(&w, "weapons");
        for (int i = 0; i < s->weapon_count && i < SAVE_MAX_WEAPONS; i++) {
          jw_obj_begin(&w, NULL);
            SI("id", s->weapon_ids[i]); SI("ammo", s->weapon_ammo[i]);
            SI("reserve", s->weapon_reserve[i]); SB("unlocked", s->weapon_unlocked[i]);
          jw_obj_end(&w);
        }
      jw_arr_end(&w);
      SI("weapon_count", s->weapon_count);
      SI("equipped", s->equipped_slot);
      jw_arr_begin(&w, "items");
        for (int i = 0; i < s->inv_count && i < SAVE_MAX_INVENTORY; i++) {
          jw_obj_begin(&w, NULL);
            SS("id", s->inv_ids[i]); SI("count", s->inv_counts[i]);
          jw_obj_end(&w);
        }
      jw_arr_end(&w);
      jw_arr_begin(&w, "grenades");
        for (int i = 0; i < 4; i++) jw_int(&w, NULL, s->grenades[i]);
      jw_arr_end(&w);
    jw_obj_end(&w);

    jw_arr_begin(&w, "missions");
      for (int i = 0; i < s->mission_count; i++) {
        const SaveMission *m = &s->missions[i];
        jw_obj_begin(&w, NULL);
          SS("id", m->id); SI("status", m->status); SI("phase", m->phase);
          SI("rank", m->rank); SN("best", m->time_best); SI("attempts", m->attempts);
          SB("stealth", m->stealth_complete);
        jw_obj_end(&w);
      }
    jw_arr_end(&w);

    jw_arr_begin(&w, "outposts");
      for (int i = 0; i < s->outpost_count; i++) {
        const SaveOutpost *o = &s->outposts[i];
        jw_obj_begin(&w, NULL);
          SS("id", o->id); SI("captured", o->captured); SI("alarm", o->alarm_level);
          SB("stealth", o->cleared_stealth); SN("time", o->capture_time);
          SB("cmd_alive", o->commander_alive);
        jw_obj_end(&w);
      }
    jw_arr_end(&w);

    jw_arr_begin(&w, "flags");
      for (int i = 0; i < s->flag_count; i++) {
        jw_obj_begin(&w, NULL);
          SS("id", s->flags[i].id); SI("v", s->flags[i].value);
        jw_obj_end(&w);
      }
    jw_arr_end(&w);

    jw_arr_begin(&w, "collect");
      for (int i = 0; i < s->collect_count; i++) {
        jw_obj_begin(&w, NULL);
          SS("id", s->collect[i].id); SI("count", s->collect[i].count);
          SI("total", s->collect[i].total);
        jw_obj_end(&w);
      }
    jw_arr_end(&w);

    jw_arr_begin(&w, "vehicles");
      for (int i = 0; i < s->vehicle_count && i < SAVE_MAX_VEHICLES; i++)
        jw_str(&w, NULL, s->vehicles[i]);
    jw_arr_end(&w);

    jw_obj_begin(&w, "stats");
      SI("enemies_defeated", s->stats.enemies_defeated);
      SI("stealth_takedowns", s->stats.stealth_takedowns);
      SI("headshots", s->stats.headshots);
      SI("melee_kills", s->stats.melee_kills);
      SI("distance_foot_m", s->stats.distance_foot_m);
      SI("distance_vehicle_m", s->stats.distance_vehicle_m);
      SI("distance_air_m", s->stats.distance_air_m);
      SI("distance_sea_m", s->stats.distance_sea_m);
      SI("photos_taken", s->stats.photos_taken);
      SI("relics_found", s->stats.relics_found);
      SI("tags_done", s->stats.tags_done);
      SI("letters_read", s->stats.letters_read);
      SI("vehicles_stolen", s->stats.vehicles_stolen);
      SI("vehicles_destroyed", s->stats.vehicles_destroyed);
      SI("jumps_landed", s->stats.jumps_landed);
      SI("poncho_pets", s->stats.poncho_pets);
      SI("poncho_saves", s->stats.poncho_saves);
      SI("dog_fetches", s->stats.dog_fetches);
      SI("missions_complete", s->stats.missions_complete);
      SI("side_complete", s->stats.side_complete);
      SI("outposts_captured", s->stats.outposts_captured);
      SI("deaths", s->stats.deaths);
      SI("arrests", s->stats.arrests);
      SI("saves_loaded", s->stats.saves_loaded);
      SI("longest_stealth_chain", s->stats.longest_stealth_chain);
      SI("highest_wanted", s->stats.highest_wanted);
      SN("biggest_jump_m", s->stats.biggest_jump_m);
      SI("money_earned", s->stats.money_earned);
      SI("money_spent", s->stats.money_spent);
      SN("best_drift_m", s->stats.best_drift_m);
      SN("longest_glide_s", s->stats.longest_glide_s);
    jw_obj_end(&w);

    jw_obj_begin(&w, "comfort");
      SB("photosensitivity", s->photosensitivity);
      SB("subtitles", s->subtitles);
      SI("colorblind_mode", s->colorblind_mode);
      SI("language", s->language);
      SB("difficulty_labels", s->difficulty_labels_shown);
    jw_obj_end(&w);

    jw_obj_end(&w);
    return jw_take(&w);
}
#undef SN
#undef SI
#undef SB
#undef SS

/* ── deserialize ───────────────────────────────────────────────────────── */
#define GF(o,k,f) do { if (o) (f) = json_get_flt((o),(k),(f)); } while (0)
#define GI(o,k,f) do { if (o) (f) = json_get_int((o),(k),(f)); } while (0)
#define GB(o,k,f) do { if (o) (f) = json_get_bool((o),(k),(f)); } while (0)
#define GS(o,k,f,sz) do { if (o) dh_strcpy_safe((f),(sz),json_get_str((o),(k),(f))); } while (0)
static Vec3 get_vec3(JsonValue *o, const char *k) {
    Vec3 v = v3(0,0,0);
    JsonValue *a = o ? json_obj_get(o, k) : NULL;
    if (a && json_arr_len(a) >= 3) {
        v.x = (float)json_arr_get(a,0)->num;
        v.y = (float)json_arr_get(a,1)->num;
        v.z = (float)json_arr_get(a,2)->num;
    }
    return v;
}

int save_deserialize(SaveGame *out, const char *json_text) {
    if (!out || !json_text) return 0;
    const char *err = NULL;
    JsonValue *root = json_parse(json_text, &err);
    if (!root) { DH_ERROR("save", "parse failed: %s", err ? err : "?"); return 0; }

    int schema = json_get_int(root, "schema", 1);
    if (schema != 1) {
        DH_WARN("save", "save schema %d, engine expects 1 — attempting forward-compatible load", schema);
    }
    SaveGame tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.version_major = json_get_int(root,"vmaj",1);
    tmp.version_minor = json_get_int(root,"vmin",0);
    tmp.version_patch = json_get_int(root,"vpat",0);
    GS(root,"name",tmp.name,sizeof(tmp.name));
    GS(root,"playthrough_id",tmp.playthrough_id,sizeof(tmp.playthrough_id));
    GS(root,"timestamp",tmp.timestamp,sizeof(tmp.timestamp));
    GF(root,"real_timestamp",tmp.real_timestamp);
    GF(root,"playtime",tmp.playtime_sec);
    GI(root,"difficulty",tmp.difficulty);
    GI(root,"ng_plus",tmp.ng_plus);
    GB(root,"ng_plus_allowed",tmp.ng_plus_allowed);
    JsonValue *mu = json_obj_get(root,"mutators");
    if (mu) for (int i = 0; i < 8 && i < json_arr_len(mu); i++) {
        JsonValue *e = json_arr_get(mu,i); if (e) tmp.mutators[i] = (int)e->num;
    }

    JsonValue *wo = json_obj_get(root,"world");
    if (wo) {
        GI(wo,"map",tmp.map); GF(wo,"time_of_day",tmp.time_of_day); GF(wo,"day_count",tmp.day_count);
        GI(wo,"weather",tmp.weather); GF(wo,"weather_intensity",tmp.weather_intensity);
        GF(wo,"island_heat",tmp.island_heat); GF(wo,"city_heat",tmp.city_heat);
        GI(wo,"wanted_level",tmp.wanted_level); GF(wo,"wanted_decay",tmp.wanted_decay_timer);
        JsonValue *dd = json_obj_get(wo,"districts");
        if (dd) for (int i = 0; i < 16 && i < json_arr_len(dd); i++) {
            JsonValue *e = json_arr_get(dd,i); if (e) tmp.district_flags[i] = (int)e->num;
        }
    }
    JsonValue *pl = json_obj_get(root,"player");
    if (pl) {
        tmp.pos = get_vec3(pl,"pos"); tmp.vel = get_vec3(pl,"vel");
        GF(pl,"yaw",tmp.yaw); GF(pl,"pitch",tmp.pitch); GF(pl,"roll",tmp.roll);
        GI(pl,"state",tmp.state);
        GF(pl,"health",tmp.health); GF(pl,"health_max",tmp.health_max);
        GF(pl,"armor",tmp.armor); GF(pl,"armor_max",tmp.armor_max);
        GF(pl,"stamina",tmp.stamina); GF(pl,"stamina_max",tmp.stamina_max);
        GF(pl,"breath",tmp.breath);
        GI(pl,"money",tmp.money); GI(pl,"level",tmp.level); GI(pl,"xp",tmp.xp);
        GI(pl,"skill_points",tmp.skill_points);
        tmp.skills_unlocked = (unsigned)json_get_int(pl,"skills",0);
        GI(pl,"stealth_rank",tmp.stealth_rank);
        GB(pl,"in_vehicle",tmp.in_vehicle); GS(pl,"vehicle_id",tmp.vehicle_id,sizeof(tmp.vehicle_id));
        tmp.checkpoint = get_vec3(pl,"checkpoint");
        GI(pl,"companions_active",tmp.companions_active);
        JsonValue *ci = json_obj_get(pl,"companion_ids");
        if (ci) for (int i = 0; i < SAVE_MAX_COMPANIONS && i < json_arr_len(ci); i++) {
            JsonValue *e = json_arr_get(ci,i); if (e) tmp.companion_ids[i] = (int)e->num;
        }
        JsonValue *ct = json_obj_get(pl,"companion_trust");
        if (ct) for (int i = 0; i < SAVE_MAX_COMPANIONS && i < json_arr_len(ct); i++) {
            JsonValue *e = json_arr_get(ct,i); if (e) tmp.companion_trust[i] = (float)e->num;
        }
    }
    JsonValue *inv = json_obj_get(root,"inventory");
    if (inv) {
        GI(inv,"weapon_count",tmp.weapon_count);
        GI(inv,"equipped",tmp.equipped_slot);
        tmp.weapon_count = dh_clampi(tmp.weapon_count, 0, SAVE_MAX_WEAPONS);
        JsonValue *ws = json_obj_get(inv,"weapons");
        if (ws) for (int i = 0; i < tmp.weapon_count && i < json_arr_len(ws); i++) {
            JsonValue *e = json_arr_get(ws,i);
            if (!e) continue;
            tmp.weapon_ids[i] = json_get_int(e,"id",0);
            tmp.weapon_ammo[i] = json_get_int(e,"ammo",0);
            tmp.weapon_reserve[i] = json_get_int(e,"reserve",0);
            tmp.weapon_unlocked[i] = json_get_bool(e,"unlocked",1);
        }
        JsonValue *it = json_obj_get(inv,"items");
        tmp.inv_count = 0;
        if (it) for (int i = 0; i < SAVE_MAX_INVENTORY && i < json_arr_len(it); i++) {
            JsonValue *e = json_arr_get(it,i);
            if (!e) continue;
            const char *id = json_get_str(e,"id",NULL);
            if (!id) continue;
            dh_strcpy_safe(tmp.inv_ids[tmp.inv_count], 24, id);
            tmp.inv_counts[tmp.inv_count] = json_get_int(e,"count",0);
            tmp.inv_count++;
        }
        JsonValue *gr = json_obj_get(inv,"grenades");
        if (gr) for (int i = 0; i < 4 && i < json_arr_len(gr); i++) {
            JsonValue *e = json_arr_get(gr,i); if (e) tmp.grenades[i] = (int)e->num;
        }
    }
    JsonValue *ms = json_obj_get(root,"missions");
    if (ms) for (int i = 0; i < SAVE_MAX_MISSIONS && i < json_arr_len(ms); i++) {
        JsonValue *e = json_arr_get(ms,i);
        if (!e) continue;
        const char *id = json_get_str(e,"id",NULL);
        if (!id) continue;
        SaveMission *m = &tmp.missions[tmp.mission_count++];
        memset(m,0,sizeof(*m));
        dh_strcpy_safe(m->id,sizeof(m->id),id);
        m->status = json_get_int(e,"status",0);
        m->phase = json_get_int(e,"phase",0);
        m->rank = json_get_int(e,"rank",0);
        m->time_best = json_get_flt(e,"best",0);
        m->attempts = json_get_int(e,"attempts",0);
        m->stealth_complete = json_get_bool(e,"stealth",0);
    }
    JsonValue *op = json_obj_get(root,"outposts");
    if (op) for (int i = 0; i < SAVE_MAX_OUTPOSTS && i < json_arr_len(op); i++) {
        JsonValue *e = json_arr_get(op,i);
        if (!e) continue;
        const char *id = json_get_str(e,"id",NULL);
        if (!id) continue;
        SaveOutpost *o = &tmp.outposts[tmp.outpost_count++];
        memset(o,0,sizeof(*o));
        dh_strcpy_safe(o->id,sizeof(o->id),id);
        o->captured = json_get_int(e,"captured",0);
        o->alarm_level = json_get_int(e,"alarm",0);
        o->cleared_stealth = json_get_bool(e,"stealth",0);
        o->capture_time = json_get_flt(e,"time",0);
        o->commander_alive = json_get_bool(e,"cmd_alive",1);
    }
    JsonValue *fl = json_obj_get(root,"flags");
    if (fl) for (int i = 0; i < SAVE_MAX_FLAGS && i < json_arr_len(fl); i++) {
        JsonValue *e = json_arr_get(fl,i);
        if (!e) continue;
        const char *id = json_get_str(e,"id",NULL);
        if (!id) continue;
        SaveFlag *f = &tmp.flags[tmp.flag_count++];
        dh_strcpy_safe(f->id,sizeof(f->id),id);
        f->value = json_get_int(e,"v",0);
    }
    JsonValue *cl = json_obj_get(root,"collect");
    if (cl) for (int i = 0; i < SAVE_MAX_COLLECT && i < json_arr_len(cl); i++) {
        JsonValue *e = json_arr_get(cl,i);
        if (!e) continue;
        const char *id = json_get_str(e,"id",NULL);
        if (!id) continue;
        SaveCollect *c = &tmp.collect[tmp.collect_count++];
        memset(c,0,sizeof(*c));
        dh_strcpy_safe(c->id,sizeof(c->id),id);
        c->count = json_get_int(e,"count",0);
        c->total = json_get_int(e,"total",0);
    }
    JsonValue *vh = json_obj_get(root,"vehicles");
    if (vh) for (int i = 0; i < SAVE_MAX_VEHICLES && i < json_arr_len(vh); i++) {
        JsonValue *e = json_arr_get(vh,i);
        if (e && e->str) dh_strcpy_safe(tmp.vehicles[tmp.vehicle_count++],24,e->str);
    }
    JsonValue *st = json_obj_get(root,"stats");
    if (st) {
        GI(st,"enemies_defeated",tmp.stats.enemies_defeated);
        GI(st,"stealth_takedowns",tmp.stats.stealth_takedowns);
        GI(st,"headshots",tmp.stats.headshots);
        GI(st,"melee_kills",tmp.stats.melee_kills);
        GI(st,"distance_foot_m",tmp.stats.distance_foot_m);
        GI(st,"distance_vehicle_m",tmp.stats.distance_vehicle_m);
        GI(st,"distance_air_m",tmp.stats.distance_air_m);
        GI(st,"distance_sea_m",tmp.stats.distance_sea_m);
        GI(st,"photos_taken",tmp.stats.photos_taken);
        GI(st,"relics_found",tmp.stats.relics_found);
        GI(st,"tags_done",tmp.stats.tags_done);
        GI(st,"letters_read",tmp.stats.letters_read);
        GI(st,"vehicles_stolen",tmp.stats.vehicles_stolen);
        GI(st,"vehicles_destroyed",tmp.stats.vehicles_destroyed);
        GI(st,"jumps_landed",tmp.stats.jumps_landed);
        GI(st,"poncho_pets",tmp.stats.poncho_pets);
        GI(st,"poncho_saves",tmp.stats.poncho_saves);
        GI(st,"dog_fetches",tmp.stats.dog_fetches);
        GI(st,"missions_complete",tmp.stats.missions_complete);
        GI(st,"side_complete",tmp.stats.side_complete);
        GI(st,"outposts_captured",tmp.stats.outposts_captured);
        GI(st,"deaths",tmp.stats.deaths);
        GI(st,"arrests",tmp.stats.arrests);
        GI(st,"saves_loaded",tmp.stats.saves_loaded);
        GI(st,"longest_stealth_chain",tmp.stats.longest_stealth_chain);
        GI(st,"highest_wanted",tmp.stats.highest_wanted);
        GF(st,"biggest_jump_m",tmp.stats.biggest_jump_m);
        GI(st,"money_earned",tmp.stats.money_earned);
        GI(st,"money_spent",tmp.stats.money_spent);
        GF(st,"best_drift_m",tmp.stats.best_drift_m);
        GF(st,"longest_glide_s",tmp.stats.longest_glide_s);
    }
    JsonValue *cf = json_obj_get(root,"comfort");
    if (cf) {
        GB(cf,"photosensitivity",tmp.photosensitivity);
        GB(cf,"subtitles",tmp.subtitles);
        GI(cf,"colorblind_mode",tmp.colorblind_mode);
        GI(cf,"language",tmp.language);
        GB(cf,"difficulty_labels",tmp.difficulty_labels_shown);
    }

    /* Sanity: reject obviously invalid saves rather than crash later. */
    if (!isfinite(tmp.pos.x) || !isfinite(tmp.pos.y) || !isfinite(tmp.pos.z)) {
        DH_ERROR("save", "non-finite player position in save — rejected");
        json_free(root); return 0;
    }
    if (tmp.health < 0 || tmp.health > 10000 || tmp.money < 0 || tmp.money > 1000000000) {
        DH_ERROR("save", "out-of-range values in save — rejected");
        json_free(root); return 0;
    }
    json_free(root);
    *out = tmp;
    return 1;
}
#undef GF
#undef GI
#undef GB
#undef GS

/* ── slot IO ───────────────────────────────────────────────────────────── */
int save_write_slot(int slot, int is_auto, const SaveGame *s) {
    if (!s) s = &g_active;
    int abs_slot = save_slot_index(slot, is_auto);
    char path[1024]; slot_path(path, sizeof(path), abs_slot);
    SaveGame copy = *s;
    copy.slot = abs_slot;
    copy.is_auto = is_auto;
    make_timestamp(copy.timestamp, sizeof(copy.timestamp), &copy.real_timestamp);
    if (!copy.name[0]) dh_strcpy_safe(copy.name, sizeof(copy.name), is_auto ? "Autosave" : "Manual Save");
    char *txt = save_serialize(&copy);
    if (!txt) { DH_ERROR("save","serialize failed"); return 0; }
    int ok = dh_fs_write_text(path, txt);
    free(txt);
    if (ok) {
        g_active = copy;
        DH_INFO("save", "wrote slot %d (%s) — %.1f KB", abs_slot, is_auto?"auto":"manual", 0.0f);
        dh_log_action("Saved game (%s slot %d)", is_auto ? "auto" : "manual", abs_slot);
    } else {
        DH_ERROR("save", "FAILED to write %s", path);
    }
    return ok;
}

int save_read_slot(int slot, int is_auto, SaveGame *out) {
    int abs_slot = save_slot_index(slot, is_auto);
    char path[1024]; slot_path(path, sizeof(path), abs_slot);
    size_t len = 0;
    char *txt = dh_fs_read_text(path, &len);
    if (!txt) { DH_WARN("save","slot %d missing", abs_slot); return 0; }
    int ok = save_deserialize(out ? out : &g_active, txt);
    free(txt);
    if (!ok) {
        /* Spec 37.4: never lose a save to corruption — quarantine the file. */
        char bad[1100]; snprintf(bad, sizeof(bad), "%s.corrupt", path);
        rename(path, bad);
        DH_ERROR("save", "slot %d corrupt — quarantined to %s", abs_slot, bad);
        return 0;
    }
    if (out == NULL || out == &g_active) {
        g_active.slot = abs_slot;
        g_active.is_auto = is_auto;
        g_active.stats.saves_loaded++;
    }
    DH_INFO("save", "loaded slot %d (%s, %.1f min)", abs_slot, out ? "preview" : "active",
            (out?out:&g_active)->playtime_sec / 60.0f);
    return 1;
}
int save_delete_slot(int slot, int is_auto) {
    int abs_slot = save_slot_index(slot, is_auto);
    char path[1024]; slot_path(path, sizeof(path), abs_slot);
    int r = remove(path);
    DH_INFO("save", "deleted slot %d (%s)", abs_slot, r == 0 ? "ok" : "not found");
    return r == 0;
}
int save_slot_exists(int abs_slot) {
    char path[1024]; slot_path(path, sizeof(path), abs_slot);
    return dh_fs_exists(path);
}

int save_list_headers(SaveHeader *out, int max) {
    int n = 0;
    for (int abs_slot = 0; abs_slot < SAVE_TOTAL_SLOTS && n < max; abs_slot++) {
        SaveHeader *h = &out[n++];
        memset(h, 0, sizeof(*h));
        h->abs_slot = abs_slot;
        h->is_auto = abs_slot < SAVE_AUTO_SLOTS;
        char path[1024]; slot_path(path, sizeof(path), abs_slot);
        h->exists = dh_fs_exists(path);
        if (!h->exists) continue;
        size_t len = 0;
        char *txt = dh_fs_read_text(path, &len);
        if (!txt) { h->corrupt = 1; continue; }
        /* Header-only parse: full body parse of 13 slots would stall the menu. */
        const char *err = NULL;
        JsonValue *root = json_parse(txt, &err);
        free(txt);
        if (!root) { h->corrupt = 1; continue; }
        dh_strcpy_safe(h->name, sizeof(h->name), json_get_str(root,"name", h->is_auto ? "Autosave" : "Save"));
        dh_strcpy_safe(h->timestamp, sizeof(h->timestamp), json_get_str(root,"timestamp","?"));
        h->playtime_sec = json_get_flt(root,"playtime",0);
        h->difficulty = json_get_int(root,"difficulty",1);
        h->ng_plus = json_get_int(root,"ng_plus",0);
        JsonValue *mu = json_obj_get(root,"mutators");
        if (mu) for (int i = 0; i < json_arr_len(mu); i++) {
            JsonValue *e = json_arr_get(mu,i);
            if (e && e->num != 0.0) h->mutators_on++;
        }
        JsonValue *wo = json_obj_get(root,"world");
        int m = wo ? json_get_int(wo,"map",0) : 0;
        dh_strcpy_safe(h->map_name, sizeof(h->map_name), m == 0 ? "Isla Sombra" : "Meridian City");
        json_free(root);
    }
    return n;
}

int save_is_ng_plus_blocked(void) {
    return settings_mutator_count() > 0;
}

void save_autosave(const char *reason) {
    static int rr = 0;
    g_active.is_auto = 1;
    if (!g_active.name[0] || strncmp(g_active.name, "Autosave", 8) == 0) {
        snprintf(g_active.name, sizeof(g_active.name), "Autosave — %s", reason ? reason : "checkpoint");
    }
    rr = (rr + 1) % SAVE_AUTO_SLOTS;        /* rotate so a bad autosave can't erase all 3 */
    save_write_slot(rr, 1, &g_active);
}
void save_autosave_tick(float dt) {
    g_autosave_timer += dt;
    if (g_autosave_timer < AUTOSAVE_INTERVAL) return;
    g_autosave_timer = 0.0f;
    /* Never autosave mid-combat (Spec 6.11) — the caller gates on wanted/threat. */
    if (g_active.wanted_level > 0 || g_active.state == PS_BLEEDOUT) return;
    save_autosave("timed checkpoint");
}
