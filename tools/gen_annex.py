#!/usr/bin/env python3
"""gen_annex.py — PART IX machine matrices for MASTER_PROMPT.md.

Seeds below are hand-authored (the design intent). Matrices are combinatorial
expansions — the same way real QA test matrices, i18n key tables, and PCG
template banks are produced in studio pipelines. Deterministic; no RNG.

Regenerate:  python3 tools/gen_annex.py >> MASTER_PROMPT.md
"""
import sys

PLATFORMS = ["UHD620", "VEGA8", "GTX750TI", "GTX1060", "RX580", "DECK"]
PROFILES = ["STOCK", "PHOTOSENS", "GAMEPAD", "UI150"]
PROF_NOTE = {
    "STOCK": "60fps target held",
    "PHOTOSENS": "zero strobe/shake (33)",
    "GAMEPAD": "glyphs correct",
    "UI150": "no overflow/clipping",
}
SYSTEMS = [
    ("BOOT", "cold boot > menu", "boot <20s; splash skippable"),
    ("SAFE_MODE", "boot --safe-mode", "GL fallback renders; menu alive"),
    ("MENU", "navigate all tabs", "micro-transitions (50.3); no NaN"),
    ("NEWGAME", "new game > Act I", "cold open plays; skip works"),
    ("SAVE", "save 3 slots mid-run", "json schema=1 written; buyback intact"),
    ("LOAD", "load each slot", "position/inv/world restored (18.7)"),
    ("AUTOSAVE", "trigger 5 autosaves", "rolling-5 (37.4); no overwrite of manual"),
    ("SETTINGS_VIDEO", "cycle all presets", "live switch; no crash (18.9)"),
    ("SETTINGS_AUDIO", "move 4 sliders", "duck rules hold (32)"),
    ("SETTINGS_INPUT", "invert+sen sliders", "affects camera; persists"),
    ("REBIND", "rebind 5 keys", "conflict warn; persists; hold/toggle ok"),
    ("PAUSE", "pause in combat/cutscene", "world audio ducks; sim frozen"),
    ("HUD", "damage+heat+tag states", "paw+eye read at 800p"),
    ("SUBTITLES", "max size+bg", "readable over white AND black frames"),
    ("PHOTOSENS", "photosens=ON full play", "grep-safe FX path (18.9b)"),
    ("GAMEPAD", "menus+gameplay pad-only", "auto-glyph swap on device change"),
    ("UI_SCALE", "150% full session", "no clipping anywhere (33)"),
    ("MOVEMENT", "walk/sprint/slide 5min", "coyote 120ms; no jitter"),
    ("MANTLE", "vault course M1", "no stuck states; <=30s run"),
    ("SWIM", "dive+surface+exit", "breath meter honest"),
    ("ZIPLINE", "ride 4 lines", "verlet sway (48); exit safe"),
    ("COMBAT_FIRE", "fire all 16 weapons", "hit-stop per 44.2 table"),
    ("COMBAT_RELOAD", "reload/empty all", "cancelable where spec'd"),
    ("THROWABLE", "throw T01-T05", "fuse/falloff per S68"),
    ("MELEE", "swing M01-M03", "takedown links; bonk sfx pitched"),
    ("TAKEDOWN", "front/back/ledge/water", "anim set swaps clean"),
    ("STEALTH_TAG", "tag x4 concurrent", "60s; S05 upgrade to 90s"),
    ("DETECTION", "meter 0->alert->combat", "LKP honest (S82)"),
    ("AI_PATROL", "watch 10min patrol", "phones/idle variety; no moonwalk"),
    ("AI_COMBAT", "fight squad of 5", "cover+flank+grenade flush at 3+"),
    ("REINFORCE", "loud alarm twice", "2-wave cap (S82); garrison routes"),
    ("WANTED", "0->5->0 full cycle", "ladder 6.5; escape cone honest"),
    ("VEHICLE_ENTER", "enter/exit/carjack 12", "states clean; civ pull-out ok"),
    ("VEHICLE_DRIVE", "soak 15min all roads", "no fall-through (18.4)"),
    ("VEHICLE_DAMAGE", "smoke->fire->explode", "thresholds per S69"),
    ("VEHICLE_RADIO", "3 stations + OFF", "volume persists per class"),
    ("TRAFFIC", "soak 15min junction", "gap rule holds; panic scatters"),
    ("PEDS", "fire weapon in crowd", "panic->doors; no T-pose"),
    ("DOG_FOLLOW", "roam 20min with Poncho", "teleport-safety <=8s (24)"),
    ("DOG_COMMAND", "all 5 commands", "distract pulls 1 enemy SUSPICIOUS"),
    ("DOG_REVIVE", "down Poncho 3x", "3s revive or 30s auto; never dies"),
    ("FISHING", "catch 3 species", "rod anims; journal logs; Poncho reacts"),
    ("ECONOMY_SHOP", "buy/sell/buyback", "55% sell; 60s buyback (72)"),
    ("SKILLS", "buy all 24 ranks", "effects apply; pre-req enforced (71)"),
    ("CRAFTING", "craft every recipe", "yields per V2; G06 doubles"),
    ("MISSIONS", "run M01-M11 smoke", "CP restore; fail-forward per beats"),
    ("CONTRACTS", "generate 12 archetypes", "daily seed stable (S82)"),
    ("PHOTO_MODE", "shoot+export card", "sim paused; PNG lands (28)"),
    ("NEWGAMEPLUS", "boot NG+ save", "carry-over; Sereno line fires (29.1)"),
    ("MUTATORS", "combo 3 mutators", "save flags; HUD warns (29.2)"),
    ("BENCHMARK", "run 3 scenes", "report emitted (37.1)"),
    ("FEEDBACK", "Send a Thought", "feedback.json + consent gate (59.1)"),
    ("MODS_LOAD", "load 3 sample mods", "data-only; disable-all works (36)"),
    ("I18N_SWITCH", "cycle 12 languages", "live reload; AR RTL intact (34)"),
    ("CRASH_LOG", "kill -9 then boot", "crash-log.txt; autosave repair"),
    ("CREDITS", "full credits roll", "38 blocks; skippable; room loads"),
    ("MUSEUM", "open at 60%", "exhibits render; honesty exhibit (38)"),
    ("DUCKS", "collect 25 ducks", "counter; Cone of Shame grants (54)"),
]
PLACES = ["Playa Coralina","Puerto Chico","Selva de Ceiba","Molino Viejo","Las Cascadas",
          "Aldea del Faro","Colina de la Capilla","Cresta Sombria","Cueva del Trueno",
          "Caleta Santa Promesa","El Agujero Azul","Boveda Sumergida","Old Harbor",
          "Downtown Meridian","Solada Beach","Alto Barro","La Herreria","Teatro Alba",
          "Aerodromo Chico","Isla Perdida","Faro Viejo","Harbor Bridge","Marina Slip 9"]
WEAPONS = [("W01","Culebra"),("W02","Bajo Grande"),("W03","Chispa"),("W04","Cortavientos"),
           ("W05","Libertad"),("W06","Carabina 51"),("W07","Festival"),("W08","Trueno"),
           ("W09","Sabado"),("W10","Mirada Larga"),("W11","Vigilante"),("W12","Ceiba"),
           ("M01","Cortesia"),("M02","Llave de Marisol"),("M03","Bastoon del Sereno"),("T00","Unarmed")]
WEV = ["fire","reload_start","reload_end","empty","draw","holster"]
ZONE = [("head",2.5),("torso",1.0),("limb",0.7)]
POOL = [("GRUNT",100),("BRUISER",160),("OFFICER",130)]
NPCS = [("N01","Ren"),("N02","Tobias"),("N03","Marisol"),("N04","Hands"),("N05","Sereno"),
        ("N06","Sargento"),("N07","Reloj"),("N08","Marea"),("N09","Fraile"),("N10","Crane"),
        ("N11","Rastra"),("N12","Vidente"),("N13","Pulpo"),("N14","Lucky"),("N15","Esteban"),
        ("N16","Viuda"),("N17","Tio Pancho"),("N18","Quintana"),("N19","Camila"),("N20","Vega"),
        ("N21","Beto"),("N22","Chispa"),("N23","El Dedito"),("N24","Hermana Luz"),("N25","Pescador")]
BARK_CTX = {
    "greet": ["buenos dias, jefe","you again. good","ah, the quiet one","see you brought the dog"],
    "idle": ["wind's turning","coffee first","counting crates","radio says rain"],
    "combat": ["contact!","left side!","reload! cover me!","he's flanking!"],
    "alert": ["who was that","check the brush","something moved","sound off!"],
    "fear": ["I did not sign up for this","the book was right","mama!","falling back!"],
    "joke": ["tell the Herald I was handsome","the dog judges me","nice poncho","is that a wrench?"],
    "farewell": ["vaya con suerte","watch the tide","say hi to the ferryman","feed that dog"],
    "stature": ["it's YOU. gracias","do not look at me","hero of the harbor","the calm one. run."],
}
SUBJ = ["MASKED FIGURE","THE STRANGER","A VERY CALM MAN","UNNAMED DIVER","GHOST OF THE DOCKS",
        "THE LONE DOG OWNER","SUNRISE CLIMBER","A GENTLEMAN WITH A WRENCH","THE PONCHO","A TOURIST, PROBABLY",
        "AN ANCHOR WITNESS","THE RADIO FRIEND","ONE WHO CUTS ROPE","THE QUIET ONE","A SWIMMER OF NOTE",
        "THE BOOK'S SHADOW","A HARBOR PHANTOM","THE BENEFACTOR","A MAN AND HIS DOG","THE LAST FERRY'S GUEST"]
VERB = ["SINKS","LIBERATES","OUTRUNS","VANISHES INTO","BUYS OUT","SILENCES","HARPOONS","WALKS AWAY FROM",
        "REPOSSESSES","REDECORATES","OUTDANCES","DISARMS","REPAIRS, THEN WRECKS","SIGNALS","REROUTES",
        "UNDERMINES","CARRIES OFF","THANKS POLITELY","RINGS THE BELL FOR","FEEDS"]
OBJ = ["THREE CRUISERS","THE OLD HARBOR","A HAIL OF BULLETS","THE MORNING SHIFT","FIVE STARS OF CHAUS",
       "A CULT TAX WAGON","THE ENTIRE NIGHT SHIFT","ONE VERY LARGE LOCK","A GUNSHIP'S FUEL LINES",
       "TASK FORCE LIMPIO","THE DOWNTOWN GRID","A BANK'S DIGNITY","THE HARBOR BRIDGE","FOUR OUTPOSTS",
       "THE LAST PEACEFUL TEA","A VERY GOOD DOG'S DINNER","THE SUGAR MILL","RUSH HOUR, PERSONALLY",
       "A BELL TOWER","THE STORM ITSELF"]
KICK = ["witnesses describe 'unnecessarily cool'","no comment from the dog","budget office weeps",
        "survivors ask for autographs","the editor is 'processing'","source: everyone, loudly",
        "the coast guard shrugs","tea industry unfazed","muralists inspired","ferryman raises rates",
        "hospital reports only pride","pigeons unharmed","one window regrettable","the tide files no complaint",
        "anonymous donor pays for damages","the DJ played them off","dog's tail: wagging","print run sold out"]
ARCH = ["escort","takedown_only","convoy","time_trial","bounty","stealth_theft","horde_holdout",
        "photo_journalism","animal_rescue","demolition","street_race","mystery_delivery"]
MUTS = ["rain_of_lead","silent_night","big_head","low_grav","turbo_night","paintball"]
TOOLS = ["U01_ojos","U02_llave","U03_dedito","U04_cuerda","U05_paciencia","U06_cuchillo","U07_linterna","U08_cantimplora"]
VEH = ["V01","V02","V03","V04","V05","V06","V07","V08","V09","V10","V11","V12"]

def sec(n, title):
    print()
    print("# SECTION %s — %s" % (n, title))

def main():
    print()
    print("PART IX — MACHINE-EXPANDED PRODUCTION MATRICES")
    print("(generated by tools/gen_annex.py from the hand-authored seeds of Parts IV-VIII;")
    print(" combinatorial coverage is intentional. Binding for QA (Sec 18/39/62) and i18n (34).)")
    # 85 QA matrix
    sec(85, "QA MATRIX (systems x platforms x profiles) — %d cases" % (len(SYSTEMS)*len(PLATFORMS)*len(PROFILES)))
    print("| ID | SYSTEM | PLATFORM | PROFILE | STEPS | EXPECTED |")
    print("|----|--------|----------|---------|-------|----------|")
    i = 0
    for sysname, steps, expect in SYSTEMS:
        for pf in PLATFORMS:
            for pr in PROFILES:
                i += 1
                print("| QA-%04d | %s | %s | %s | %s | %s; %s |" % (i, sysname, pf, pr, steps, expect, PROF_NOTE[pr]))
    # 86 feat verify
    sec(86, "FEAT VERIFY ROWS (60 feats x 3 profiles)")
    print("| ID | FEAT | PROFILE | VERIFY |")
    print("|----|------|---------|--------|")
    featv = ["unlock fires once","share card exports","poster stamps"]
    for f in range(1, 61):
        for j, pr in enumerate(["STOCK", "GAMEPAD", "UI150"]):
            print("| FV-%03d-%d | F%02d | %s | %s |" % (f, j+1, f, pr, featv[j]))
    # 87 mission checkpoint tests
    sec(87, "MISSION CHECKPOINT GRID (14 chapters x 12 checks)")
    print("| ID | CH | CHECK | EXPECTED |")
    print("|----|----|-------|----------|")
    checks = ["start CP","beat loop","fail state A","fail-forward B","autosave tick","load mid-beat",
              "inventory persist","dog state persist","five_moment guard","skip cutscene","dialogue sub on","exit clean"]
    for m in range(1, 15):
        ch = "M%02d" % m if m <= 11 else "ST%d" % (m - 11)
        for j, ck in enumerate(checks):
            print("| MC-%03d-%02d | %s | %s | %s |" % (m, j+1, ch, ck, "pass per Sec 18.4/18.8"))
    # 88 i18n keys
    sec(88, "I18N KEY TABLE (source EN + 12-lang MT queue) — loc.*")
    print("| KEY | EN SOURCE | STATUS |")
    print("|-----|-----------|--------|")
    def emit_key(k, en):
        print("| loc.%s | %s | MT-12 pending (34.1) |" % (k, '"' + en.replace('|', '/') + '"'))
    menu = ["Continue","New Game","Load Game","Save Game","Settings","Extras","Credits","Quit to Desktop",
            "Museum","Benchmark","Mod Menu","OST Player","Gallery","Making Of","Postmortem","Roadmap",
            "Send a Thought","Accessibility","Language","Back"]
    for x in menu: emit_key("ui.menu." + x.lower().replace(' ', '_'), x)
    for grp, keys in [("display",["Fullscreen","Resolution","Vsync","Quality Preset","FOV","Motion Blur","Render Scale","Shadows","SSAO","AA Mode","HD Textures","Draw Distance","Foliage Density","Traffic Density","Brightness"]),
                      ("audio",["Master","Music","SFX","Voice","Radio Volume","Duck Strength","Dynamic Range","Subtitle Language"]),
                      ("gameplay",["Difficulty","Enemy Damage","Enemy Aim","Detection Speed","Aim Assist","Slow Motion","Auto Win QTE","Hints","Gore Reduced","Damage Numbers","Camera Shake","Crosshair","Hit Marker","Iron Sight Hold","Tutorial Level"]),
                      ("controls",["Invert Y","Sensitivity","ADS Sensitivity","Aim Smoothing","Rumble","Hold Sprint","Hold Crouch","Hold Aim","Hold Breath","Glyph Set"]),
                      ("access",["Colorblind Mode","Reticle Color","Photosensitivity","UI Scale","Dyslexic Font","Narration","Audio Cues","Safe Area","Co-Pilot","One Hand Preset"])]:
        for x in keys: emit_key("ui.settings.%s.%s" % (grp, x.lower().replace(' ', '_')), x)
    for x in ["Health","Armor","Ammo","Minimap","Compass","Objective","Stealth Eye","Heat Stars","Alert Meter","Paw Icon","Stamina","Breath","Prompt","Subtitle","Toast","Crosshair","Damage Flash","Hit Confirm","Loot Glow","Ping Sonar","Companion Down","Vehicle Health","Radio Now Playing","Scrapbook New","Feat Unlocked"]:
        emit_key("hud." + x.lower().replace(' ', '_'), x)
    for wid, wname in WEAPONS:
        emit_key("item.%s.name" % wid.lower(), wname)
        emit_key("item.%s.desc" % wid.lower(), "Field manual entry for " + wname)
    for t in TOOLS: emit_key("item.%s.name" % t.lower(), t.split('_')[1].title())
    for f in ["Rubia","Carpintero","Abuelo","Colmillo","Viuda Negra","Fantasma","Diamante","Pez Globo Triste","El Pez Gordo","Rey Marea","Pez Farol","Corazon Roto"]:
        emit_key("item.fish." + f.lower().replace(' ', '_'), f)
    skills = ["Quiet Soles I","Quiet Soles II","Soft Hands","Double Takedown","Ghost Tag","Vanish","Last Known Fade","Sereno's Whisper",
              "Steady Hand I","Steady Hand II","Fast Hands","Combat Roll","Adrenal Focus","Heavy Bones","Double Down","Festival Doctrine",
              "Runner's Lung","Hold Breath x2","Glider Boost","Deep Lungs","Fast Search","Scavenge x2","Mule Pocket","Poncho Pro"]
    for i, s in enumerate(skills, 1):
        emit_key("skill.s%02d.name" % i, s)
        emit_key("skill.s%02d.desc" % i, "Rank effect per PART V S71 row S%02d" % i)
    named_feats = ["Tea With The Devil","Good Dog","Sunrise Club","Ghost of Alto Barro","Dueno del Mar",
                   "Unnecessarily Cool","The World According to Ren","Antes del Amanecer","One More Cast","Hereje",
                   "Formar","Sincronizado","La Agua Custodia","La Tercer Campana","Ciudad Limpia","???","???","Cone of Shame",
                   "Nepotismo Musical","Poncho Pro"]
    verb_pool = ["Catch","Craft","Clear","Spare","Sink","Find","Hear","Feed","Drive","Paint"]
    noun_pool = ["the Tide","the Mill","the Bridge","the Choir","the Shift","the Faro","the Ledger","the Last Bell","the Market","the Dawn"]
    feat_names = list(named_feats)
    while len(feat_names) < 60:
        i = len(feat_names)
        feat_names.append(verb_pool[i % 10] + " " + noun_pool[(i // 10) % 10] + " " + str(1 + i // 20))
    for i, f in enumerate(feat_names, 1):
        emit_key("feat.f%02d.name" % i, f)
        emit_key("feat.f%02d.desc" % i, "Unlock per S80 row F%02d" % i)
    for m in range(1, 12):
        emit_key("mission.m%02d.title" % m, "Chapter %d title (PART VI)" % m)
        for b in range(1, 11):
            emit_key("mission.m%02d.obj%02d" % (m, b), "Objective %d phrasing (PART VI beats)" % b)
        for fl in range(1, 3):
            emit_key("mission.m%02d.fail%02d" % (m, fl), "Fail-forward message %d" % fl)
    for st in range(1, 4):
        emit_key("mission.st%d.title" % st, "Stronghold %d title" % st)
        for b in range(1, 11):
            emit_key("mission.st%d.obj%02d" % (st, b), "Stronghold objective %d" % b)
        for fl in range(1, 3):
            emit_key("mission.st%d.fail%02d" % (st, fl), "Stronghold fail %d" % fl)
    for nid, nname in NPCS:
        emit_key("npc.%s.name" % nid.lower(), nname)
        emit_key("npc.%s.job" % nid.lower(), "Occupation line (S66)")
    for p in PLACES: emit_key("place." + p.lower().replace(' ', '_'), p)
    for stn in ["costera", "noche", "pueblo"]:
        for t in range(1, 11): emit_key("radio.%s.track%02d" % (stn, t), "Track %d title (%s)" % (t, stn))
        for d in range(1, 6): emit_key("radio.%s.dj%02d" % (stn, d), "DJ line %d (%s)" % (d, stn))
    for a in ARCH:
        emit_key("contract.%s.name" % a, a.replace('_', ' ').title())
        emit_key("contract.%s.brief" % a, "Briefing text template (%s)" % a)
    for p in ["Interact","Reload","Sprint","Crouch","Tag","Command Dog","Radio Next","Photo Mode","Ping","Hold Breath"]:
        emit_key("prompt." + p.lower().replace(' ', '_'), p + " [%s]")
    for s in ["SUBJ", "VERB", "OBJ", "KICKER"]:
        emit_key("herald.slot." + s.lower(), s + " lexicon tag (S79)")
    # 89 Herald bank
    sec(89, "HERALD HEADLINE BANK (resolved samples + slots) — front pages (27)")
    print("| ID | HEADLINE | KICKER | STATS HOOK |")
    print("|----|----------|--------|------------|")
    for i in range(420):
        head = "%s %s %s" % (SUBJ[i % len(SUBJ)], VERB[(i // 3) % len(VERB)], OBJ[(i // 7) % len(OBJ)])
        print("| H-%03d | %s | %s | {N%d}=%d |" % (i + 1, head, KICK[(i // 5) % len(KICK)], (i % 3) + 1, (i % 9) + 1))
    # 90 contract grid
    sec(90, "CONTRACT GENERATION GRID (archetype x place x mutator x tier)")
    print("| ID | ARCHETYPE | PLACE | MUTATOR | TIER | SEED FORMULA |")
    print("|----|-----------|-------|---------|------|--------------|")
    for i in range(360):
        a = ARCH[i % 12]; p = PLACES[(i * 5) % len(PLACES)]; m = MUTS[(i * 7) % 6]; t = (i % 3) + 1
        print("| C-%03d | %s | %s | %s | T%d | seed=int(YYYYMMDD)*7+%d |" % (i + 1, a, p, m, t, i % 12))
    # 91 audio events
    sec(91, "AUDIO EVENT TABLE (WWise-style ids — 32 mix rules apply)")
    print("| ID | SRC | EVENT | NOTE |")
    print("|----|-----|-------|------|")
    k = 0
    for wid, wname in WEAPONS:
        for ev in WEV:
            k += 1
            print("| A-%04d | %s | %s | hit-stop ref 44.2 |" % (k, wid, ev))
    for t in TOOLS:
        for ev in ["use", "fail"]:
            k += 1
            print("| A-%04d | %s | %s | foley |" % (k, t, ev))
    for v in VEH:
        for ev in ["door", "horn", "engine_loop", "crash"]:
            k += 1
            print("| A-%04d | %s | %s | veh |" % (k, v, ev))
    for i, d in enumerate(["woof_1","woof_2","whine","growl","howl_siren","jingle_run","dig","shake","yawn","sleep_pant","fetch_drop","happy_bark","sad_ears","pant"], 1):
        print("| A-%04d | PONCHO | %s | 14 variants pitch-rand (24) |" % (k + i, d))
    k += 14
    for i, u in enumerate(["ui_click","ui_confirm","ui_back","ui_stamp_feat","ui_map_unfurl","ui_pause_duck","ui_toast","ui_buyback","ui_radio_blip","ui_ping"], 1):
        print("| A-%04d | UI | %s | 50.3 language |" % (k + i, u))
    k += 10
    for i, f in enumerate(["step_grass","step_mud","step_metal","step_wood","step_water","rain_roof","rain_open","thunder_near","thunder_far","wind_gust","fire_loop","explosion_5m","glass_break","crate_smash","cloth_flap","collar_jingle","bell_toll","hymn_live","hymn_corrupt","shanty_drunk"], 1):
        print("| A-%04d | WORLD | %s | 45/51 refs |" % (k + i, f))
    # 92 balance matrix
    sec(92, "BALANCE VERIFY MATRIX (S68 numbers derived — recompute, never guess)")
    print("| ID | WPN | ZONE | RANGE_M | DMG_OUT | POOL | HITS_TO_KILL |")
    print("|----|-----|------|---------|---------|------|--------------|")
    k = 0
    base_dmg = {"W01":24,"W02":55,"W03":16,"W04":20,"W05":28,"W06":26,"W07":30,"W08":11,"W09":13,"W10":120,"W11":85,"W12":90,"M01":45,"M02":55,"M03":60,"T00":12}
    for wid, wname in WEAPONS:
        d0 = base_dmg[wid]
        for zname, zmul in ZONE:
            for ri, rmul in enumerate([1.0, 0.75, 0.5], 1):
                k += 1
                out = d0 * zmul * rmul
                for pname, hp in POOL[:1]:
                    ttk = max(1, round(hp / max(out, 1)))
                    print("| B-%04d | %s | %s | R%d | %.1f | %s | %d |" % (k, wid, zname, ri, out, pname, ttk))
    for wid, wname in WEAPONS:
        for pname, hp in POOL:
            k += 1
            torso = base_dmg[wid]
            print("| B-%04d | %s | torso_full | R1 | %.1f | %s | %d |" % (k, wid, torso, pname, max(1, round(hp / max(torso, 1)))))
    # 93 barks
    sec(93, "NPC BARK BANK (25 NPCs x 8 contexts) — 32 duck applies")
    print("| ID | NPC | CONTEXT | LINE |")
    print("|----|-----|---------|------|")
    for nid, nname in NPCS:
        for ci, (ctx, lines) in enumerate(BARK_CTX.items()):
            print("| bk.%s.%s | %s | %s | \"%s\" |" % (nid, ctx, nname, ctx, lines[ci % len(lines)]))
    # 94 world strings
    sec(94, "WORLD STRING BANK (prompts, tips, objective alts, fail-forwards)")
    print("| ID | KIND | TEXT |")
    print("|----|------|------|")
    tips = ["Hold breath to steady aim.", "Poncho can sniff caches.", "Golden hour lengthens shadows.", "Silenced shots still make noise at 8m.",
            "You can always come back louder.", "Rain hides footsteps.", "The Herald remembers everything.", "Bodies alert patrols.",
            "Bribe clears heat at El Banco Menor.", "Robes confuse distant guards."]
    for i in range(40):
        print("| ws-%03d | tip | \"%s (%d/40)\" |" % (i + 1, tips[i % len(tips)], i + 1))
    for i in range(60):
        print("| ws-%03d | fail_fwd | \"Checkpoint restored — %s\" |" % (41 + i, ["try the other road","the tide was wrong","the dog is fine","they were expecting you","reload and breathe"][i % 5]))
    for i in range(100):
        print("| ws-%03d | obj_alt | \"%s\" |" % (101 + i, ["Reach the marker", "Escape the zone", "Hold the position", "Follow Poncho", "Return to base"][i % 5] + " (phrasing %d)" % (i + 1)))
    for i in range(100):
        print("| ws-%03d | prompt | \"%s\" |" % (201 + i, ["[E] Talk", "[E] Loot", "[E] Pet Poncho", "[E] Fish", "[E] Read tape"][i % 5] + " alt %d" % (i + 1)))
    # 95 coda
    sec(95, "REGENERATION, COUNTS & END OF DOCUMENT")
    print("Regenerate this Part: python3 tools/gen_annex.py >> MASTER_PROMPT.md")
    print("Matrix rows are Binding. Hand-authored design intent lives in Parts I-VIII.")
    print("If a matrix row conflicts with Parts I-VIII prose, THE PROSE WINS — file the row as stale in the M-report.")
    print()
    print("END OF MASTER PROMPT — PARTS I-IX COMPLETE (63 hand sections + 11 matrix sections).")
    print("The game to build: DIVIDED HORIZON. The promises to keep: Sections 1, 15, 17, 18, 35, 58.")
    print("The whole world is rooting for you. BUILD. /")

if __name__ == "__main__":
    main()
