"""S141 animation ledger."""
from .common import *

ACTIONS = ["idle-breathe","idle-look","draw","holster","raise-ADS","lower-ADS","fire-single","fire-auto-loop","fire-last-round","dry-fire","reload-tactical",
           "reload-empty","reload-interrupt","reload-shell-insert","chamber-cycle","inspect","weapon-swap-in","weapon-swap-out","sprint-low-ready","melee-bash",
           "throw-grenade","hit-flinch-additive","recoil-additive","equip-attachment","toggle-fire-mode","toggle-light","toggle-laser","lean-left","lean-right",
           "jump-land-weapon","slide-fire","mantle-weapon-stow","vault-weapon-stow","swim-weapon-stow","ladder-stow","death-weapon-drop"]
MELEE_ACTIONS = ["idle-ready","draw","holster","light-swing-1","light-swing-2","light-swing-3","heavy-swing","block","parry","riposte","shove","stab-takedown-start","stagger-recover","throw"]
STANCES = ["stand", "crouch", "cover-high", "cover-low", "moving-ready"]
DIRS = ["F","FR15","FR30","FR45","FR60","FR75","R","BR105","BR120","BR135","BR150","BR165","B","BL-165","BL-150","BL-135"]
SPEEDS = ["0.0","0.5","1.0","1.5","2.0","3.0","4.0","5.0","6.0"]
GAITS = ["unarmed-relaxed", "pistol-ready", "rifle-ready", "heavy-ready", "stealth-crouch", "injured-limp"]
SLOPES = ["flat", "uphill-15", "downhill-15", "stairs"]
TRAV = ["step-up 30cm","step-up 45cm","vault low","vault over-rail","vault fence","vault car-hood","mantle 1.4m","mantle 1.8m","mantle 2.2m","hang-climb 2.6m",
        "hang-climb 3.0m","ledge shimmy L","ledge shimmy R","ledge drop","wall-climb pipe","cliff-climb hand-over-hand","rope-climb up","rope-climb down","rope-slide","rappel start",
        "rappel bounce","rappel land","ladder up","ladder down","ladder mount top","ladder dismount top","zipline grab","zipline ride","zipline dismount","glide launch",
        "glide bank","glide land","swim-entry high","swim-entry low","swim-exit ledge","swim-exit beach","dive-in","surface-breach","window-enter","window-exit"]
TVAR = ["L-hand lead","R-hand lead","armed-stow","unarmed","carrying (crate)","wet","injured","night-stealth","sprint-in","walk-in","dog-assist","low-stamina"]
GEST_V = ["wave","point","beckon","shrug","nod","shake head","thumbs up","facepalm","salute","stop-hand","welcome","dismiss","thinking","count fingers","clap","offer hand","show map","phone-call","radio-call","cheer"]
GEST_M = ["casual","urgent","tired","formal","angry","playful"]
EMO = ["neutral","joy","amusement","pride","relief","affection","calm","curiosity","surprise","awe","confusion","doubt","worry","anxiety","fear","terror","sadness","grief",
       "guilt","shame","embarrassment","disappointment","irritation","anger","rage","contempt","disgust","jealousy","suspicion","smugness","determination","focus","boredom",
       "fatigue","pain","agony","shock","resignation","longing","nostalgia","tenderness","gratitude","hope","triumph","bitterness","sarcasm","flirt","menace","zeal","serenity",
       "panic","dread","amazement","skepticism","sympathy","apology","pleading","defiance","threat","warmth"]
VIS = ["sil","PP","FF","TH","DD","kk","CH","SS","nn","RR","aa","E"]

def gen_141():
    out = sec(141, "ANIMATION LEDGER — weapon actions, motion-matching coverage, traversal, gestures, facial", [
        "Runtime 60 fps (30 fps capture resampled, 115.4). NOTIFIES from 113.6 are mandatory per class; MEM = compressed ACL size estimate.",
        "SRC: CAP = performance capture, LIB = licensed library, PRO = procedural/ML-assisted (validated identically, 115.2). STATUS closes at G-ANIM."])
    out += ["", "## 141.A — WEAPON ACTION CLIPS"]
    rows = []
    i = 0
    for wid, wn, wc, amm in WEAPONS:
        if wc == "unarmed":
            acts = MELEE_ACTIONS
        elif wc == "melee":
            acts = MELEE_ACTIONS
        else:
            acts = ACTIONS
        for st in STANCES:
            for a in acts:
                i += 1
                fr = RI(8, 90, wid, a, "fr") if "loop" not in a and "idle" not in a else RI(60, 240, wid, a, "fr")
                rows.append(["ANM-W%05d" % i, wid, wn, st, a, fr, "loop" if ("loop" in a or "idle" in a or "sprint" in a) else "once",
                             PK(["MagOut,MagIn,BoltRelease", "Foley(Gear)", "ShellEject,Foley", "Whoosh,Impact", "Breath,Foley(Cloth)", "CameraShake,Rumble"], wid, a),
                             "CAP" if H(wid, a, st) % 3 else PK(["LIB", "PRO"], wid, a, st), "%d KB" % (fr * RI(1, 3, wid, a, "kb")), "pending"])
    out += table(["ID", "WPN", "NAME", "STANCE", "ACTION", "FRAMES", "MODE", "NOTIFIES", "SRC", "MEM", "STATUS"], rows)
    out += ["", "## 141.B — MOTION-MATCHING COVERAGE (16 dirs x 9 speeds x 6 gaits x 4 slope states = 3,456 rows)"]
    rows = []
    i = 0
    for g in GAITS:
        for sl in SLOPES:
            for d in DIRS:
                for sp in SPEEDS:
                    i += 1
                    need = 0.0 if sp == "0.0" and d != "F" else 1.0
                    sec_ = round(RF(1.2, 7.0, g, sl, d, sp, "s") * (1.0 if sp != "0.0" else 2.0) * (0.6 if sl != "flat" else 1.0), 1) if need else 0.0
                    rows.append(["MM-%05d" % i, g, sl, d, sp, "%.1f s" % sec_ if need else "derived (idle pivot)", "foot-labels", "pose-search:PSD_%s" % g.split("-")[0],
                                 "%.2f" % RF(0.0, 0.9, g, sl, d, sp, "slide") if False else "<=1.0cm", "pending"])
    out += table(["ID", "GAIT_DB", "SLOPE", "DIR", "SPEED_MS", "CAPTURE_LEN", "LABELS", "SCHEMA", "SLIDE_GATE", "STATUS"], rows)
    out += ["", "## 141.C — TRAVERSAL CLIP LEDGER (40 moves x 12 variants = 480 rows)"]
    rows = []
    i = 0
    for t in TRAV:
        for v in TVAR:
            i += 1
            rows.append(["ANM-T%04d" % i, t, v, RI(30, 150, t, v, "fr"), "warp:%s" % PK(["hands", "feet", "hands+feet", "root", "pelvis"], t), "<=2cm",
                         "MotionWarpWindow,HandOnSurface,Foley" , "CAP" if H(t, v) % 4 else "PRO", "pending"])
    out += table(["ID", "MOVE", "VARIANT", "FRAMES", "WARP_TARGET", "LAND_ERR", "NOTIFIES", "SRC", "STATUS"], rows)
    out += ["", "## 141.D — GESTURES (20 x 6 styles x 5 intensity layers = 600 rows)"]
    rows = []
    i = 0
    for gv in GEST_V:
        for gm in GEST_M:
            for lv in range(1, 6):
                i += 1
                rows.append(["GS-%04d" % i, gv, gm, "L%d" % lv, RI(24, 120, gv, gm, lv, "fr"), "additive-upper" if lv < 4 else "full-upper", "head/eye saccade procedural", "pending"])
    out += table(["ID", "GESTURE", "STYLE", "LAYER", "FRAMES", "BLEND", "PROCEDURAL", "STATUS"], rows)
    out += ["", "## 141.E — FACIAL: %d emotional states x 12 visemes = %d rows" % (len(EMO), len(EMO) * 12)]
    rows = []
    i = 0
    for e in EMO:
        for v in VIS:
            i += 1
            rows.append(["FA-%04d" % i, e, v, "%.2f" % RF(0.3, 1.0, e, v, "w"), "brow:%.2f eye:%.2f mouth:%.2f" % (RF(-1, 1, e, "b"), RF(0, 1, e, "e"), RF(0, 1, e, v, "m")),
                         "wrinkle:%d" % RI(0, 3, e, v, "wr"), "lipsync<=2f", "pending"])
    out += table(["ID", "EMOTION", "VISEME", "WEIGHT", "RIG_PARAMS", "WRINKLE_MAP", "GATE", "STATUS"], rows)
    return out
