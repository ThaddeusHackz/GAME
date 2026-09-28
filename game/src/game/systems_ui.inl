/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M5 screens + HUD extras (textually included by game.c
   after draw_hud's helpers). Pure 2D quads + bitmap font: identical on the
   CPU rasterizer and GL11.
   ══════════════════════════════════════════════════════════════════════════ */

static void sys_draw_hud(Game *g) {
    Settings *s = settings();
    float ui = s->ui_scale, fs = 2.0f * ui;
    float W = (float)g->w, H = (float)g->h, pad = 12.0f * ui;
    const Progress *pr = &g->prog;
    char buf[160];
    float by = H - pad - 46.0f * ui, bw = 180.0f * ui;

    /* armor sliver over the health bar */
    if (g->armor > 0.5f)
        bar(pad, by - 3.0f * ui, bw, 2.0f * ui, g->armor / 100.f, 0xFFE0D0A0u, 0x60000000u);

    /* level + xp (above the vitals block) */
    float ly = by - 34.0f * ui;
    snprintf(buf, sizeof buf, "LV %d%s", pr->level, pr->skill_points > 0 ? "  +SKILL [I]" : "");
    font_text_shadow(pad, ly - 11.0f * ui, fs * 0.8f, buf, pr->skill_points > 0 ? C_GOLD : C_WHITE);
    float need = (float)prog_xp_to_next(pr->level);
    bar(pad, ly, bw, 4.0f * ui, pr->level >= PROG_LEVEL_CAP ? 1.f : (float)pr->xp / need, 0xFFF0C040u, C_DARK);

    /* bag summary */
    snprintf(buf, sizeof buf, "MEDKIT %d  BANDAGE %d  PLATE %d  [H]",
             pr->items[IT_MEDKIT], pr->items[IT_BANDAGE], pr->items[IT_ARMOR_PLATE]);
    font_text_shadow(pad, H - pad - 4.0f * ui, fs * 0.65f, buf, C_DIM);
    if (g->act == 0) {
        snprintf(buf, sizeof buf, "CASH $%d", g->cash);
        font_text_shadow(pad + bw + 8.0f * ui, by - 14.0f * ui, fs * 0.9f, buf, C_GOLD);
    }

    /* island alert level under the minimap */
    if (g->act == 0) {
        int al = prog_alert_level(pr);
        static const uint32_t AC[4] = { 0xFF80C080u, 0xFF60D0F0u, 0xFF4090F0u, 0xFF4050E0u };
        snprintf(buf, sizeof buf, "ISLAND: %s", prog_alert_name(al));
        font_text_shadow(W - pad - 132.0f * ui, pad + 140.0f * ui, fs * 0.7f, buf, AC[al]);
    }

    /* context prompts */
    if (g->in_vehicle < 0 && g->mode == GM_PLAY) {
        const char *prompt = NULL;
        int vi = economy_vendor_near(&g->econ, g->act, g->player.pos, 3.2f);
        if (vi >= 0) { snprintf(buf, sizeof buf, "[E] TRADE - %s", g->econ.vendors[vi].name); prompt = buf; }
        else if (v3_dist_xz(g->player.pos, g->safehouse[g->act]) < 3.0f &&
                 (g->act == 0 || g->city.built)) prompt = "[E] REST + SAVE (SAFEHOUSE)";
        else if (g->act == 0) {
            for (int i = 0; i < g->critter_count; i++)
                if (g->critters[i].state == 3 && v3_dist_xz(g->critters[i].pos, g->player.pos) < 2.8f) {
                    prompt = "[E] SKIN"; break;
                }
        }
        if (prompt) font_text_center(W * 0.5f, H - 134.0f * ui, fs * 0.85f, prompt, C_GOLD);
    }
}

/* ── panels ── */
static void panel(float x, float y, float w, float h) {
    rend_quad2d(x, y, w, h, -1, 0,0,1,1, 0xE0100C0Au);
    rend_quad2d(x, y, w, 2.f, -1, 0,0,1,1, C_GOLD);
}

static void draw_map_screen(Game *g) {
    float ui = settings()->ui_scale, fs = 2.0f * ui;
    float W = (float)g->w, H = (float)g->h;
    const Progress *pr = &g->prog;
    char buf[128];
    rend_quad2d(0, 0, W, H, -1, 0,0,1,1, 0xD0080604u);
    float ms = dh_minf(W * 0.62f, H * 0.86f);
    float mx = W * 0.04f, my = (H - ms) * 0.5f;
    float cs = ms / (float)MAPW;
    for (int rz = 0; rz < MAPW; rz++)          /* run-length rows: ~10x fewer draws */
        for (int cx = 0; cx < MAPW;) {
            uint32_t col = g->fog[rz * MAPW + cx] ? g->map_cell[rz * MAPW + cx] : 0xFF1A1612u;
            int e = cx + 1;
            while (e < MAPW && (g->fog[rz * MAPW + e] ? g->map_cell[rz * MAPW + e] : 0xFF1A1612u) == col) e++;
            rend_quad2d(mx + cx * cs, my + rz * cs, (e - cx) * cs + 0.6f, cs + 0.6f, -1, 0,0,1,1, col);
            cx = e;
        }
    float k = ms / WORLD_SIZE;
    #define MAP_ICON(wx, wz, col, sz) rend_quad2d(mx + (wx) * k - (sz) * 0.5f, my + (wz) * k - (sz) * 0.5f, sz, sz, -1, 0,0,1,1, col)
    if (g->act == 0 && g->outpost.built) {
        const Outpost *o = &g->outpost;
        int ocx = dh_clampi((int)(o->center.x / (WORLD_SIZE / MAPW)), 0, MAPW - 1);
        int ocz = dh_clampi((int)(o->center.z / (WORLD_SIZE / MAPW)), 0, MAPW - 1);
        if (g->fog[ocz * MAPW + ocx]) MAP_ICON(o->center.x, o->center.z, o->captured ? C_JADE : C_RED, 9.f * ui);
        if (o->mast_synced) MAP_ICON(o->mast_pos.x, o->mast_pos.z, C_JADE, 6.f * ui);
    }
    for (int i = 0; i < g->econ.vendor_n; i++)
        if (g->econ.vendors[i].act == g->act)
            MAP_ICON(g->econ.vendors[i].pos.x, g->econ.vendors[i].pos.z, 0xFF40C0F0u, 7.f * ui);
    MAP_ICON(g->safehouse[g->act].x, g->safehouse[g->act].z, 0xFFF0F0F0u, 8.f * ui);
    for (int i = 0; i < FT_N; i++)
        if (pr->ft[i].act == g->act && pr->ft_unlocked[i]) {
            uint32_t c = (i == g->ui_sel) ? C_GOLD : 0xFFD8C84Fu;
            MAP_ICON(pr->ft[i].pos.x, pr->ft[i].pos.z, c, (i == g->ui_sel ? 11.f : 6.f) * ui);
        }
    MAP_ICON(g->player.pos.x, g->player.pos.z, C_WHITE, 7.f * ui);
    MAP_ICON(g->player.pos.x + sinf(g->player.yaw) * 9.f / k * ui, g->player.pos.z + cosf(g->player.yaw) * 9.f / k * ui, C_GOLD, 3.f * ui);
    #undef MAP_ICON

    float lx = mx + ms + 24.f * ui, ly = my;
    float lw = W - lx - 20.f * ui;
    panel(lx - 8.f * ui, ly - 8.f * ui, lw + 16.f * ui, ms + 16.f * ui);
    font_text_shadow(lx, ly, fs * 1.1f, g->act == 0 ? "ISLA SOMBRA" : "MERIDIAN CITY", C_GOLD);
    snprintf(buf, sizeof buf, "EXPLORED %d%%", game_map_reveal_pct(g));
    font_text_shadow(lx, ly + 18.f * ui, fs * 0.8f, buf, C_DIM);
    font_text_shadow(lx, ly + 44.f * ui, fs * 0.9f, "FAST TRAVEL", C_WHITE);
    for (int i = 0; i < FT_N; i++) {
        float yy = ly + 62.f * ui + i * 16.f * ui;
        int here = pr->ft[i].act == g->act;
        uint32_t c = !pr->ft_unlocked[i] ? 0xFF706860u : !here ? 0xFF908878u : C_WHITE;
        if (i == g->ui_sel) rend_quad2d(lx - 4.f * ui, yy - 3.f * ui, lw, 14.f * ui, -1, 0,0,1,1, 0x6064C8F0u);
        snprintf(buf, sizeof buf, "%s%s", pr->ft_unlocked[i] ? pr->ft[i].name : "???",
                 pr->ft_unlocked[i] && !here ? (pr->ft[i].act ? "  (CITY)" : "  (ISLAND)") : "");
        font_text_shadow(lx, yy, fs * 0.8f, buf, c);
    }
    float ky = ly + ms - 70.f * ui;
    font_text_shadow(lx, ky, fs * 0.7f, "WHITE  YOU / SAFEHOUSE", C_DIM);
    font_text_shadow(lx, ky + 12.f * ui, fs * 0.7f, "BLUE   VENDOR", 0xFF40C0F0u);
    font_text_shadow(lx, ky + 24.f * ui, fs * 0.7f, "RED/GREEN  OUTPOST", C_DIM);
    font_text_shadow(lx, ky + 44.f * ui, fs * 0.7f, "UP/DN  E TRAVEL  TAB CLOSE", C_GOLD);
}

static void draw_char_screen(Game *g) {
    float ui = settings()->ui_scale, fs = 2.0f * ui;
    float W = (float)g->w, H = (float)g->h;
    const Progress *pr = &g->prog;
    char buf[160];
    rend_quad2d(0, 0, W, H, -1, 0,0,1,1, 0xD8080604u);
    float x0 = W * 0.05f, y0 = H * 0.07f;
    snprintf(buf, sizeof buf, "LEVEL %d   XP %d/%d   SKILL POINTS %d   CASH $%d",
             pr->level, pr->xp, prog_xp_to_next(pr->level), pr->skill_points, g->cash);
    font_text_shadow(x0, y0, fs, buf, C_GOLD);
    font_text_shadow(x0, y0 + 16.f * ui, fs * 0.75f,
                     g->ui_tab == 0 ? "[SKILLS]   CRAFTING      LEFT/RIGHT TREE+TAB  UP/DOWN  E UNLOCK  I CLOSE"
                                    : " SKILLS   [CRAFTING]     LEFT/RIGHT TAB  UP/DOWN  E CRAFT  I CLOSE", C_DIM);
    float ty = y0 + 40.f * ui;
    if (g->ui_tab == 0) {
        float colw = (W * 0.90f) / TREE_N;
        for (int t = 0; t < TREE_N; t++) {
            float cx = x0 + t * colw;
            panel(cx, ty, colw - 10.f * ui, H * 0.56f);
            font_text_shadow(cx + 8.f * ui, ty + 8.f * ui, fs * 0.95f, skill_tree_name(t), C_GOLD);
            for (int k = 0; k < 6; k++) {
                int si = t * 6 + k;
                const SkillDef *d = skill_def(si);
                float rowh = (H * 0.56f - 34.f * ui) / 6.f;
                float yy = ty + 28.f * ui + k * rowh;
                int own = prog_has(pr, si), can = prog_can_unlock(pr, si) == 0;
                if (si == g->ui_sel) rend_quad2d(cx + 3.f * ui, yy - 3.f * ui, colw - 16.f * ui, 30.f * ui, -1, 0,0,1,1, 0x5064C8F0u);
                uint32_t c = own ? C_JADE : can ? C_WHITE : 0xFF8A8070u;
                snprintf(buf, sizeof buf, "%s%s", own ? "* " : "", d->name);
                font_text_shadow(cx + 8.f * ui, yy, fs * 0.8f, buf, c);
                snprintf(buf, sizeof buf, "T%d  %dSP  LV%d", d->tier, d->cost, d->min_level);
                font_text_shadow(cx + 8.f * ui, yy + 11.f * ui, fs * 0.6f, buf, C_DIM);
            }
        }
        const SkillDef *d = skill_def(g->ui_sel);
        if (d) {
            float dy = ty + H * 0.56f + 14.f * ui;
            panel(x0, dy, W * 0.90f - 10.f * ui, 44.f * ui);
            snprintf(buf, sizeof buf, "%s - %s", d->name, d->desc);
            font_text_shadow(x0 + 8.f * ui, dy + 8.f * ui, fs * 0.9f, buf, C_WHITE);
            int r = prog_can_unlock(pr, g->ui_sel);
            font_text_shadow(x0 + 8.f * ui, dy + 24.f * ui, fs * 0.7f,
                             r == 0 ? "PRESS E TO UNLOCK" : prog_unlock_error(r), r == 0 ? C_GOLD : C_DIM);
        }
    } else {
        panel(x0, ty, W * 0.44f, H * 0.62f);
        for (int i = 0; i < g->econ.recipe_n; i++) {
            const Recipe *rc = &g->econ.recipes[i];
            float yy = ty + 12.f * ui + i * 30.f * ui;
            if (i == g->ui_sel) rend_quad2d(x0 + 3.f * ui, yy - 3.f * ui, W * 0.44f - 6.f * ui, 27.f * ui, -1, 0,0,1,1, 0x5064C8F0u);
            font_text_shadow(x0 + 8.f * ui, yy, fs * 0.85f, rc->name, prog_can_craft(pr, rc) ? C_WHITE : 0xFF8A8070u);
            char need[96] = "";
            for (int m = 0; m < MAT_N; m++) if (rc->mat[m]) {
                char t[32]; snprintf(t, sizeof t, "%d %s  ", rc->mat[m], material_name(m));
                strncat(need, t, sizeof need - strlen(need) - 1);
            }
            font_text_shadow(x0 + 8.f * ui, yy + 12.f * ui, fs * 0.6f, need, C_DIM);
        }
        float bx = x0 + W * 0.46f;
        panel(bx, ty, W * 0.44f, H * 0.62f);
        font_text_shadow(bx + 8.f * ui, ty + 10.f * ui, fs * 0.95f, "BAG", C_GOLD);
        for (int m = 0; m < MAT_N; m++) {
            snprintf(buf, sizeof buf, "%-10s %d", material_name(m), pr->mat[m]);
            font_text_shadow(bx + 8.f * ui, ty + (32.f + m * 14.f) * ui, fs * 0.8f, buf, C_WHITE);
        }
        for (int i = 0; i < IT_N; i++) {
            snprintf(buf, sizeof buf, "%-10s %d", item_name(i), pr->items[i]);
            font_text_shadow(bx + 8.f * ui, ty + (100.f + i * 14.f) * ui, fs * 0.8f, buf, C_WHITE);
        }
        snprintf(buf, sizeof buf, "AMMO POUCHES %d/3   ARMOR %.0f", pr->pouches, g->armor);
        font_text_shadow(bx + 8.f * ui, ty + 150.f * ui, fs * 0.8f, buf, C_DIM);
        for (int a = 0; a < AMMO_N - 1; a++) {
            snprintf(buf, sizeof buf, "%-6s %d / %d", ammo_name(a), g->ammo[a], game_ammo_cap(g, a));
            font_text_shadow(bx + 8.f * ui, ty + (170.f + a * 14.f) * ui, fs * 0.75f, buf, C_DIM);
        }
    }
}

static void draw_shop_screen(Game *g) {
    float ui = settings()->ui_scale, fs = 2.0f * ui;
    float W = (float)g->w, H = (float)g->h;
    const Vendor *v = &g->econ.vendors[g->ui_vendor];
    const Progress *pr = &g->prog;
    char buf[128];
    float pw = W * 0.5f, ph = 60.f * ui + v->offer_n * 18.f * ui + 40.f * ui;
    float x0 = (W - pw) * 0.5f, y0 = (H - ph) * 0.5f;
    rend_quad2d(0, 0, W, H, -1, 0,0,1,1, 0x90080604u);
    panel(x0, y0, pw, ph);
    font_text_shadow(x0 + 10.f * ui, y0 + 10.f * ui, fs * 1.05f, v->name, C_GOLD);
    snprintf(buf, sizeof buf, "CASH $%d%s", g->cash, prog_has(pr, SK_SMOOTH_TALKER) ? "   (SMOOTH TALKER -15%)" : "");
    font_text_shadow(x0 + 10.f * ui, y0 + 28.f * ui, fs * 0.8f, buf, C_WHITE);
    for (int i = 0; i < v->offer_n; i++) {
        const Offer *o = &v->offers[i];
        float yy = y0 + 52.f * ui + i * 18.f * ui;
        if (i == g->ui_sel) rend_quad2d(x0 + 4.f * ui, yy - 3.f * ui, pw - 8.f * ui, 16.f * ui, -1, 0,0,1,1, 0x5064C8F0u);
        if (o->kind == OFFER_SELL_MAT) {
            int each = (int)((float)o->price * prog_mod(pr, MOD_EXCHANGE) + 0.5f);
            snprintf(buf, sizeof buf, "%-22s have %d  +$%d ea", o->name, pr->mat[o->value], each);
        } else {
            snprintf(buf, sizeof buf, "%-22s $%d", o->name, prog_price(pr, o->price));
        }
        uint32_t c = (o->kind != OFFER_SELL_MAT && g->cash < prog_price(pr, o->price)) ? 0xFF8A8070u : C_WHITE;
        font_text_shadow(x0 + 12.f * ui, yy, fs * 0.8f, buf, c);
    }
    font_text_shadow(x0 + 10.f * ui, y0 + ph - 18.f * ui, fs * 0.7f, "UP/DOWN SELECT   E BUY/SELL   ESC LEAVE", C_GOLD);
}

static void herald_draw_screen(Game *g);   /* herald.inl */
static void sys_draw_screen(Game *g) {
    switch (g->ui) {
    case UI_MAP:  draw_map_screen(g); break;
    case UI_CHAR: draw_char_screen(g); break;
    case UI_SHOP: draw_shop_screen(g); break;
    case UI_CARD: story_draw_card(g); break;
    case UI_HERALD: herald_draw_screen(g); break;
    default: break;
    }
}
