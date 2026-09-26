/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M6 story glue (textually included by game.c after
   systems.inl). The rules live in meta/mission.c; this file only observes
   the world, reacts to mission events, and draws the tracker / text cards.
   ══════════════════════════════════════════════════════════════════════════ */

static void story_ctx(Game *g, MissionCtx *c) {
    memset(c, 0, sizeof *c);
    c->act = g->act;
    c->pos = g->player.pos;
    c->anchor[ANC_BEACH] = g->safehouse[0];
    c->anchor[ANC_SAFEHOUSE_I] = g->safehouse[0];
    c->anchor[ANC_SAFEHOUSE_C] = g->safehouse[1];
    for (int i = 0; i < g->econ.vendor_n; i++)
        c->anchor[g->econ.vendors[i].act ? ANC_VENDOR_C : ANC_VENDOR_I] = g->econ.vendors[i].pos;
    /* island anchors survive the ferry through Progress' fast-travel table */
    c->anchor[ANC_OUTPOST] = g->prog.ft[FT_OUTPOST].pos;
    c->anchor[ANC_MAST] = g->prog.ft[FT_MAST].pos;
    if (g->act == 0 && g->outpost.built) {
        c->anchor[ANC_OUTPOST] = g->outpost.center;
        c->anchor[ANC_MAST] = g->outpost.mast_pos;
    }
    c->anchor[ANC_LIGHTHOUSE] = g->arena_center;
    c->anchor[ANC_JOB_BOARD] = g->prog.ft[FT_JOB_BOARD].pos;
    c->anchor[ANC_PIER] = g->prog.ft[FT_CITY_SAFEHOUSE].pos;
    if (g->act == 1 && g->city.built) {
        c->anchor[ANC_JOB_BOARD] = g->city.job_board;
        c->anchor[ANC_PIER] = g->city.player_spawn;
    }
    const Progress *pr = &g->prog;
    c->herbs = g->stat_herbs;   c->buys = g->stat_buys;
    c->rests = g->stat_rests;   c->ferries = g->stat_ferries;
    c->hunts = pr->ev_count[EV_HUNT];
    c->crafts = pr->ev_count[EV_CRAFT];
    c->kills = pr->ev_count[EV_KILL];
    c->jobs = pr->ev_count[EV_JOB];
    c->outpost_captured = (g->act == 0) ? g->outpost.captured : pr->outpost_captured;
    c->mast_synced = (g->act == 0) ? g->outpost.mast_synced : pr->mast_synced;
    c->stars = (g->act == 1) ? g->city.heat.stars : 0;
    c->driving = (g->act == 1 && g->in_vehicle >= 0);
    c->assault_left = -1;
    if (g->ms.assault_started) {
        int left = 0;
        if (g->arena_active)
            for (int i = 0; i < g->enemies.count; i++)
                if (g->enemies.v[i].faction == 0 && g->enemies.v[i].state != EN_DEAD) left++;
        c->assault_left = left;
    }
}

static void story_banner(Game *g, float t, const char *fmt, const char *a, const char *b) {
    snprintf(g->mission_banner, sizeof g->mission_banner, fmt, a, b);
    g->mission_banner_t = t;
}

static void story_open_card(Game *g) {
    g->ui = UI_CARD; g->card_page = 0; g->card_result = 0; g->card_pick = 0;
}

static void story_on_event(Game *g, MissionEvent ev, int finished_idx) {
    const MissionDef *m;
    switch (ev) {
    case MEV_STARTED:
        m = mission_current(&g->ms, &g->missions);
        if (m) {
            story_banner(g, 4.5f, "%s  %s", m->id, m->title);
            game_message(g, "%s", m->logline); g->message_t = 4.5f;
            DH_INFO("story", "mission start %s %s", m->id, m->title);
        }
        break;
    case MEV_MISSION_DONE: case MEV_ALL_DONE:
        if (finished_idx >= 0 && finished_idx < g->missions.n) {
            m = &g->missions.m[finished_idx];
            g->cash += m->cash;
            prog_add_xp(&g->prog, m->xp);
            char rw[48]; snprintf(rw, sizeof rw, "+$%d  +%d XP", m->cash, m->xp);
            story_banner(g, 4.5f, "MISSION COMPLETE - %s   %s", m->title, rw);
            DH_INFO("story", "mission done %s (+$%d +%dxp)", m->id, m->cash, m->xp);
        }
        if (ev == MEV_ALL_DONE) {
            game_message(g, "CAMPAIGN COMPLETE - ending %d. Free roam continues.", g->ms.ending);
            g->message_t = 6.f;
        }
        break;
    case MEV_OBJ_DONE:
        g->mission_banner_t = dh_maxf(g->mission_banner_t, 0.f);
        break;
    default: break;
    }
}

static void story_frame(Game *g, float dt) {
    if (g->mission_banner_t > 0.f) g->mission_banner_t -= dt;
    if (!g->story || g->ui != UI_NONE || player_is_down(&g->player)) return;
    MissionCtx c; story_ctx(g, &c);
    int before = g->ms.cur;
    MissionEvent ev = mission_tick(&g->ms, &g->missions, &c, dt);
    if (ev == MEV_CARD) { story_open_card(g); return; }
    if (ev == MEV_ASSAULT) {
        int n = g->ms.pending_assault;
        g->ms.pending_assault = 0;
        if (g->act == 0) {
            game_arena_start(g, n);
            game_message(g, "SERENO'S CONGREGATION - %d hostiles. End this.", n);
            g->message_t = 4.f;
        }
        return;
    }
    story_on_event(g, ev, before);
    if (g->ms.card_open) story_open_card(g);
}

/* modal text card: E / SPACE pages, 1 / 2 picks on choice cards */
static void story_card_input(Game *g, const PlatInput *in) {
    const Objective *o = mission_objective(&g->ms, &g->missions);
    if (!o || !g->ms.card_open) { g->ui = UI_NONE; return; }
    uint32_t pr = in->pressed;
    int adv = (pr & (BTN_USE | BTN_JUMP)) != 0;
    int last = g->card_page >= o->line_n - 1;
    MissionCtx c; story_ctx(g, &c);
    int before = g->ms.cur;
    if (o->kind == OBJ_CHOICE && last && !g->card_result) {
        int pick = (pr & BTN_SLOT1) ? 1 : (pr & BTN_SLOT2) ? 2 : 0;
        if (pr & BTN_UP) g->card_pick = 1;
        if (pr & BTN_DOWN) g->card_pick = 2;
        if (!pick && adv && g->card_pick) pick = g->card_pick;
        if (pick) { g->card_pick = pick; g->card_result = 1; }
        return;
    }
    if (!adv) return;
    if (!last && !g->card_result) { g->card_page++; return; }
    MissionEvent ev = mission_card_done(&g->ms, &g->missions, &c,
                                        o->kind == OBJ_CHOICE ? g->card_pick : 0);
    g->ui = UI_NONE;
    story_on_event(g, ev, before);
    if (g->ms.card_open) story_open_card(g);     /* back-to-back cards */
}

static int story_waypoint(Game *g, Vec3 *out) {
    if (!g->story) return 0;
    MissionCtx c; story_ctx(g, &c);
    return mission_waypoint(&g->ms, &g->missions, &c, out);
}

/* ── drawing ── */
static void story_draw_hud(Game *g) {
    if (!g->story || g->mode != GM_PLAY) return;
    Settings *s = settings();
    float ui = s->ui_scale, fs = 2.0f * ui;
    float W = (float)g->w, H = (float)g->h, pad = 12.0f * ui;
    MissionCtx c; story_ctx(g, &c);
    char buf[160];
    const MissionDef *m = mission_current(&g->ms, &g->missions);
    float ty = pad + 50.0f * fs;
    if (m && g->ms.active)
        { snprintf(buf, sizeof buf, "%s %s", m->id, m->title);
          font_text_shadow(pad, ty, fs, buf, C_GOLD); }
    mission_tracker(&g->ms, &g->missions, &c, buf, sizeof buf);
    font_text_shadow(pad, ty + 11.0f * ui, fs, buf, C_WHITE);

    Vec3 wp;
    if (mission_waypoint(&g->ms, &g->missions, &c, &wp)) {
        float d = v3_dist_xz(g->player.pos, wp);
        wp.y = terrain_height(&g->terrain, wp.x, wp.z) + 3.0f;
        float sx, sy;
        char db[24]; snprintf(db, sizeof db, "%dm", (int)d);
        if (world_to_screen(g, wp, &sx, &sy) && sx > 0 && sx < W && sy > 0 && sy < H) {
            float r = 5.0f * ui;
            rend_quad2d(sx - r, sy - r, 2 * r, 2 * r, -1, 0,0,1,1, 0xC000C8FFu);
            rend_quad2d(sx - r * 0.5f, sy - r * 0.5f, r, r, -1, 0,0,1,1, C_WHITE);
            font_text_center(sx, sy + r + 3.0f * ui, fs, db, C_GOLD);
        } else {   /* off-screen: bearing arrow at the top edge */
            float ang = atan2f(wp.x - g->player.pos.x, wp.z - g->player.pos.z) - g->player.yaw;
            while (ang > 3.14159265f) ang -= 6.2831853f;
            while (ang < -3.14159265f) ang += 6.2831853f;
            float ex = W * 0.5f + dh_clampf(-ang / 3.14159265f, -1.f, 1.f) * W * 0.45f;
            snprintf(buf, sizeof buf, "%s %s", ang > 0.f ? "<" : ">", db);
            font_text_center(ex, pad + 2.0f * ui, fs, buf, 0xFF00C8FFu);
        }
    }
    if (g->mission_banner_t > 0.f && g->mission_banner[0]) {
        float a = dh_clampf(g->mission_banner_t, 0.f, 1.f);
        uint32_t al = (uint32_t)(a * 200.f) << 24;
        rend_quad2d(0, H * 0.24f, W, 26.0f * ui, -1, 0,0,1,1, al | 0x00101418u);
        font_text_center(W * 0.5f, H * 0.24f + 7.0f * ui, fs, g->mission_banner,
                         ((uint32_t)(a * 255.f) << 24) | (C_GOLD & 0x00FFFFFFu));
    }
}

/* word-wrap one card line into the panel */
static float story_wrap(float x, float y, float w, float sc, const char *t, uint32_t col) {
    char line[160]; int ln = 0;
    const char *p = t;
    float lh = font_height(sc) + 3.0f;
    while (*p) {
        const char *we = p; while (*we && *we != ' ') we++;
        int wl = (int)(we - p);
        char trial[160];
        snprintf(trial, sizeof trial, "%.*s%s%.*s", ln, line, ln ? " " : "", wl, p);
        if (ln && font_width(trial, sc) > w) {
            line[ln] = 0; font_text_shadow(x, y, sc, line, col); y += lh; ln = 0;
            continue;
        }
        snprintf(line, sizeof line, "%s", trial); ln = (int)strlen(line);
        p = we; while (*p == ' ') p++;
    }
    if (ln) { line[ln] = 0; font_text_shadow(x, y, sc, line, col); y += lh; }
    return y;
}

static void story_draw_card(Game *g) {
    const Objective *o = mission_objective(&g->ms, &g->missions);
    if (!o) return;
    Settings *s = settings();
    float ui = s->ui_scale, fs = 2.0f * ui;
    float W = (float)g->w, H = (float)g->h;
    float pw = dh_minf(W - 40.0f * ui, 620.0f * ui), ph = 150.0f * ui;
    float x0 = (W - pw) * 0.5f, y0 = H - ph - 30.0f * ui;
    rend_quad2d(x0, y0, pw, ph, -1, 0,0,1,1, 0xE0101418u);
    rend_quad2d(x0, y0, pw, 2.0f * ui, -1, 0,0,1,1, 0xFF40B0E0u);
    font_text_shadow(x0 + 12.f * ui, y0 + 10.f * ui, fs,
                     o->speaker[0] ? o->speaker : "", C_GOLD);
    float ty = y0 + 30.f * ui, tw = pw - 24.f * ui;
    int page = dh_clampi(g->card_page, 0, o->line_n > 0 ? o->line_n - 1 : 0);
    if (g->card_result && g->card_pick >= 1) {
        ty = story_wrap(x0 + 12.f * ui, ty, tw, fs, o->result[g->card_pick - 1], C_WHITE);
    } else if (o->line_n > 0) {
        ty = story_wrap(x0 + 12.f * ui, ty, tw, fs, o->lines[page], C_WHITE);
    }
    char hint[96];
    int last = page >= o->line_n - 1;
    if (o->kind == OBJ_CHOICE && last && !g->card_result) {
        for (int i = 0; i < 2; i++) {
            snprintf(hint, sizeof hint, "[%d] %s", i + 1, o->opt[i]);
            font_text_shadow(x0 + 24.f * ui, ty + 6.f * ui + (float)i * 14.f * ui, fs, hint,
                             g->card_pick == i + 1 ? C_JADE : 0xFF00C8FFu);
        }
        snprintf(hint, sizeof hint, "1 / 2 TO CHOOSE");
    } else {
        snprintf(hint, sizeof hint, "%d/%d   E CONTINUE", g->card_result ? o->line_n : page + 1,
                 o->line_n > 0 ? o->line_n : 1);
    }
    font_text_right(x0 + pw - 12.f * ui, y0 + ph - 16.f * ui, fs, hint, C_DIM);
}

/* ── persistence (save flags) ── */
static void story_save(Game *g, SaveGame *s) {
    const MissionState *st = &g->ms;
    save_set_flag(s, "story_on", g->story);
    save_set_flag(s, "m_cur", st->cur);
    save_set_flag(s, "m_obj", st->obj);
    save_set_flag(s, "m_active", st->active);
    save_set_flag(s, "m_mask", st->done_mask);
    save_set_flag(s, "m_ending", st->ending);
    char k[16];
    for (int i = 0; i < MIS_MAX; i++) { snprintf(k, sizeof k, "m_ch%d", i); save_set_flag(s, k, st->choices[i]); }
    save_set_flag(s, "st_herbs", g->stat_herbs);
    save_set_flag(s, "st_buys", g->stat_buys);
    save_set_flag(s, "st_rests", g->stat_rests);
    save_set_flag(s, "st_ferries", g->stat_ferries);
    for (int e = 0; e < EV_N; e++) { snprintf(k, sizeof k, "ev%d", e); save_set_flag(s, k, g->prog.ev_count[e]); }
}

static void story_load(Game *g, const SaveGame *s) {
    MissionState *st = &g->ms;
    mission_reset(st);
    g->story = save_get_flag(s, "story_on", g->story);
    st->cur = dh_clampi(save_get_flag(s, "m_cur", 0), 0, g->missions.n);
    st->obj = save_get_flag(s, "m_obj", 0);
    st->active = save_get_flag(s, "m_active", 0);
    st->done_mask = save_get_flag(s, "m_mask", 0);
    st->ending = save_get_flag(s, "m_ending", 0);
    char k[16];
    for (int i = 0; i < MIS_MAX; i++) { snprintf(k, sizeof k, "m_ch%d", i); st->choices[i] = save_get_flag(s, k, 0); }
    g->stat_herbs = save_get_flag(s, "st_herbs", 0);
    g->stat_buys = save_get_flag(s, "st_buys", 0);
    g->stat_rests = save_get_flag(s, "st_rests", 0);
    g->stat_ferries = save_get_flag(s, "st_ferries", 0);
    for (int e = 0; e < EV_N; e++) { snprintf(k, sizeof k, "ev%d", e); g->prog.ev_count[e] = save_get_flag(s, k, 0); }
    /* resume at the start of the saved objective (checkpoint) */
    const MissionDef *m = mission_current(st, &g->missions);
    if (!m || st->obj < 0 || st->obj >= m->obj_n) { st->obj = 0; st->active = 0; st->wait_t = 1.f; return; }
    if (st->active) {
        MissionCtx c; story_ctx(g, &c);
        mission_resume(st, &g->missions, &c);
    }
}

int game_story_waypoint(Game *g, Vec3 *out) { return story_waypoint(g, out); }
