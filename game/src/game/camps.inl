/* ───────────────────────────────────────────────────────────────────────────
   M20 — the Isla Sombra camp network (outposts 2..12, §28).

   Honest scope: Punta Quemada stays the showcase (alarm box, reinforcement
   waves, signal mast). These eleven are lighter "liberation camps": an
   authored sandbag ring with one of three layouts, a streamed garrison
   (3 + difficulty enemies, faction CAMP_FACTION), and the same liberation rule
   — clear the garrison, hold the flag 3 s. Captures persist in saves
   (SaveOutpost ids "camp_00".."camp_10") and count toward 12/12.
   Garrisons stream in at 130 m and are reclaimed past 230 m so the 64-slot
   enemy pool never fills up while touring the island.
   ─────────────────────────────────────────────────────────────────────────── */

static const struct { const char *name; int diff, theme; } CAMP_DEF[CAMP_N] = {
    { "LA SALINA",        1, 0 }, { "MIRADOR VIEJO",   2, 1 },
    { "CANTERA ROJA",     2, 2 }, { "PLAYA DEL HUESO", 1, 0 },
    { "EL ASERRADERO",    3, 1 }, { "POZO NEGRO",      3, 2 },
    { "FARO CAIDO",       4, 1 }, { "LOS MANGLES",     2, 0 },
    { "CRUZ DE PIEDRA",   4, 2 }, { "EL TRAPICHE",     3, 0 },
    { "BAHIA MUDA",       5, 2 },
};

#define CAMP_R          14.f   /* ring half-size */
#define CAMP_STREAM_IN  130.f
#define CAMP_STREAM_OUT 230.f

int game_camp_count(const Game *g) {
    int n = 0;
    if (g) for (int i = 0; i < g->camp_n; i++) n += g->camps[i].stages <= 1;
    return n;
}
int game_camps_captured(const Game *g) {
    int n = (g && g->outpost.captured) ? 1 : 0;
    if (g) for (int i = 0; i < g->camp_n; i++) n += g->camps[i].stages <= 1 && g->camps[i].captured;
    return n;
}
int game_outposts_total(const Game *g) { return (g && g->outpost.built ? 1 : 0) + game_camp_count(g); }
int game_stronghold_slot(const Game *g, int k) {
    if (!g || k < 0 || k >= SH_N) return -1;
    int s = CAMP_N + k;
    return (s < g->camp_n && g->camps[s].stages > 1) ? s : -1;
}
int game_stronghold_count(const Game *g) {
    int n = 0; for (int k = 0; k < SH_N; k++) n += game_stronghold_slot(g, k) >= 0; return n;
}
int game_strongholds_captured(const Game *g) {
    int n = 0;
    for (int k = 0; k < SH_N; k++) { int s = game_stronghold_slot(g, k); if (s >= 0) n += g->camps[s].captured; }
    return n;
}

/* M21 — the three strongholds (§73 ST1-ST3). Multi-stage: three garrison
   waves, each must be cleared before the next marches in, then a 6 s hold.
   HONEST DEVIATION: ST2 "Refinería La Herrería" is specced for Meridian City,
   but the city plate has no free lot outside the block grid, so it stands on
   Isla Sombra as the cartel's island refinery. Logged in M21_report.md. */
static const struct { const char *name; int diff; } SH_DEF[SH_N] = {
    { "MOLINO FORTRESS", 4 }, { "REFINERIA LA HERRERIA", 5 }, { "FARO VIEJO", 5 },
};
static int camp_hold_s(const Camp *c) { return c->stages > 1 ? 6 : 3; }

static int camps_veg_excluded(Game *g, float x, float z) {
    for (int i = 0; i < g->camp_n; i++)
        if (fabsf(x - g->camps[i].center.x) < CAMP_R + 5.f &&
            fabsf(z - g->camps[i].center.z) < CAMP_R + 5.f) return 1;
    return 0;
}

static int camp_site_ok(Game *g, float x, float z, float *h_out) {
    float h = terrain_height(&g->terrain, x, z);
    if (h < 2.5f || h > 16.f) return 0;
    if (terrain_steepness(&g->terrain, x, z) > 0.24f) return 0;
    Vec3 p = v3(x, 0, z);
    if (fabsf(x - g->course_center.x) < 130.f && fabsf(z - g->course_center.z) < 110.f) return 0;
    if (fabsf(x - 150.f) < 100.f && fabsf(z - 620.f) < 90.f) return 0;   /* landing beach */
    if (v3_dist_xz(p, g->arena_center) < 190.f) return 0;
    if (g->outpost.built) {
        if (v3_dist_xz(p, g->outpost.center) < 120.f) return 0;
        if (v3_dist_xz(p, g->outpost.mast_pos) < 50.f) return 0;
    }
    for (int i = 0; i < g->camp_n; i++)
        if (v3_dist_xz(p, g->camps[i].center) < 115.f) return 0;
    /* whole pad must be land — no half-sunk sandbags */
    for (int k = 0; k < 4; k++) {
        float hx = terrain_height(&g->terrain, x + (k & 1 ? CAMP_R : -CAMP_R),
                                               z + (k & 2 ? CAMP_R : -CAMP_R));
        if (hx < 1.2f || fabsf(hx - h) > 5.f) return 0;
    }
    *h_out = h;
    return 1;
}

static void camp_build_one(Game *g, Camp *c) {
    const Vec3 C = c->center; const float P = c->pad_h;
    const int WOOD = proc_tex.wood, CONC = proc_tex.concrete, MET = proc_tex.metal,
              FAB = proc_tex.fabric;
    const uint32_t BAGS = 0xFF6EA0B4u, WOODC = 0xFFC0D0E0u, CW = 0xFFB0C8D8u;
    /* ring with a gate on the south and a gap on the east */
    prop_top_box(g, C.x - 8.f, P + 1.1f, C.z + CAMP_R, 6.f, 0.55f, 0.4f, CONC, BAGS);
    prop_top_box(g, C.x + 9.f, P + 1.1f, C.z + CAMP_R, 5.f, 0.55f, 0.4f, CONC, BAGS);
    prop_top_box(g, C.x,       P + 1.1f, C.z - CAMP_R, CAMP_R, 0.55f, 0.4f, CONC, BAGS);
    prop_top_box(g, C.x - CAMP_R, P + 1.1f, C.z, 0.4f, 0.55f, CAMP_R, CONC, BAGS);
    prop_top_box(g, C.x + CAMP_R, P + 1.1f, C.z - 7.f, 0.4f, 0.55f, 7.f, CONC, BAGS);
    switch (c->theme) {
    case 0: /* tent camp: two canvas tents + cook fire crates */
        for (int t = 0; t < 2; t++) {
            float tx = C.x - 7.f + t * 6.f, tz = C.z - 7.f;
            prop_top_box(g, tx, P + 2.2f, tz, 2.2f, 1.1f, 1.8f, FAB, t ? 0xFF4A6A5Au : 0xFF3E5E70u);
        }
        break;
    case 1: /* watch tower: legs + deck at 4.6 m + crate stairs */
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                prop_box(g, v3(C.x - 8.f + sx * 1.2f, P + 2.3f, C.z - 8.f + sz * 1.2f),
                         v3(0.15f, 2.3f, 0.15f), WOOD, WOODC, 0);
        prop_top_box(g, C.x - 8.f, P + 4.6f, C.z - 8.f, 1.8f, 0.2f, 1.8f, WOOD, WOODC);
        prop_top_box(g, C.x - 4.8f, P + 1.1f, C.z - 5.2f, 0.9f, 0.55f, 0.9f, WOOD, WOODC);
        prop_top_box(g, C.x - 5.9f, P + 2.4f, C.z - 6.3f, 0.9f, 0.65f, 0.9f, WOOD, WOODC);
        prop_top_box(g, C.x - 7.0f, P + 3.6f, C.z - 7.4f, 0.9f, 0.6f, 0.9f, WOOD, WOODC);
        break;
    default: /* concrete bunker with a roof you can take */
        prop_top_box(g, C.x - 6.f, P + 2.4f, C.z - 10.f, 4.5f, 1.2f, 0.3f, CONC, CW);
        prop_top_box(g, C.x - 10.2f, P + 2.4f, C.z - 7.f, 0.3f, 1.2f, 3.f, CONC, CW);
        prop_top_box(g, C.x - 1.8f,  P + 2.4f, C.z - 7.f, 0.3f, 1.2f, 3.f, CONC, CW);
        prop_top_box(g, C.x - 6.f, P + 2.7f, C.z - 7.f, 4.6f, 0.15f, 3.4f, CONC, 0xFF9AA8B8u);
        prop_top_box(g, C.x - 0.9f, P + 1.1f, C.z - 3.2f, 0.8f, 0.55f, 0.8f, WOOD, WOODC);
        break;
    }
    /* yard cover + flag pole */
    prop_top_box(g, C.x + 6.f, P + 1.2f, C.z + 5.f, 0.8f, 0.6f, 0.8f, WOOD, WOODC);
    prop_top_box(g, C.x - 5.f, P + 1.2f, C.z + 6.f, 0.8f, 0.6f, 0.8f, WOOD, WOODC);
    prop_top_box(g, C.x + 8.f, P + 1.1f, C.z - 6.f, 0.4f, 0.55f, 0.4f, MET, 0xFF4E5A64u);
    c->flag_pos = v3(C.x + 3.f, P, C.z);
    prop_top_box(g, c->flag_pos.x, P + 6.f, c->flag_pos.z, 0.08f, 3.f, 0.08f, MET, 0xFFB0B4B8u);
}

static void stronghold_build_one(Game *g, Camp *c, int k) {
    const Vec3 C = c->center; const float P = c->pad_h;
    const int WOOD = proc_tex.wood, CONC = proc_tex.concrete, MET = proc_tex.metal;
    const uint32_t WALL = 0xFF8C9CA8u, WOODC = 0xFFC0D0E0u;
    /* 2.4 m curtain walls (vault-able from crates) with a south gate */
    prop_top_box(g, C.x - 8.5f, P + 2.4f, C.z + CAMP_R, 5.5f, 1.2f, 0.5f, CONC, WALL);
    prop_top_box(g, C.x + 8.5f, P + 2.4f, C.z + CAMP_R, 5.5f, 1.2f, 0.5f, CONC, WALL);
    prop_top_box(g, C.x, P + 2.4f, C.z - CAMP_R, CAMP_R, 1.2f, 0.5f, CONC, WALL);
    prop_top_box(g, C.x - CAMP_R, P + 2.4f, C.z, 0.5f, 1.2f, CAMP_R, CONC, WALL);
    prop_top_box(g, C.x + CAMP_R, P + 2.4f, C.z, 0.5f, 1.2f, CAMP_R, CONC, WALL);
    /* crate steps inside + outside the gate so walls are climbable (M1 verbs) */
    prop_top_box(g, C.x - 4.f, P + 1.1f, C.z + CAMP_R + 1.4f, 0.8f, 0.55f, 0.8f, WOOD, WOODC);
    prop_top_box(g, C.x + 4.f, P + 1.1f, C.z + CAMP_R - 1.4f, 0.8f, 0.55f, 0.8f, WOOD, WOODC);
    /* two corner towers (deck 5 m) */
    for (int t = 0; t < 2; t++) {
        float tx = C.x + (t ? CAMP_R - 2.f : -CAMP_R + 2.f), tz = C.z - CAMP_R + 2.f;
        prop_top_box(g, tx, P + 5.f, tz, 1.4f, 2.5f, 1.4f, CONC, WALL);
    }
    /* signature keep per stronghold */
    if (k == 0) {        /* mill: chimney + press house */
        prop_top_box(g, C.x - 5.f, P + 3.f, C.z - 5.f, 4.f, 1.5f, 3.f, CONC, 0xFF6A7A9Au);
        prop_top_box(g, C.x - 9.f, P + 14.f, C.z - 9.f, 1.1f, 7.f, 1.1f, CONC, 0xFF5A6A8Au);
    } else if (k == 1) { /* refinery: tanks + pipe rack */
        prop_top_box(g, C.x - 6.f, P + 4.f, C.z - 6.f, 2.2f, 2.f, 2.2f, MET, 0xFF7A8A8Au);
        prop_top_box(g, C.x - 1.f, P + 4.f, C.z - 6.f, 2.2f, 2.f, 2.2f, MET, 0xFF7A8A8Au);
        prop_top_box(g, C.x + 5.f, P + 3.2f, C.z - 4.f, 4.f, 0.2f, 0.4f, MET, 0xFF4E5A64u);
    } else {             /* lighthouse: white tower with a lamp room */
        prop_top_box(g, C.x - 6.f, P + 16.f, C.z - 6.f, 1.8f, 8.f, 1.8f, CONC, 0xFFE8ECF0u);
        prop_top_box(g, C.x - 6.f, P + 17.4f, C.z - 6.f, 2.1f, 0.7f, 2.1f, MET, 0xFF40C0F0u);
    }
    prop_top_box(g, C.x + 6.f, P + 1.2f, C.z + 5.f, 0.8f, 0.6f, 0.8f, WOOD, WOODC);
    prop_top_box(g, C.x - 3.f, P + 1.2f, C.z + 7.f, 0.8f, 0.6f, 0.8f, WOOD, WOODC);
    c->flag_pos = v3(C.x + 4.f, P, C.z);
    prop_top_box(g, c->flag_pos.x, P + 7.f, c->flag_pos.z, 0.1f, 3.5f, 0.1f, MET, 0xFFB0B4B8u);
}

/* called from build_island after the mast, BEFORE vegetation + chunk meshing */
static void camps_build(Game *g) {
    memset(g->camps, 0, sizeof g->camps);
    g->camp_n = 0;
    /* 4x4 sector sweep so the camps spread over the whole island; inside a
       sector, a deterministic 24 m grid picks the first acceptable pad */
    static const int order[16] = { 5, 10, 0, 15, 3, 12, 6, 9, 1, 14, 2, 13, 4, 11, 7, 8 };
    for (int pass = 0; pass < 2 && g->camp_n < CAMP_N; pass++)
        for (int oi = 0; oi < 16 && g->camp_n < CAMP_N; oi++) {
            int sx = order[oi] % 4, sz = order[oi] / 4;
            int found = 0;
            for (float z = 40.f + sz * 230.f + 12.f; z < 40.f + (sz + 1) * 230.f && !found; z += 24.f)
                for (float x = 40.f + sx * 230.f + 12.f; x < 40.f + (sx + 1) * 230.f && !found; x += 24.f) {
                    float h;
                    if (!camp_site_ok(g, x, z, &h)) continue;
                    Camp *c = &g->camps[g->camp_n];
                    int i = g->camp_n++;
                    snprintf(c->id, sizeof c->id, "camp_%02d", i);
                    dh_strcpy_safe(c->name, sizeof c->name, CAMP_DEF[i].name);
                    c->difficulty = CAMP_DEF[i].diff;
                    c->theme = CAMP_DEF[i].theme;
                    c->stages = 1;
                    c->pad_h = h;
                    c->center = v3(x, h, z);
                    terrain_flatten(&g->terrain, c->center, CAMP_R + 2.f, CAMP_R + 2.f, h, 8.f);
                    camp_build_one(g, c);
                    found = 1;
                    if (pass == 0) break;
                }
            (void)found;
        }
    if (g->camp_n < CAMP_N)
        DH_WARN("game", "camps: only %d/%d sites found (terrain seed)", g->camp_n, CAMP_N);
    /* strongholds only index cleanly when all camps exist (slot = CAMP_N + k) */
    for (int k = 0; k < SH_N && g->camp_n == CAMP_N + k; k++) {
        int found = 0;
        for (float z = 60.f; z < 940.f && !found; z += 20.f)
            for (float x = 60.f; x < 940.f && !found; x += 20.f) {
                /* spread: ST1 west, ST2 centre, ST3 east (lighthouse on the coast side) */
                float band0 = 60.f + k * 293.f;
                if (x < band0 || x > band0 + 293.f) continue;
                float h;
                if (!camp_site_ok(g, x, z, &h)) continue;
                Camp *c = &g->camps[g->camp_n++];
                snprintf(c->id, sizeof c->id, "stronghold_%d", k + 1);
                dh_strcpy_safe(c->name, sizeof c->name, SH_DEF[k].name);
                c->difficulty = SH_DEF[k].diff; c->theme = 3; c->stages = 3;
                c->pad_h = h; c->center = v3(x, h, z);
                terrain_flatten(&g->terrain, c->center, CAMP_R + 2.f, CAMP_R + 2.f, h, 8.f);
                stronghold_build_one(g, c, k);
                found = 1;
            }
        if (!found) DH_WARN("game", "stronghold %d: no site found", k + 1);
    }
    DH_INFO("game", "camps: %d built, props now %d", g->camp_n, g->prop_count);
}

static void camps_restore(Game *g) {
    for (int i = 0; i < g->camp_n; i++)
        if (g->prog.camps_mask & (1u << i)) {
            Camp *c = &g->camps[i];
            c->captured = c->cleared = 1; c->flag_raise = 1.f; c->capture_t = 3.f;
        }
}

/* Find a slot for one camp enemy: reuse a dead-or-distant camp slot in place
   (other systems hold indices only to their own factions), else append. */
static int camp_slot_spawn(Game *g, Vec3 pos, int arch, Vec3 pa, Vec3 pb, int tag) {
    EnemySet *s = &g->enemies;
    for (int k = 0; k < s->count; k++) {
        Enemy *e = &s->v[k];
        if (e->faction != CAMP_FACTION) continue;
        int dead = e->state == EN_DEAD, owner = e->tag - 100;
        int far = v3_dist_xz(e->pos, g->player.pos) > CAMP_STREAM_OUT;
        int owner_done = owner < 0 || owner >= g->camp_n || g->camps[owner].cleared;
        if (!(far || (dead && owner_done))) continue;
        if (!dead && s->alive_count > 0) s->alive_count--;
        int keep = s->count;
        s->count = k;
        int r = enemies_spawn(s, pos, arch, CAMP_FACTION, pa, pb);
        s->count = keep;
        if (r >= 0) s->v[r].tag = tag;
        return r;
    }
    if (s->count >= ENEMY_MAX - 12) return -1;          /* keep room for bosses/waves */
    int r = enemies_spawn(s, pos, arch, CAMP_FACTION, pa, pb);
    if (r >= 0) s->v[r].tag = tag;
    return r;
}

static void camp_spawn(Game *g, Camp *c) {
    int n = c->stages > 1 ? 4 + 2 * c->stage : 3 + c->difficulty;   /* ST waves 4/6/8 */
    if (n > 8) n = 8;
    int ci = (int)(c - g->camps), got = 0;
    const Vec3 C = c->center; const float P = c->pad_h;
    static const float px[8] = { -3.f, 3.f, 0.f, -9.f, 9.f, -6.f, 5.f, 0.f };
    static const float pz[8] = { 12.f, 12.f, 2.f, 0.f, -4.f, -4.f, 6.f, -10.f };
    for (int k = 0; k < n; k++) {
        int arch = (k == n - 1 && (c->difficulty >= 3 || c->stage == 2)) ? EN_OFFICER
                 : (k % 3 == 2) ? EN_BRUISER : EN_GRUNT;
        Vec3 pos = v3(C.x + px[k], P, C.z + pz[k]);
        if (c->theme == 1 && k == 2) pos = v3(C.x - 8.f, P + 4.8f, C.z - 8.f);   /* tower sentry */
        Vec3 pb = v3(C.x + px[(k + 3) & 7], P, C.z + pz[(k + 3) & 7]);
        if (c->theme == 1 && k == 2) pb = pos;
        if (camp_slot_spawn(g, pos, arch, pos, pb, 100 + ci) >= 0) got++;
    }
    c->spawned = got > 0;
}

/* pop trailing camp enemies that are dead or far from the player */
static void camps_reclaim(Game *g) {
    EnemySet *s = &g->enemies;
    while (s->count > 0) {
        Enemy *e = &s->v[s->count - 1];
        if (e->faction != CAMP_FACTION) break;
        int dead = e->state == EN_DEAD;
        if (!dead && v3_dist_xz(e->pos, g->player.pos) < CAMP_STREAM_OUT) break;
        if (!dead && s->alive_count > 0) s->alive_count--;
        s->count--;
    }
}

static void camps_frame(Game *g, float dt) {
    if (g->act != 0 || g->camp_n == 0) return;
    Player *p = &g->player;
    for (int i = 0; i < g->camp_n; i++) {
        Camp *c = &g->camps[i];
        if (c->captured) {
            if (c->flag_raise < 1.f) c->flag_raise = dh_minf(1.f, c->flag_raise + dt / 2.5f);
            continue;
        }
        float dp = v3_dist_xz(p->pos, c->center);
        int alive = 0, dead = 0;
        for (int k = 0; k < g->enemies.count; k++) {
            const Enemy *e = &g->enemies.v[k];
            if (e->faction != CAMP_FACTION || e->tag != 100 + i) continue;
            if (e->state == EN_DEAD) dead++; else alive++;
        }
        if (!c->cleared) {
            if (c->spawned && alive == 0 && dead > 0 && c->stage < c->stages - 1) {
                /* stronghold: next wave marches in; release this wave's corpses */
                c->stage++; c->spawned = 0;
                for (int k = 0; k < g->enemies.count; k++)
                    if (g->enemies.v[k].faction == CAMP_FACTION && g->enemies.v[k].tag == 100 + i)
                        g->enemies.v[k].tag = 0;
                game_message(g, "%s - wave %d/%d incoming", c->name, c->stage + 1, c->stages);
            } else if (c->spawned && alive == 0 && dead > 0) {
                c->cleared = 1;
                game_message(g, "%s garrison down - hold the flag", c->name);
            } else if (c->spawned && alive == 0 && dead == 0) {
                c->spawned = 0;                     /* wiped/reclaimed: restream */
            } else if (!c->spawned && dp < CAMP_STREAM_IN && !g->arena_active && !g->boss.active) {
                camp_spawn(g, c);
            } else if (c->spawned && dp > CAMP_STREAM_OUT) {
                c->spawned = 0;                     /* despawn: orphan the slots */
                for (int k = 0; k < g->enemies.count; k++) {
                    Enemy *e = &g->enemies.v[k];
                    if (e->faction != CAMP_FACTION || e->tag != 100 + i) continue;
                    if (e->state != EN_DEAD && g->enemies.alive_count > 0) g->enemies.alive_count--;
                    e->state = EN_DEAD; e->tag = 0; e->pos.y = -500.f;   /* hidden, reusable */
                }
            }
            continue;
        }
        /* cleared: hold the flag */
        if (v3_dist_xz(p->pos, c->flag_pos) < 7.f && p->health > 0.f) {
            c->capture_t += dt;
            if (c->capture_t >= (float)camp_hold_s(c)) {
                c->captured = 1;
                g->prog.camps_mask |= 1u << i;
                int sh = c->stages > 1;
                sys_xp(g, sh ? EV_OUTPOST_LOUD : EV_OUTPOST_STEALTH);
                game_stature_add(g, sh ? 6.f : 3.f);
                g->cash += sh ? 500 : 40 + 20 * c->difficulty;
                if (sh) {
                    pickup_add(g, v3(c->flag_pos.x, c->pad_h + 0.4f, c->flag_pos.z + 2.f), PK_AMMO, AMMO_12G, 24.f);
                    game_message(g, "%s FALLS - strongholds %d/%d", c->name,
                                 game_strongholds_captured(g), game_stronghold_count(g));
                    g->message_t = 7.f;
                }
                pickup_add(g, v3(c->flag_pos.x - 2.f, c->pad_h + 0.4f, c->flag_pos.z), PK_HEALTH, 0, 40.f);
                pickup_add(g, v3(c->flag_pos.x + 2.f, c->pad_h + 0.4f, c->flag_pos.z), PK_AMMO, AMMO_556, 60.f);
                if (!sh) {
                    game_message(g, "%s LIBERATED - Isla Sombra %d/%d", c->name,
                                 game_camps_captured(g), game_outposts_total(g));
                    g->message_t = 6.f;
                }
                DH_INFO("game", "camp captured: %s", c->name);
            }
        } else if (c->capture_t > 0.f) {
            c->capture_t = dh_maxf(0.f, c->capture_t - dt * 2.f);
        }
    }
    camps_reclaim(g);
}

static void camps_draw(Game *g) {
    if (g->act != 0) return;
    for (int i = 0; i < g->camp_n; i++) {
        const Camp *c = &g->camps[i];
        Vec3 fp = c->flag_pos;
        if (!rend_should_draw(&fp, 6.f)) continue;
        float fy = fp.y + 1.3f + c->flag_raise * (c->stages > 1 ? 5.f : 4.f);
        float wave = sinf(g->time * 3.f + (float)i) * 0.06f;
        Mat4 m = m4_mul(m4_translate(v3(fp.x + 0.7f, fy + wave, fp.z)),
                        m4_scale(v3(1.4f, 0.46f, 0.03f)));
        rend_mesh_lit(g->mesh_box, &m, proc_tex.fabric,
                      c->captured ? 0xFF8CBE3Cu : 0xFF3030A0u, 1.f, 1, 1, 1, 0, 1);
    }
}

/* test/dev hooks */
Vec3 game_camp_flag(const Game *g, int i) { return (g && i >= 0 && i < g->camp_n) ? g->camps[i].flag_pos : v3(0,0,0); }
int  game_camp_state(const Game *g, int i) {   /* 0 hostile · 1 cleared · 2 liberated */
    if (!g || i < 0 || i >= g->camp_n) return -1;
    return g->camps[i].captured ? 2 : g->camps[i].cleared ? 1 : 0;
}
