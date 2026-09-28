/* cobradores.inl — M13: Los Cobradores bounty squad (Spec §26.2).
   Four named hunters sent after the player once Infamy crosses a threshold.
   Infamy is derived by diffing the lifetime tallies (kills, alarms, loud
   outposts, loud shots, heat stars) and decays slowly over time.
   Each hunter has a distinct gimmick built on the existing enemy AI:
     RASTRA   tracker  — spawns on your trail (behind you), Poncho growls first
     VIDENTE  marksman — spawns far (45 m) on high ground, officer accuracy
     PULPO    brawler  — bruiser that brings two grunts
     LUCKY    charmer  — spawns close and calm; turns hostile once you're near
   Beating a hunter "wounds" them: they retreat and return later with a scar
   (+25% HP per scar). The third defeat is final: a Herald obituary prints.
   Escaping (120 m away or 120 s) also ends an encounter without a defeat.
   All four fallen = the squad is broken: trinket (+stature) and a feat.
   Honest scope: hunters reuse the three enemy archetype meshes (tinted HUD
   marker + name bark only); no bespoke models or voice.
   Included from game.c after feats.inl. */

#define COB_FACTION   3
#define COB_THRESHOLD 30.f
#define COB_COOLDOWN  150.f

static const char *k_cob_role[COB_N] = { "the tracker", "the marksman", "the brawler", "the charmer" };
static const char *k_cob_bark[COB_N][3] = {
    { "RASTRA: \"I smelled you three streets back.\"", "RASTRA: \"The scar itches. It remembers you.\"", "RASTRA: \"Last hunt. One of us stops walking.\"" },
    { "VIDENTE: \"Hold still. This won't take long.\"", "VIDENTE: \"I missed once. Never twice.\"", "VIDENTE: \"I've already seen how this ends.\"" },
    { "PULPO: \"Boys! Grab the arms!\"", "PULPO: \"Round two. I brought more arms.\"", "PULPO: \"No more games, drifter.\"" },
    { "LUCKY: \"Relax, friend. Just talking.\"", "LUCKY: \"Still lucky? Let's check.\"", "LUCKY: \"Double or nothing.\"" },
};

static float cob_raw(Game *g) {
    const Progress *pr = &g->prog;
    int stars = g->act == 1 ? g->city.heat.stars : 0;
    return (float)pr->ev_count[EV_KILL] * 1.0f + (float)pr->ev_count[EV_ALARM] * 4.0f +
           (float)pr->ev_count[EV_OUTPOST_LOUD] * 6.0f + (float)pr->ev_count[EV_LOUD_SHOT] * 0.1f +
           (float)stars * 3.0f;
}

int game_cob_fallen(const Game *g) {
    int n = 0; for (int i = 0; i < COB_N; i++) if (g->cob.enc[i] >= 3) n++; return n;
}
float game_cob_infamy(const Game *g) { return g->cob.infamy; }
int game_cob_active(const Game *g) { return g->cob.active ? g->cob.who : -1; }

static int cob_pick(const Game *g) {
    for (int k = 0; k < COB_N; k++) {
        int i = (g->cob.next + k) % COB_N;
        if (g->cob.enc[i] < 3) return i;
    }
    return -1;
}

static Vec3 cob_ground(Game *g, float x, float z) {
    return v3(x, terrain_height(&g->terrain, x, z), z);
}

/* spawn hunter `who` relative to the player; returns 1 on success */
int game_cob_spawn(Game *g, int who) {
    Cobradores *C = &g->cob;
    if (who < 0 || who >= COB_N || C->enc[who] >= 3 || C->active) return 0;
    if (g->enemies.count + 3 > ENEMY_MAX) return 0;
    const Player *p = &g->player;
    float fy = p->yaw, sx = sinf(fy), cz = cosf(fy);
    float dist = who == 0 ? -22.f : who == 1 ? 45.f : who == 2 ? 26.f : 9.f;
    float side = who == 1 ? 12.f : 0.f;
    Vec3 pos = cob_ground(g, p->pos.x + sx * dist + cz * side, p->pos.z + cz * dist - sx * side);
    int arch = who == 1 ? EN_OFFICER : who == 2 ? EN_BRUISER : EN_GRUNT;
    int slot = enemies_spawn(&g->enemies, pos, arch, COB_FACTION, pos, pos);
    if (slot < 0) return 0;
    Enemy *e = &g->enemies.v[slot];
    float hp = (who == 2 ? 3.0f : 2.5f) * (1.f + 0.25f * (float)C->enc[who]);
    e->health_max *= hp; e->health = e->health_max;
    e->lkp = p->pos; e->lkp_age = 0.f;
    if (who != 3) { e->meter = 1.0f; e->state = EN_COMBAT; }     /* they came for you */
    C->goons = 0;
    if (who == 2) for (int k = -1; k <= 1; k += 2) {
        Vec3 gp = cob_ground(g, pos.x + cz * 4.f * (float)k, pos.z - sx * 4.f * (float)k);
        int s2 = enemies_spawn(&g->enemies, gp, EN_GRUNT, COB_FACTION, gp, gp);
        if (s2 >= 0) { g->enemies.v[s2].meter = 1.f; g->enemies.v[s2].state = EN_COMBAT; g->enemies.v[s2].lkp = p->pos; C->goons++; }
    }
    C->active = 1; C->who = who; C->slot = slot; C->t = 0.f;
    C->tag_t = 5.f;
    game_message(g, "%s", k_cob_bark[who][C->enc[who] > 2 ? 2 : C->enc[who]]);
    g->message_t = 4.f;
    pol_cue(g, SFX_CARD, 0.8f, 0.6f);
    return 1;
}

static void cob_end(Game *g, int defeated) {
    Cobradores *C = &g->cob;
    int who = C->who;
    C->active = 0; C->cd = COB_COOLDOWN;
    C->next = (who + 1) % COB_N;
    if (!defeated) {
        game_message(g, "%s slipped away. They'll be back.", k_cob_names[who]);
        return;
    }
    C->enc[who]++;
    C->infamy -= 20.f; if (C->infamy < 0.f) C->infamy = 0.f;
    prog_add_xp(&g->prog, 150);
    if (C->enc[who] >= 3) {
        game_message(g, "%s, %s, falls for good.", k_cob_names[who], k_cob_role[who]);
        game_herald_print(g, HT_OBIT, who);
        if (game_cob_fallen(g) == COB_N && !C->trinket) {
            C->trinket = 1;
            game_stature_add(g, 8.f);
            game_herald_print(g, HT_SQUAD, COB_N);
            game_message(g, "LOS COBRADORES ARE BROKEN - took Lucky's silver coin");
        }
    } else {
        game_message(g, "%s is wounded and retreats (%d/3)", k_cob_names[who], C->enc[who]);
    }
    g->message_t = 4.f;
}

static void cob_frame(Game *g, float dt) {
    Cobradores *C = &g->cob;
    float raw = cob_raw(g);
    if (!C->primed) { C->prev_raw = raw; C->primed = 1; }
    if (raw > C->prev_raw) C->infamy += raw - C->prev_raw;
    C->prev_raw = raw;
    C->infamy -= dt * 0.02f; if (C->infamy < 0.f) C->infamy = 0.f;
    if (C->infamy > 100.f) C->infamy = 100.f;
    if (C->tag_t > 0.f) C->tag_t -= dt;
    if (g->mode != GM_PLAY) return;

    if (C->active) {
        C->t += dt;
        if (C->slot >= g->enemies.count || g->enemies.v[C->slot].faction != COB_FACTION) {
            C->active = 0; C->cd = 30.f; return;          /* world reloaded under us */
        }
        Enemy *e = &g->enemies.v[C->slot];
        float d = v3_len(v3_sub(e->pos, g->player.pos));
        if (C->who == 3 && e->state != EN_DEAD && e->state != EN_COMBAT && d < 5.f) {
            e->meter = 1.f; e->state = EN_COMBAT; e->lkp = g->player.pos;
            game_message(g, "LUCKY: \"Nothing personal.\""); g->message_t = 2.5f;
        }
        if (e->state == EN_DEAD) { cob_end(g, 1); return; }
        if (d > 120.f || C->t > 120.f) {
            e->state = EN_DEAD; e->health = 0.f;         /* despawn: they withdraw */
            cob_end(g, 0);
        }
        return;
    }
    if (C->cd > 0.f) { C->cd -= dt; return; }
    if (g->arena_active || player_is_down(&g->player)) return;
    if (C->infamy < COB_THRESHOLD) return;
    int who = cob_pick(g);
    if (who < 0) return;
    if (who == 0 && g->dog.state != DOG_TRAPPED && C->warn_t <= 0.f) {
        C->warn_t = 3.f;
        game_message(g, "Poncho growls at something behind you...");
        g->message_t = 3.f;
        return;
    }
    if (C->warn_t > 0.f) { C->warn_t -= dt; if (C->warn_t > 0.f) return; }
    if (!game_cob_spawn(g, who)) C->cd = 10.f;
}

static void cob_draw_hud(Game *g) {
    const Cobradores *C = &g->cob;
    if (!C->active) return;
    float ui = settings()->ui_scale, fs = 1.6f * ui;
    char b[96];
    snprintf(b, sizeof b, "BOUNTY HUNTER: %s  (%d/3)", k_cob_names[C->who], C->enc[C->who]);
    float w = font_width(b, fs) + 16.f * ui;
    float x = ((float)g->w - w) * 0.5f, y = (float)g->h * 0.62f;
    rend_quad2d(x, y, w, 20.f * ui, -1, 0, 0, 1, 1, 0xC0101830);
    font_text(x + 8.f * ui, y + 4.f * ui, fs, b, 0xFF4060F0);
    if (C->slot < g->enemies.count) {
        const Enemy *e = &g->enemies.v[C->slot];
        float f = e->health_max > 0.f ? e->health / e->health_max : 0.f;
        rend_quad2d(x, y + 20.f * ui, w * f, 3.f * ui, -1, 0, 0, 1, 1, 0xFF3040E0);
    }
}
