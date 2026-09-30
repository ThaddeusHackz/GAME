"""Shared deterministic helpers for the v2 ledger generators (no RNG, no clock)."""
import hashlib, math

def H(*a):
    return int(hashlib.blake2b("|".join(str(x) for x in a).encode(), digest_size=8).hexdigest(), 16)

def RI(lo, hi, *k):
    """deterministic int in [lo, hi]"""
    return lo + H(*k) % (hi - lo + 1)

def RF(lo, hi, *k, nd=2):
    return round(lo + (H(*k) % 1000003) / 1000003.0 * (hi - lo), nd)

def PK(seq, *k):
    return seq[H(*k) % len(seq)]

def sec(n, title, blurb=None):
    out = ["", "# SECTION %d — %s" % (n, title)]
    if blurb:
        out += blurb if isinstance(blurb, list) else [blurb]
    return out

def table(cols, rows):
    out = ["| " + " | ".join(cols) + " |", "|" + "|".join("-" * (len(c) + 2) for c in cols) + "|"]
    out += ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]
    return out

# ---- smooth value noise (deterministic, pure python) -------------------------
def _lat(seed, ix, iy):
    return (H("n", seed, ix, iy) % 100000) / 100000.0

def _sm(t):
    return t * t * (3 - 2 * t)

def vnoise(seed, x, y):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = _sm(x - ix), _sm(y - iy)
    a, b = _lat(seed, ix, iy), _lat(seed, ix + 1, iy)
    c, d = _lat(seed, ix, iy + 1), _lat(seed, ix + 1, iy + 1)
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy

def fbm(seed, x, y, oct=5):
    v, amp, f, tot = 0.0, 0.5, 1.0, 0.0
    for o in range(oct):
        v += amp * vnoise(seed + o * 101, x * f, y * f)
        tot += amp
        amp *= 0.5
        f *= 2.0
    return v / tot

# ---- shared vocabularies (from legacy Parts IV/V; ids bind) ------------------
WEAPONS = [("W01","Culebra","pistol","9mm"),("W02","Bajo Grande","revolver",".44"),("W03","Chispa","smg","9mm"),
           ("W04","Cortavientos","smg","9mm"),("W05","Libertad","rifle","7.62x39"),("W06","Carabina 51","carbine","5.56"),
           ("W07","Festival","shotgun","12ga"),("W08","Trueno","shotgun","12ga"),("W09","Sabado","lmg","5.56"),
           ("W10","Mirada Larga","sniper","7.62x51"),("W11","Vigilante","dmr","7.62x51"),("W12","Ceiba","launcher","rocket"),
           ("M01","Cortesia","melee","-"),("M02","Llave de Marisol","melee","-"),("M03","Bastoon del Sereno","melee","-"),
           ("T00","Unarmed","unarmed","-")]
FIREARMS = WEAPONS[:12]
BASE_DMG = {"W01":24,"W02":55,"W03":16,"W04":20,"W05":28,"W06":26,"W07":30,"W08":11,"W09":13,"W10":120,"W11":85,"W12":90,"M01":45,"M02":55,"M03":60,"T00":12}
MUZZLE_V = {"9mm":360,"9mm ":360,".44":440,"7.62x39":715,"5.56":910,"12ga":450,"7.62x51":840,"rocket":120,"-":0}
PLACES = ["Playa Coralina","Puerto Chico","Selva de Ceiba","Molino Viejo","Las Cascadas","Aldea del Faro","Colina de la Capilla",
          "Cresta Sombria","Cueva del Trueno","Caleta Santa Promesa","El Agujero Azul","Boveda Sumergida","Old Harbor",
          "Downtown Meridian","Solada Beach","Alto Barro","La Herreria","Teatro Alba","Aerodromo Chico","Isla Perdida",
          "Faro Viejo","Harbor Bridge","Marina Slip 9"]
NPCS = [("N01","Ren"),("N02","Tobias"),("N03","Marisol"),("N04","Hands"),("N05","Sereno"),("N06","Sargento"),("N07","Reloj"),
        ("N08","Marea"),("N09","Fraile"),("N10","Crane"),("N11","Rastra"),("N12","Vidente"),("N13","Pulpo"),("N14","Lucky"),
        ("N15","Esteban"),("N16","Viuda"),("N17","Tio Pancho"),("N18","Quintana"),("N19","Camila"),("N20","Vega"),("N21","Beto"),
        ("N22","Chispa"),("N23","El Dedito"),("N24","Hermana Luz"),("N25","Pescador")]
VEHICLES = [("V01","Coralina","car"),("V02","Tiburon GT","car"),("V03","Almuden","car"),("V04","Ranchero","car"),("V05","Guagua","car"),
            ("V06","Sirena","car"),("V07","Fenix","moto"),("V08","Panga","boat"),("V09","Delfin","boat"),("V10","Neumatico","boat"),
            ("V11","Obelisco","car"),("V12","Cosecha Buggy","car")]
TIERS = ["T1","T2","T2H","T3","T4","T5"]
