/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M5 Systems & Progression (game-side glue)
   Textually included by game.c (needs its static prop/pickup/fog helpers).
   The rules themselves live in meta/progress.c; this file wires them into the
   world: safehouses, vendors, hunting, healing, fast travel, saves and the
   island state that must survive Beto's ferry.
   ══════════════════════════════════════════════════════════════════════════ */

static const float AMMO_BASE_CAP[AMMO_N] = { 300.f, 600.f, 80.f, 60.f, 30.f };
#define ISLAND_OUTPOST_ID "punta_quemada"

int game_ammo_cap(const Game *g, int a) {
    if (a < 0 || a >= AMMO_N) return 0;
    return (int)(AMMO_BASE_CAP[a] * prog_mod(&g->prog, MOD_RESERVE) + 0.5f);
}

void game_apply_skills(Game *g) {
    Player *p = &g->player;
    float hpmax = 100.f * prog_mod(&g->prog, MOD_MAX_HP);
    if (p->health_max < 1.f) p->health_max = 100.f;
    if (hpmax > p->health_max && p->health > 0.f) p->health += hpmax - p->health_max;
    p->health_max = hpmax;
    if (p->health > hpmax) p->health = hpmax;
    float bm = PL_BREATH_BASE * prog_mod(&g->prog, MOD_BREATH);
    if (p->breath_max > 0.f) p->breath *= bm / p->breath_max;
    p->breath_max = bm;
    p->stamina_mul = prog_mod(&g->prog, MOD_STAMINA);
}

int game_map_reveal_pct(const Game *g) {
    int n = 0;
    for (int i = 0; i < MAPW * MAPW; i++) if (g->fog[i]) n++;
    return n * 100 / (MAPW * MAPW);
}

static float ground_y(Game *g, float x, float z) { return terrain_height(&g->terrain, x, z); }

/* ── world dressing: safehouse hut, vendor stall, herb patches ── */
static void sys_build_vendor_stall(Game *g, Vec3 p, uint32_t awning) {
    float y = ground_y(g, p.x, p.z);
    prop_box(g, v3(p.x, y + 0.5f, p.z), v3(1.4f, 0.5f, 0.5f), proc_tex.wood, 0xFFB8C8D8u, 0);
    prop_visual(g, v3(p.x, y + 2.3f, p.z - 0.2f), v3(1.7f, 0.08f, 1.0f), proc_tex.fabric, awning, 1, 0);
    prop_visual(g, v3(p.x - 1.5f, y + 1.15f, p.z - 0.2f), v3(0.06f, 1.15f, 0.06f), proc_tex.wood, 0xFFFFFFFFu, 0, 0);
    prop_visual(g, v3(p.x + 1.5f, y + 1.15f, p.z - 0.2f), v3(0.06f, 1.15f, 0.06f), proc_tex.wood, 0xFFFFFFFFu, 0, 0);
}

static void sys_build_safehouse(Game *g, Vec3 door, uint32_t wall, uint32_t roof) {
    /* door faces +x; the hut body sits behind it (-x) */
    float y = ground_y(g, door.x, door.z);
    Vec3 c = v3(door.x - 3.2f, y, door.z);
    prop_box(g, v3(c.x, y + 1.4f, c.z - 2.4f), v3(2.6f, 1.4f, 0.15f), proc_tex.wood, wall, 0);
    prop_box(g, v3(c.x, y + 1.4f, c.z + 2.4f), v3(2.6f, 1.4f, 0.15f), proc_tex.wood, wall, 0);
    prop_box(g, v3(c.x - 2.5f, y + 1.4f, c.z), v3(0.15f, 1.4f, 2.4f), proc_tex.wood, wall, 0);
    prop_visual(g, v3(c.x, y + 3.0f, c.z), v3(3.0f, 0.18f, 2.8f), proc_tex.fabric, roof, 1, 0);
    prop_visual(g, v3(c.x - 1.4f, y + 0.25f, c.z), v3(0.9f, 0.25f, 0.5f), proc_tex.fabric, 0xFF6C78C8u, 0, 0); /* cot */
}

static void sys_build_island_extras(Game *g) {
    Vec3 sh = v3(144.f, 0.f, 626.f);
    sh.y = ground_y(g, sh.x, sh.z);
    g->safehouse[0] = sh;
    sys_build_safehouse(g, sh, 0xFF9EC0DCu, 0xFF3C8CC8u);
    for (int i = 0; i < g->econ.vendor_n; i++)
        if (g->econ.vendors[i].act == 0) {
            Vendor *v = &g->econ.vendors[i];
            v->pos.y = ground_y(g, v->pos.x, v->pos.z);
            sys_build_vendor_stall(g, v->pos, 0xFF5078E8u);
        }
    /* herb patches: deterministic local RNG so M2/M3 combat rolls don't shift */
    Rng hr; rng_seed(&hr, g->seed ^ 0x4E5B1u);
    int placed = 0;
    for (int t = 0; t < 400 && placed < 16; t++) {
        float a = rng_f(&hr) * 6.2831853f, r = 25.f + rng_f(&hr) * 190.f;
        float x = 230.f + sinf(a) * r, z = 560.f + cosf(a) * r;
        float h = ground_y(g, x, z);
        if (h < 2.6f || h > 30.f || terrain_steepness(&g->terrain, x, z) > 0.4f) continue;
        if (veg_excluded(g, x, z)) continue;
        for (int i = 0; i < PICKUP_MAX; i++) {
            Pickup *k = &g->pickups.v[i];
            if (k->live) continue;
            k->pos = v3(x, h, z); k->kind = PK_HERB; k->ammo = 0; k->amount = 1.f;
            k->live = 1; k->bob = (float)placed;
            if (i >= g->pickups.count) g->pickups.count = i + 1;
            placed++;
            break;
        }
    }
    g->prog.ft[FT_ISLAND_SAFEHOUSE].pos = sh;
    if (g->outpost.built) {
        g->prog.ft[FT_OUTPOST].pos = g->outpost.flag_pos;
        g->prog.ft[FT_MAST].pos = g->outpost.mast_pos;
    }
}

static void sys_build_city_extras(Game *g) {
    Vec3 sh = v3(486.f, 0.f, 640.f);
    sh.y = ground_y(g, sh.x, sh.z);
    g->safehouse[1] = sh;
    sys_build_safehouse(g, sh, 0xFF8A96A6u, 0xFF4A5AA8u);
    for (int i = 0; i < g->econ.vendor_n; i++)
        if (g->econ.vendors[i].act == 1) {
            Vendor *v = &g->econ.vendors[i];
            v->pos.y = ground_y(g, v->pos.x, v->pos.z);
            sys_build_vendor_stall(g, v->pos, 0xFF40B0E0u);
        }
    g->prog.ft[FT_CITY_SAFEHOUSE].pos = sh;
    g->prog.ft[FT_JOB_BOARD].pos = g->city.job_board;
}

static void story_save(Game *g, SaveGame *s);          /* story.inl */
static void poncho_save(Game *g, SaveGame *s);         /* poncho.inl */
static void poncho_load(Game *g, const SaveGame *s);
static void legend_save(Game *g, SaveGame *s);          /* herald.inl */
static void legend_load(Game *g, const SaveGame *s);
static void story_load(Game *g, const SaveGame *s);
/* ── island persistence across the ferry (and into saves) ── */
void game_island_store(Game *g) {
    if (g->act != 0) return;
    Progress *pr = &g->prog;
    pr->outpost_captured = g->outpost.captured;
    pr->mast_synced = g->outpost.mast_synced;
    pr->alarm_destroyed = g->outpost.alarm_destroyed;
    prog_fog_store(pr, g->fog, MAPW * MAPW);
    pr->island_saved = 1;
}

static void sys_island_restore(Game *g) {
    Progress *pr = &g->prog;
    if (!pr->island_saved) return;
    Outpost *o = &g->outpost;
    if (pr->outpost_captured && o->built) {
        /* liberated stays liberated: garrison gone, flag up (§37.3) */
        for (int i = 0; i < g->enemies.count; i++)
            if (g->enemies.v[i].faction == 2) g->enemies.v[i].state = EN_DEAD;
        g->enemies.alive_count = 0;
        for (int i = 0; i < g->enemies.count; i++)
            if (g->enemies.v[i].state != EN_DEAD) g->enemies.alive_count++;
        o->captured = 1; o->capture_t = 3.f; o->flag_raise = 1.f; o->alarm = 0;
    }
    if (pr->alarm_destroyed) o->alarm_destroyed = 1;
    prog_fog_restore(pr, g->fog, MAPW * MAPW);
    if (pr->mast_synced) { o->mast_synced = 1; fog_sync_all(g); }
}

/* ── healing / armor ── */
int game_use_heal(Game *g) {
    Player *p = &g->player;
    if (player_is_down(p)) return 0;
    float missing = p->health_max - p->health;
    float mul = prog_mod(&g->prog, MOD_HEAL);
    int it = -1; float amt = 0.f;
    if (missing > 0.5f) {
        if (missing > 30.f && g->prog.items[IT_MEDKIT] > 0) { it = IT_MEDKIT; amt = 60.f; }
        else if (g->prog.items[IT_BANDAGE] > 0)              { it = IT_BANDAGE; amt = 25.f; }
        else if (g->prog.items[IT_MEDKIT] > 0)               { it = IT_MEDKIT; amt = 60.f; }
    }
    if (it >= 0) {
        prog_use_item(&g->prog, it);
        float before = p->health;
        p->health = dh_clampf(p->health + amt * mul, 0.f, p->health_max);
        game_message(g, "%s +%.0f HP", item_name(it), p->health - before);
        g->message_t = 1.6f;
        return 1;
    }
    if (g->armor < 50.f && g->prog.items[IT_ARMOR_PLATE] > 0) {
        prog_use_item(&g->prog, IT_ARMOR_PLATE);
        g->armor = dh_minf(100.f, g->armor + 50.f);
        game_message(g, "ARMOR PLATE STRAPPED ON (%.0f)", g->armor);
        g->message_t = 1.6f;
        return 1;
    }
    game_message(g, missing > 0.5f ? "NO MEDKITS OR BANDAGES - buy or craft (I)" : "HEALTH FULL");
    g->message_t = 1.6f;
    return 0;
}

/* ── hunting: skin the nearest carcass (§70) ── */
int game_skin_nearest(Game *g) {
    int best = -1; float bd = 2.8f;
    for (int i = 0; i < g->critter_count; i++) {
        Critter *c = &g->critters[i];
        if (c->state != 3) continue;
        float d = v3_dist_xz(c->pos, g->player.pos);
        if (d < bd) { bd = d; best = i; }
    }
    if (best < 0) return 0;
    Critter *c = &g->critters[best];
    int n = (int)prog_mod(&g->prog, MOD_HIDES);
    g->prog.mat[c->species == 0 ? MAT_DEER_HIDE : MAT_BOAR_HIDE] += n;
    c->state = 4;
    sys_xp(g, EV_HUNT);
    game_message(g, "%s HIDE x%d - sell to Rosa or craft (I)", c->species == 0 ? "DEER" : "BOAR", n);
    g->message_t = 2.2f;
    return n;
}

static int sys_in_combat(Game *g) {
    for (int i = 0; i < g->enemies.count; i++) {
        const Enemy *e = &g->enemies.v[i];
        if (e->state == EN_COMBAT && v3_dist_xz(e->pos, g->player.pos) < 150.f) return 1;
    }
    return 0;
}
static int sys_wanted(Game *g) { return g->act == 1 && g->city.heat.stars > 0; }

/* ── shops (§72) ── */
int game_shop_buy(Game *g, int vi, int oi) {
    if (vi < 0 || vi >= g->econ.vendor_n) return -9;
    Vendor *v = &g->econ.vendors[vi];
    if (oi < 0 || oi >= v->offer_n) return -9;
    Offer *o = &v->offers[oi];
    if (o->kind == OFFER_AMMO) {
        int cap = game_ammo_cap(g, o->value);
        if (g->ammo[o->value] >= cap) { game_message(g, "AMMO FULL (%d cap)", cap); g->message_t = 1.5f; return -2; }
    }
    if (o->kind == OFFER_MAP) {
        int all = 1;
        for (int i = 0; i < MAPW * MAPW; i++) if (!g->fog[i]) { all = 0; break; }
        if (all || g->act != 0) { game_message(g, "YOU ALREADY KNOW THESE ROADS"); g->message_t = 1.5f; return -2; }
    }
    int before = g->cash;
    int r = prog_buy(&g->prog, o, &g->cash);
    if (r == -1) { game_message(g, "NOT ENOUGH CASH ($%d NEEDED)", prog_price(&g->prog, o->price)); g->message_t = 1.8f; return r; }
    if (r == -2) { game_message(g, "YOU CAN'T CARRY MORE"); g->message_t = 1.5f; return r; }
    if (r == -3) { game_message(g, "NOTHING TO SELL"); g->message_t = 1.5f; return r; }
    if (o->kind == OFFER_AMMO) {
        int cap = game_ammo_cap(g, o->value);
        g->ammo[o->value] = dh_clampi(g->ammo[o->value] + o->amount, 0, cap);
    } else if (o->kind == OFFER_MAP) {
        for (int i = 0; i < MAPW * MAPW; i++) if (!g->fog[i]) g->fog[i] = 1;
    }
    if (o->kind != OFFER_SELL_MAT) g->stat_buys++;
    if (o->kind == OFFER_SELL_MAT) game_message(g, "SOLD - +$%d (cash $%d)", g->cash - before, g->cash);
    else game_message(g, "BOUGHT %s - $%d", o->name, before - g->cash);
    g->message_t = 1.8f;
    return 0;
}

/* ── fast travel ── */
int game_fast_travel(Game *g, int node) {
    if (g->in_vehicle >= 0) return -5;
    int r = prog_fast_travel_check(&g->prog, node, g->act, sys_wanted(g), sys_in_combat(g));
    if (r) return r;
    Vec3 p = g->prog.ft[node].pos;
    Vec3 dst = v3(p.x + 2.5f, 0.f, p.z + 2.5f);
    if (node == FT_MAST) dst = v3(p.x + 4.f, 0.f, p.z + 4.f);
    player_set_spawn(&g->player, dst);
    g->player.vel = v3(0, 0, 0);
    g->spawn = g->player.pos;                 /* travel point doubles as checkpoint */
    game_set_time(g, fmodf(g->day_t + 0.02f, 1.f));   /* ~30 min on the road */
    fog_reveal(g, dst.x, dst.z, 95.f);
    game_message(g, "FAST TRAVEL - %s", g->prog.ft[node].name);
    g->message_t = 2.5f;
    DH_INFO("game", "fast travel -> %s", g->prog.ft[node].name);
    return 0;
}

static const char *ft_error(int r) {
    switch (r) {
    case -1: return "NOT DISCOVERED YET";
    case -2: return "TAKE BETO'S FERRY FIRST (K)";
    case -3: return "LOSE THE HEAT FIRST";
    case -4: return "NOT WHILE HOSTILES ARE ON YOU";
    case -5: return "STEP OUT OF THE VEHICLE";
    default: return "CAN'T TRAVEL";
    }
}

/* ── saves (§10.5): everything goes through SaveGame JSON ── */
static const char *AMMO_KEYS[AMMO_N] = { "ammo_9mm", "ammo_556", "ammo_12g", "ammo_338", "ammo_arrow" };
static const char *MAT_KEYS[MAT_N]   = { "mat_deer_hide", "mat_boar_hide", "mat_herb", "mat_scrap" };
static const char *ITEM_KEYS[IT_N]   = { "item_medkit", "item_bandage", "item_armor" };

static void inv_put(SaveGame *s, const char *id, int n) {
    if (s->inv_count >= SAVE_MAX_INVENTORY) return;
    dh_strcpy_safe(s->inv_ids[s->inv_count], sizeof s->inv_ids[0], id);
    s->inv_counts[s->inv_count++] = n;
}
static int inv_get(const SaveGame *s, const char *id, int dflt) {
    for (int i = 0; i < s->inv_count; i++) if (!strcmp(s->inv_ids[i], id)) return s->inv_counts[i];
    return dflt;
}

int game_save(Game *g, int slot, int is_auto) {
    if (g->act == 0) game_island_store(g);
    static SaveGame s;                         /* ~60 KB: keep it off the stack */
    save_new_game(&s, 1, g->act);
    const Progress *pr = &g->prog;
    const Player *p = &g->player;
    s.map = g->act;
    s.time_of_day = g->day_t;
    s.island_heat = pr->alert;
    s.wanted_level = (g->act == 1) ? g->city.heat.stars : pr->city_stars;
    s.pos = p->pos; s.yaw = p->yaw; s.pitch = p->pitch;
    s.health = p->health; s.health_max = p->health_max;
    s.armor = g->armor; s.armor_max = 100.f;
    s.money = g->cash;
    s.level = pr->level; s.xp = pr->xp; s.skill_points = pr->skill_points;
    s.skills_unlocked = pr->skills;
    s.checkpoint = g->spawn;
    s.weapon_count = WPN_SLOT_MAX;
    for (int i = 0; i < WPN_SLOT_MAX; i++) {
        s.weapon_ids[i] = g->wpn_def[i];
        s.weapon_ammo[i] = g->wpn_mag[i];
        s.weapon_reserve[i] = 0;
        s.weapon_unlocked[i] = g->wpn_def[i] >= 0;
    }
    s.equipped_slot = g->wpn_slot;
    s.inv_count = 0;
    for (int a = 0; a < AMMO_N; a++) inv_put(&s, AMMO_KEYS[a], g->ammo[a]);
    for (int m = 0; m < MAT_N; m++)  inv_put(&s, MAT_KEYS[m], pr->mat[m]);
    for (int i = 0; i < IT_N; i++)   inv_put(&s, ITEM_KEYS[i], pr->items[i]);
    inv_put(&s, "upgrade_pouch", pr->pouches);
    s.outpost_count = 0;
    SaveOutpost *so = save_outpost(&s, ISLAND_OUTPOST_ID);
    if (so) { so->captured = pr->outpost_captured; so->alarm_level = pr->alarm_destroyed; }
    s.flag_count = 0;
    save_set_flag(&s, "island_saved", pr->island_saved);
    save_set_flag(&s, "mast_synced", pr->mast_synced);
    story_save(g, &s);
    poncho_save(g, &s);
    legend_save(g, &s);
    save_set_flag(&s, "feats", (int)g->feats.got);
    save_set_flag(&s, "cob_enc", g->cob.enc[0] | g->cob.enc[1] << 2 | g->cob.enc[2] << 4 | g->cob.enc[3] << 6 | g->cob.trinket << 8);
    save_set_flag(&s, "cob_inf", (int)g->cob.infamy);
    save_set_flag(&s, "boss", g->boss.beaten | g->boss.clean_mask << 8);
    save_set_flag(&s, "xp_total", pr->xp_total);
    for (int i = 0; i < FT_N; i++) {
        char k[16]; snprintf(k, sizeof k, "ft_%d", i);
        save_set_flag(&s, k, pr->ft_unlocked[i]);
    }
    for (int i = 0; i < 64 * 64 / 32; i++) {
        char k[16]; snprintf(k, sizeof k, "fog_%03d", i);
        if (pr->fog_bits[i]) save_set_flag(&s, k, (int)pr->fog_bits[i]);
    }
    s.stats.money_earned = pr->money_earned;
    s.stats.money_spent = pr->money_spent;
    s.stats.enemies_defeated = g->kills;
    int ok = save_write_slot(slot, is_auto, &s);
    if (ok) g->saves_written++;
    return ok;
}

int game_load(Game *g, int slot, int is_auto) {
    static SaveGame s;
    if (!save_read_slot(slot, is_auto, &s)) return 0;
    Progress *pr = &g->prog;
    prog_init(pr);
    pr->level = dh_clampi(s.level, 1, PROG_LEVEL_CAP);
    pr->xp = s.xp < 0 ? 0 : s.xp;
    pr->skill_points = s.skill_points < 0 ? 0 : s.skill_points;
    pr->skills = s.skills_unlocked & ((1u << SKILL_N) - 1u);
    pr->alert = dh_clampf(s.island_heat, 0.f, 100.f);
    pr->xp_total = save_get_flag(&s, "xp_total", 0);
    for (int m = 0; m < MAT_N; m++) pr->mat[m] = inv_get(&s, MAT_KEYS[m], 0);
    for (int i = 0; i < IT_N; i++)  pr->items[i] = inv_get(&s, ITEM_KEYS[i], 0);
    pr->pouches = dh_clampi(inv_get(&s, "upgrade_pouch", 0), 0, 3);
    for (int i = 0; i < FT_N; i++) {
        char k[16]; snprintf(k, sizeof k, "ft_%d", i);
        pr->ft_unlocked[i] = save_get_flag(&s, k, i == FT_ISLAND_SAFEHOUSE);
    }
    for (int i = 0; i < 64 * 64 / 32; i++) {
        char k[16]; snprintf(k, sizeof k, "fog_%03d", i);
        pr->fog_bits[i] = (uint32_t)save_get_flag(&s, k, 0);
    }
    SaveOutpost *so = save_outpost(&s, ISLAND_OUTPOST_ID);
    pr->outpost_captured = so ? so->captured : 0;
    pr->alarm_destroyed = so ? so->alarm_level : 0;
    pr->mast_synced = save_get_flag(&s, "mast_synced", 0);
    pr->island_saved = save_get_flag(&s, "island_saved", 0);
    pr->money_earned = (int)s.stats.money_earned;
    pr->money_spent = (int)s.stats.money_spent;
    pr->city_stars = dh_clampi(s.wanted_level, 0, 5);

    if (s.map == 1) game_load_city(g); else game_load_island(g);

    Player *p = &g->player;
    g->cash = s.money < 0 ? 0 : s.money;
    g->armor = dh_clampf(s.armor, 0.f, 100.f);
    for (int i = 0; i < WPN_SLOT_MAX && i < s.weapon_count; i++) {
        g->wpn_def[i] = (s.weapon_unlocked[i] && weapons_get(s.weapon_ids[i])) ? s.weapon_ids[i] : -1;
        g->wpn_mag[i] = g->wpn_def[i] >= 0 ? s.weapon_ammo[i] : 0;
    }
    g->wpn_slot = dh_clampi(s.equipped_slot, 0, WPN_SLOT_MAX - 1);
    for (int a = 0; a < AMMO_N; a++) g->ammo[a] = inv_get(&s, AMMO_KEYS[a], 0);
    game_set_time(g, s.time_of_day);
    if (v3_valid(s.checkpoint) && (s.checkpoint.x != 0.f || s.checkpoint.z != 0.f)) g->spawn = s.checkpoint;
    if (v3_valid(s.pos)) player_set_spawn(p, s.pos);
    p->yaw = s.yaw;
    game_apply_skills(g);
    p->health = dh_clampf(s.health > 0.f ? s.health : p->health_max, 1.f, p->health_max);
    if (s.map == 1 && pr->city_stars > 0) {        /* wanted persistence */
        static const float TH[6] = { 0.f, 1.f, 3.f, 6.f, 10.f, 15.f };
        heat_add_evidence(&g->city.heat, TH[pr->city_stars]);
    }
    story_load(g, &s);
    poncho_load(g, &s);
    legend_load(g, &s);
    g->feats.got = (uint32_t)save_get_flag(&s, "feats", 0); g->feats.primed = 0;
    { int ce = save_get_flag(&s, "cob_enc", 0);
      for (int i = 0; i < COB_N; i++) g->cob.enc[i] = (ce >> (i * 2)) & 3;
      g->cob.trinket = (ce >> 8) & 1; g->cob.infamy = (float)save_get_flag(&s, "cob_inf", 0);
      g->cob.active = 0; g->cob.primed = 0; g->cob.cd = 60.f; }
    { int bf = save_get_flag(&s, "boss", 0);
      g->boss.beaten = bf & 0xFF; g->boss.clean_mask = (bf >> 8) & 0xFF; g->boss.active = 0; }
    g->mode = GM_PLAY;
    g->ui = UI_NONE;
    save_add_stat_ll(&s.stats.saves_loaded, 1);
    game_message(g, "LOADED - %s, level %d, $%d", s.map ? "MERIDIAN CITY" : "ISLA SOMBRA", pr->level, g->cash);
    g->message_t = 3.f;
    DH_INFO("game", "save loaded (map %d, level %d)", s.map, pr->level);
    return 1;
}

/* ── safehouse: rest = heal, set checkpoint, morning, autosave ── */
int game_safehouse_rest(Game *g) {
    if (sys_wanted(g)) { game_message(g, "THE COPS WOULD FOLLOW YOU IN - LOSE THE HEAT"); g->message_t = 2.5f; return 0; }
    if (sys_in_combat(g)) { game_message(g, "CAN'T REST WITH HOSTILES NEARBY"); g->message_t = 2.5f; return 0; }
    Player *p = &g->player;
    p->health = p->health_max;
    p->breath = p->breath_max;
    g->spawn = v3(g->safehouse[g->act].x + 1.5f, 0.f, g->safehouse[g->act].z);
    g->prog.ft_unlocked[g->act == 0 ? FT_ISLAND_SAFEHOUSE : FT_CITY_SAFEHOUSE] = 1;
    if (g->act == 0) g->prog.alert = dh_maxf(0.f, g->prog.alert - 20.f);   /* the island cools off */
    game_set_time(g, 0.04f);                     /* wake at ~07:00 */
    g->stat_rests++;
    int ok = game_save(g, 0, 1);
    game_message(g, ok ? "RESTED UNTIL MORNING - GAME SAVED" : "RESTED - SAVE FAILED (disk?)");
    g->message_t = 3.f;
    return 1;
}

/* ── per-frame systems tick (play mode, before city_frame) ── */
static int sys_interact(Game *g, const PlatInput *in) {
    if (!(in->pressed & BTN_USE) || g->in_vehicle >= 0 || player_is_down(&g->player)) return 0;
    if (g->act == 1 && g->city.use_cd > 0.f) return 0;
    Vec3 pp = g->player.pos;
    int vi = economy_vendor_near(&g->econ, g->act, pp, 3.2f);
    int used = 0;
    if (vi >= 0) {
        g->ui = UI_SHOP; g->ui_vendor = vi; g->ui_sel = 0;
        used = 1;
    } else if (v3_dist_xz(pp, g->safehouse[g->act]) < 3.0f &&
               (g->act == 0 || g->city.built)) {
        game_safehouse_rest(g);
        used = 1;
    } else if (g->act == 0 && game_skin_nearest(g) > 0) {
        used = 1;
    }
    if (used && g->act == 1) g->city.use_cd = 0.35f;   /* the city must not also eat E */
    return used;
}

static void story_frame(Game *g, float dt);   /* story.inl */
static void sys_frame(Game *g, const PlatInput *in, float dt) {
    if (g->act == 0) prog_alert_tick(&g->prog, dt);
    else if (g->prog.last_stand_cd > 0.f) g->prog.last_stand_cd -= dt;
    if (in->pressed & BTN_HEAL) game_use_heal(g);
    sys_interact(g, in);
    story_frame(g, dt);
    if (g->act == 1 && g->city.built) {
        if (!g->prog.ft_unlocked[FT_CITY_SAFEHOUSE] && v3_dist_xz(g->player.pos, g->safehouse[1]) < 15.f) {
            g->prog.ft_unlocked[FT_CITY_SAFEHOUSE] = 1;
            game_message(g, "HARBOR SAFEHOUSE FOUND - rest here to save (E)"); g->message_t = 3.f;
        }
        if (!g->prog.ft_unlocked[FT_JOB_BOARD] && v3_dist_xz(g->player.pos, g->city.job_board) < 12.f)
            g->prog.ft_unlocked[FT_JOB_BOARD] = 1;
        g->prog.city_stars = g->city.heat.stars;
    }
}

/* ── modal screens: map (TAB), character (I), shop (E at a vendor) ── */
static int ui_list_len(Game *g) {
    if (g->ui == UI_MAP) return FT_N;
    if (g->ui == UI_SHOP) return g->econ.vendors[g->ui_vendor].offer_n;
    if (g->ui == UI_CHAR) return g->ui_tab == 0 ? SKILL_N : g->econ.recipe_n;
    return 0;
}

static void story_card_input(Game *g, const PlatInput *in);   /* story.inl */
static void sys_ui_input(Game *g, const PlatInput *in) {
    if (g->ui == UI_CARD) { story_card_input(g, in); return; }
    uint32_t pr = in->pressed;
    if (pr & (BTN_MENU | BTN_PAUSE)) { g->ui = UI_NONE; return; }
    if (g->ui == UI_MAP && (pr & BTN_MAP)) { g->ui = UI_NONE; return; }
    if (g->ui == UI_CHAR && (pr & BTN_CHAR)) { g->ui = UI_NONE; return; }
    int n = ui_list_len(g);
    if (n > 0) {
        if (pr & BTN_UP)   g->ui_sel = (g->ui_sel + n - 1) % n;
        if (pr & BTN_DOWN) g->ui_sel = (g->ui_sel + 1) % n;
    }
    if (g->ui == UI_CHAR && (pr & (BTN_LEFT | BTN_RIGHT))) {
        int step = (pr & BTN_LEFT) ? -1 : 1;
        int s = g->ui_sel;
        if (g->ui_tab == 0 && (s % 6 + 0) >= 0) {   /* skills: left/right jumps trees */
            int tree = (s / 6 + step + TREE_N) % TREE_N;
            if ((pr & BTN_LEFT) && s / 6 == 0) { g->ui_tab = 1; g->ui_sel = 0; return; }
            if ((pr & BTN_RIGHT) && s / 6 == TREE_N - 1) { g->ui_tab = 1; g->ui_sel = 0; return; }
            g->ui_sel = tree * 6 + s % 6;
        } else { g->ui_tab = 0; g->ui_sel = 0; }
    }
    if (!(pr & BTN_USE)) return;
    if (g->ui == UI_MAP) {
        int r = game_fast_travel(g, g->ui_sel);
        if (r == 0) g->ui = UI_NONE;
        else { game_message(g, "%s", ft_error(r)); g->message_t = 2.f; }
    } else if (g->ui == UI_SHOP) {
        game_shop_buy(g, g->ui_vendor, g->ui_sel);
    } else if (g->ui == UI_CHAR) {
        if (g->ui_tab == 0) {
            int r = prog_unlock(&g->prog, g->ui_sel);
            game_message(g, "%s - %s", skill_def(g->ui_sel)->name, prog_unlock_error(r));
            g->message_t = 2.f;
            if (r == 0) game_apply_skills(g);
        } else {
            const Recipe *rc = &g->econ.recipes[g->ui_sel];
            if (prog_craft(&g->prog, rc) == 0) game_message(g, "CRAFTED %s", rc->name);
            else game_message(g, "MISSING MATERIALS FOR %s", rc->name);
            g->message_t = 2.f;
        }
    }
}
