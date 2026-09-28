/* ══════════════════════════════════════════════════════════════════════════
   M11 — Stature (Spec §26.4) + the Herald (Spec §27).
   Stature is a hidden -100..+100 slider (renegade .. hero). It is never shown
   as a number. It moves by diffing the lifetime event tallies the rest of the
   game already keeps (prog.ev_count, heat stars, missions, outpost, Poncho),
   so no other system needs a hook.
   Observable reactions (five):
     1. vendor prices  ±5% per tier (prog.price_k)
     2. heat           hero tiers shake cops faster (escape clock bonus)
     3. street barks   Meridian pedestrians comment as you pass
     4. the Herald     headline tone + the street-poll sidebar
     5. epilogue       the ending front page is written by your stature
   The Herald prints a front page for notable deeds; N opens the scrapbook.
   Honest scope: headlines are assembled from authored template tables
   (13 events x 3 tones x 2 variants), not free text; pages are 2D layouts.
   ══════════════════════════════════════════════════════════════════════════ */

enum { HT_EDITION = 0, HT_OUTPOST_QUIET, HT_OUTPOST_LOUD, HT_MAST, HT_ALARM, HT_WANTED,
       HT_MANHUNT, HT_MISSION, HT_BODYCOUNT, HT_PONCHO, HT_JOB, HT_FERRY, HT_ENDING, HT_OBIT, HT_SQUAD, HT_BOSS, HT_N };
static const char *k_boss_names[BOSS_N] = { "LA SARGENTO", "EL RELOJ", "BOSS 3", "BOSS 4", "BOSS 5", "BOSS 6" };
const char *game_boss_name(int who) { return (who >= 0 && who < BOSS_N) ? k_boss_names[who] : ""; }
static const char *k_cob_names[COB_N] = { "RASTRA", "VIDENTE", "PULPO", "LUCKY" };
const char *game_cob_name(int who) { return (who >= 0 && who < COB_N) ? k_cob_names[who] : ""; }

/* [template][tone 0 renegade / 1 neutral / 2 hero][variant] ; '#' = value */
static const char *k_hd_head[HT_N][3][2] = {
 { { "STRANGER WASHES ASHORE", "WHO IS THE DRIFTER?" },
   { "STRANGER WASHES ASHORE", "NEW FACE ON THE DOCKS" },
   { "STRANGER WASHES ASHORE", "A NEW ARRIVAL, A NEW HOPE?" } },
 { { "GHOST GUTS GARRISON", "OUTPOST FALLS WITHOUT A SHOT" },
   { "OUTPOST CHANGES HANDS QUIETLY", "SILENT NIGHT AT THE FORT" },
   { "SHADOW FREES THE FORT", "NOT ONE ALARM: FORT LIBERATED" } },
 { { "BLOODBATH AT THE OUTPOST", "FORT STORMED IN HAIL OF LEAD" },
   { "GUNFIGHT ENDS IN CAPTURE", "OUTPOST TAKEN BY FORCE" },
   { "FORT LIBERATED AFTER FIREFIGHT", "RESISTANCE ROUTS GARRISON" } },
 { { "PIRATE SIGNAL ON THE MAST", "VANDAL CLIMBS RADIO TOWER" },
   { "MAST BACK ON THE AIR", "TOWER RESYNCED" },
   { "ISLAND HEARS ITSELF AGAIN", "RADIO RETURNS TO THE PEOPLE" } },
 { { "SIRENS AS STRANGER CAUSES PANIC", "ALARM! ALARM! ALARM!" },
   { "ALARM RAISED AT GARRISON", "SIRENS OVER THE JUNGLE" },
   { "WHISTLE BLOWN ON GARRISON", "ALARM CAN'T SAVE THE OCCUPIERS" } },
 { { "CITY TERROR: # STARS", "POLICE HUNT RECKLESS DRIFTER" },
   { "PURSUIT DOWNTOWN", "# STAR ALERT ISSUED" },
   { "COPS CHASE LOCAL FAVOURITE", "\"LEAVE THEM BE\" SAY WITNESSES" } },
 { { "MANHUNT! CITY LOCKED DOWN", "FIVE STARS FOR PUBLIC ENEMY" },
   { "CITYWIDE MANHUNT UNDERWAY", "CHOPPERS OVER MERIDIAN" },
   { "CITY RALLIES AROUND FUGITIVE", "MANHUNT FOR THE PEOPLE'S HERO" } },
 { { "TROUBLE FOLLOWS STRANGER", "ANOTHER JOB, ANOTHER MESS" },
   { "CHAPTER # CLOSES", "THE STRANGER'S STORY GROWS" },
   { "STRANGER DELIVERS AGAIN", "PROMISE KEPT: CHAPTER #" } },
 { { "BODY COUNT HITS #", "# DEAD. WHO IS NEXT?" },
   { "# FALLEN SINCE THE ARRIVAL", "TOLL RISES TO #" },
   { "# OCCUPIERS FEWER", "THE TIDE TURNS: # DOWN" } },
 { { "STRAY DOG JOINS DRIFTER", "DOG AND DESPERADO" },
   { "DOG RESCUED FROM SNARE", "A STRANGER AND HIS DOG" },
   { "GOOD BOY SAVED!", "PONCHO FREE, ISLAND CHEERS" } },
 { { "SHADY WORK FOR SHADY STRANGER", "# JOBS AND COUNTING" },
   { "ODD JOBS PILE UP: #", "THE STRANGER WORKS HARD" },
   { "STRANGER LENDS A HAND - # TIMES", "WORKING FOR THE CITY" } },
 { { "TROUBLE TAKES THE FERRY", "BETO'S BOAT CARRIES A PROBLEM" },
   { "STRANGER CROSSES THE STRAIT", "FERRY ARRIVAL" },
   { "THE ISLAND'S HOPE COMES TO TOWN", "WELCOME ASHORE, FRIEND" } },
 { { "THE MENACE WALKS AWAY", "CITY EXHALES AS CHAOS ENDS" },
   { "IT IS OVER", "THE LAST PAGE TURNS" },
   { "A HORIZON UNDIVIDED", "HERO'S WORK IS DONE" } },
 { { "@ PUT DOWN LIKE A DOG", "BOUNTY HUNTER @ FOUND DEAD" },
   { "OBITUARY: @, HUNTER", "@ WILL HUNT NO MORE" },
   { "@ FALLS: HUNTER HUNTED", "THE STREETS BREATHE: @ IS GONE" } },
 { { "COBRADORES WIPED OUT", "WHO HUNTS THE HUNTER? THIS ONE." },
   { "LOS COBRADORES DISBANDED", "ALL # HUNTERS FALLEN" },
   { "BOUNTY SQUAD BROKEN", "# HUNTERS, ONE STRANGER" } },
 { { "& SILENCED: DRUMS GO QUIET", "STRANGER BREAKS &'S UNIT" },
   { "& DEFEATED IN THE YARD", "THE DRUMS STOP FOR &" },
   { "& FALLS, UNIT SCATTERS", "LIBERATOR ENDS &'S DRILL" } },
};
static const char *k_hd_sub[3] = {
    "Residents lock doors as the drifter's legend darkens.",
    "Nobody can yet say whose side the stranger is on.",
    "Street murals of the stranger are appearing overnight." };
static const char *k_hd_body[3][4] = {
 { "Shopkeepers report the stranger pays late and leaves early.",
   "A fisherman says he no longer sails at dusk.",
   "Officials urge calm and promise more patrols.",
   "Some say the chaos was deserved. Most say nothing at all." },
 { "Witnesses gave conflicting accounts.",
   "The Herald could not reach the stranger for comment.",
   "Market talk is of little else this week.",
   "Whether this is good news depends on who you ask." },
 { "Children play 'stranger and dog' in the streets.",
   "Vendors say the stranger never pays full price anymore.",
   "A grandmother left fruit on the safehouse step.",
   "The garrison is said to be nervous. Good." } };
static const char *k_bark[5][3] = {
 { "\"Keep walking, menace.\"", "\"That's the one from the paper...\"", "\"Don't look at them. Just don't.\"" },
 { "\"Trouble's back.\"", "\"You again? Great.\"", "\"Nobody wants a scene.\"" },
 { "\"Nice day, huh?\"", "\"Watch the curb.\"", "\"Heard you're new.\"" },
 { "\"Hey, I read about you!\"", "\"Keep it up.\"", "\"Thanks for the tower.\"" },
 { "\"It's really you! A hero!\"", "\"Coffee's on me, friend.\"", "\"The island owes you.\"" } };

int game_stature_tier(const Game *g) {
    float s = g->leg.stature;
    return s >= 60.f ? 2 : s >= 20.f ? 1 : s > -20.f ? 0 : s > -60.f ? -1 : -2;
}
const char *game_stature_name(const Game *g) {
    static const char *n[5] = { "PUBLIC MENACE", "LOOSE CANNON", "UNKNOWN QUANTITY", "LOCAL HOPE", "FOLK HERO" };
    return n[game_stature_tier(g) + 2];
}
void game_stature_add(Game *g, float d) { g->leg.stature = dh_clampf(g->leg.stature + d, -100.f, 100.f); }
static int stature_tone(const Game *g) { int t = game_stature_tier(g); return t < 0 ? 0 : t > 0 ? 2 : 1; }

int game_herald_print(Game *g, int tmpl, int val) {
    Legend *L = &g->leg;
    if (tmpl < 0 || tmpl >= HT_N) return -1;
    if (L->n == HERALD_MAX) { memmove(L->pages, L->pages + 1, sizeof(HeraldPage) * (HERALD_MAX - 1)); L->n--; }
    HeraldPage *p = &L->pages[L->n];
    p->tmpl = tmpl; p->tone = stature_tone(g); p->day = L->day + 1; p->val = val; p->act = g->act;
    uint32_t hs = (uint32_t)(L->printed + 1) * 2654435761u ^ (uint32_t)tmpl * 40503u ^ (uint32_t)(g->time * 97.f) ^ g->seed;
    hs ^= hs >> 15; hs *= 2246822519u; hs ^= hs >> 13;
    p->seed = hs;
    L->printed++;
    L->banner_t = 4.5f;
    return L->n++;
}

static void hd_fill(char *out, int cap, const char *src, int val) {
    int o = 0;
    for (const char *c = src; *c && o < cap - 12; c++) {
        if (*c == '#') o += snprintf(out + o, (size_t)(cap - o), "%d", val);
        else if (*c == '@') o += snprintf(out + o, (size_t)(cap - o), "%s", game_cob_name(val));
        else if (*c == '&') o += snprintf(out + o, (size_t)(cap - o), "%s", game_boss_name(val));
        else out[o++] = *c;
    }
    out[o] = 0;
}
int game_herald_headline(const Game *g, int page, char *out, int cap) {
    if (page < 0 || page >= g->leg.n || cap < 16) { if (cap) out[0] = 0; return 0; }
    const HeraldPage *p = &g->leg.pages[page];
    hd_fill(out, cap, k_hd_head[p->tmpl][p->tone][p->seed & 1], p->val);
    return (int)strlen(out);
}

/* ── frame: diff tallies → stature deltas + editions; drive the reactions ── */
static void legend_frame(Game *g, float dt) {
    Legend *L = &g->leg;
    Progress *pr = &g->prog;
    int stars = g->act == 1 ? g->city.heat.stars : 0;
    int done = 0;
    for (int i = 0; i < 31; i++) if (g->ms.done_mask & (1 << i)) done++;
    int dogfree = g->dog.state != DOG_TRAPPED;
    if (g->day_t + 0.5f < L->day_prev) L->day++;
    L->day_prev = g->day_t;
    if (!L->primed) {
        for (int i = 0; i < EV_N && i < 16; i++) L->prev_ev[i] = pr->ev_count[i];
        L->prev_stars = stars; L->prev_done = done; L->prev_outpost = pr->outpost_captured;
        L->prev_dog = dogfree; L->prev_ending = g->ms.ending; L->prev_act = g->act; L->prev_kills = g->kills;
        L->primed = 1;
        if (L->n == 0) game_herald_print(g, HT_EDITION, 0);
        L->banner_t = 0.f;
        return;
    }
    static const float DS[EV_N] = { -0.6f, 0.f, 0.3f, 2.f, 6.f, 3.f, 1.5f, -0.3f, 0.f, -0.15f, -3.f };
    int dev[EV_N];
    for (int i = 0; i < EV_N; i++) {
        dev[i] = pr->ev_count[i] - L->prev_ev[i];
        if (dev[i] > 0) game_stature_add(g, DS[i] * (float)dev[i]);
        L->prev_ev[i] = pr->ev_count[i];
    }
    if (dev[EV_OUTPOST_STEALTH] > 0) game_herald_print(g, HT_OUTPOST_QUIET, 0);
    else if (dev[EV_OUTPOST_LOUD] > 0) game_herald_print(g, HT_OUTPOST_LOUD, 0);
    if (dev[EV_MAST] > 0) game_herald_print(g, HT_MAST, 0);
    if (dev[EV_ALARM] > 0) game_herald_print(g, HT_ALARM, 0);
    if (dev[EV_JOB] > 0 && pr->ev_count[EV_JOB] % 3 == 0) game_herald_print(g, HT_JOB, pr->ev_count[EV_JOB]);
    if (stars > L->prev_stars) {
        game_stature_add(g, -2.f * (float)(stars - L->prev_stars));
        if (stars >= 5) game_herald_print(g, HT_MANHUNT, stars);
        else if (stars >= 3 && L->prev_stars < 3) game_herald_print(g, HT_WANTED, stars);
    }
    L->prev_stars = stars;
    static const int KM[] = { 10, 25, 50, 100, 200, 400 };
    for (int i = 0; i < 6; i++)
        if (L->prev_kills < KM[i] && g->kills >= KM[i]) game_herald_print(g, HT_BODYCOUNT, KM[i]);
    L->prev_kills = g->kills;
    if (done > L->prev_done) { game_stature_add(g, 4.f); game_herald_print(g, HT_MISSION, done); }
    L->prev_done = done;
    if (dogfree && !L->prev_dog) { game_stature_add(g, 3.f); game_herald_print(g, HT_PONCHO, 0); }
    L->prev_dog = dogfree;
    if (g->act == 1 && L->prev_act == 0) game_herald_print(g, HT_FERRY, 0);
    L->prev_act = g->act;
    if (g->ms.ending && !L->prev_ending) game_herald_print(g, HT_ENDING, g->ms.ending);
    L->prev_ending = g->ms.ending;
    L->prev_outpost = pr->outpost_captured;

    /* reactions */
    int tier = game_stature_tier(g);
    pr->price_k = 1.f - 0.05f * (float)tier;                       /* 1 */
    if (g->act == 1 && tier > 0 && g->city.heat.searching)         /* 2 */
        g->city.heat.escape_t += dt * 0.25f * (float)tier;
    if (L->banner_t > 0.f) L->banner_t -= dt;
    if (L->bark_cd > 0.f) L->bark_cd -= dt;
    else if (g->act == 1 && g->in_vehicle < 0 && g->message_t <= 0.f) {  /* 3 */
        for (int i = 0; i < g->city.ped_count; i++) {
            const Ped *pd = &g->city.peds[i];
            float dx = pd->pos.x - g->player.pos.x, dz = pd->pos.z - g->player.pos.z;
            if (pd->state != 0 || dx * dx + dz * dz > 3.5f * 3.5f) continue;
            game_message(g, "%s", k_bark[tier + 2][(unsigned)(g->time * 7.f + i) % 3]);
            g->message_t = 2.2f;
            L->bark_cd = 20.f;
            break;
        }
    }
}

static void herald_input(Game *g, const PlatInput *in) {
    Legend *L = &g->leg;
    uint32_t pr = in->pressed;
    if (pr & (BTN_HERALD | BTN_MENU | BTN_PAUSE)) { g->ui = UI_NONE; return; }
    if (L->n <= 0) return;
    if (pr & (BTN_LEFT | BTN_UP))    L->view = L->view > 0 ? L->view - 1 : 0;
    if (pr & (BTN_RIGHT | BTN_DOWN)) L->view = L->view < L->n - 1 ? L->view + 1 : L->n - 1;
}

static void herald_draw_banner(Game *g) {
    const Legend *L = &g->leg;
    if (L->banner_t <= 0.f || L->n <= 0 || g->ui != UI_NONE) return;
    float ui = settings()->ui_scale, fs = 2.f * ui;
    char h[96]; game_herald_headline(g, L->n - 1, h, sizeof h);
    float a = L->banner_t > 0.5f ? 1.f : L->banner_t * 2.f;
    uint32_t al = (uint32_t)(a * 235.f) << 24;
    float w = font_width(h, fs * 1.1f) + 40.f * ui; if (w < 260.f * ui) w = 260.f * ui;
    float x = g->w * 0.5f - w * 0.5f, y = 54.f * ui;
    rend_quad2d(x, y, w, 44.f * ui, -1, 0,0,1,1, al | 0x00D8EEF4u);
    rend_quad2d(x, y + 13.f * ui, w, 1.f * ui, -1, 0,0,1,1, al | 0x00202020u);
    font_text_center(g->w * 0.5f, y + 3.f * ui, fs * 0.7f, "THE HERALD  -  EXTRA!   [N] READ", al | 0x00302828u);
    font_text_center(g->w * 0.5f, y + 20.f * ui, fs * 1.1f, h, al | 0x00181818u);
}

/* The atlas space cell bleeds a faint baseline on light paper, so ink text
   is drawn word by word. */
static void ink_text(float x, float y, float sc, const char *s, uint32_t col) {
    char w[96]; float sp = font_width(" ", sc);
    while (*s) {
        int n = 0;
        while (*s == ' ') { x += sp; s++; }
        while (*s && *s != ' ' && n < 95) w[n++] = *s++;
        w[n] = 0;
        if (n) { font_text(x, y, sc, w, col); x += font_width(w, sc); }
    }
}
static void ink_center(float cx, float y, float sc, const char *s, uint32_t col) {
    ink_text(cx - font_width(s, sc) * 0.5f, y, sc, s, col);
}

/* word-wrap into the column; returns y after the last line */
static float hd_wrap(float x, float y, float wmax, float sc, const char *txt, uint32_t col, int center) {
    char line[224] = {0}, word[64]; int li = 0; const char *c = txt; float lh = sc * 9.5f;
    while (1) {
        int wi = 0;
        while (*c == ' ') c++;
        while (*c && *c != ' ' && wi < 63) word[wi++] = *c++;
        word[wi] = 0;
        char trial[300];
        snprintf(trial, sizeof trial, li ? "%s %s" : "%s%s", line, word);
        if (wi && li && font_width(trial, sc) > wmax) {
            if (center) ink_center(x + wmax * 0.5f, y, sc, line, col); else ink_text(x, y, sc, line, col);
            y += lh; snprintf(line, sizeof line, "%s", word); li = wi;
        } else if (wi) { memcpy(line, trial, sizeof line - 1); line[sizeof line - 1] = 0; li = (int)strlen(line); }
        if (!*c) break;
    }
    if (li) { if (center) ink_center(x + wmax * 0.5f, y, sc, line, col); else ink_text(x, y, sc, line, col); y += lh; }
    return y;
}

static void herald_draw_screen(Game *g) {
    const Legend *L = &g->leg;
    float ui = settings()->ui_scale, fs = 2.f * ui;
    const uint32_t PAPER = 0xF6D8EEF4u, INK = 0xFF181818u, RULE = 0xFF303030u;
    float pw = g->w * 0.62f, ph = g->h * 0.84f;
    if (pw < 420.f * ui) pw = g->w * 0.92f;
    float x0 = g->w * 0.5f - pw * 0.5f, y0 = g->h * 0.08f, pad = 14.f * ui;
    rend_quad2d(0, 0, (float)g->w, (float)g->h, -1, 0,0,1,1, 0xA0000000u);
    rend_quad2d(x0 + 5.f * ui, y0 + 5.f * ui, pw, ph, -1, 0,0,1,1, 0x70000000u);
    rend_quad2d(x0, y0, pw, ph, -1, 0,0,1,1, PAPER);
    if (L->n <= 0) {
        ink_center(g->w * 0.5f, y0 + ph * 0.45f, fs, "NO EDITIONS YET - MAKE SOME NEWS.", INK);
        return;
    }
    const HeraldPage *p = &L->pages[L->view];
    float cx = g->w * 0.5f, y = y0 + pad;
    ink_center(cx, y, fs * 2.0f, p->act ? "THE MERIDIAN HERALD" : "THE SOMBRA HERALD", INK);
    y += fs * 20.f;
    rend_quad2d(x0 + pad, y, pw - pad * 2, 2.f * ui, -1, 0,0,1,1, RULE);
    char buf[160];
    snprintf(buf, sizeof buf, "DAY %d   -   EDITION %d OF %d   -   25 CENTS   -   ALL THE NEWS THAT FITS", p->day, L->view + 1, L->n);
    ink_center(cx, y + 5.f * ui, fs * 0.6f, buf, RULE);
    y += 14.f * ui;
    rend_quad2d(x0 + pad, y, pw - pad * 2, 1.f * ui, -1, 0,0,1,1, RULE);
    y += 10.f * ui;
    char h[96]; game_herald_headline(g, L->view, h, sizeof h);
    y = hd_wrap(x0 + pad, y, pw - pad * 2, fs * 1.7f, h, INK, 1) + 4.f * ui;
    ink_center(cx, y, fs * 0.8f, k_hd_sub[p->tone], RULE);
    y += 18.f * ui;
    /* woodcut-style picture box: sky band, horizon, sun/moon by tone */
    float colw = (pw - pad * 3) * 0.5f, bx = x0 + pad, bh = ph * 0.26f;
    static const uint32_t SKY[3] = { 0xFF3A3A6Au, 0xFF8A8A8Au, 0xFF90C8E8u };
    rend_quad2d(bx, y, colw, bh, -1, 0,0,1,1, SKY[p->tone]);
    rend_quad2d(bx, y + bh * 0.62f, colw, bh * 0.38f, -1, 0,0,1,1, p->act ? 0xFF505050u : 0xFF3C6A30u);
    for (int i = 0; i < 6; i++) {
        uint32_t r = p->seed >> (i * 5);
        float bw2 = colw * (0.06f + (r & 3) * 0.02f), bh2 = bh * (p->act ? 0.25f + ((r >> 2) & 7) * 0.05f : 0.08f + (r & 3) * 0.03f);
        rend_quad2d(bx + colw * (0.05f + i * 0.155f), y + bh * 0.62f - bh2, bw2, bh2, -1, 0,0,1,1, 0xFF282828u);
    }
    rend_quad2d(bx + colw * 0.72f, y + bh * 0.14f, colw * 0.12f, colw * 0.12f, -1, 0,0,1,1, p->tone == 0 ? 0xFF3030C0u : 0xFF60E0F8u);
    ink_text(bx + 3.f * ui, y + bh + 3.f * ui, fs * 0.55f, "ARTIST'S IMPRESSION", RULE);
    /* body column */
    float by = y, tx = bx + colw + pad;
    for (int i = 0, skip = (int)((p->seed >> 8) % 4); i < 4; i++)
        if (i != skip) by = hd_wrap(tx, by, colw, fs * 0.72f, k_hd_body[p->tone][i], INK, 0) + 4.f * ui;
    /* street poll sidebar (reaction 4: the only in-world read of stature) */
    int pct = (int)dh_clampf(50.f + g->leg.stature * 0.47f, 3.f, 97.f);
    tx = bx; by = y + bh + 14.f * ui;
    rend_quad2d(tx, by, colw, 1.f * ui, -1, 0,0,1,1, RULE);
    snprintf(buf, sizeof buf, "STREET POLL: %d%% say hero", pct);
    ink_text(tx, by + 4.f * ui, fs * 0.72f, buf, INK);
    by += 16.f * ui;
    rend_quad2d(tx, by + 2.f * ui, colw, 6.f * ui, -1, 0,0,1,1, 0xFF9090A0u);
    rend_quad2d(tx, by + 2.f * ui, colw * pct / 100.f, 6.f * ui, -1, 0,0,1,1, INK);
    ink_center(cx, y0 + ph - 16.f * ui, fs * 0.7f, "LEFT/RIGHT TURN PAGE     N / ESC CLOSE", RULE);
}

static void legend_save(Game *g, SaveGame *s) {
    Legend *L = &g->leg;
    save_set_flag(s, "leg_st", (int)(L->stature * 10.f));
    save_set_flag(s, "leg_day", L->day);
    save_set_flag(s, "hd_n", L->n);
    for (int i = 0; i < L->n; i++) {
        const HeraldPage *p = &L->pages[i];
        char k[16];
        snprintf(k, sizeof k, "hdT%d", i); save_set_flag(s, k, p->tmpl | p->tone << 8 | p->act << 10 | p->day << 12);
        snprintf(k, sizeof k, "hdV%d", i); save_set_flag(s, k, p->val);
        snprintf(k, sizeof k, "hdS%d", i); save_set_flag(s, k, (int)p->seed);
    }
}
static void legend_load(Game *g, const SaveGame *s) {
    Legend *L = &g->leg;
    memset(L, 0, sizeof *L);
    L->stature = dh_clampf(save_get_flag(s, "leg_st", 0) / 10.f, -100.f, 100.f);
    L->day = save_get_flag(s, "leg_day", 0);
    int n = save_get_flag(s, "hd_n", 0);
    if (n < 0) n = 0;
    if (n > HERALD_MAX) n = HERALD_MAX;
    for (int i = 0; i < n; i++) {
        HeraldPage *p = &L->pages[i];
        char k[16];
        snprintf(k, sizeof k, "hdT%d", i); int t = save_get_flag(s, k, 0);
        p->tmpl = t & 0xFF; p->tone = (t >> 8) & 3; p->act = (t >> 10) & 1; p->day = t >> 12;
        if (p->tmpl >= HT_N) p->tmpl = 0;
        if (p->tone > 2) p->tone = 1;
        snprintf(k, sizeof k, "hdV%d", i); p->val = save_get_flag(s, k, 0);
        snprintf(k, sizeof k, "hdS%d", i); p->seed = (uint32_t)save_get_flag(s, k, 0);
    }
    L->n = n; L->view = n - 1;
    L->day_prev = g->day_t;   /* primed = 0: next frame re-baselines tallies without re-printing */
}
