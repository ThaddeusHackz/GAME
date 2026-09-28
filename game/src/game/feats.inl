/* feats.inl — M12: the Feats poster wall (Spec 53).
   24 feats (2 secret), each a predicate over live game state polled 2x/sec.
   Stamping plays a wax-seal toast (scale-in, no flash — photosensitive safe),
   grants 50 XP, and persists as a bitmask save flag "feats".
   Included from game.c after herald.inl. */

typedef int (*FeatFn)(Game *g);

static int ft_bond5(Game *g)     { return dog_bond_level(&g->dog) >= 5; }
static int ft_freed(Game *g)     { return g->dog.state != DOG_TRAPPED; }
static int ft_ghost(Game *g)     { return g->prog.ev_count[EV_OUTPOST_STEALTH] >= 1; }
static int ft_loud(Game *g)      { return g->prog.ev_count[EV_OUTPOST_LOUD] >= 1; }
static int ft_mast(Game *g)      { return g->prog.ev_count[EV_MAST] >= 1; }
static int ft_head(Game *g)      { return g->prog.ev_count[EV_HEADSHOT] >= 10; }
static int ft_takedown(Game *g)  { return g->prog.ev_count[EV_TAKEDOWN] >= 5; }
static int ft_hunt(Game *g)      { return g->prog.ev_count[EV_HUNT] >= 3; }
static int ft_craft(Game *g)     { return g->prog.ev_count[EV_CRAFT] >= 3; }
static int ft_jobs(Game *g)      { return g->prog.ev_count[EV_JOB] >= 3; }
static int ft_5stars(Game *g)    { return g->act == 1 && g->city.heat.stars >= 5; }
static int ft_getaway(Game *g)   { return g->feats.peak_stars < 0; }   /* set by feats_frame */
static int ft_city(Game *g)      { return g->act == 1; }
static int ft_photo(Game *g)     { return g->ph_shots >= 1; }
static int ft_herald(Game *g)    { return g->leg.n >= 10; }
static int ft_hero(Game *g)      { return game_stature_tier(g) >= 2; }
static int ft_menace(Game *g)    { return game_stature_tier(g) <= -2; }
static int ft_pets(Game *g)      { return g->dog.pets >= 10; }
static int ft_revive(Game *g)    { return g->dog.revives >= 1; }
static int ft_level(Game *g)     { return g->prog.level >= 10; }
static int ft_chapters(Game *g)  { int n = 0; for (uint32_t m = (uint32_t)g->ms.done_mask; m; m &= m - 1) n++; return n >= 6; }
static int ft_ending(Game *g)    { return g->ms.ending != 0; }
static int ft_cob1(Game *g)      { return g->cob.enc[0] + g->cob.enc[1] + g->cob.enc[2] + g->cob.enc[3] >= 1; }
static int ft_cob4(Game *g)      { return g->cob.trinket; }
static int ft_boss1(Game *g)     { return g->boss.clean_mask & 1; }
static int ft_boss2(Game *g)     { return (g->boss.clean_mask >> 1) & 1; }
static int ft_s_distract(Game *g){ return g->dog.distracts >= 5; }
static int ft_s_menacedog(Game *g){ return ft_menace(g) && ft_bond5(g); }

static const struct { const char *name, *desc; FeatFn fn; int secret; } k_feats[] = {
    { "FIRST FRIEND",       "Free Poncho from his cage.",                 ft_freed, 0 },
    { "GOOD DOG",           "Reach bond level 5 with Poncho.",            ft_bond5, 0 },
    { "EAR SCRATCHES",      "Pet Poncho 10 times.",                       ft_pets, 0 },
    { "NOBODY GETS LEFT",   "Revive Poncho after he goes down.",          ft_revive, 0 },
    { "GHOST",              "Capture an outpost undetected.",             ft_ghost, 0 },
    { "FIESTA",             "Capture an outpost the loud way.",           ft_loud, 0 },
    { "SIGNAL BOOST",       "Sync a radio mast.",                         ft_mast, 0 },
    { "SHARPSHOOTER",       "Land 10 headshots.",                         ft_head, 0 },
    { "QUIET HANDS",        "Perform 5 stealth takedowns.",               ft_takedown, 0 },
    { "PROVIDER",           "Complete 3 hunts.",                          ft_hunt, 0 },
    { "BENCH WORK",         "Craft 3 items.",                             ft_craft, 0 },
    { "ODD JOBBER",         "Finish 3 side jobs.",                        ft_jobs, 0 },
    { "CROSSING",           "Take the ferry to Meridian City.",           ft_city, 0 },
    { "MOST WANTED",        "Reach five stars in Meridian.",              ft_5stars, 0 },
    { "CLEAN GETAWAY",      "Lose the police from three stars or more.",  ft_getaway, 0 },
    { "SAY CHEESE",         "Take a photo in photo mode.",                ft_photo, 0 },
    { "EXTRA! EXTRA!",      "Make the Herald print 10 editions.",         ft_herald, 0 },
    { "FOLK HERO",          "Reach the Folk Hero stature tier.",          ft_hero, 0 },
    { "PUBLIC MENACE",      "Sink to the Menace stature tier.",           ft_menace, 0 },
    { "SEASONED",           "Reach level 10.",                            ft_level, 0 },
    { "CHAPTER AND VERSE",  "Complete 6 story missions.",                 ft_chapters, 0 },
    { "THE LAST PAGE",      "See an ending.",                             ft_ending, 0 },
    { "COLLECTION REFUSED",  "Send a Cobrador bounty hunter packing.",     ft_cob1, 0 },
    { "LUCKY'S COIN",        "Break Los Cobradores for good.",             ft_cob4, 0 },
    { "FORMAR",              "Beat La Sargento without dropping below half health.", ft_boss1, 0 },
    { "SINCRONIZADO",        "Beat El Reloj without a single bomb going off.", ft_boss2, 0 },
    { "THE DOG JUDGES YOU", "Send Poncho to distract guards 5 times.",    ft_s_distract, 1 },
    { "WHO'S A GOOD MENACE","Be a Menace whose dog still adores them.",   ft_s_menacedog, 1 },
};
#define FEAT_N ((int)(sizeof k_feats / sizeof k_feats[0]))

int game_feat_count(void) { return FEAT_N; }
const char *game_feat_name(int i) { return (i >= 0 && i < FEAT_N) ? k_feats[i].name : ""; }
int game_feat_secret(int i) { return (i >= 0 && i < FEAT_N) ? k_feats[i].secret : 0; }
int game_feats_stamped(const Game *g) {
    int n = 0; for (uint32_t m = g->feats.got; m; m &= m - 1) n++; return n;
}
int game_feat_unlock(Game *g, int i) {
    if (i < 0 || i >= FEAT_N || (g->feats.got & (1u << i))) return 0;
    g->feats.got |= 1u << i;
    g->feats.last = i; g->feats.stamp_t = 4.0f;
    prog_add_xp(&g->prog, 50);
    pol_cue(g, SFX_CARD, 0.7f, 0.9f);
    return 1;
}

static void feats_frame(Game *g, float dt) {
    Feats *F = &g->feats;
    if (F->stamp_t > 0.f) F->stamp_t -= dt;
    /* clean-getaway tracker: remember the peak, fire when heat clears */
    if (g->act == 1) {
        int s = g->city.heat.stars;
        if (s > F->peak_stars && F->peak_stars >= 0) F->peak_stars = s;
        if (s == 0 && F->peak_stars >= 3) F->peak_stars = -1;
    }
    F->check_acc += dt;
    if (F->check_acc < 0.5f) return;
    F->check_acc = 0.f;
    for (int i = 0; i < FEAT_N; i++)
        if (!(F->got & (1u << i)) && k_feats[i].fn(g)) { game_feat_unlock(g, i); break; } /* one per tick → toasts don't stack */
    if (F->peak_stars < 0 && (F->got & (1u << 14))) F->peak_stars = 0;
}

static void feats_input(Game *g, const PlatInput *in) {
    uint32_t pr = in->pressed;
    if (pr & (BTN_FEATS | BTN_MENU | BTN_PAUSE)) { g->ui = UI_NONE; return; }
    if (pr & BTN_LEFT)  g->ui_sel = (g->ui_sel + FEAT_N - 1) % FEAT_N;
    if (pr & BTN_RIGHT) g->ui_sel = (g->ui_sel + 1) % FEAT_N;
    if (pr & BTN_UP)    g->ui_sel = (g->ui_sel + FEAT_N - 4) % FEAT_N;
    if (pr & BTN_DOWN)  g->ui_sel = (g->ui_sel + 4) % FEAT_N;
}

static void wax_seal(float cx, float cy, float r, uint32_t a) {
    /* layered squares approximating a round blob of red wax */
    for (int k = 0; k < 4; k++) {
        float rr = r * (1.f - k * 0.18f), w = rr * (k & 1 ? 1.6f : 2.f), h = rr * (k & 1 ? 2.f : 1.6f);
        uint32_t c = k == 3 ? 0x002838A8u : (k == 2 ? 0x001E2A90u : 0x00202C98u);
        rend_quad2d(cx - w * 0.5f, cy - h * 0.5f, w, h, -1, 0,0,1,1, a | c);
    }
}

static void feats_draw_toast(Game *g) {
    const Feats *F = &g->feats;
    if (F->stamp_t <= 0.f || g->ui != UI_NONE) return;
    float ui = settings()->ui_scale, fs = 2.f * ui;
    float t = 4.0f - F->stamp_t;
    float a = F->stamp_t > 0.5f ? 1.f : F->stamp_t * 2.f;
    uint32_t al = (uint32_t)(a * 235.f) << 24;
    float w = 300.f * ui, h = 46.f * ui, x = g->w - w - 16.f * ui, y = g->h * 0.30f;
    rend_quad2d(x, y, w, h, -1, 0,0,1,1, al | 0x00A8D4E8u);
    float sc = t < 0.25f ? 2.2f - t * 4.8f : 1.f;        /* seal slams down */
    wax_seal(x + 24.f * ui, y + h * 0.5f, 15.f * ui * sc, al);
    font_text(x + 46.f * ui, y + 6.f * ui, fs * 0.6f, "FEAT STAMPED  +50 XP   [J] WALL", al | 0x00303838u);
    ink_text(x + 46.f * ui, y + 20.f * ui, fs * 0.95f, k_feats[F->last].name, al | 0x00181818u);
}

static void feats_draw_screen(Game *g) {
    const Feats *F = &g->feats;
    float ui = settings()->ui_scale, fs = 2.f * ui;
    rend_quad2d(0, 0, (float)g->w, (float)g->h, -1, 0,0,1,1, 0xB0000000u);
    float pw = g->w * 0.86f, ph = g->h * 0.88f, x0 = (g->w - pw) * 0.5f, y0 = g->h * 0.06f;
    rend_quad2d(x0, y0, pw, ph, -1, 0,0,1,1, 0xFF3C5068u);          /* cork board */
    char buf[96];
    snprintf(buf, sizeof buf, "FEATS  -  %d / %d STAMPED", game_feats_stamped(g), FEAT_N);
    ink_center(g->w * 0.5f, y0 + 8.f * ui, fs * 1.3f, buf, 0xFF181818u);
    int cols = 4, rows = (FEAT_N + cols - 1) / cols;
    float gx = x0 + 10.f * ui, gy = y0 + 34.f * ui, gw = (pw - 20.f * ui) / cols, gh = (ph - 60.f * ui) / rows;
    for (int i = 0; i < FEAT_N; i++) {
        int got = (F->got >> i) & 1, hide = k_feats[i].secret && !got;
        float cx = gx + (i % cols) * gw, cy = gy + (i / cols) * gh;
        uint32_t card = got ? 0xFFD8EEF4u : 0xFF687880u;
        if (i == g->ui_sel) rend_quad2d(cx + 1.f * ui, cy + 1.f * ui, gw - 2.f * ui, gh - 2.f * ui, -1, 0,0,1,1, 0xFF30C8F0u);
        rend_quad2d(cx + 3.f * ui, cy + 3.f * ui, gw - 6.f * ui, gh - 6.f * ui, -1, 0,0,1,1, card);
        uint32_t ink = got ? 0xFF181818u : 0xFF505860u;
        ink_text(cx + 8.f * ui, cy + 7.f * ui, fs * 0.72f, hide ? "? ? ?" : k_feats[i].name, ink);
        hd_wrap(cx + 8.f * ui, cy + 7.f * ui + fs * 9.f, gw - 16.f * ui - (got ? 30.f * ui : 0), fs * 0.5f,
                hide ? "A secret. Keep playing." : k_feats[i].desc, ink, 0);
        if (got) wax_seal(cx + gw - 22.f * ui, cy + gh - 20.f * ui, 12.f * ui, 0xFF000000u);
    }
    font_text_center(g->w * 0.5f, y0 + ph - 18.f * ui, fs * 0.6f, "ARROWS SELECT   -   J / ESC CLOSE", 0xFF202020u);
}
