"""S142 combat tables."""
import math
from .common import *
from .art import PROP_FAMILIES

MATERIALS = ["drywall","plaster","brick","concrete","reinforced concrete","cinder block","wood plank","plywood","hardwood","glass (window)","glass (laminated)",
             "glass (vehicle)","steel plate","sheet metal","car door","engine block","oil drum","ceramic tile","sandbag","hay bale","foliage","tree trunk","earth bank",
             "sand","water","rock","flesh","body armor","cloth","plastic","rubber tyre","chain-link","cardboard","ice/foam","bone","coral"]
ANGLES = ["0-30 deg (head-on)", "30-60 deg", "60-90 deg (grazing)"]
ZONES12 = ["head","neck","chest","stomach","pelvis","upper-arm L","upper-arm R","forearm L","forearm R","thigh L","thigh R","calf L","calf R"][:12]
DIR8 = ["front","front-R","right","back-R","back","back-L","left","front-L"]
ATTACH = ["red dot","holo","2x optic","4x optic","8x scope","thermal-lite","suppressor","compensator","long barrel","short barrel","vertical grip","angled grip",
          "extended mag","fast mag","drum mag","skeleton stock","heavy stock","laser","flashlight","bipod","choke (tight)","choke (wide)","speedloader","match trigger","ported barrel"]
TAKEDOWNS = ["silent back","silent front","ledge pull","water drown","cover-over-wall","foliage ambush","vehicle window","aerial drop","knife throw","dog-assisted",
             "neck snap","chokehold","blade rear","blade front","wrench bash","baton strike","garrote rope","disarm+strike","shoulder throw","knee break",
             "pistol whip","door slam","ladder kick","bush drag","tower push","cliff shove","vehicle door crush","comedic slip"]
TD_CTX = ["flat ground","slope 15","stairs","narrow corridor","open jungle","tall grass","shallow water","deep water","night","rain","roof edge","cliff edge","inside vehicle",
          "next to vehicle","doorway","ladder base","behind cover","in foliage","on dock","on beach","in cave","on bridge","alley","warehouse","crowd (civilians near)",
          "with dog","low stamina","injured","enemy reloading","enemy smoking"]
COVER = ["low wall","high wall","corner L","corner R","window","vehicle","crate stack","tree trunk"]
ARCH = ["GRUNT","BRUISER","OFFICER","SNIPER","MEDIC","RUNNER","BREACHER","HEAVY","DRIVER","PILOT","ELITE","ZEALOT","DOG-HANDLER"]
LOADS = ["pistol","smg","carbine","rifle","shotgun","lmg"]
COND = ["day clear","night","rain","fog","storm","dense foliage"]
FLAM = ["dry brush","palm frond","wood structure","grass field","canopy","oil spill","vehicle wreck","straw roof","cane field","crate stack","tarp","cloth tent","fuel depot",
        "wooden dock","bamboo","leaf litter","jungle undergrowth","thatch hut","paper stack","fuel drum","rope bridge","wet wood","green canopy","mangrove","reed bed",
        "cardboard pile","furniture","carpet","tyre pile","hay barn"]
EXPL = ["grenade","frag grenade","flashbang","smoke","molotov","C4","rocket","vehicle explosion","barrel","fuel depot","gunship missile","mortar","dynamite","demolition charge"]
ENVS = ["open ground","jungle dense","inside room","corridor","stairwell","vehicle interior","near water","on dock","bridge","cliff edge","cave","alley","warehouse","rooftop","crowd","beach"]

def impact_vfx(m):
    k = m.lower()
    table_ = [("glass", "glass shards+spray"), ("steel", "sparks+metal ping"), ("sheet metal", "sparks+dent puff"), ("car door", "sparks+paint flecks"),
              ("engine", "sparks+oil mist"), ("oil drum", "sparks+fluid jet"), ("chain", "sparks"), ("brick", "brick dust+chips"), ("concrete", "concrete dust+chips"),
              ("cinder", "block dust+chips"), ("drywall", "white dust puff"), ("plaster", "plaster dust puff"), ("wood", "splinters+sawdust"), ("plywood", "splinters"),
              ("ceramic", "tile shards"), ("sandbag", "sand puff"), ("hay", "straw burst"), ("foliage", "leaf burst"), ("tree", "bark chips"), ("earth", "dirt kick-up"),
              ("sand", "sand spray"), ("water", "splash+ripple"), ("rock", "rock chips+dust"), ("flesh", "blood mist"), ("body armor", "fabric puff+thud"), ("cloth", "fibre puff"),
              ("plastic", "plastic chips"), ("rubber", "rubber puff"), ("cardboard", "paper shred"), ("ice", "foam crumbs"), ("bone", "bone chips"), ("coral", "coral dust")]
    for key, v in table_:
        if key in k: return v
    return "dust"

def gen_142():
    out = sec(142, "COMBAT TABLES (ballistics, penetration, reactions, recoil, attachments, takedowns, destruction, fire, explosives, AI encounters)", [
        "These tables are the DATA SOURCE for DataTables in /Content/Data (10.4). S68/S92 numbers remain authoritative for damage; these add the physics/behaviour layer.",
        "All numbers are starting values; the G-COMBAT gate (134.6) verifies implementation, tuning changes are logged in M-reports."])
    # A: penetration
    out += ["", "## 142.A — PENETRATION & SURFACE RESPONSE (16 weapons x %d materials x 3 angles)" % len(MATERIALS)]
    thick = {m: RI(10, 220, m, "th") for m in MATERIALS}
    hardness = {m: RF(0.05, 1.0, m, "hd") for m in MATERIALS}
    for m, (t, h) in {"drywall": (12, .08), "plaster": (20, .15), "wood plank": (25, .22), "plywood": (18, .18), "foliage": (200, .02), "cardboard": (15, .03), "cloth": (5, .03),
                      "brick": (100, .7), "concrete": (150, .85), "reinforced concrete": (200, .95), "steel plate": (8, 1.0), "engine block": (150, 1.0), "water": (300, .3), "flesh": (200, .15),
                      "sandbag": (250, .5), "hay bale": (300, .1), "glass (window)": (4, .2), "earth bank": (400, .6), "body armor": (12, .9), "rock": (300, .98)}.items():
        thick[m] = t; hardness[m] = h
    rows = []
    i = 0
    for wid, wn, wc, amm in WEAPONS:
        pw = {"9mm": 0.5, ".44": 0.8, "7.62x39": 1.0, "5.56": 0.9, "12ga": 0.6, "7.62x51": 1.4, "rocket": 0.0, "-": 0.0}[amm]
        if wid == "W07": pw = 0.3
        if wid == "W08": pw = 0.7
        for m in MATERIALS:
            for ai, ag in enumerate(ANGLES):
                i += 1
                if pw == 0:
                    if wc == "launcher": res, loss, ric = "detonate", "100%", "-"
                    else: res, loss, ric = "impact-only (melee %s)" % ("cut" if H(wid, m) % 2 else "blunt"), "-", "-"
                else:
                    eff = pw * (1.0 - hardness[m]) * (1.0 - 0.35 * ai) * 260.0
                    if ai == 2 and hardness[m] > 0.55: res, loss, ric = "ricochet", "40%", "%d deg" % RI(5, 35, wid, m, "ric")
                    elif eff >= thick[m]: res, loss, ric = "penetrate", "%d%%" % min(95, int(100 * thick[m] / max(eff, 1))), "-"
                    elif hardness[m] < 0.3 and eff > thick[m] * 0.5: res, loss, ric = "penetrate (partial)", "%d%%" % min(95, int(100 * thick[m] / max(eff, 1)) + 20), "-"
                    elif m in ("glass (window)", "glass (laminated)", "glass (vehicle)"): res, loss, ric = "shatter", "20%", "-"
                    else: res, loss, ric = "stop", "100%", "-"
                rows.append(["PEN-%05d" % i, wid, m, ai + 1, res, loss, ric, "bh-%s-%02d" % (m.split()[0][:6], RI(1, 16, wid, m, ai, "dc")),
                             "AI-noise %d m" % RI(6, 60, wid, "n") if pw else "AI-noise 8 m", impact_vfx(m)])
    out += table(["ID", "WPN", "MATERIAL", "ANGLE_BIN", "RESULT", "ENERGY_LOSS", "RICOCHET", "DECAL", "AI_EVENT", "VFX"], rows)
    # B: hit reactions
    out += ["", "## 142.B — HIT REACTION MATRIX (16 weapons x 12 zones x 8 directions = 1,536 rows)"]
    rows = []
    i = 0
    for wid, wn, wc, amm in WEAPONS:
        for z in ZONES12:
            for d in DIR8:
                i += 1
                mag = {"pistol": 1, "revolver": 3, "smg": 1, "rifle": 2, "carbine": 2, "shotgun": 4, "lmg": 2, "sniper": 5, "dmr": 4, "launcher": 6, "melee": 3, "unarmed": 1}[wc]
                lvl = "flinch-additive" if mag <= 1 else ("stagger" if mag <= 3 else ("knockdown" if mag <= 5 else "launch"))
                if z.startswith("head"): lvl = "head-snap" if mag < 4 else "kill-ragdoll"
                rows.append(["HR-%05d" % i, wid, z, d, lvl, "%d ms" % RI(150, 700, wid, z, d, "dur") if "ragdoll" not in lvl else "death", "PA-blend:%.2f" % RF(0.1, 1.0, wid, z, "pa"),
                             "impulse %d Ns" % (mag * RI(20, 70, wid, z, d, "imp")), "grunt:%s" % PK(["short", "pain", "gasp", "yell"], wid, z, d), "pending"])
    out += table(["ID", "WPN", "ZONE", "FROM", "REACTION", "DURATION", "PHYS_ANIM", "IMPULSE", "VOCAL", "STATUS"], rows)
    # C: recoil
    out += ["", "## 142.C — RECOIL PATTERNS (12 firearms x 30 shots; degrees, +up/+right)"]
    rows = []
    i = 0
    rv = {"W01": .6, "W02": 2.2, "W03": .5, "W04": .55, "W05": 1.1, "W06": .8, "W07": 3.0, "W08": 2.4, "W09": .7, "W10": 4.0, "W11": 2.0, "W12": 5.0}
    rh = {"W01": .2, "W02": .6, "W03": .9, "W04": .7, "W05": .6, "W06": .4, "W07": .5, "W08": .5, "W09": .8, "W10": .3, "W11": .5, "W12": .4}
    for wid, wn, wc, amm in FIREARMS:
        yaw = 0.0
        pitch = 0.0
        for n in range(1, 31):
            i += 1
            dv = rv[wid] * (1.0 if n == 1 else max(0.25, 1.0 - 0.035 * n))
            sway = math.sin(n * (0.9 + H(wid) % 7 * 0.12)) * rh[wid] + (RF(-0.25, 0.25, wid, n, "j") * rh[wid])
            pitch += dv
            yaw += sway
            rows.append(["RC-%04d" % i, wid, n, "%+.2f" % dv, "%+.2f" % sway, "%.2f" % pitch, "%+.2f" % yaw, "%d%%" % (65 if wc != "sniper" else 40), "deterministic"])
    out += table(["ID", "WPN", "SHOT", "DPITCH", "DYAW", "CUM_PITCH", "CUM_YAW", "RECOVER", "MODE"], rows)
    # D: attachments
    out += ["", "## 142.D — ATTACHMENT STAT DELTAS (12 firearms x 25 attachments)"]
    rows = []
    i = 0
    for wid, wn, wc, amm in FIREARMS:
        for a in ATTACH:
            i += 1
            opt = "scope" in a or "optic" in a or a in ("red dot", "holo", "thermal-lite")
            ok = (wc in ("shotgun", "launcher") and a in ("extended mag", "fast mag", "drum mag", "speedloader", "choke (tight)", "choke (wide)")) or wc not in ("launcher",)
            if wc not in ("shotgun",) and a.startswith("choke"): ok = False
            if wc != "revolver" and a == "speedloader": ok = False
            if wc != "lmg" and a in ("bipod", "drum mag"): ok = wc in ("sniper", "dmr") and a == "bipod"
            rows.append(["AT-%04d" % i, wid, a, "Y" if ok else "N (n/a)", "%+d%%" % (RI(-12, 18, wid, a, "rec") if ok else 0), "%+.1f deg" % (RF(-0.4, 0.4, wid, a, "spr") if ok else 0),
                         "%+d ms" % (RI(-40, 60, wid, a, "ads") if ok else 0), "%+.2f kg" % (RF(0.0, 0.9, a, "m") if ok else 0), "suppress -%d dB" % 28 if a == "suppressor" and ok else "-",
                         "zoom x%s" % {"red dot": 1, "holo": 1, "2x optic": 2, "4x optic": 4, "8x scope": 8, "thermal-lite": 3}.get(a, "-") if opt and ok else "-"])
    out += table(["ID", "WPN", "ATTACHMENT", "COMPAT", "RECOIL", "SPREAD", "ADS_TIME", "MASS", "SOUND", "OPTIC"], rows)
    # E: ballistic drop
    out += ["", "## 142.E — BALLISTIC DROP & TIME-OF-FLIGHT (12 firearms x 24 ranges 25..600 m; flat-fire 15 C, sea level)"]
    rows = []
    i = 0
    dragk = {"9mm": 0.0032, ".44": 0.0027, "7.62x39": 0.0014, "5.56": 0.0011, "12ga": 0.0040, "7.62x51": 0.0009, "rocket": 0.0}
    v0s = {"9mm": 360, ".44": 440, "7.62x39": 715, "5.56": 910, "12ga": 450, "7.62x51": 840, "rocket": 120}
    for wid, wn, wc, amm in FIREARMS:
        v0 = v0s[amm]; k = dragk[amm]
        for r in range(25, 601, 25):
            i += 1
            vr = v0 * math.exp(-k * r)
            tof = (math.exp(k * r) - 1) / (k * v0) if k else r / v0
            drop_cm = 0.5 * 9.81 * tof * tof * 100
            if amm == "rocket": drop_cm *= 0.08   # rocket-assisted: thrust-compensated flight
            rows.append(["BD-%04d" % i, wid, amm, r, "%.0f" % vr, "%.3f" % tof, "%.1f" % drop_cm, "%.1f" % (math.degrees(math.atan(drop_cm / 100.0 / r)) * 60), "%d%%" % max(5, int(100 * (vr / v0) ** 2))])
    out += table(["ID", "WPN", "AMMO", "RANGE_M", "VEL_MS", "TOF_S", "DROP_CM", "DROP_MOA", "ENERGY"], rows)
    # F: takedowns
    out += ["", "## 142.F — TAKEDOWN x CONTEXT MATRIX (28 types x 30 contexts = 840)"]
    rows = []
    i = 0
    for t in TAKEDOWNS:
        for c in TD_CTX:
            i += 1
            legal = H(t, c) % 7 != 0
            rows.append(["TD-%04d" % i, t, c, "allowed" if legal else "blocked: geometry", "sync-pair:SP-%03d" % RI(1, 300, t, c) if legal else "-", "%.1f s" % RF(1.2, 4.8, t, c, "len") if legal else "-",
                         "warp:%s" % PK(["both-root", "attacker-root", "victim-root", "pelvis-lock"], t, c), "noise %d m" % RI(0, 12, t, c, "n") if legal else "-", "%s" % PK(["quiet", "loud", "silent", "comedic"], t)])
    out += table(["ID", "TAKEDOWN", "CONTEXT", "ELIGIBILITY", "ANIM_PAIR", "LENGTH", "WARP", "NOISE", "TONE"], rows)
    # G: destructibles
    out += ["", "## 142.G — DESTRUCTIBLE LEDGER (300 objects x 4 fracture tiers = 1,200)"]
    rows = []
    i = 0
    objs = []
    for fam in list(PROP_FAMILIES)[:20]:
        for it in PROP_FAMILIES[fam][5].split(",")[:15]:
            objs.append((fam, it))
    for fam, it in objs:
        for tier, tn in enumerate(["intact", "damaged (cracks)", "fractured (chunks)", "collapsed (debris)"]):
            i += 1
            rows.append(["DX-%04d" % i, fam, it, tn, "HP %d-%d" % (RI(20, 400, fam, it, "hp") * (3 - tier) // 3 + 1, RI(20, 400, fam, it, "hp")), "chunks %d" % (0 if tier < 2 else RI(6, 40, fam, it, "ch")),
                         "cleanup 20 s" if tier >= 2 else "-", "sfx.brk.%s" % fam.split()[0].lower(), "GC" if tier >= 2 else "swap-mesh", "tier-%s" % PROP_FAMILIES[fam][4]])
    out += table(["ID", "FAMILY", "OBJECT", "STATE", "HP_BAND", "CHUNKS", "CLEANUP", "AUDIO", "TECH", "SRC_TIER"], rows)
    # H: fire
    out += ["", "## 142.H — FIRE PROPAGATION (30 materials x 6 wind levels x 4 wetness = 720)"]
    rows = []
    i = 0
    for m in FLAM:
        burn = RI(4, 90, m, "burn")
        ign = RF(0.05, 0.95, m, "ign")
        for w in range(0, 6):
            for wt in WETS_:
                i += 1
                wetk = {"dry": 1.0, "damp": 0.55, "wet": 0.15, "soaked": 0.0}[wt]
                p = min(0.99, ign * wetk * (1 + 0.22 * w))
                rows.append(["FR-%04d" % i, m, "Beaufort %d" % w, wt, "%.2f" % p, "%.1f m" % (RF(0.5, 3.5, m, "rad") * (1 + 0.3 * w) * max(wetk, 0.05)), "%d s" % (burn if wetk else 0),
                             "%s" % PK(["grey", "black", "white", "brown", "orange-tinged"], m), "char+collapse" if burn > 30 else "char"])
    out += table(["ID", "MATERIAL", "WIND", "WETNESS", "IGNITE_P", "SPREAD_R", "BURN_TIME", "SMOKE", "END_STATE"], rows)
    # I: explosives
    out += ["", "## 142.I — EXPLOSIVE x ENVIRONMENT (14 x 16 = 224)"]
    rows = []
    i = 0
    base_r = {"grenade": 6, "frag grenade": 8, "flashbang": 10, "smoke": 12, "molotov": 4, "C4": 9, "rocket": 7, "vehicle explosion": 5, "barrel": 4, "fuel depot": 25,
              "gunship missile": 10, "mortar": 8, "dynamite": 6, "demolition charge": 14}
    base_d = {"grenade": 90, "frag grenade": 120, "flashbang": 0, "smoke": 0, "molotov": 25, "C4": 160, "rocket": 150, "vehicle explosion": 80, "barrel": 70,
              "fuel depot": 250, "gunship missile": 180, "mortar": 140, "dynamite": 100, "demolition charge": 300}
    mult = {"open ground": 1.0, "jungle dense": 0.65, "inside room": 1.5, "corridor": 1.7, "stairwell": 1.6, "vehicle interior": 2.0, "near water": 0.9, "on dock": 1.1,
            "bridge": 1.0, "cliff edge": 1.0, "cave": 1.6, "alley": 1.4, "warehouse": 1.25, "rooftop": 1.0, "crowd": 1.0, "beach": 0.85}
    for e in EXPL:
        for env in ENVS:
            i += 1
            rows.append(["EX-%04d" % i, e, env, "%.1f m" % (base_r[e] * (1.0 + (mult[env] - 1) * 0.4)), "%d" % int(base_d[e] * mult[env]), "%d m" % int(base_r[e] * 4 * mult[env]),
                         "crater %d cm" % (RI(0, 120, e, env, "cr") if base_d[e] > 100 else 0), "tinnitus %.1f s" % RF(0.3, 5.0, e, env, "tin") if base_d[e] or e == "flashbang" else "-",
                         "ignites:%s" % ("Y" if e in ("molotov", "fuel depot", "vehicle explosion", "barrel", "gunship missile") else "N")])
    out += table(["ID", "EXPLOSIVE", "ENVIRONMENT", "KILL_RADIUS", "PEAK_DMG", "HEARING_RADIUS", "CRATER", "EAR_EFFECT", "IGNITES"], rows)
    # J: AI encounters
    out += ["", "## 142.J — AI ENCOUNTER GRID (13 archetypes x 8 cover x 6 loadouts x 6 conditions x 2 difficulty = %d)" % (13 * 8 * 6 * 6 * 2)]
    rows = []
    i = 0
    for a in ARCH:
        for c in COVER:
            for ld in LOADS:
                for cn in COND:
                    for acc in ("fair", "hard"):
                        i += 1
                        react = RI(180, 450, a, "rt") if acc == "fair" else RI(120, 300, a, "rt")
                        rows.append(["EN-%05d" % i, a, c, ld, cn, acc, "react %d ms" % react, "acc %d%%" % (RI(20, 60, a, ld, "ac") if acc == "fair" else RI(45, 85, a, ld, "ac")),
                                     "role:%s" % PK(["SUPPRESS", "FLANK", "RUSH", "OVERWATCH", "BREACH", "RALLY", "HOLD"], a, c, ld),
                                     "vis x%.2f" % ({"day clear": 1.0, "night": 0.45, "rain": 0.7, "fog": 0.35, "storm": 0.5, "dense foliage": 0.6}[cn]), "tokens:%d" % RI(1, 3, a, acc, "tok"), "bot-test:pending"])
    out += table(["ID", "ARCHETYPE", "COVER", "LOADOUT", "CONDITION", "DIFFICULTY", "REACTION", "ACCURACY", "ROLE", "VISIBILITY", "ATTACK_TOKENS", "TEST"], rows)
    return out

WETS_ = ["dry", "damp", "wet", "soaked"]
