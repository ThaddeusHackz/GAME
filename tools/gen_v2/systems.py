"""S143 vehicles, S147 performance, S148 screenshots + automation, S149 WBS, S150 cinematics, S153 audio."""
from .common import *
from .art import SURF

CAR_PARTS = ["front bumper","rear bumper","hood","trunk lid","roof","door FL","door FR","door RL","door RR","fender FL","fender FR","fender RL","fender RR",
             "windshield","rear window","side window FL","side window FR","side window RL","side window RR","mirror L","mirror R","headlight L","headlight R",
             "taillight L","taillight R","grille","radiator","engine","gearbox","exhaust","fuel tank","axle F","axle R","wheel FL","wheel FR","wheel RL","wheel RR",
             "suspension FL","suspension FR","chassis frame"]
MOTO_PARTS = ["front fork","rear shock","front wheel","rear wheel","handlebars","fairing L","fairing R","tank","seat","engine","exhaust","headlight","taillight",
              "mirror L","mirror R","frame","footpeg L","footpeg R","chain","mudguard"]
BOAT_PARTS = ["bow","hull port","hull starboard","hull stern","keel","deck","windscreen","console","seat","outboard engine","propeller","fuel tank","steering",
              "cleat bow","cleat stern","rail port","rail starboard","bilge pump","light nav","horn","hatch","anchor","buoyancy tube","transom"]
DSTATE = ["pristine", "scuffed", "dented", "crushed", "destroyed"]

def gen_143():
    out = sec(143, "VEHICLE LEDGER — damage zones x states, surface handling, engine audio layers", [
        "Numbers from S69 bind (top speed, HP, seats). Zones from 114.4. DEFORM = crumple method; DETACH = part may separate under impact energy.",
        "Handling rows give the grip multiplier applied to the tyre-model peak (114.1) by surface and wetness."])
    out += ["", "## 143.A — DAMAGE ZONE x STATE"]
    rows = []
    i = 0
    for vid, vn, cl in VEHICLES:
        parts = {"car": CAR_PARTS, "moto": MOTO_PARTS, "boat": BOAT_PARTS}[cl]
        for p in parts:
            for si, s in enumerate(DSTATE):
                i += 1
                det = ("Y" if (any(k in p for k in ("door", "bumper", "hood", "trunk", "mirror", "wheel", "fairing", "rail", "mudguard", "headlight", "taillight")) and si >= 3) else "N")
                rows.append(["VD-%05d" % i, vid, vn, p, s, "HP %d-%d%%" % (100 - si * 20, 80 - si * 20 if si < 4 else 0), "morph+crumple" if si in (1, 2) else ("mesh-swap" if si >= 3 else "-"),
                             det, "glass:%d" % (si if "window" in p or "windshield" in p else 0), "handling %+d%%" % (-si * RI(1, 6, vid, p, "h") if p in ("suspension FL","suspension FR","axle F","axle R","engine","front fork","propeller","keel") else 0),
                             "fx:%s" % PK(["sparks", "smoke", "fluid-leak", "steam", "fire", "-"], p, si)])
    out += table(["ID", "VEH", "NAME", "ZONE", "STATE", "HP_BAND", "DEFORM", "DETACH", "GLASS_STAGE", "HANDLING_EFFECT", "FX"], rows)
    out += ["", "## 143.B — SURFACE GRIP (12 vehicles x 30 surfaces x dry/wet = 720)"]
    rows = []
    i = 0
    for vid, vn, cl in VEHICLES:
        for s in SURF[:30]:
            for w in ("dry", "wet"):
                i += 1
                g = RF(0.35, 1.0, s, "g") * (0.7 if w == "wet" and any(k in s for k in ("asphalt", "concrete", "steel", "tile", "marble", "pavers")) else 0.85 if w == "wet" else 1.0)
                if cl == "boat": g = 0.0 if not s.startswith("water") else RF(0.6, 1.0, vid, "bg")
                rows.append(["VH-%05d" % i, vid, s, w, "%.2f" % g, "slip-peak %.2f" % RF(0.08, 0.18, vid, s, w, "sp"), "drift@%.2f" % RF(0.30, 0.42, vid, "dr"),
                             "rolling-res %.3f" % RF(0.01, 0.12, s, "rr"), "sfx.tyre.%s.%s" % (s.split("-")[0], w)])
    out += table(["ID", "VEH", "SURFACE", "WET", "GRIP_MULT", "SLIP_PEAK", "DRIFT_THRESH", "ROLL_RES", "TYRE_AUDIO"], rows)
    out += ["", "## 143.C — ENGINE AUDIO RPM LAYERS (12 vehicles x 16 layers = 192)"]
    rows = []
    i = 0
    for vid, vn, cl in VEHICLES:
        for l in range(16):
            i += 1
            rpm = 800 + l * (RI(300, 480, vid, "step"))
            rows.append(["VA-%04d" % i, vid, vn, l + 1, rpm, "on-throttle" if l % 2 == 0 else "off-throttle", "granular grain %d ms" % RI(40, 180, vid, l, "gr"),
                         "turbo-whine" if vid in ("V02", "V07", "V09", "V11") and l > 10 else "-", "MS_Engine_%s" % vid])
    out += table(["ID", "VEH", "NAME", "LAYER", "RPM", "LOAD", "SYNTH", "EXTRA", "METASOUND"], rows)
    return out

# ------------------------------------------------------------------ PERFORMANCE
SCENE_SCN = ["hero vista at noon", "traversal sprint", "firefight with 24 AI"]
SCENES = []
for p in ["Playa Coralina","Puerto Chico","Selva de Ceiba","Molino Viejo","Las Cascadas","Aldea del Faro","Colina de la Capilla","Cresta Sombria","Cueva del Trueno",
          "Caleta Santa Promesa","Old Harbor","Downtown Meridian","Solada Beach","Alto Barro","La Herreria","Aerodromo Chico","Harbor Bridge","Marina Slip 9"]:
    SCENES.append("%s — %s" % (p, PK(SCENE_SCN, p)))
SCENES += ["Downtown night rain, 120 cars", "Stadium event crowd 1,500", "Island storm at sea", "Wildfire in jungle (Q3 fire)", "Gunship boss over harbor",
           "Underwater reef dive", "Highway chase at 160 km/h", "Photo-mode path-traced still", "Airport take-off streaming", "Cave lantern exploration",
           "Rainforest canopy ziplines", "Mangrove river boat", "Night jungle stealth", "Helicopter flight across island", "Market street 400 pedestrians",
           "Explosion chain at fuel depot", "Cutscene (hero close-up, hair+skin)", "Menu-to-world load", "Fast-travel streaming montage", "Bridge collapse set-piece",
           "Sunset beach bonfire", "Police 5-star pursuit", "Interior hero mansion", "Sugar mill boss arena", "Heat-shimmer noon city",
           "Cult compound alarm + reinforcements", "Pier fishing minigame", "Rooftop parkour run", "Lighthouse finale storm", "Cold-boot first-minute shader warmup"]
UP = ["native", "TSR-Quality", "TSR-Performance", "DLSS/FSR-Balanced"]
TFPS = {"T1": 30, "T2": 45, "T2H": 40, "T3": 60, "T4": 60, "T5": 90}
TRES = {"T1": "1920x1080", "T2": "1920x1080", "T2H": "1280x800", "T3": "2560x1440", "T4": "3840x2160", "T5": "3840x2160"}
PASSES = ["Nanite visibility+raster", "Base/material pass", "Lumen GI", "Lumen reflections", "Virtual shadow maps", "Volumetric fog/clouds", "Translucency+VFX", "Post+upscale", "Hair/strands", "UI", "Water", "Decals/other"]
PSHARE = [0.156, 0.144, 0.168, 0.072, 0.132, 0.054, 0.066, 0.108, 0.030, 0.018, 0.030, 0.022]

def gen_147():
    out = sec(147, "PERFORMANCE BUDGET SHEETS — %d scenes x 6 tiers x 4 upscale modes = %d rows; + per-pass ms table" % (len(SCENES), len(SCENES) * 24), [
        "Frame target by tier: T1 30, T2 45, T2H 40, T3 60, T4 60, T5 90 FPS (109.1). Budget = 1000/fps ms. VRAM/RAM caps from 109.4.",
        "Measured columns (AVG/P1/P0.1) are filled by the nightly profiling run (109.9); budget columns are targets."])
    out += ["", "## 147.A — SCENE x TIER x UPSCALE BUDGETS"]
    rows = []
    i = 0
    for sc in SCENES:
        heavy = 1.0 + (H(sc) % 25) / 100.0
        for t in TIERS:
            fps = TFPS[t]
            for up in UP:
                i += 1
                ms = 1000.0 / fps
                rows.append(["PB-%05d" % i, sc, t, TRES[t], up, fps, "%.2f" % ms, "%.2f" % (ms * 0.92), "%.1f" % (ms * 0.6), "%.2f" % (ms * 0.55),
                             "VRAM<=%.1f GB" % {"T1": 5.6, "T2": 6.2, "T2H": 10.0, "T3": 7.2, "T4": 14.0, "T5": 15.0}[t], "P1>=%d" % int(fps * 0.75), "hitch<=33ms", "AVG:- P1:- P0.1:-"])
    out += table(["ID", "SCENE", "TIER", "OUTPUT_RES", "UPSCALE", "FPS_TGT", "FRAME_MS", "GPU_MS_CAP", "GAME_THREAD_MS", "RENDER_THREAD_MS", "VRAM_CAP", "P1_TGT", "HITCH", "MEASURED"], rows)
    out += ["", "## 147.B — GPU PASS BUDGET BY TIER x SCENE-TYPE (12 passes x 6 tiers x 10 types = 720)"]
    types = ["open jungle", "beach+sea", "dense city day", "neon city night", "interior", "cave", "combat heavy", "vehicle high-speed", "cinematic close-up", "storm"]
    rows = []
    i = 0
    for ty in types:
        for t in TIERS:
            ms = 1000.0 / TFPS[t] * 0.92
            for p, sh in zip(PASSES, PSHARE):
                i += 1
                adj = sh * (1.0 + (RF(-0.3, 0.45, ty, p, "adj")))
                rows.append(["PP-%05d" % i, ty, t, p, "%.2f" % (ms * adj), "%.1f%%" % (adj * 100), "knob:%s" % PK(["r.Nanite.MaxPixelsPerEdge", "r.Lumen.ScreenProbeGather.*", "r.Shadow.Virtual.*", "r.VolumetricFog.*", "sg.EffectsQuality", "r.ScreenPercentage", "sg.PostProcessQuality", "r.Hair.*"], p, "k"), "alert>%+d%%" % 10])
    out += table(["ID", "SCENE_TYPE", "TIER", "PASS", "BUDGET_MS", "SHARE", "PRIMARY_KNOB", "ALERT"], rows)
    return out

# ------------------------------------------------------------------ SCREENSHOTS & AUTOMATION
CAMTYPES = ["vista-wide","street-level","hero-closeup","interior-key","vehicle-chase","water-surface","canopy-low","night-neon","combat-over-shoulder","cinematic-2.39",
            "aerial-glide","underwater"]
TODS = ["06:00 dawn","12:00 noon","18:00 golden hour","22:00 night"]
WX3 = ["Clear","Rain","Fog/Mist"]
V2SYS = ["Nanite cluster streaming","Lumen GI stability","Lumen reflections on wet asphalt","Virtual shadow maps cache","Substrate wet overlay","Strand hair wind+wet","Chaos cloth poncho",
 "MetaHuman facial runtime","Audio-driven visemes","Motion matching idle->walk","Motion matching pivots","Foot IK on stairs","Foot IK on slopes","Stride warping","Orientation warping",
 "Mantle warp accuracy","Vault chains","Cliff climb","Rappel","Zipline verlet","Glide lift/drag","Swim transitions","Underwater caustics","FFT ocean LOD","Boat wake","Weapon sway+inertia",
 "ADS alignment","Reload cancel windows","Ballistic drop","Penetration table lookup","Ricochet angles","Recoil pattern replay","Hit reaction additive","Physical animation blend","Ragdoll settle",
 "Takedown sync error","Cover snap+vault","Squad director roles","Attack tokens fairness","Flank EQS","Perception LKP","Fire propagation","Explosion falloff","Destruction tiers GC",
 "Vehicle suspension","Tyre grip by surface","Vehicle damage morph","Engine MetaSound layers","Mass crowd LOD handoff","Mass traffic junctions","Wildlife ecosystem tick","World Partition load/unload",
 "HLOD pop-free","Interior streaming","Weather transitions","Time-of-day exposure","PSO precache hitch","Save/load world diff","Photo mode path-trace capture"]

def gen_148():
    cams = []
    for p in PLACES:
        for ct in CAMTYPES[:14]:
            cams.append((p, ct))
    cams = cams[:300 + 40]
    out = sec(148, "SCREENSHOT-REGRESSION & V2 AUTOMATION GRID — %d cameras x 4 TOD x 3 weather = %d shots; + v2 gauntlet cases" % (len(cams), len(cams) * 12), [
        "Each SHOT is a fixed camera (world-space transform stored in /Content/Data/QA/Cameras.csv) rendered headless at the tier's output res.",
        "Pass = SSIM >= 0.985 vs approved golden AND luminance histogram within 108.8 bands AND no NaN/black pixels. Goldens re-approved only via PR review."])
    out += ["", "## 148.A — SCREENSHOT MATRIX"]
    rows = []
    i = 0
    for p, ct in cams:
        for tod in TODS:
            for wx in WX3:
                i += 1
                rows.append(["SHOT-%05d" % i, p, ct, tod, wx, PK(TIERS[3:], p, ct, "t"), "%dmm" % PK([14, 18, 24, 35, 50, 85], p, ct, "f"), "f/%.1f" % PK([1.8, 2.8, 4, 5.6, 8], p, ct, "a"),
                             "SSIM>=0.985", "hist:ok", "golden:pending"])
    out += table(["ID", "LOCATION", "CAMERA", "TOD", "WEATHER", "TIER", "LENS", "APERTURE", "DIFF", "HISTOGRAM", "GOLDEN"], rows)
    out += ["", "## 148.B — V2 GAUNTLET CASES (%d systems x 3 tiers x 5 scenarios = %d)" % (len(V2SYS), len(V2SYS) * 15)]
    scn = ["nominal 5 min", "stress (2x density)", "weather storm", "night", "soak 30 min"]
    rows = []
    i = 0
    for s in V2SYS:
        for t in ("T1", "T3", "T5"):
            for sc in scn:
                i += 1
                rows.append(["GA-%05d" % i, s, t, sc, PK(["no hitch>33ms", "no NaN", "no leak>2%", "error<=spec", "budget met", "deterministic replay"], s, t, sc), "artifact:csv+trace", "pending"])
    out += table(["ID", "SYSTEM", "TIER", "SCENARIO", "ASSERT", "EVIDENCE", "STATUS"], rows)
    return out

# ------------------------------------------------------------------ WBS
EPICS = [
 ("E01","Foundation & CI","PRODUCER","project skeleton,module layout,CI pipeline,Git LFS policy,validators,ADR set,dashboards"),
 ("E02","Rendering core","RENDERING/TA","Nanite setup,Lumen tuning,VSM tuning,Substrate masters,atmosphere,post stack"),
 ("E03","Materials & PBR","RENDERING/TA","master materials,wetness system,decal system,RVT blends,anti-tiling,PBR validator"),
 ("E04","Landscape Isla Sombra","WORLD/ENV","heightfield,erosion pass,landscape layers,rivers,beaches,ridge,caldera,sea shelf"),
 ("E05","PCG & vegetation","WORLD/ENV","rainforest graph,cloud forest graph,mangrove graph,cane fields,beach dressing,rock scatter"),
 ("E06","Island outposts & POIs","WORLD/ENV","outpost layouts,signal masts,strongholds,hunting grounds,caves,wrecks,ruins,caches"),
 ("E07","Meridian city layout","CITY","road network,district blockout,hero blocks,bridges,airport,harbor,railyard,highway"),
 ("E08","Architecture kits","CITY","facade modules,roof kits,shopfronts,balconies,signage kit,utilities kit"),
 ("E09","Interiors","CITY","hero interiors,room templates,dressing variants,lighting passes,smart objects,streaming"),
 ("E10","Characters & faces","CHARACTERS","hero heads,hero bodies,NPC cast,crowd bases,costume kit,hair grooms"),
 ("E11","Cloth & gear sim","CHARACTERS","poncho cloth,jackets,flags,straps physics,wet cloth,holster rig"),
 ("E12","Locomotion","MOVEMENT/ANIM","MM databases,foot IK,warping,stamina,camera feel,latency"),
 ("E13","Traversal","MOVEMENT/ANIM","vault,mantle,climb,rappel,zipline,glide,swim,cover"),
 ("E14","FP body & weapon rig","MOVEMENT/ANIM","FP body,hand IK,sway,reload rig,attachments,notifies"),
 ("E15","Gunplay","COMBAT","ballistics,penetration,recoil,spread,hit reg,weapon feel,ammo types"),
 ("E16","Melee & takedowns","COMBAT","melee hit detection,takedown sync,stealth entry,disarm,dog takedowns,comedy takedowns"),
 ("E17","Hit reactions & ragdoll","COMBAT","additive flinch,physical animation,ragdoll tuning,wounds,death anims,civilian reactions"),
 ("E18","Enemy AI","AI","perception,StateTree,cover,squad director,EQS,archetypes"),
 ("E19","Fire, explosions, destruction","COMBAT","fire propagation,explosions,GC destruction,crater stamps,vegetation interaction,physics toys"),
 ("E20","Vehicles","VEHICLES","tyre model,suspension,damage,boats,aircraft,vehicle audio,camera"),
 ("E21","Traffic & crowds","AI","Mass traffic,Mass pedestrians,schedules,panic waves,LOD handoff,wildlife flocks"),
 ("E22","Ecology & wildlife","AI","species roster,predator-prey,hunting,fishing fauna,reef life,dog companion"),
 ("E23","World streaming","TOOLS/PIPELINE","World Partition,HLOD,data layers,interior streaming,act transitions,streaming telemetry"),
 ("E24","Weather & time of day","RENDERING/TA","sun/moon,weather states,clouds,rain,lightning,fog,wind field"),
 ("E25","Water & ocean","RENDERING/TA","FFT ocean,rivers,wakes,underwater,caustics,shore foam"),
 ("E26","Audio core","AUDIO","MetaSounds framework,propagation,mix states,reverb zones,weapon audio,foley"),
 ("E27","Music & VO","MUSIC/VO","dynamic music,leitmotifs,radio stations,VO pipeline,lipsync,localization audio"),
 ("E28","UI/UX & accessibility","UI/UX","HUD,map,menus,photo mode,accessibility,input glyphs"),
 ("E29","Soul systems (Poncho, bosses)","GAMEPLAY","Poncho,bosses,emergence,Herald,stature,contracts"),
 ("E30","Campaign missions","MISSIONS/NARRATIVE","M01-M11 scripting,strongholds,checkpoints,dialogue,objectives,endings"),
 ("E31","Cinematics","CINEMATICS","mocap sessions,facial capture,sequencer scenes,camera work,cutscene lighting,render graph"),
 ("E32","Minigames & sideways joy","GAMEPLAY","fishing,racing,darts,photo jobs,stunts,collectibles"),
 ("E33","Modding & tools","TOOLS/PIPELINE","mod loader,data SDK,editor tools,validators,content health,replay tools"),
 ("E34","Economy & progression","GAMEPLAY","shops,skills,crafting,safehouse,saves,NG+"),
 ("E35","Physics & toys","COMBAT","Chaos tuning,sleep management,toys,cranes,barrels,debris cleanup"),
 ("E36","VFX library","VFX","muzzle VFX,impact VFX,fire VFX,weather VFX,vehicle VFX,ambient VFX"),
 ("E37","Performance","PERFORMANCE","GPU passes,CPU threading,memory,streaming hitches,PSO,scalability"),
 ("E38","QA automation","QA/AUTOMATION","functional tests,gauntlet,screenshot diff,soak bots,perf dashboards,crash triage"),
 ("E39","Localization","LOCALIZATION","string pipeline,12 languages,RTL,VO localization,font coverage,cultural review"),
 ("E40","Legal & licensing","LEGAL/LICENSE","license register,AI disclosure,music rights,VO consent,IP scan,store compliance"),
 ("E41","Build & release","BUILD/CI","cook,pak,installer,signing,symbols,patching"),
 ("E42","Art direction & Art Lock","RENDERING/TA","moodboards,color scripts,Art Lock reviews,Blind Bench,lookdev,LUT library"),
 ("E43","Narrative & writing","MISSIONS/NARRATIVE","script polish,barks,Herald banks,radio scripts,tape scripts,codex entries"),
 ("E44","Live/Love patches","PRODUCER","feedback loop,hotfix pipeline,love patch,DLC hooks,community tools,postmortem"),
]
PHASES = ["Spec lock","Greybox/prototype","Core implement","Content production","Integration","Automated tests","Optimization","Art/feel review","Polish","Accessibility & loc","Evidence capture","Sign-off"]
PHF = {"Spec lock": 0.4, "Greybox/prototype": 1.0, "Core implement": 2.0, "Content production": 2.6, "Integration": 1.2, "Automated tests": 1.0, "Optimization": 1.4,
       "Art/feel review": 0.8, "Polish": 1.3, "Accessibility & loc": 0.7, "Evidence capture": 0.3, "Sign-off": 0.2}
SCOPES = ["Act I island", "Act II city"]
GATEOF = {"RENDERING/TA": "G-ART", "WORLD/ENV": "G-WORLD", "CITY": "G-WORLD", "CHARACTERS": "G-ART", "MOVEMENT/ANIM": "G-ANIM", "COMBAT": "G-COMBAT", "AI": "G-WORLD",
          "VEHICLES": "G-PERF", "TOOLS/PIPELINE": "G-STAB", "AUDIO": "G-ACCESS", "MUSIC/VO": "G-CONTENT", "UI/UX": "G-ACCESS", "MISSIONS/NARRATIVE": "G-CONTENT",
          "CINEMATICS": "G-ART", "PRODUCER": "G-EVIDENCE", "GAMEPLAY": "G-CONTENT", "VFX": "G-PERF", "PERFORMANCE": "G-PERF", "QA/AUTOMATION": "G-STAB", "LOCALIZATION": "G-ACCESS",
          "LEGAL/LICENSE": "G-LEGAL", "BUILD/CI": "G-SHIP"}

def gen_149():
    n = 0
    total = sum(len(e[3].split(",")) for e in EPICS) * 2 * 12
    out = sec(149, "PRODUCTION WBS — %d epics x 6-7 deliverables x 2 scopes x 12 phases = %d tasks" % (len(EPICS), total), [
        "Agent-hours (AH) are swarm estimates (one AH = one agent-hour of focused work); parallel lanes divide calendar time, not AH.",
        "DEP = previous phase of the same deliverable/scope. GATE = the Section 134 gate that the task's evidence feeds. Status is maintained by the PRODUCER lane."])
    rows = []
    for eid, en, lane, items in EPICS:
        for it in items.split(","):
            for sc in SCOPES:
                for pi, ph in enumerate(PHASES):
                    n += 1
                    ah = round(PHF[ph] * RF(6, 40, eid, it, sc, "ah"), 0)
                    tid = "T-%s-%03d" % (eid, n)
                    rows.append([tid, "%s %s" % (eid, en), it, sc, ph, lane, "%d AH" % ah, ("T-%s-%03d" % (eid, n - 1)) if pi else "-",
                                 "S%s" % PK(["101-110", "111-115", "116-121", "122-130", "131-136"], eid), GATEOF[lane], "pending"])
            # restart numbering per epic for readability
    out += table(["ID", "EPIC", "DELIVERABLE", "SCOPE", "PHASE", "LANE", "EST", "DEP", "SPEC", "GATE", "STATUS"], rows)
    return out

# ------------------------------------------------------------------ CINEMATICS
CINE = ["Cold open: the voicemail","M01 Landing","M02 The Harbor Deal","M03 First Outpost","M04 Dog Days","M05 Sugar Mill","M06 Tuxedo Night","M07 Bridge Run",
        "M08 Armored Van","M09 Reckoning","M10 Storm Return","M11 Lighthouse","Stronghold 1 intro","Stronghold 2 intro","Stronghold 3 intro","Boss: Sargento",
        "Boss: Reloj","Boss: Fraile","Boss: Crane","Boss: Rastra","Boss: El Sereno (tea)","Act I to II ferry","Act II to III storm voyage","Ending A","Ending B",
        "Credits roll","Poncho first meeting","Tobias tape reveal","Marisol safehouse","Hands vendor intro","Camila radio studio","Herald front page montage",
        "Museum tour","Photo-mode intro","Duck easter egg","NG+ Sereno line","Daily contract briefing","Race intro","Heist planning board","Tutorial awakening"]
SHOTTYPE = ["establishing","wide","medium","close-up","extreme close-up","over-the-shoulder","insert","OTS reverse","low angle hero","high angle","tracking","dolly-in","crane-up","handheld","whip-pan","POV"]

def gen_150():
    out = sec(150, "CINEMATIC SHOT LISTS — %d cutscenes x 30 shots = %d rows; + VO/LIPSYNC LEDGER 25 NPC x 30 lines = 750" % (len(CINE), len(CINE) * 30), [
        "Grammar per 55.1 (180-degree rule, motivated moves, <=2 min between checkpoints). LENS in mm (full-frame), AP = aperture. CAP = performance capture scope.",
        "RENDER: in-engine Sequencer (real-time) or Movie Render Graph (pre-render for trailers only). Facial = MetaHuman Animator from performance video or audio-driven."])
    rows = []
    i = 0
    for ci, c in enumerate(CINE, 1):
        for s in range(1, 31):
            i += 1
            st = PK(SHOTTYPE, c, s, "st")
            rows.append(["SH-%04d" % i, "CS%02d" % ci, c, s, st, "%dmm" % PK([14, 18, 24, 35, 50, 85, 100], c, s, "l"), "f/%.1f" % PK([1.4, 2.0, 2.8, 4.0, 5.6], c, s, "a"),
                         "%.1f s" % RF(1.2, 9.0, c, s, "dur"), PK(["locked", "slow push", "handheld micro", "dolly", "crane", "steadicam"], c, s, "mv"),
                         PK(["motivated key", "rim+fill", "practical-lit", "golden hour", "neon spill", "storm flash", "lamp-lit"], c, s, "lt"),
                         PK(["body+face CAP", "face CAP", "body CAP", "MM-driven", "keyframe", "crowd sim"], c, s, "cap"), "pending"])
    out += ["", "## 150.A — SHOTS"]
    out += table(["ID", "CUT", "CUTSCENE", "SHOT", "TYPE", "LENS", "AP", "LEN", "MOVE", "LIGHT", "CAPTURE", "STATUS"], rows)
    out += ["", "## 150.B — VO / LIPSYNC LEDGER"]
    rows = []
    i = 0
    for nid, nn in NPCS:
        for l in range(1, 31):
            i += 1
            rows.append(["VO-%05d" % i, nid, nn, "line %02d" % l, PK(["story", "bark-adjacent", "tape", "radio", "combat", "idle", "cinematic"], nid, l, "k"), "%.1f s" % RF(0.8, 14.0, nid, l, "len"),
                         "en+%d langs" % RI(0, 12, "loc"), PK(["neutral", "joy", "worry", "anger", "sadness", "calm", "smug", "fear"], nid, l, "emo"), "viseme-error<=2f", "consent:signed-performer", "pending"])
    out += table(["ID", "NPC", "NAME", "LINE", "CLASS", "LEN", "LANGS", "EMOTION", "LIPSYNC_GATE", "RIGHTS", "STATUS"], rows)
    return out

# ------------------------------------------------------------------ AUDIO
REVERBS = ["open field","forest","canyon","city canyon","alley","small room","large hall","tunnel","cave","underwater","vehicle interior","stairwell","warehouse","bathroom tile",
           "rooftop","dock/pier","beach","jungle canopy","mountain ridge","bridge underside"]
LAYERS = ["close mechanical","close crack","body/boom","tail (env-convolved)","distant slapback"]
BEDS = ["beach day","beach night","mangrove","lowland rainforest day","rainforest night","cloud forest","ridge wind","cane fields","river gorge","cult village","cave","reef underwater","open sea",
        "caldera lake","downtown day","downtown night","harbor day","harbor night","Solada promenade","Solada nightlife","Alto Barro day","Alto Barro night","La Herreria yard","highway",
        "airfield","stadium","market","residential day","residential night","countryside","church interior","bar interior","hospital interior","bank interior","police HQ interior",
        "warehouse interior","mansion interior","parking structure","rooftop","tunnel"]
def gen_153():
    out = sec(153, "AUDIO LEDGER — weapon x reverb x layers; ambience beds x TOD x weather", [
        "Weapon layers convolved per reverb zone (127.2). LEVEL in dBFS at 1 m reference; RT60 in seconds. Beds: 4-layer structure (127.3)."])
    out += ["", "## 153.A — WEAPON AUDIO (16 x 20 x 5 = 1,600)"]
    rows = []
    i = 0
    for wid, wn, wc, amm in WEAPONS:
        for rv in REVERBS:
            for ly in LAYERS:
                i += 1
                rows.append(["WA-%05d" % i, wid, wn, rv, ly, "%.2f s" % RF(0.05, 4.5, rv, "rt"), "%d dBFS" % RI(-24, -6, wid, ly, "lv"), "wet %d%%" % RI(5, 90, rv, ly, "wet"), "sfx.wpn.%s.%s" % (wn.split()[0].lower(), ly.split()[0]), "%d m" % RI(20, 2500, wid, ly, "aud")])
    out += table(["ID", "WPN", "NAME", "REVERB", "LAYER", "RT60", "LEVEL", "WET", "EVENT", "AUDIBLE_RANGE"], rows)
    out += ["", "## 153.B — AMBIENCE BEDS (40 beds x 4 TOD x 3 weather = 480)"]
    rows = []
    i = 0
    for b in BEDS:
        for tod in ("dawn", "day", "dusk", "night"):
            for wx in ("clear", "rain", "storm"):
                i += 1
                rows.append(["AB-%04d" % i, b, tod, wx, RI(3, 6, b, "layers"), "%d dB" % RI(28, 62, b, tod, wx, "spl"), "oneshots/min %d" % RI(0, 14, b, tod, wx, "os"), "duck:combat -%d dB" % RI(3, 9, b), "MS_Bed_%03d" % RI(1, 400, b, tod, wx)])
    out += table(["ID", "BED", "TOD", "WEATHER", "LAYERS", "LEVEL", "ONE_SHOTS", "DUCK", "METASOUND"], rows)
    return out

# ------------------------------------------------------------------ ART LOCK
LOCK_REGIONS = ["B01 Coral Beach","B02 Mangrove","B03 Lowland Rainforest","B04 Cloud Forest","B05 Volcanic Ridge","B06 Sugar-Cane Fields","B07 River Gorge","B08 Cult Village",
                "B09 Cave Mouth","B10 Shallow Reef","B11 Deep Ocean","B12 Caldera Lake","DOWNTOWN","OLD HARBOR","SOLADA BEACH","ALTO BARRO","LA HERRERIA","RESIDENTIAL",
                "COUNTRYSIDE","AERODROMO","HARBOR WATER","HERO INTERIORS"]
LOCK_CAMS = ["dawn vista","noon hero prop","golden-hour silhouette","dusk reflections","night key-light","rain close","fog depth","foot-level 1.7 m","driver-eye 1.2 m",
             "aerial 120 m","worst-case LOD seam","interior-to-exterior transition"]
LOCK_CHECKS = ["no visible texture tiling (10% contrast boost)","no floating or intersecting props","no light leaks / Lumen splotches","shadows stable (no VSM flicker)",
               "reflections coherent with scene","foliage/prop density meets spec (105.4/106.5)","wetness coherent (puddles, film, darkening)","LOD/HLOD pop-free at walk speed",
               "LOD/HLOD pop-free at vehicle speed","no default/placeholder materials","luminance histogram within bands (108.8)","color script fidelity (43.1)",
               "silhouette/landmark readable in clay render","scale believability (doors/steps/handrails)","sound matches what is seen (bed, reverb, surface)"]

def gen_156():
    out = sec(156, "ART LOCK CHECKLIST LEDGER — %d regions x %d cameras x %d checks = %d rows" % (len(LOCK_REGIONS), len(LOCK_CAMS), len(LOCK_CHECKS), len(LOCK_REGIONS) * len(LOCK_CAMS) * len(LOCK_CHECKS)), [
        "Art Lock (110.8, 134.3): a region may not reach feature-lock until all its rows are PASS at T3 and T4 (T5 spot-check).",
        "EVIDENCE is a captured PNG (+ debug-view capture where noted) stored under docs/evidence/ART/<region>/. A row with no artifact is RED by definition."])
    rows = []
    i = 0
    for r in LOCK_REGIONS:
        for c in LOCK_CAMS:
            for k in LOCK_CHECKS:
                i += 1
                dbg = PK(["lit", "Lumen GI view", "VSM cache view", "Nanite overdraw", "shader complexity", "wireframe-LOD", "reflections-only", "lit"], r, c, k, "dbg")
                rows.append(["AL-%05d" % i, r, c, k, "T3+T4", dbg, "SSIM-golden" if "LOD" in k or "shadows" in k else "review-rubric",
                             "docs/evidence/ART/%s/%s.png" % (r.split()[0].lower(), c.split()[0]), "PENDING"])
    out += table(["ID", "REGION", "CAMERA", "CHECK", "TIERS", "DEBUG_VIEW", "METHOD", "EVIDENCE", "STATUS"], rows)
    return out
