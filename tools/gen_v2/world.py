"""S137 world cells, S138 lighting/weather grid, S151 wildlife, S152 roads, S154 buildings."""
import math
from .common import *

# ------------------------------------------------------------------ ISLAND
ISL_N, ISL_CELL = 120, 50      # 120 x 120 cells of 50 m = 6 km
BIOMES = {"B01":"Coral Beach","B02":"Mangrove","B03":"Lowland Rainforest","B04":"Cloud Forest","B05":"Volcanic Ridge",
          "B06":"Sugar-Cane Fields","B07":"River Gorge","B08":"Cult Village","B09":"Cave Mouth","B10":"Shallow Reef",
          "B11":"Deep Ocean","B12":"Caldera Lake"}
FOLIAGE = {"B01":400,"B02":1800,"B03":3500,"B04":2800,"B05":350,"B06":6000,"B07":2200,"B08":900,"B09":600,"B10":0,"B11":0,"B12":0}
BED = {"B01":"surf+gulls","B02":"frogs+insects+lap","B03":"canopy-insects+birds","B04":"mist-drip+owls","B05":"wind-howl",
       "B06":"cane-rustle+crickets","B07":"river-roar","B08":"village-life+bell","B09":"cave-drip+bats","B10":"underwater-reef",
       "B11":"open-sea+swell","B12":"lake-lap+loons"}
_isl = None

def island():
    global _isl
    if _isl: return _isl
    N = ISL_N
    E = [[0.0]*N for _ in range(N)]
    RIV = [[False]*N for _ in range(N)]
    cx, cy = 60.0, 60.0
    for y in range(N):
        for x in range(N):
            dx, dy = (x-cx)/57.0, (y-cy)/57.0
            d = math.hypot(dx, dy) * (1 + 0.22*(fbm(11, x*0.04, y*0.04, 3)-0.5))
            mask = max(0.0, 1.0 - d**1.9)
            n = fbm(3, x*0.035, y*0.035, 5)
            ridge = math.exp(-(((y-36)/13.0)**2)) * (0.55+0.45*fbm(5, x*0.08, y*0.08, 3))
            e = mask*(0.30+0.45*n) + 0.38*ridge*mask
            m = e*800.0 - 3.0
            if mask <= 0.0: m = -3.0 - (d-1.0)*70.0
            cd = math.hypot(x-60, y-38)
            if cd < 6: m = 300 - (6-cd)*3           # caldera basin
            E[y][x] = m
            r = abs(fbm(9, x*0.05, y*0.05, 4) - 0.5)
            RIV[y][x] = (r < 0.012 and m > 8 and cd > 7)
    # slope
    S = [[0.0]*N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            gx = (E[y][min(N-1,x+1)] - E[y][max(0,x-1)]) / (2*ISL_CELL)
            gy = (E[min(N-1,y+1)][x] - E[max(0,y-1)][x]) / (2*ISL_CELL)
            S[y][x] = round(math.degrees(math.atan(math.hypot(gx, gy))), 1)
    # biome
    B = [[None]*N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            m, s = E[y][x], S[y][x]
            moist = fbm(21, x*0.06, y*0.06, 3)
            cd = math.hypot(x-60, y-38)
            if cd < 6 and m <= 302: b = "B12"
            elif m <= -12: b = "B11"
            elif m <= 0: b = "B10"
            elif m < 14: b = "B02" if fbm(77, x*0.08, y*0.08, 3) > 0.56 else "B01"
            elif RIV[y][x]: b = "B07"
            elif m > 430: b = "B05"
            elif m > 300: b = "B04"
            elif m < 150 and s < 12 and fbm(31, x*0.1, y*0.1, 2) > 0.47: b = "B06"
            else: b = "B03"
            B[y][x] = b
    # village discs (B08) before POIs
    vc = sorted((H("vill", x, y), x, y) for y in range(10, N-10) for x in range(10, N-10)
                if B[y][x] in ("B03", "B06") and S[y][x] < 10 and 6 < E[y][x] < 160)
    villages = []
    for _, x, y in vc:
        if len(villages) >= 7: break
        if all(max(abs(x-a), abs(y-b)) >= 16 for a, b in villages): villages.append((x, y))
    for vx, vy in villages:
        for yy in range(vy-2, vy+3):
            for xx in range(vx-2, vx+3):
                if B[yy][xx] in ("B03", "B06") and math.hypot(xx-vx, yy-vy) <= 2.3: B[yy][xx] = "B08"
    # POI placement (greedy, spaced)
    poi = {}
    taken = []
    def place(tag, allowed, n, spacing, pref=None, maxslope=30):
        cands = [(H("poi", tag, x, y), x, y) for y in range(4, N-4) for x in range(4, N-4)
                 if B[y][x] in allowed and S[y][x] <= maxslope and (x, y) not in poi and (pref is None or pref(x, y))]
        cands.sort()
        cnt = 0
        for _, x, y in cands:
            if cnt >= n: break
            if all(max(abs(x-a), abs(y-b)) >= spacing for a, b in taken):
                poi[(x, y)] = "%s" % (tag % (cnt+1) if "%" in tag else tag)
                taken.append((x, y)); cnt += 1
    land = {"B01","B02","B03","B04","B06","B08","B05"}
    place("ST%d", {"B03","B08","B04"}, 3, 14, maxslope=18)
    place("OP%02d", {"B03","B08","B01","B06","B04"}, 24, 7, maxslope=16)
    place("MAST%d", {"B05","B04","B03"}, 8, 11, maxslope=25, pref=lambda x, y: E[y][x] > 120)
    place("CAVE%d", {"B03","B04","B05","B07"}, 8, 8, maxslope=40)
    place("HUNT%02d", {"B03","B04","B06","B02"}, 20, 6, maxslope=14)
    place("POI%02d", land | {"B07","B09"}, 60, 4, maxslope=22)
    place("CACHE%03d", land | {"B07"}, 120, 2, maxslope=35)
    for (px, py), tg in list(poi.items()):
        if tg.startswith("CAVE"):
            for yy in range(py-1, py+2):
                for xx in range(px-1, px+2):
                    if B[yy][xx] in ("B03", "B04", "B05", "B07"): B[yy][xx] = "B09"
    named = [("Playa Coralina", {"B01"}, lambda x,y: y>80),("Puerto Chico", {"B01","B03"}, lambda x,y: x<45 and y>60),
             ("Faro Viejo", {"B01","B03"}, lambda x,y: y<40 and x>60),("Aldea del Faro", {"B03","B08"}, lambda x,y: y<45),
             ("Molino Viejo", {"B06","B03"}, lambda x,y: x>60 and y>55),("Las Cascadas", {"B07","B03"}, lambda x,y: True),
             ("Selva de Ceiba", {"B03"}, lambda x,y: 40<x<80 and 50<y<85),("Colina de la Capilla", {"B03","B04"}, lambda x,y: x<60),
             ("Cresta Sombria", {"B05"}, lambda x,y: True),("Caleta Santa Promesa", {"B01","B02"}, lambda x,y: x>75),
             ("Laguna Quieta", {"B12"}, lambda x,y: True),("Isla Perdida (islet gate)", {"B01"}, lambda x,y: x>95)]
    for nm, al, pf in named:
        c = sorted((H("named", nm, x, y), x, y) for y in range(4, N-4) for x in range(4, N-4)
                   if B[y][x] in al and (x, y) not in poi and pf(x, y))
        if c:
            _, x, y = c[0]; poi[(x, y)] = "LM:" + nm
    _isl = (E, S, B, poi)
    return _isl

def gen_137():
    E, S, B, poi = island()
    out = sec(137, "WORLD CELL GRID — ISLA SOMBRA (120x120 @ 50 m = 14,400 cells) + MERIDIAN (80x80 @ 100 m = 6,400 cells)", [
        "Each row is one World-Partition authoring cell. ELEV from the heightfield blockout model (Gaea/WorldCreator",
        "replaces it 1:1 at the same cell resolution); BIOME per 105.3; FOLIAGE = target Nanite instances/ha (105.4);",
        "POI = placed content (OP=outpost, MAST=signal mast, ST=stronghold, HUNT=hunting ground, CAVE, POI, CACHE, LM=landmark).",
        "STREAM: core (<=1 km of a hub), mid, far (HLOD-only until approach). QA column is closed by G-WORLD (134.7)."])
    out += ["", "## 137.A — ISLA SOMBRA CELL GRID"]
    cols = ["CELL", "XY", "ELEV_M", "SLOPE", "BIOME", "FOLIAGE/HA", "POI", "AUDIO BED", "STREAM", "TRAVERSAL", "QA"]
    rows = []
    for y in range(ISL_N):
        for x in range(ISL_N):
            b, e, s = B[y][x], E[y][x], S[y][x]
            fo = int(FOLIAGE[b] * (0.75 + 0.5 * ((H("fo", x, y) % 1000) / 1000.0))) if FOLIAGE[b] else 0
            dist = math.hypot(x-60, y-60)
            st = "core" if dist < 20 else ("mid" if dist < 40 else "far")
            if b in ("B10","B11","B12"): tr = "swim/boat"
            elif s > 55: tr = "cliff (climb/rappel)"
            elif s > 32: tr = "steep (slide/climb)"
            elif b == "B07": tr = "river (wade/jump)"
            else: tr = "walk"
            rows.append(["ISL-x%03dy%03d" % (x, y), "%d,%d" % (x, y), int(e), s, "%s %s" % (b, BIOMES[b]), fo,
                         poi.get((x, y), "-"), BED[b], st, tr, "PENDING"])
    out += table(cols, rows)
    # ---------------- MERIDIAN
    out += ["", "## 137.B — MERIDIAN CELL GRID (districts per 106.7)"]
    cols = ["CELL", "XY", "DISTRICT", "LANDUSE", "ELEV_M", "BLDGS", "HMAX_M", "ROAD", "VEH/KM2", "PED/KM2", "ENTERABLE", "POI", "NIGHT_LIGHT", "QA"]
    rows = []
    for c in meridian():
        rows.append(["MER-x%02dy%02d" % (c["x"], c["y"]), "%d,%d" % (c["x"], c["y"]), c["district"], c["landuse"], c["elev"],
                     c["bldgs"], c["hmax"], c["road"], c["veh"], c["ped"], c["enter"], c["poi"], c["night"], "PENDING"])
    out += table(cols, rows)
    return out

# ------------------------------------------------------------------ CITY
CITY_N = 80
_city = None
def meridian():
    global _city
    if _city: return _city
    N = CITY_N
    cells = []
    cx, cy = 40, 42
    # hero block placement deterministic
    hero = set()
    cand = sorted((H("hero", x, y), x, y) for y in range(6, 76) for x in range(10, 76))
    for _, x, y in cand:
        if len(hero) >= 60: break
        if all(max(abs(x-a), abs(y-b)) >= 6 for a, b in hero): hero.add((x, y))
    hero_names = ["Bank","Police HQ","Mansion","Stadium","Mall","Hospital","University","Prison Gate","Opera","Cathedral",
                  "Central Station","Casino","Courthouse","Museum","Lighthouse","Aquarium","Brewery","Radio Tower","Tech Campus","Ferry Terminal"]
    hi = 0
    for y in range(N):
        for x in range(N):
            coast_w = 5 + 4*fbm(41, x*0.12, y*0.12, 3) * 3
            coast_s = 72 - 3*fbm(43, x*0.1, y*0.1, 3) * 4
            d = math.hypot(x-cx, y-cy)
            if x < coast_w:
                dist, lu = "HARBOR WATER", "water"
            elif y > coast_s:
                dist, lu = "OPEN SEA", "water"
            elif x > 66 and 12 < y < 26:
                dist, lu = "AERODROMO", "airfield"
            elif d < 9:
                dist, lu = "DOWNTOWN", "commercial-core"
            elif x < 22 and 40 < y < 68:
                dist, lu = "OLD HARBOR", "port-warehouse"
            elif x > 58 and 26 <= y < 62 and x < 72:
                dist, lu = "SOLADA BEACH", "tourist-strip"
            elif 26 < x < 56 and y < 24:
                dist, lu = "ALTO BARRO", "hillside-informal"
            elif 38 < x < 70 and y > 58 and y <= 72:
                dist, lu = "LA HERRERIA", "industrial-rail"
            elif d > 34 or x > 72:
                dist, lu = "COUNTRYSIDE", "rural-fringe"
            else:
                dist, lu = "RESIDENTIAL", "residential"
            elev = 0 if lu == "water" else 2 + int(6*fbm(47, x*0.05, y*0.05, 3))
            if dist == "ALTO BARRO": elev = 20 + int((24 - y) * 4.5 + 25*fbm(49, x*0.1, y*0.1, 3))
            if dist == "COUNTRYSIDE": elev += int(40*fbm(51, x*0.04, y*0.04, 3))
            if lu == "water": bl, hm, road, veh, ped, en, night = 0, 0, "none", 0, 0, 0, 0
            else:
                base = {"DOWNTOWN":(18,250),"OLD HARBOR":(12,30),"SOLADA BEACH":(14,45),"ALTO BARRO":(30,16),"LA HERRERIA":(6,22),
                        "RESIDENTIAL":(16,24),"COUNTRYSIDE":(3,9),"AERODROMO":(2,18)}[dist]
                bl = max(0, int(base[0] * (0.6 + 0.8*((H("bl", x, y) % 100)/100.0))))
                hm = max(3, int(base[1] * (0.4 + 1.0*((H("hm", x, y) % 100)/100.0))))
                cls = {"DOWNTOWN":"ARTERIAL","OLD HARBOR":"COLLECTOR","SOLADA BEACH":"PROMENADE","ALTO BARRO":"ALLEY/STAIR",
                       "LA HERRERIA":"INDUSTRIAL","RESIDENTIAL":"LOCAL","COUNTRYSIDE":"RURAL","AERODROMO":"SERVICE"}[dist]
                if x % 8 == 0 or y % 8 == 0: cls = "HIGHWAY" if (x % 40 == 0 or y % 40 == 0) else "ARTERIAL"
                road = cls
                veh = {"HIGHWAY":900,"ARTERIAL":620,"COLLECTOR":360,"PROMENADE":260,"INDUSTRIAL":280,"LOCAL":140,"ALLEY/STAIR":30,"RURAL":40,"SERVICE":60}[cls]
                veh = int(veh * (0.7 + 0.6*((H("vh", x, y) % 100)/100.0)))
                ped = {"DOWNTOWN":2500,"SOLADA BEACH":2200,"OLD HARBOR":700,"ALTO BARRO":1800,"LA HERRERIA":250,"RESIDENTIAL":900,"COUNTRYSIDE":60,"AERODROMO":80}[dist]
                ped = int(ped * (0.6 + 0.8*((H("pd", x, y) % 100)/100.0)))
                en = int(bl * {"DOWNTOWN":0.55,"SOLADA BEACH":0.5,"OLD HARBOR":0.35,"ALTO BARRO":0.25,"RESIDENTIAL":0.4,"LA HERRERIA":0.2,"COUNTRYSIDE":0.1,"AERODROMO":0.1}[dist])
                night = {"DOWNTOWN":9,"SOLADA BEACH":8,"OLD HARBOR":5,"ALTO BARRO":6,"LA HERRERIA":4,"RESIDENTIAL":4,"COUNTRYSIDE":1,"AERODROMO":3}[dist]
            poi = "-"
            if (x, y) in hero:
                poi = "HERO:" + hero_names[hi % len(hero_names)] + (" %d" % (hi//len(hero_names)+1) if hi >= len(hero_names) else "")
                hi += 1
            elif lu != "water" and H("v", x, y) % 61 == 0: poi = "VENDOR"
            elif lu != "water" and H("r", x, y) % 89 == 0: poi = "RACE-GATE"
            elif lu != "water" and H("j", x, y) % 131 == 0: poi = "STUNT-JUMP"
            elif lu != "water" and H("d", x, y) % 97 == 0: poi = "DUCK"
            cells.append(dict(x=x, y=y, district=dist, landuse=lu, elev=elev, bldgs=bl, hmax=hm, road=road, veh=veh, ped=ped, enter=en, poi=poi, night=night))
    _city = cells
    return cells

# ------------------------------------------------------------------ LIGHTING / WEATHER
WEATHER = [("Clear",5,0.97,0.0010,0.00,3.0,12000),("Hazy",20,0.85,0.0030,0.05,2.5,6000),("Partly Cloudy",45,0.80,0.0015,0.05,5.0,14000),
           ("Overcast",90,0.28,0.0020,0.15,4.0,9000),("Drizzle",95,0.22,0.0035,0.45,4.5,4500),("Rain",100,0.15,0.0050,0.80,6.0,2800),
           ("Storm",100,0.07,0.0070,1.00,14.0,1400),("Tropical Squall",98,0.10,0.0080,0.95,18.0,900),("Fog/Mist",85,0.35,0.0200,0.30,1.0,220),
           ("Heat Shimmer",0,1.00,0.0008,0.00,1.5,15000),("Night Clear",0,1.0,0.0008,0.00,2.0,600),("Night Storm",100,0.07,0.0070,1.00,12.0,300)]
ZONES = [("Beach",1.0,0.4),("Rainforest floor",0.12,0.0),("Cloud forest",0.30,0.0),("Volcanic ridge",1.0,0.0),
         ("Open sea",1.0,0.0),("City downtown",0.55,22.0),("City harbor",0.85,7.0),("Cave mouth",0.06,0.0)]

def sun(hour, lat=11.0, dec=0.0):
    h = math.radians(15.0 * (hour - 12.0)); la = math.radians(lat); de = math.radians(dec)
    se = math.sin(la)*math.sin(de) + math.cos(la)*math.cos(de)*math.cos(h)
    el = math.degrees(math.asin(se))
    az = math.degrees(math.atan2(math.sin(h), math.cos(h)*math.sin(la) - math.tan(de)*math.cos(la)))
    return el, (az + 180.0) % 360.0

def gen_138():
    out = sec(138, "LIGHTING & WEATHER GRID — 48 TOD slots x 12 weather x 8 lighting zones = 4,608 rows", [
        "Sun position: lat 11.0 N, equinox geometry (108.1). EV100 = log2(E_lux/2.5). E = (direct+sky)*zone_factor + artificial.",
        "Used by G-ART histogram bands (108.8): expected EV is the TARGET; auto-exposure clamps per 108.3; +/-1.0 EV tolerance."])
    cols = ["ID", "TOD", "SUN_EL", "SUN_AZ", "WEATHER", "ZONE", "CLOUD%", "SUN_LUX", "SKY_LUX", "EV100", "SKY_K", "FOG_DENS", "WETNESS", "WIND_MS", "VIS_M", "GAMEPLAY"]
    rows = []
    i = 0
    for slot in range(48):
        hr = slot / 2.0
        el, az = sun(hr)
        for wi, (wn, cl, tr, fog, wet, wind, vis) in enumerate(WEATHER):
            night_w = wn.startswith("Night")
            for zn, zf, art in ZONES:
                i += 1
                if el > 0:
                    direct = 110000.0 * (math.sin(math.radians(el)) ** 0.75) * tr if not night_w else 0.0
                    sky = 18000.0 * (0.25 + 0.75*math.sin(math.radians(el))) * (0.6 + 0.4*(1-cl/100.0)) if not night_w else 0.0
                else:
                    direct = 0.0
                    twi = max(0.0, 1.0 + el/6.0)            # civil twilight ~ -6 deg
                    sky = 400.0 * twi**3 + 0.25 * (0.4 if cl > 80 else 1.0)
                    if night_w and el > 0: sky = 0.25
                E = (direct + sky) * zf + (art if el < 4 else art * 0.3)
                ev = math.log2(max(E, 0.01) / 2.5)
                k = int(3200 + 2600*max(0, math.sin(math.radians(max(el, 0)))) + (1200 if cl > 80 else 0)) if el > -6 else 9500
                if el <= 0: k = 9500 if zn.startswith("City") is False else 4200
                vis_m = vis if not (night_w and zn.startswith("City")) else vis * 2
                gp = []
                if ev < 2: gp.append("stealth+")
                if wn in ("Rain","Storm","Tropical Squall","Night Storm","Drizzle"): gp.append("noise-mask")
                if fog > 0.01: gp.append("detect-range-x0.5")
                if wn == "Heat Shimmer": gp.append("mirage-fx")
                rows.append(["LG-%05d" % i, "%02d:%02d" % (int(hr), int((hr % 1)*60)), round(el, 1), round(az, 0) if el > -18 else "-", wn, zn, cl,
                             int(direct), int(sky*zf), round(ev, 1), k, fog, wet, wind, vis_m, ",".join(gp) or "-"])
    out += table(cols, rows)
    return out

# ------------------------------------------------------------------ WILDLIFE
SPECIES = [("boar","mammal","B03,B06",80,7.5),("chancho","mammal","B03,B02",60,6.5),("capybara-like","mammal","B02,B07",55,4.5),
 ("jaguar","predator","B03,B04",150,12.0),("ocelot","predator","B03",60,10.0),("caiman","reptile","B02,B07",120,4.0),("iguana","reptile","B01,B03",8,4.0),
 ("coati","mammal","B03",15,5.0),("howler monkey","primate","B03,B04",20,6.0),("spider monkey","primate","B03",12,8.0),
 ("agouti","mammal","B03,B06",10,7.0),("tapir","mammal","B03,B07",200,6.0),("sloth","mammal","B03",8,0.5),("anteater","mammal","B06,B03",40,4.0),
 ("macaw","bird","B03,B04",1,14.0),("toucan","bird","B03",1,12.0),("heron","bird","B02,B07,B12",2,10.0),("pelican","bird","B01,B10",5,13.0),
 ("frigatebird","bird","B11,B01",2,17.0),("gull","bird","B01,B11",1,12.0),("vulture","bird","B05,B06",3,15.0),("harpy eagle","predator-bird","B04,B03",6,20.0),
 ("owl","bird","B03,B04",1,11.0),("bat colony","flock","B09,B03",0.05,9.0),("crab","crustacean","B01,B02",0.5,1.5),("hermit crab","crustacean","B01",0.2,0.5),
 ("sea turtle","reptile","B01,B10",100,1.5),("reef shark","predator-fish","B10,B11",80,6.0),("ray","fish","B10",40,3.0),("barracuda","fish","B10,B11",15,8.0),
 ("reef fish school","flock","B10",0.3,2.0),("tarpon","fish","B02,B10",60,7.0),("river trout-like","fish","B07",2,3.0),("eel","fish","B02,B07",3,2.0),
 ("pit viper","reptile","B03,B06",2,1.5),("tarantula","arthropod","B03,B09",0.1,0.5),("fireflies","flock","B03,B02",0.0,1.0),("dragonflies","flock","B07,B12",0.0,3.0),
 ("feral dog","predator","B08,B06",25,8.0),("goat","mammal","B05,B08",45,6.0)]
BEHAV = ["idle","graze/forage","drink","patrol-territory","rest/sleep","groom","flee-light","flee-panic","alert-freeze","stalk","chase","attack",
         "ambush","call/alarm","mate/display","migrate-herd","swim/fly-transit","hide-burrow","investigate-player","habituated-approach",
         "injured-limp","death-fall","scavenge","react-fire-weather"]

def gen_151():
    out = sec(151, "ECOLOGY LEDGER — 40 SPECIES x 24 BEHAVIORS (+ roster) = 1,000 rows", [
        "Mass LOD: actor <=30 m (skeletal, MM-quad DB), instanced-skinned 30-120 m, dot 120-400 m, sim-only beyond (124.1).",
        "TRIGGER thresholds are starting values; designers tune in DataAssets. Spawn/despawn NEVER in view (124.6)."])
    out += ["", "## 151.A — SPECIES ROSTER"]
    rows = []
    for i, (n, cl, bm, kg, sp) in enumerate(SPECIES, 1):
        rows.append(["SP-%02d" % i, n, cl, bm, kg, sp, PK(["diurnal","nocturnal","crepuscular","cathemeral"], n),
                     "%d-%d" % (RI(1, 4, n, "lo"), RI(5, 40, n, "hi")), {"predator":"hunt-drop: pelt","mammal":"hunt-drop: meat+hide"}.get(cl, "-"), "SK-%s" % cl.split("-")[0]])
    out += table(["ID", "SPECIES", "CLASS", "BIOMES", "MASS_KG", "TOP_MS", "ACTIVITY", "GROUP", "HUNT_DROP", "SKELETON"], rows)
    out += ["", "## 151.B — BEHAVIOR LEDGER"]
    rows = []
    trig = ["no threat <30 m", "hunger>0.6", "thirst>0.6", "TOD dusk/dawn", "fatigue>0.7", "player<12 m", "gunshot <80 m", "fire <60 m",
            "predator<25 m", "prey<35 m", "rain", "storm", "player crouched", "dog present", "night", "injury>0.3"]
    i = 0
    for si, (n, cl, bm, kg, sp) in enumerate(SPECIES, 1):
        for bi, b in enumerate(BEHAV):
            i += 1
            rows.append(["WLD-%04d" % i, n, b, PK(trig, n, b, "t"), "MM-%s/%s" % (cl.split("-")[0], b.split("/")[0].split("-")[0]),
                         "aud.wld.%s.%s" % (n.split()[0].split("-")[0], b.split("/")[0].replace("-", "_")), RI(2, 12, n, b, "pri"),
                         "Mass>=30m" if H(n, b) % 3 else "Actor-only"])
    out += table(["ID", "SPECIES", "BEHAVIOR", "TRIGGER", "ANIM_SET", "AUDIO_ID", "PRIORITY", "LOD_RULE"], rows)
    return out

# ------------------------------------------------------------------ ROADS
def gen_152():
    cells = meridian()
    out = sec(152, "ROAD NETWORK LEDGER — one row per road cell of Meridian (traffic graph nodes/edges)", [
        "LANES/SPEED by class; SIGNAL at junction cells (x%8==0 and y%8==0); STREETLIGHT by district (106.6); SURFACE feeds 145/143 grip.",
        "Traffic graph export: tools/export_traffic_graph.py reads this ledger + spline assets and emits Mass Traffic lane data."])
    cls = {"HIGHWAY":(6,100,"none","LED cobra"),"ARTERIAL":(4,60,"signal","LED cobra"),"COLLECTOR":(2,50,"signal/stop","sodium"),
           "PROMENADE":(2,30,"crosswalk","decor LED"),"INDUSTRIAL":(2,50,"stop","sulfur"),"LOCAL":(2,40,"stop","sodium/LED"),
           "ALLEY/STAIR":(1,15,"none","wall bulbs"),"RURAL":(2,70,"none","none"),"SERVICE":(2,40,"gate","LED")}
    rows = []
    i = 0
    for c in cells:
        if c["road"] == "none": continue
        ln, sp, sig, lt = cls[c["road"]]
        junction = (c["x"] % 8 == 0 and c["y"] % 8 == 0)
        surf = {"HIGHWAY":"asphalt-new","ARTERIAL":"asphalt","COLLECTOR":"asphalt-worn","PROMENADE":"pavers","INDUSTRIAL":"concrete/asphalt-rough",
                "LOCAL":"asphalt-worn","ALLEY/STAIR":"concrete-stairs/cobble","RURAL":"asphalt-cracked/dirt","SERVICE":"concrete"}[c["road"]]
        i += 1
        oneway = "Y" if (c["road"] in ("LOCAL","COLLECTOR","ALLEY/STAIR") and H("ow", c["x"], c["y"]) % 3 == 0) else "N"
        rows.append(["RD-%05d" % i, "%d,%d" % (c["x"], c["y"]), c["district"], c["road"], ln, sp,
                     (sig + " [junction]") if junction and sig != "none" else ("junction-uncontrolled" if junction else "-"),
                     oneway, surf, lt, "park:%s" % ("street" if c["road"] in ("LOCAL","COLLECTOR") else "none"),
                     "bus" if (H("bus", c["x"], c["y"]) % 23 == 0 and c["road"] in ("ARTERIAL","COLLECTOR")) else "-"])
    out += table(["ID", "XY", "DISTRICT", "CLASS", "LANES", "LIMIT_KPH", "CONTROL", "ONEWAY", "SURFACE", "STREETLIGHT", "PARKING", "TRANSIT"], rows)
    return out

# ------------------------------------------------------------------ BUILDINGS
DIST_TYPES = {
 "DOWNTOWN": ["glass office tower","stone bank","hotel tower","mixed-use podium","parking structure","civic hall","luxury condo","data center","transit hub","department store"],
 "OLD HARBOR": ["brick warehouse","fish market hall","customs house","chandlery","cold-storage block","ferry shed","tenement","sailors' hostel","net loft","dry-dock office"],
 "SOLADA BEACH": ["pastel hotel","beach bar","dive shop","boutique villa","surf school","marina club","ice-cream parlor","timeshare block","aquarium annex","promenade arcade"],
 "ALTO BARRO": ["stacked brick house","corrugated shack","rooftop chapel","community kitchen","tin-roof workshop","stair-side shop","water-tank house","radio shack","mural wall block","cantilever dwelling"],
 "LA HERRERIA": ["steel foundry","rail depot","container stack office","tank farm control","boiler house","freight shed","crane cabin","scrapyard office","signal tower","loco workshop"],
 "RESIDENTIAL": ["townhouse","courtyard apartments","duplex","corner shop","school","clinic","church","laundromat","family villa","walk-up block"],
 "COUNTRYSIDE": ["farmhouse","barn","roadside diner","gas station","winery shed","greenhouse","radio mast hut","chapel","orchard shed","motel"],
 "ISLAND VILLAGE": ["stilt house","palm-thatch hut","cult meeting hall","sugar mill","smokehouse","boat shed","lighthouse keeper house","cistern","shrine","watchtower"]}
SIZES = ["S","M","L","XL","HERO"]
VARS = ["pristine","weathered","tropical-stained","graffiti","damaged","night-lit"]

def gen_154():
    out = sec(154, "BUILDINGS & INTERIORS LEDGER — 400 archetypes x 6 variants = 2,400 rows; + 60 room templates x 20 dressings = 1,200 rows", [
        "Archetype = district x type x size. Kit modules snap to 50 cm (104.5). Nanite budget is TOTAL visible facade+roof tris at LOD0.",
        "Window-interior mapping on non-enterable facades; ENTERABLE = real streamed rooms (123.3). HERO archetypes are hand-dressed (106.3)."])
    out += ["", "## 154.A — BUILDING ARCHETYPE VARIANTS"]
    rows = []
    i = 0
    for d, types in DIST_TYPES.items():
        for t in types:
            for sz in SIZES:
                a = "BA-%s-%s-%s" % (d.split()[0][:3], t.split()[0][:4].upper(), sz)
                fl = {"S":(1,2),"M":(2,4),"L":(4,8),"XL":(8,30),"HERO":(3,60)}[sz]
                if d == "DOWNTOWN" and sz in ("L","XL","HERO"): fl = (12, 70)
                if d in ("ALTO BARRO","ISLAND VILLAGE","COUNTRYSIDE") and sz in ("XL","HERO"): fl = (3, 6)
                for v in VARS:
                    i += 1
                    f = RI(fl[0], fl[1], a, v, "fl")
                    foot = "%dx%d" % (RI(6, 14, a, "w") * {"S":1,"M":1.5,"L":2,"XL":3,"HERO":4}[sz], RI(6, 20, a, "d") * {"S":1,"M":1.5,"L":2,"XL":3,"HERO":4}[sz])
                    tris = RI(40, 400, a, v, "tr") * 1000 * (f ** 0.5) / 2
                    rows.append(["BV-%05d" % i, a, d, t, sz, v, foot, f, RI(14, 220, a, "mods") + f * 6, int(tris),
                                 "%dK" % PK([2, 4, 4, 8], a, v, "tx"), "Y" if (sz in ("HERO", "XL") or H(a) % 3 == 0) else "interior-mapping",
                                 "P%d" % PK([1, 2, 2, 2, 3], a, v, "src"), "PENDING"])
    out += table(["ID", "ARCHETYPE", "DISTRICT", "TYPE", "SIZE", "VARIANT", "FOOTPRINT_M", "FLOORS", "MODULES", "NANITE_TRIS", "TEX", "ENTERABLE", "SRC", "STATUS"], rows)
    out += ["", "## 154.B — INTERIOR ROOM TEMPLATES x DRESSING PERMUTATIONS"]
    rooms = ["studio flat","bedroom","kitchen","bathroom","living room","corner shop floor","bar","restaurant kitchen","bank vault","bank lobby",
             "police briefing room","cell block","office open-plan","server room","warehouse floor","cold room","workshop","foundry floor","chapel nave","sacristy",
             "hotel room","hotel lobby","nightclub","stairwell","elevator lobby","parking level","rooftop access","clinic ward","operating room","school classroom",
             "cult hall","smokehouse","sugar-mill gear room","boiler room","lighthouse lamp room","boat shed","cistern","shrine chamber","stilt hut","watch-tower cab",
             "radio studio","mansion study","mansion ballroom","casino floor","arcade","laundromat","barber shop","pharmacy","library","museum gallery",
             "aquarium tunnel","ferry waiting hall","train cab","container office","crane cabin","cave camp","mine gallery","safehouse","garage","penthouse"]
    dress = ["tidy","lived-in","messy","abandoned","raided","night-shift","festival","hoarder","minimalist","luxury","squalid","flooded-ankle","fire-damaged",
             "under-renovation","cult-decor","police-seized","hidden-stash","party-aftermath","storm-shuttered","empty"]
    rows = []
    i = 0
    for r in rooms:
        for dr in dress:
            i += 1
            area = RI(6, 140, r, "area")
            rows.append(["IR-%04d" % i, r, dr, area, "%.1f" % RF(2.4, 4.2, r, "h"), RI(18, 160, r, dr, "props"), RI(1, 9, r, dr, "lights"),
                         "%s" % PK(["lamp","window","overhead","neon","candle","screen","fire"], r, dr, "key"), RI(2, 8, r, dr, "exits") // 2 or 1,
                         "Y" if H(r, dr) % 4 == 0 else "N", "smart-objects:%d" % RI(2, 14, r, dr, "so"), "PENDING"])
    out += table(["ID", "ROOM", "DRESSING", "AREA_M2", "CEIL_M", "PROPS", "LIGHTS", "KEY_LIGHT", "EXITS", "LOOT", "SMART_OBJ", "STATUS"], rows)
    return out
