/* boss.inl — M14: boss fights (Spec §25 / §75). Slot B1: LA SARGENTO.
   Sargento Marta Reyes drills her unit in the combat yard east of the
   traversal course. Beat the red war drum at the yard's south edge (E) to
   call her out.
   Mechanic (the "cadence"): two drummers at the back of the yard keep a beat.
   While the beat plays, Marta and her four shield-bearers stand in a disciplined
   wall and shrug off 90% / 80% of damage. Killing a drummer breaks the rhythm:
   the unit STAGGERS for 4 s and takes full damage. With every drummer down the
   wall stays broken for the rest of the phase.
     P1 FORMAR     100–66%  beat every 0.6 s
     P2 CORREGIR    66–33%  drummers re-spawn + 2 flankers; faster 2-2-3 beat
     P3 MAS FUERTE  <33%    drums gone for good; Marta charges and whistles
                            (10 m: 0.8 s stun + 8 dmg every 7 s)
   Down line: "The unit... stands."  Clean win (never below 50% HP) = FORMAR.
   Honest deviations from the spec: drums are shot by killing the drummer (no
   separate destructible drum prop); rain/lightning is not staged; Marta reuses
   the officer mesh; no voice acting (text barks only).

   Slot B2: EL RELOJ (M15). Wind the clock dial at the yard's south-west edge (E).
   Four bomb columns sit at the yard corners (±14, ±14) on a shared timer
   (45 s; 35 s from P2). Hold E within 2.5 m of a live column to defuse it. A
   timer running out detonates every live column (12 dmg each) and re-arms them.
   Defusing all four EXPOSES El Reloj for 6 s (armor 0.9 → 0) before re-arming.
   A clock hand sweeps the yard (radius 26 m): standing within 1.2 m of it
   burns 25 HP/s.  P1 TIC 0.35 rad/s, P2 TAC 0.55 rad/s + 35 s timers,
   P3 MEDIANOCHE: the hand reverses at 0.85 rad/s and El Reloj speeds up.
   Clean win (zero detonations) = SINCRONIZADO. Honest deviations: the hand and
   bombs are drawn as simple props/HUD markers, not bespoke models; officer mesh
   reused; text barks only. Bosses 3–6 NOT built. */

#define BOSS_FACTION 5
static const char *k_boss_phase[4] = { "", "FORMAR", "CORREGIR", "MAS FUERTE!" };
static const char *k_reloj_phase[4] = { "", "TIC", "TAC", "MEDIANOCHE!" };

int game_boss_active(const Game *g) { return g->boss.active ? g->boss.who : -1; }
int game_boss_phase(const Game *g)  { return g->boss.active ? g->boss.phase : 0; }
Vec3 game_boss_drum_pos(const Game *g) { return v3_add(g->arena_center, v3(8.f, 0.f, -38.f)); }
Vec3 game_boss_clock_pos(const Game *g) { return v3_add(g->arena_center, v3(-8.f, 0.f, -38.f)); }
Vec3 game_boss_bomb_pos(const Game *g, int k) {
    Vec3 p = v3_add(g->arena_center, v3((k & 1) ? 14.f : -14.f, 0.f, (k & 2) ? 14.f : -14.f));
    p.y = terrain_height(&g->terrain, p.x, p.z); return p;
}
#define RELOJ_R 26.f

static int boss_alive(const Game *g, int slot) {
    return slot >= 0 && slot < g->enemies.count && g->enemies.v[slot].faction == BOSS_FACTION &&
           g->enemies.v[slot].state != EN_DEAD;
}
static int boss_drums_up(const Game *g) {
    return boss_alive(g, g->boss.drum[0]) || boss_alive(g, g->boss.drum[1]);
}
int game_boss_shielded(const Game *g) {
    const Bosses *B = &g->boss;
    if (B->active && B->who == 1) return B->expose_t <= 0.f;
    return B->active && B->phase < 3 && B->stagger_t <= 0.f && boss_drums_up(g);
}

static int boss_spawn_at(Game *g, float dx, float dz, int arch, float hpk) {
    Vec3 A = g->arena_center;
    float x = A.x + dx, z = A.z + dz;
    Vec3 p = v3(x, terrain_height(&g->terrain, x, z), z);
    int s = enemies_spawn(&g->enemies, p, arch, BOSS_FACTION, p, p);
    if (s < 0) return -1;
    Enemy *e = &g->enemies.v[s];
    e->health_max *= hpk; e->health = e->health_max;
    e->meter = 1.f; e->state = EN_COMBAT; e->lkp = g->player.pos; e->lkp_age = 0.f;
    return s;
}

static void boss_spawn_drums(Game *g) {
    for (int k = 0; k < 2; k++) g->boss.drum[k] = boss_spawn_at(g, k ? 10.f : -10.f, 26.f, EN_GRUNT, 1.2f);
}

static int reloj_start(Game *g) {
    Bosses *B = &g->boss;
    if (g->enemies.count + 5 > ENEMY_MAX) return 0;
    memset(B->drum, -1, sizeof B->drum); memset(B->shield, -1, sizeof B->shield);
    memset(B->flank, -1, sizeof B->flank);
    B->slot = boss_spawn_at(g, 0.f, 0.f, EN_OFFICER, 9.f);
    if (B->slot < 0) return 0;
    for (int k = 0; k < 2; k++) B->flank[k] = boss_spawn_at(g, k ? 8.f : -8.f, 6.f, EN_GRUNT, 1.0f);
    B->active = 1; B->who = 1; B->phase = 1; B->clean = 1; B->t = 0.f; B->stun_t = 0.f;
    B->bomb_t = 45.f; B->bomb_live = 0xF; B->hand_ang = 0.f; B->hand_spd = 0.35f;
    B->expose_t = 0.f; B->dets = 0; B->cycle = 0;
    B->fights++;
    game_message(g, "EL RELOJ: \"Tic, tac. Four bombs, one clock - and you are already late.\"");
    g->message_t = 4.f;
    pol_cue(g, SFX_ALARM, 0.7f, 1.3f);
    return 1;
}

int game_boss_start(Game *g, int who) {
    Bosses *B = &g->boss;
    if (who == 1 && !B->active && g->act == 0 && !g->arena_active) return reloj_start(g);
    if (who != 0 || B->active || g->act != 0 || g->arena_active) return 0;   /* only B1 exists */
    if (g->enemies.count + 11 > ENEMY_MAX) return 0;
    memset(B->drum, -1, sizeof B->drum); memset(B->shield, -1, sizeof B->shield);
    memset(B->flank, -1, sizeof B->flank);
    B->slot = boss_spawn_at(g, 0.f, 20.f, EN_OFFICER, 9.f);
    if (B->slot < 0) return 0;
    for (int k = 0; k < 4; k++) B->shield[k] = boss_spawn_at(g, -6.f + 4.f * (float)k, 14.f, EN_BRUISER, 1.0f);
    boss_spawn_drums(g);
    B->active = 1; B->who = 0; B->phase = 1; B->clean = 1;
    B->t = 0.f; B->beat_t = 0.f; B->beat_i = 0; B->stagger_t = 0.f; B->whistle_t = 7.f; B->stun_t = 0.f;
    B->fights++;
    game_message(g, "LA SARGENTO: \"UNIT! FORMAR! Nobody breaks rank!\"");
    g->message_t = 4.f;
    pol_cue(g, SFX_ALARM, 0.7f, 0.7f);
    return 1;
}

static void boss_clear(Game *g) {
    for (int i = 0; i < g->enemies.count; i++) {
        Enemy *e = &g->enemies.v[i];
        if (e->faction == BOSS_FACTION && e->state != EN_DEAD) { e->state = EN_DEAD; e->health = 0.f; }
    }
    g->boss.active = 0; g->boss.stun_t = 0.f;
}

static void boss_win(Game *g) {
    Bosses *B = &g->boss;
    int who = B->who;
    boss_clear(g);
    B->beaten |= 1 << who;
    if (B->clean) B->clean_mask |= 1 << who;
    prog_add_xp(&g->prog, 500);
    game_stature_add(g, 5.f);
    game_herald_print(g, HT_BOSS, who);
    game_message(g, who == 1 ? "EL RELOJ: \"Synchronized... at last.\"" : "LA SARGENTO: \"The unit... stands.\"");
    g->message_t = 5.f;
    pol_cue(g, SFX_MISSION, 0.9f, 1.0f);
}

static void boss_set_armor(Game *g, int shielded) {
    Bosses *B = &g->boss;
    if (boss_alive(g, B->slot)) g->enemies.v[B->slot].armor = shielded ? 0.9f : 0.f;
    for (int k = 0; k < 4; k++)
        if (boss_alive(g, B->shield[k])) g->enemies.v[B->shield[k]].armor = shielded ? 0.8f : 0.f;
}

/* distance from point to the clock-hand segment (centre → RELOJ_R along hand_ang) */
float game_boss_hand_dist(const Game *g, Vec3 p) {
    Vec3 A = g->arena_center;
    float dx = sinf(g->boss.hand_ang), dz = cosf(g->boss.hand_ang);
    float px = p.x - A.x, pz = p.z - A.z;
    float t = dh_clampf(px * dx + pz * dz, 0.f, RELOJ_R);
    float ex = px - dx * t, ez = pz - dz * t;
    return sqrtf(ex * ex + ez * ez);
}
static int reloj_bomb_count(int m) { int n = 0; for (int k = 0; k < 4; k++) n += (m >> k) & 1; return n; }

static void reloj_frame(Game *g, const PlatInput *in, float dt) {
    Bosses *B = &g->boss;
    Player *p = &g->player;
    Enemy *m = &g->enemies.v[B->slot];
    float f = m->health / m->health_max;
    if (B->phase == 1 && f < 0.66f) {
        B->phase = 2; B->hand_spd = 0.55f;
        if (B->bomb_t > 35.f) B->bomb_t = 35.f;
        game_message(g, "EL RELOJ: \"TAC! Let's quicken the tempo.\""); g->message_t = 3.f;
    } else if (B->phase == 2 && f < 0.33f) {
        B->phase = 3; B->hand_spd = -0.85f; m->speed *= 1.4f;
        game_message(g, "EL RELOJ: \"MEDIANOCHE! Time runs backwards for you now.\""); g->message_t = 3.f;
    }
    float timer = B->phase == 1 ? 45.f : 35.f;
    /* exposure window */
    if (B->expose_t > 0.f) {
        B->expose_t -= dt;
        if (B->expose_t <= 0.f) {
            B->bomb_live = 0xF; B->bomb_t = timer;
            game_message(g, "The columns re-arm - tic, tac."); g->message_t = 2.f;
        }
    } else {
        /* defuse */
        if (in && (in->pressed & BTN_USE))
            for (int k = 0; k < 4; k++) {
                if (!((B->bomb_live >> k) & 1)) continue;
                Vec3 d = v3_sub(game_boss_bomb_pos(g, k), p->pos); d.y = 0.f;
                if (v3_len(d) < 2.5f) {
                    B->bomb_live &= ~(1 << k);
                    pol_cue(g, SFX_CARD, 0.8f, 1.2f);
                    if (!B->bomb_live) {
                        B->expose_t = 6.f; B->cycle++;
                        game_message(g, "All columns defused - EL RELOJ IS EXPOSED!");
                    } else {
                        snprintf(g->message, sizeof g->message, "Column defused - %d left", reloj_bomb_count(B->bomb_live));
                    }
                    g->message_t = 2.5f;
                    break;
                }
            }
        if (B->bomb_live) {
            B->bomb_t -= dt;
            if (B->bomb_t <= 0.f) {
                int n = reloj_bomb_count(B->bomb_live);
                B->dets += n; B->clean = 0;
                p->health = dh_clampf(p->health - 12.f * (float)n, 0.f, p->health_max);
                B->bomb_live = 0xF; B->bomb_t = timer;
                snprintf(g->message, sizeof g->message, "BOOM! %d column%s detonated - re-armed", n, n > 1 ? "s" : "");
                g->message_t = 2.5f;
                pol_cue(g, SFX_SHOT_SHOTGUN, 1.0f, 0.4f);
            }
        }
    }
    boss_set_armor(g, B->expose_t <= 0.f);
    /* clock hand sweep */
    B->hand_ang += B->hand_spd * dt;
    float gy = terrain_height(&g->terrain, p->pos.x, p->pos.z);
    if (p->pos.y < gy + 0.6f && game_boss_hand_dist(g, p->pos) < 1.2f)
        p->health = dh_clampf(p->health - 25.f * dt, 0.f, p->health_max);
}

static void boss_frame(Game *g, const PlatInput *in, float dt) {
    Bosses *B = &g->boss;
    if (B->prompt_t > 0.f) B->prompt_t -= dt;
    if (g->mode != GM_PLAY) return;
    if (!B->active) {
        if (g->act != 0 || g->arena_active) return;
        for (int w = 0; w < 2; w++) {
            Vec3 d = v3_sub(w ? game_boss_clock_pos(g) : game_boss_drum_pos(g), g->player.pos); d.y = 0.f;
            if (v3_len(d) < 3.f) {
                B->prompt_t = 0.2f; B->prompt_who = w;
                if (in && (in->pressed & BTN_USE)) game_boss_start(g, w);
            }
        }
        return;
    }
    if (!boss_alive(g, B->slot) && (B->slot >= g->enemies.count || g->enemies.v[B->slot].faction != BOSS_FACTION)) {
        B->active = 0; return;                         /* world reloaded under us */
    }
    B->t += dt;
    Player *p = &g->player;
    if (p->health < p->health_max * 0.5f) B->clean = 0;
    if (player_is_down(p)) {
        boss_clear(g); B->losses++;
        game_message(g, B->who == 1 ? "The clock stops for you. Wind the dial to try again." :
                                      "The drums fade. Beat the war drum to try again."); g->message_t = 4.f;
        return;
    }
    if (g->enemies.v[B->slot].state == EN_DEAD) { boss_win(g); return; }
    if (B->who == 1) { reloj_frame(g, in, dt); return; }

    /* drummer deaths → stagger */
    for (int k = 0; k < 2; k++) {
        int s = B->drum[k];
        if (s >= 0 && s < g->enemies.count && g->enemies.v[s].state == EN_DEAD) {
            B->drum[k] = -1;
            B->stagger_t = 4.f;
            game_message(g, boss_drums_up(g) ? "A drum falls silent - the wall STAGGERS!" :
                                                "The drums are silent - the wall is BROKEN!");
            g->message_t = 2.5f;
        }
    }
    if (B->stagger_t > 0.f) B->stagger_t -= dt;

    Enemy *m = &g->enemies.v[B->slot];
    float f = m->health / m->health_max;
    if (B->phase == 1 && f < 0.66f) {
        B->phase = 2;
        boss_spawn_drums(g);
        for (int k = 0; k < 2; k++) B->flank[k] = boss_spawn_at(g, k ? 22.f : -22.f, 0.f, EN_GRUNT, 1.0f);
        game_message(g, "LA SARGENTO: \"CORREGIR! Flanks, move!\""); g->message_t = 3.f;
    } else if (B->phase == 2 && f < 0.33f) {
        B->phase = 3;
        for (int k = 0; k < 2; k++) if (boss_alive(g, B->drum[k])) {
            Enemy *e = &g->enemies.v[B->drum[k]]; e->state = EN_DEAD; e->health = 0.f; }
        B->drum[0] = B->drum[1] = -1;
        m->speed *= 1.5f;
        game_message(g, "LA SARGENTO: \"MAS FUERTE! I'll do it myself!\""); g->message_t = 3.f;
    }
    int sh = game_boss_shielded(g);
    boss_set_armor(g, sh);

    /* the cadence: audible telegraph of the shield wall */
    if (sh) {
        B->beat_t -= dt;
        if (B->beat_t <= 0.f) {
            static const float pat2[3] = { 0.35f, 0.35f, 0.7f };
            B->beat_t = B->phase == 1 ? 0.6f : pat2[B->beat_i % 3];
            B->beat_i++;
            pol_cue(g, SFX_LAND, 0.6f, 0.55f);
        }
    }
    /* P3 whistle */
    if (B->phase == 3) {
        B->whistle_t -= dt;
        float d = v3_len(v3_sub(m->pos, p->pos));
        if (B->whistle_t <= 0.f && d < 10.f) {
            B->whistle_t = 7.f; B->stun_t = 0.8f;
            p->health -= 8.f; if (p->health < 1.f) p->health = 1.f;
            game_message(g, "*WHISTLE* - you're rattled!"); g->message_t = 1.5f;
            pol_cue(g, SFX_WHINE, 0.8f, 1.6f);
        }
    }
    if (B->stun_t > 0.f) { B->stun_t -= dt; p->vel.x *= 0.2f; p->vel.z *= 0.2f; }
}

static void boss_draw_hud(Game *g) {
    const Bosses *B = &g->boss;
    float ui = settings()->ui_scale, fs = 1.6f * ui;
    if (!B->active) {
        if (B->prompt_t > 0.f) {
            const char *t = B->prompt_who == 1 ?
                            ((B->beaten & 2) ? "[E] WIND THE CLOCK - rematch EL RELOJ"
                                             : "[E] WIND THE CLOCK - challenge EL RELOJ") :
                            (B->beaten & 1) ? "[E] BEAT THE WAR DRUM - rematch LA SARGENTO"
                                            : "[E] BEAT THE WAR DRUM - challenge LA SARGENTO";
            float w = font_width(t, fs) + 16.f * ui;
            float x = ((float)g->w - w) * 0.5f, y = (float)g->h * 0.70f;
            rend_quad2d(x, y, w, 20.f * ui, -1, 0, 0, 1, 1, 0xC0101010);
            font_text(x + 8.f * ui, y + 4.f * ui, fs, t, 0xFFFFFFFF);
        }
        return;
    }
    float w = (float)g->w * 0.5f, x = (float)g->w * 0.25f, y = 34.f * ui;
    char b[96];
    snprintf(b, sizeof b, "%s  -  %s", game_boss_name(B->who), (B->who == 1 ? k_reloj_phase : k_boss_phase)[B->phase]);
    font_text(x, y - 16.f * ui, fs, b, 0xFF60A0FF);
    rend_quad2d(x, y, w, 8.f * ui, -1, 0, 0, 1, 1, 0xC0101010);
    if (B->slot >= 0 && B->slot < g->enemies.count) {
        const Enemy *e = &g->enemies.v[B->slot];
        float f = e->health_max > 0.f ? e->health / e->health_max : 0.f;
        rend_quad2d(x, y, w * f, 8.f * ui, -1, 0, 0, 1, 1, 0xFF2030D0);
    }
    if (B->who == 1) {
        if (B->expose_t > 0.f) snprintf(b, sizeof b, "EXPOSED %.1fs - FIRE!", B->expose_t);
        else snprintf(b, sizeof b, "BOMBS %d/4 LIVE  -  %02d s  -  defuse all to expose him",
                      reloj_bomb_count(B->bomb_live), (int)ceilf(B->bomb_t));
        font_text(x, y + 11.f * ui, fs * 0.85f, b, B->expose_t > 0.f ? 0xFF40E0FF : 0xFF4080FF);
        return;
    }
    if (game_boss_shielded(g)) snprintf(b, sizeof b, "SHIELD WALL HOLDS - silence the drummers");
    else if (B->stagger_t > 0.f) snprintf(b, sizeof b, "STAGGERED %.1fs - FIRE!", B->stagger_t);
    else snprintf(b, sizeof b, "WALL BROKEN");
    font_text(x, y + 11.f * ui, fs * 0.85f, b, game_boss_shielded(g) ? 0xFFA0A0A0 : 0xFF40E0FF);
}
