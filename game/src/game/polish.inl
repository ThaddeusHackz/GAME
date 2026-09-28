/* ══════════════════════════════════════════════════════════════════════════
   M7 POLISH glue — included by game.c after story.inl.
   • audio cues by edge-detecting game state once per frame (so no gameplay
     call-site had to change — fewer places to break, P5)
   • adaptive music intensity (enemies in combat / outpost alarm / heat stars)
   • ambient bird flocks on Isla Sombra that scatter from gunfire
   • camera shake, gated by Settings.camera_shake AND photosensitivity
   • difficulty → enemy damage / accuracy multipliers
   ══════════════════════════════════════════════════════════════════════════ */

static float pol_diff_damage(void) {
    static const float k[DIFF_COUNT] = { 0.5f, 1.0f, 1.5f, 1.0f };
    Settings *s = settings();
    return k[dh_clampi(s->difficulty, 0, DIFF_COUNT - 1)] * s->enemy_damage;
}
static float pol_diff_aim(void) {
    static const float k[DIFF_COUNT] = { 0.7f, 1.0f, 1.2f, 1.0f };
    Settings *s = settings();
    return k[dh_clampi(s->difficulty, 0, DIFF_COUNT - 1)] * s->enemy_aim;
}
static int pol_shake_allowed(void) {
    Settings *s = settings();
    return s->camera_shake && !s->photosensitivity;
}
static void pol_shake(Game *g, float amt) {
    if (!pol_shake_allowed()) return;
    if (amt > g->pol.shake) g->pol.shake = dh_minf(amt, 0.12f);
}
static void pol_cue(Game *g, SfxId id, float vol, float pitch) {
    audio_play(id, vol, pitch);
    g->pol.cues++;
}

static void pol_flocks_init(Game *g) {
    Vec3 c = g->player.pos;
    for (int f = 0; f < BIRD_FLOCKS; f++) {
        Flock *k = &g->pol.flock[f];
        float a = 2.1f * f + 0.4f;
        k->home = v3(c.x + cosf(a) * 70.f, 0.f, c.z + sinf(a) * 70.f);
        k->home.y = dh_maxf(terrain_height(&g->terrain, k->home.x, k->home.z), 0.f)
                    + 26.f + 6.f * f;
        k->off = v3(0, 0, 0); k->vel = v3(0, 0, 0);
        k->radius = 14.f + 5.f * f; k->phase = a; k->speed = 0.35f + 0.08f * f;
        k->scatter_t = 0.f;
    }
}

static void pol_flocks_update(Game *g, float dt) {
    if (g->act != 0) return;
    for (int f = 0; f < BIRD_FLOCKS; f++) {
        Flock *k = &g->pol.flock[f];
        k->phase += k->speed * dt;
        Vec3 bc = v3_add(k->home, k->off);
        /* gunfire within 90 m spooks the flock: burst up & away */
        if (g->noise_t > 0.5f && k->scatter_t <= 0.f &&
            v3_dist_xz(g->player.pos, bc) < 90.f) {
            Vec3 away = v3_sub(bc, g->player.pos); away.y = 0.f;
            float l = sqrtf(away.x * away.x + away.z * away.z);
            if (l < 0.01f) { away = v3(1, 0, 0); l = 1.f; }
            k->vel = v3(away.x / l * 14.f, 9.f, away.z / l * 14.f);
            k->scatter_t = 10.f;
            g->pol.scatters++;
            if (v3_dist(g->cam_pos, bc) < 70.f) pol_cue(g, SFX_BIRDS, 0.5f, 1.1f);
        }
        if (k->scatter_t > 0.f) {
            k->scatter_t -= dt;
            k->off = v3_add(k->off, v3_mul(k->vel, dt));
            k->vel = v3_mul(k->vel, 1.f - 0.6f * dt);
        } else {                                  /* drift home */
            k->off = v3_mul(k->off, 1.f - 0.25f * dt);
        }
    }
}

static void pol_flocks_draw(Game *g) {
    if (g->act != 0) return;
    Vec3 pts[BIRD_FLOCKS * BIRDS_PER * 4];
    int seg = 0;
    for (int f = 0; f < BIRD_FLOCKS; f++) {
        const Flock *k = &g->pol.flock[f];
        Vec3 bc = v3_add(k->home, k->off);
        if (v3_dist(g->cam_pos, bc) > 260.f) continue;
        if (!rend_should_draw(&bc, k->radius + 6.f)) continue;
        for (int b = 0; b < BIRDS_PER; b++) {
            float a = k->phase + b * (6.2832f / BIRDS_PER) + 0.3f * sinf(k->phase * 0.7f + b);
            float r = k->radius * (0.7f + 0.3f * sinf(b * 1.7f));
            Vec3 p = v3(bc.x + cosf(a) * r, bc.y + 2.5f * sinf(a * 2.f + b), bc.z + sinf(a) * r);
            Vec3 fw = v3(-sinf(a), 0.f, cosf(a));             /* tangent heading */
            Vec3 rt = v3(fw.z, 0.f, -fw.x);
            float flap = 0.35f * sinf(g->time * 11.f + b * 1.3f);
            float w = 0.55f;
            Vec3 tipL = v3_add(v3_add(p, v3_mul(rt,  w)), v3(0, flap, 0));
            Vec3 tipR = v3_add(v3_add(p, v3_mul(rt, -w)), v3(0, flap, 0));
            pts[seg * 2] = tipL; pts[seg * 2 + 1] = p; seg++;
            pts[seg * 2] = p; pts[seg * 2 + 1] = tipR; seg++;
        }
    }
    if (seg > 0) rend_lines(pts, seg, 0xFF202028u, 1.4f);
}

/* ── per-frame edge detection → sound cues + music intensity ── */
static void pol_frame(Game *g, float dt) {
    Polish *P = &g->pol;
    Player *p = &g->player;
    Settings *s = settings();
    if (!P->inited) {
        P->inited = 1;
        P->prev_slot = g->wpn_slot; P->prev_mag = g->wpn_mag[g->wpn_slot];
        P->prev_jumps = p->stats.jumps; P->prev_land = p->stats.landings;
        P->prev_done = g->ms.done_mask; P->prev_mode = g->mode; P->prev_ui = g->ui;
        pol_flocks_init(g);
    }
    AudioMix mx = { s->vol_master, s->vol_music, s->vol_sfx,
                    (g->mode != GM_PLAY || g->ui != UI_NONE) ? 1.f : 0.f };
    audio_set_mix(&mx);
    audio_set_music_on(s->adaptive_music || 1);   /* pad always; combat layer adaptive */

    /* weapon fire / reload / dry */
    int sl = g->wpn_slot, mag = g->wpn_mag[sl];
    if (sl == P->prev_slot && mag < P->prev_mag) {
        const WeaponDef *w = weapons_get(g->wpn_def[sl]);
        SfxId id = SFX_SHOT_RIFLE; float kick = 0.02f;
        if (w) {
            if (!strcmp(w->cls, "pistol"))       { id = SFX_SHOT_PISTOL; kick = 0.012f; }
            else if (!strcmp(w->cls, "shotgun")) { id = SFX_SHOT_SHOTGUN; kick = 0.05f; }
            else if (!strcmp(w->cls, "sniper") || !strcmp(w->cls, "dmr")) { id = SFX_SHOT_SNIPER; kick = 0.06f; }
            else if (!strcmp(w->cls, "bow"))     { id = SFX_JUMP; kick = 0.f; }
        }
        pol_cue(g, id, 0.9f, 0.95f + 0.1f * rng_f(&g->rng));
        pol_shake(g, kick);
    }
    if (g->reloading && !P->prev_reload) pol_cue(g, SFX_RELOAD, 0.6f, 1.f);
    P->prev_slot = sl; P->prev_mag = mag; P->prev_reload = g->reloading;

    /* hits, kills, hurt */
    if (g->hitmark_t > P->prev_hit + 0.05f && s->hit_marker)
        pol_cue(g, g->hitmark_kill ? SFX_KILL : SFX_HIT, 0.55f, 1.f);
    if (g->damage_flash > P->prev_dmg + 0.1f) {
        pol_cue(g, SFX_HURT, 0.8f, 0.9f + 0.2f * rng_f(&g->rng));
        pol_shake(g, 0.04f);
    }
    P->prev_hit = g->hitmark_t; P->prev_dmg = g->damage_flash;

    /* traversal: jump / land / footsteps (every ~2.1 m walking) */
    if (p->stats.jumps > P->prev_jumps) pol_cue(g, SFX_JUMP, 0.35f, 1.f);
    if (p->stats.landings > P->prev_land) pol_cue(g, SFX_LAND, 0.5f, 1.f);
    P->prev_jumps = p->stats.jumps; P->prev_land = p->stats.landings;
    if (g->mode == GM_PLAY && p->grounded && g->in_vehicle < 0) {
        float sp = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
        P->step_dist += sp * dt;
        if (P->step_dist > 2.1f && sp > 0.8f) {
            P->step_dist = 0.f;
            pol_cue(g, p->in_water ? SFX_STEP_WATER : SFX_STEP,
                    p->crouched ? 0.15f : 0.3f, 0.85f + 0.3f * rng_f(&g->rng));
        }
    }

    /* story / UI */
    if (g->ms.card_open && !P->prev_card) pol_cue(g, SFX_CARD, 0.7f, 1.f);
    if (g->ms.done_mask != P->prev_done)  pol_cue(g, SFX_MISSION, 0.8f, 1.f);
    if (g->ui != P->prev_ui) pol_cue(g, SFX_UI_CLICK, 0.5f, 1.f);
    if ((int)g->mode != P->prev_mode && g->mode == GM_PAUSE) pol_cue(g, SFX_UI_CLICK, 0.5f, 0.8f);
    P->prev_card = g->ms.card_open; P->prev_done = g->ms.done_mask;
    P->prev_ui = g->ui; P->prev_mode = g->mode;

    /* alarms & sirens */
    int alarm = g->outpost.built && g->outpost.alarm && !g->outpost.captured;
    if (alarm && !P->prev_alarm) pol_cue(g, SFX_ALARM, 0.7f, 1.f);
    P->prev_alarm = alarm;
    int stars = (g->act == 1) ? g->city.heat.stars : 0;
    if (stars > 0) {
        P->siren_t -= dt;
        if (P->siren_t <= 0.f) { pol_cue(g, SFX_SIREN, 0.15f + 0.08f * stars, 1.f); P->siren_t = 1.6f; }
    } else P->siren_t = 0.f;
    P->prev_stars = stars;

    /* island ambience: occasional distant birdsong in daylight */
    if (g->act == 0 && g->mode == GM_PLAY) {
        P->bird_t -= dt;
        if (P->bird_t <= 0.f) { pol_cue(g, SFX_BIRDS, 0.18f, 0.9f + 0.3f * rng_f(&g->rng));
                                 P->bird_t = 9.f + 8.f * rng_f(&g->rng); }
    }

    /* adaptive music: combat layer rises with active threat */
    int fighting = 0;
    for (int i = 0; i < g->enemies.count; i++) {
        const Enemy *e = &g->enemies.v[i];
        if (e->state == EN_COMBAT && e->health > 0.f &&
            v3_dist_xz(e->pos, p->pos) < 120.f) fighting++;
    }
    float it = dh_clampf(fighting * 0.25f + (alarm ? 0.35f : 0.f) + stars * 0.2f, 0.f, 1.f);
    if (!s->adaptive_music) it = 0.f;
    P->intensity = it;
    audio_set_intensity(it);

    /* camera shake decay + flocks */
    P->shake -= P->shake * dh_minf(6.f * dt, 1.f); if (P->shake < 1e-4f) P->shake = 0.f;
    if (!pol_shake_allowed()) P->shake = 0.f;
    pol_flocks_update(g, dt);
}

static void pol_apply_camera(Game *g) {
    if (g->pol.shake <= 0.f) return;
    float a = g->pol.shake, t = g->time * 47.f;
    g->cam_pos = v3_add(g->cam_pos, v3(a * sinf(t), a * sinf(t * 1.31f + 1.f) * 0.7f, a * cosf(t * 0.87f)));
}

float pol_diff_damage_pub(void) { return pol_diff_damage(); }
