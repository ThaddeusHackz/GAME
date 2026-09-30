"""S139 props, S140 vegetation, S144 characters, S145 surfaces+foley, S146 VFX, S155 shader permutations."""
from .common import *

# name -> (mass_kg, tri_lo_k, tri_hi_k, phys_material, src_bias, items...)
PROP_FAMILIES = {
 "Street furniture": (25, 8, 60, "metal-painted", "P2", "bench,bus shelter,bollard,trash bin,newspaper box,hydrant,street sign,lamp post,parking meter,planter,phone booth,bike rack,guard rail,crosswalk button,kiosk"),
 "Utilities": (80, 6, 50, "metal-rusted", "P2", "transformer,pole,junction box,cable spool,manhole cover,water meter,gas tank,generator,pipe run,valve wheel,AC unit,satellite dish,antenna,fuse cabinet,pump"),
 "Vehicles static": (900, 60, 400, "metal-vehicle", "P4", "abandoned sedan,burnt van,pickup wreck,tuk cart,bicycle,moped,handcart,fishing truck,taxi roof sign,tow hook,tyre stack,car door,engine block,axle,bus seat"),
 "Household": (8, 3, 40, "wood-furniture", "P2", "sofa,armchair,table,chair,bed frame,wardrobe,shelf,lamp,rug,fan,radio,TV,fridge,stove,sink"),
 "Kitchen & food": (2, 2, 20, "ceramic-glass", "P1", "pot,pan,plate stack,mug,bottle,can,crate of fruit,sack of rice,chili string,fish rack,tortilla press,coffee maker,knife block,jar,basket"),
 "Market": (15, 4, 45, "cloth-wood", "P2", "stall frame,awning,scale,fruit pyramid,fish ice box,flower bucket,spice sack,cloth bolt,cart,price board,umbrella,crate stack,cash box,hanging hams,straw hat"),
 "Industrial": (300, 10, 120, "metal-heavy", "P2", "gear wheel,boiler,conveyor segment,steel beam,pallet stack,forklift,oil drum,chain hoist,lathe,anvil,crucible,welding rig,barrel rack,toolbox,crane hook"),
 "Port & maritime": (120, 8, 90, "rope-metal-wood", "P2", "mooring bollard,rope coil,fishing net,buoy,crab trap,dock crane,shipping container,life ring,anchor,winch,lobster pot,oar,boat hull,pier plank,tackle box"),
 "Construction": (60, 6, 70, "metal-concrete", "P2", "scaffold,cement mixer,rebar bundle,brick pile,sand heap,wheelbarrow,plank stack,cone,barrier,tarp roll,ladder,paint bucket,jackhammer,porta-loo,crate of tools"),
 "Religious & ritual": (12, 4, 50, "wood-stone", "P2", "altar,candle rack,bell,cult banner,hymn book,incense burner,saint statue,wooden cross,offering bowl,mask,ritual drum,robe rack,shrine,prayer flag,lantern"),
 "Cult compound": (20, 4, 60, "wood-cloth", "P2", "watch platform,loudspeaker,tax ledger table,barbed fence,sermon podium,sleeping cot,ration crate,signal fire pit,cage,banner pole,gate,bell tower frame,crate of pamphlets,spotlight,radio set"),
 "Military": (150, 10, 100, "metal-olive", "P2", "ammo crate,sandbag wall,tent,cot,weapon rack,radio pack,jerrycan,barbed wire,checkpoint barrier,flag pole,searchlight,mortar tube,camo net,field table,map board"),
 "Police": (30, 6, 60, "metal-plastic", "P2", "barricade,evidence board,interview table,holding bench,locker,radio base,riot shield rack,patrol light bar,tape reel,cone,gun safe,camera pole,traffic stick,ticket machine,cell door"),
 "Bank & office": (40, 6, 80, "metal-wood", "P2", "teller desk,vault door,safe deposit wall,ATM,server rack,office chair,cubicle panel,printer,water cooler,filing cabinet,projector,plant pot,reception desk,metal detector,shredder"),
 "Hospital & lab": (50, 6, 70, "metal-plastic", "P2", "gurney,IV stand,monitor,privacy curtain,wheelchair,surgical light,x-ray panel,fridge,lab bench,microscope,bed,crash cart,sink,sharps bin,stretcher"),
 "Nightlife": (20, 4, 60, "metal-glass", "P2", "neon sign,bar counter,stool,speaker stack,DJ booth,disco light,booth seat,velvet rope,fog machine,glass rack,pool table,jukebox,arcade cabinet,strobe rig,menu board"),
 "Beach & tourism": (10, 3, 40, "cloth-plastic", "P1", "sun lounger,umbrella,surfboard,kayak,beach bar hut,volleyball net,towel,cooler,sand castle,lifeguard tower,jet-ski rack,snorkel rack,postcard stand,palm kiosk,hammock"),
 "Jungle dressing": (30, 10, 120, "wood-organic", "P1", "fallen log,stump,root buttress,vine curtain,termite mound,boulder moss,rotting canoe,bone pile,hollow trunk,nest,bamboo cluster,mud wallow,leaf litter pile,orchid cluster,fungi shelf"),
 "Rock & terrain": (2000, 20, 300, "rock", "P1", "sea stack,cliff face,boulder,scree pile,lava rock,cave stalactite,river stone,coral block,sandstone slab,pebble bed,rock arch,terraced ledge,tide pool rock,basalt column,mineral vein"),
 "Ruins": (800, 15, 200, "stone-brick", "P2", "mill wall,chimney stack,collapsed roof,arch,stairway ruin,fountain,wellhead,aqueduct pier,tomb,gate pillar,foundation,cistern vault,bell tower stump,sugar press,boiler shell"),
 "Farming": (40, 6, 70, "wood-metal", "P2", "plough,hay bale,water trough,scarecrow,cane press,sack pile,irrigation pump,fence post,tractor,cart,cane bundle,storage bin,barn door,well,beehive"),
 "Fishing & sea life": (5, 2, 30, "organic", "P1", "drying fish,net float,fishing rod,bait bucket,smoked rack,shell pile,sea fan,kelp bundle,sponge,crab pot,ice chest,tuna carcass,sail patch,outboard motor,oar lock"),
 "Waste & litter": (1, 1, 15, "mixed-trash", "P1", "plastic bag,bottle,newspaper,food wrapper,tyre,pallet,cardboard,can,broken crate,rag,glass shard,cigarette pack,fast-food cup,shopping cart,mattress"),
 "Signage & graffiti": (3, 1, 20, "metal-paint", "P2", "hand-painted sign,billboard,shop banner,mural panel,stencil tag,poster wall,neon arrow,road sign,menu chalkboard,election poster,flyer pile,warning placard,memorial board,sticker cluster,flag string"),
 "Lighting fixtures": (6, 2, 25, "metal-glass", "P2", "street lamp head,neon tube,string lights,hurricane lamp,floodlight,tube light,candle,torch,lantern,bulb cage,headlamp,beacon,flare,searchlight,LED strip"),
 "Electronics": (4, 2, 30, "plastic-metal", "P2", "radio,phone,CRT TV,laptop,speaker,walkie-talkie,camera,cassette deck,router,CCTV,transmitter,battery pack,headphones,drone wreck,tablet"),
 "Stationery & documents": (1, 1, 10, "paper", "P1", "ledger,map,newspaper (Herald),letter,passport,ticket,notebook,photo,pamphlet,tape (S76/77),envelope,receipt,file folder,poster,postcard"),
 "Weapons dressing": (4, 4, 40, "metal-wood", "P2", "rifle on rack,holster,ammo belt,knife display,machete,spent shells,pistol on table,grenade crate,broken stock,bow,sling,suppressor,scope,barrel,cleaning kit"),
 "Containers & crates": (25, 3, 40, "wood-metal", "P2", "wood crate,steel crate,barrel,tin drum,chest,trunk,cooler,gas can,duffel bag,sack,pallet box,ammo box,toolbox,suitcase,cargo net"),
 "Playground & civic": (30, 5, 60, "metal-wood", "P2", "swing,slide,seesaw,fountain,statue,bandstand,clock tower,bus stop,notice board,flagpole,monument,picnic table,bench (stone),gazebo,drinking fountain"),
 "Airfield": (200, 10, 120, "metal-heavy", "P2", "windsock,fuel bowser,hangar door,prop plane wreck,tie-down,runway light,control tower cab,baggage cart,tow tractor,prop,wheel chock,tarmac crack,beacon,wind tee,radio mast"),
 "Rail yard": (400, 10, 150, "metal-rusted", "P2", "boxcar,flatcar,signal,switch lever,buffer stop,sleeper stack,rail bundle,turntable,coal hopper,water tower,crossing gate,loco cab,coupler,sand dome,platform"),
 "Bridge & road": (500, 10, 200, "concrete-steel", "P2", "expansion joint,cable stay,pier cap,guard rail,toll booth,jersey barrier,road sign gantry,lamp arm,culvert,retaining wall,overpass rib,stair tower,drain grate,speed hump,pot-hole patch"),
 "Garden & yard": (15, 3, 40, "organic-wood", "P1", "clothesline,washing,raised bed,rain barrel,chicken coop,hammock,garden gnome,compost,trellis,hose reel,birdbath,pergola,tool shed,fence panel,terracotta pot"),
 "Ocean & underwater": (30, 6, 90, "organic-rock", "P1", "coral head,brain coral,sea fan,anemone,wreck plate,anchor chain,amphora,kelp stalk,sponge,urchin,ship hull section,cargo hold crates,sunken car,dive flag,diver's lamp"),
 "Cave & mine": (100, 10, 150, "rock-wood", "P2", "mine cart,rail,support beam,lantern,stalagmite,crystal,bone,pickaxe,ore pile,tunnel door,winch,sluice,ladder,pump,dynamite crate"),
 "Festival & event": (8, 3, 40, "cloth-paper", "P2", "bunting,float,stage,lantern string,confetti pile,parade drum,costume rack,banner arch,firework tube,sound tower,picnic blanket,ticket booth,puppet,piñata,mask wall"),
 "Children & toys": (1, 1, 20, "plastic-cloth", "P1", "ball,doll,kite,toy car,slingshot,chalk drawing,skipping rope,tricycle,puzzle,crayon pile,plush,marbles,top,paper boat,tin-can phone"),
 "Dog & pet (Poncho)": (2, 4, 30, "organic-cloth", "P2", "dog bowl,chew bone,collar,leash,dog bed,tennis ball,treat jar,water dish,pet carrier,dog biscuit,tag,dog toy rope,harness,blanket,kennel"),
 "Safehouse dressing": (20, 4, 50, "mixed", "P2", "trophy shelf,mission board,weapon bench,crafting table,bed,radio,map wall,fridge,safe,couch,dog corner,window plant,clothing rack,photo wall,coffee station"),
}
PVAR = ["clean", "weathered", "overgrown", "damaged", "graffiti-marked", "burnt", "flooded-stained"]
DESTR = {"P1": "T1", "P2": "T1", "P3": "T1", "P4": "T2"}

def gen_139():
    out = sec(139, "PROP ASSET LEDGER — %d families x 15 items x 7 variants = %d rows" % (len(PROP_FAMILIES), len(PROP_FAMILIES) * 15 * 7), [
        "TRIS = Nanite LOD0 budget; TEX = texture set (BC7 ORM pack, 104.6); SRC = pipeline (104.1); DESTR = destruction tier (120.3);",
        "INST/KM2 = placement budget from PCG/hand-dressing; every row must satisfy the 104.9 asset definition of done."])
    rows = []
    i = 0
    for fam, (mass, tlo, thi, pm, src, items) in PROP_FAMILIES.items():
        for it in items.split(","):
            for v in PVAR:
                i += 1
                aid = "PR-%05d" % i
                tris = RI(tlo, thi, fam, it, "tri") * 1000
                tdim = PK([1024, 2048, 2048, 4096], fam, it)
                dens = {1024: 256, 2048: 512, 4096: 1024}[tdim]
                ms = round(mass * RF(0.6, 1.6, fam, it, "m"), 1)
                destr = "T0" if ms > 1500 else ("T2" if (ms > 100 and "wood" in pm) else DESTR[src] if H(it) % 3 else "T1")
                inst = RI(5, 900, fam, it, v, "inst")
                rows.append([aid, fam, it, v, tris, "%dpx/%dpxm" % (tdim, dens), pm, ms, src, destr,
                             "surf.%s" % pm.split("-")[0], inst, "pending"])
    out += table(["ID", "FAMILY", "ITEM", "VARIANT", "TRIS", "TEX/TEXEL", "PHYS_MAT", "MASS_KG", "SRC", "DESTR", "AUDIO_SURF", "INST/KM2", "STATUS"], rows)
    return out

# ------------------------------------------------------------------ VEGETATION
VEG = [("Ceiba","tree"),("Kapok","tree"),("Mahogany","tree"),("Strangler fig","tree"),("Mango","tree"),("Breadfruit","tree"),("Guava","tree"),
 ("Cacao","tree"),("Rubber tree","tree"),("Cecropia","tree"),("Balsa","tree"),("Teak","tree"),("Bayan","tree"),("Almond (sea)","tree"),("Flamboyant","tree"),
 ("Jacaranda","tree"),("Lignum vitae","tree"),("Tree fern","tree"),("Cloud-forest oak","tree"),("Elfin cedar","tree"),("Coconut palm","palm"),
 ("Royal palm","palm"),("Sabal palm","palm"),("Areca palm","palm"),("Oil palm","palm"),("Rattan palm","palm"),("Fan palm","palm"),("Bottle palm","palm"),
 ("Red mangrove","mangrove"),("Black mangrove","mangrove"),("White mangrove","mangrove"),("Buttonwood","mangrove"),("Sea grape","shrub"),
 ("Hibiscus","shrub"),("Bougainvillea","shrub"),("Coffee shrub","shrub"),("Croton","shrub"),("Heliconia shrub","shrub"),("Ixora","shrub"),
 ("Oleander","shrub"),("Acacia scrub","shrub"),("Cassava","crop"),("Sugar cane","crop"),("Banana","crop"),("Plantain","crop"),("Pineapple","crop"),
 ("Papaya","crop"),("Maize","crop"),("Coffee field","crop"),("Yam vine","crop"),("Sword fern","fern"),("Bracken","fern"),("Staghorn fern","fern"),
 ("Bird's-nest fern","fern"),("Filmy fern","fern"),("Lycopod","fern"),("Giant taro","herb"),("Elephant ear","herb"),("Ginger lily","herb"),
 ("Heliconia","herb"),("Bromeliad","herb"),("Orchid (ground)","herb"),("Bird-of-paradise","herb"),("Aloe","herb"),("Agave","herb"),
 ("Dune grass","grass"),("Elephant grass","grass"),("Guinea grass","grass"),("Bamboo (clump)","grass"),("Bamboo (giant)","grass"),("Reed","grass"),
 ("Sawgrass","grass"),("Razor sedge","grass"),("Lemongrass","grass"),("Liana (woody)","vine"),("Philodendron vine","vine"),("Passionflower","vine"),
 ("Morning glory","vine"),("Ivy (tropical)","vine"),("Strangler root curtain","vine"),("Epiphyte orchid","epiphyte"),("Tillandsia","epiphyte"),
 ("Moss mat","moss"),("Lichen crust","moss"),("Seagrass","aquatic"),("Kelp","aquatic"),("Water lily","aquatic"),("Water hyacinth","aquatic"),
 ("Lotus","aquatic"),("Algae mat","aquatic"),("Coral (branching)","aquatic"),("Coral (brain)","aquatic"),("Sea fan","aquatic")]
VSTATE = ["sapling", "young", "mature", "old-growth", "dead-standing", "fallen", "burnt", "wind-damaged"]

def gen_140():
    out = sec(140, "VEGETATION LEDGER — %d species x 8 states = %d rows; PCG BIOME RULES 12 x %d = %d rows" % (len(VEG), len(VEG) * 8, len(VEG), 12 * len(VEG)), [
        "Nanite Foliage where available (103.1). WIND = WPO sway params (105.5). HEIGHT in metres. LODS: Nanite (no classic LODs) + far impostor for > 600 m.",
        "PCG rule rows: DENSITY in instances/ha before the biome multiplier (105.4); SLOPE_MAX deg; WET = wetness affinity 0..1; ALT = altitude band (m)."])
    out += ["", "## 140.A — SPECIES x STATE"]
    h0 = {"tree": (18, 55), "palm": (8, 28), "mangrove": (4, 15), "shrub": (1, 4), "crop": (0.5, 4), "fern": (0.4, 3), "herb": (0.4, 3), "grass": (0.3, 12),
          "vine": (1, 30), "epiphyte": (0.1, 0.8), "moss": (0.01, 0.05), "aquatic": (0.05, 1.5)}
    rows = []
    i = 0
    for n, cl in VEG:
        lo, hi = h0[cl]
        for st in VSTATE:
            i += 1
            k = {"sapling": 0.12, "young": 0.45, "mature": 1.0, "old-growth": 1.25, "dead-standing": 0.95, "fallen": 0.15, "burnt": 0.9, "wind-damaged": 0.85}[st]
            ht = round(RF(lo, hi, n, "h") * k, 2)
            tris = int(ht * 1000 * RF(0.8, 3.0, n, st, "t") + 4000)
            rows.append(["VG-%04d" % i, n, cl, st, ht, tris, "%dpx" % PK([512, 1024, 2048], n, "tex"), RF(0.2, 1.6, n, st, "wind"),
                         "Y" if (cl in ("tree", "palm", "shrub") and st not in ("fallen",)) else "N",
                         {"burnt": "char+ember", "dead-standing": "desat+bare", "fallen": "static-debris", "wind-damaged": "broken-limbs"}.get(st, "live"), "P%d" % PK([1, 1, 2], n)])
    out += table(["ID", "SPECIES", "CLASS", "STATE", "HEIGHT_M", "NANITE_TRIS", "LEAF_ATLAS", "WIND_K", "CAST_SHADOW", "MAT_STATE", "SRC"], rows)
    out += ["", "## 140.B — PCG BIOME PLACEMENT RULES"]
    bn = ["B01 Coral Beach","B02 Mangrove","B03 Lowland Rainforest","B04 Cloud Forest","B05 Volcanic Ridge","B06 Sugar-Cane Fields","B07 River Gorge",
          "B08 Cult Village","B09 Cave Mouth","B10 Shallow Reef","B11 Deep Ocean","B12 Caldera Lake"]
    aff = {"B01": {"palm", "grass", "shrub", "herb"}, "B02": {"mangrove", "aquatic", "grass"}, "B03": {"tree", "palm", "fern", "herb", "vine", "epiphyte", "shrub", "moss"},
           "B04": {"tree", "fern", "epiphyte", "moss", "vine", "herb"}, "B05": {"grass", "shrub", "moss"}, "B06": {"crop", "grass", "shrub"},
           "B07": {"fern", "herb", "tree", "grass", "moss"}, "B08": {"crop", "palm", "shrub", "tree", "herb"}, "B09": {"moss", "fern", "vine"},
           "B10": {"aquatic"}, "B11": {"aquatic"}, "B12": {"aquatic", "grass"}}
    alt = {"B01": "0-10", "B02": "0-6", "B03": "5-300", "B04": "300-430", "B05": "430-540", "B06": "5-150", "B07": "5-350", "B08": "5-200", "B09": "5-500", "B10": "-12-0", "B11": "<-12", "B12": "298-302"}
    rows = []
    i = 0
    for b in bn:
        bid = b[:3]
        for n, cl in VEG:
            i += 1
            ok = cl in aff[bid] and not ((bid in ("B10", "B11")) and cl != "aquatic")
            if bid in ("B10", "B11") and n not in ("Seagrass", "Kelp", "Coral (branching)", "Coral (brain)", "Sea fan", "Algae mat"): ok = False
            dens = int(RI(20, 1800, n, bid, "d") * {"tree": 0.3, "palm": 0.25, "crop": 3.0, "grass": 4.0, "moss": 5.0}.get(cl, 1.0)) if ok else 0
            rows.append(["PCG-%s-%03d" % (bid, (i - 1) % len(VEG) + 1), n, b, dens if ok else "-",
                         RI(8, 40, n, bid, "s") if ok else "-", RF(0, 1, n, bid, "w") if ok else "-", alt[bid], "cluster:%d" % RI(1, 12, n, bid, "c") if ok else "-",
                         "allowed" if ok else "excluded"])
    out += table(["RULE", "SPECIES", "BIOME", "DENSITY/HA", "SLOPE_MAX", "WET", "ALT_M", "CLUSTER", "STATUS"], rows)
    return out

# ------------------------------------------------------------------ CHARACTERS
OUTFITS = ["signature", "work", "formal", "rain gear", "night", "disguise/cult robe", "ragged/wounded", "festival"]
CSTATE = ["clean", "sweaty", "wet", "dirty", "injured"]
OCC = ["fisher", "dock worker", "vendor", "office clerk", "banker", "police officer", "nurse", "teacher", "student", "tourist", "surfer", "cook", "waiter",
       "mechanic", "taxi driver", "street performer", "retiree", "farmer", "cult acolyte", "smuggler"]
AREAS = ["Old Harbor", "Downtown", "Solada Beach", "Alto Barro", "La Herreria", "Residential", "Countryside", "Aerodromo", "Island coast", "Island village", "Island jungle", "Island mill"]
CROWD_VAR = ["work", "leisure", "night", "rain", "heat", "festival"]
SLOTS = ["head", "hair", "face", "torso", "outer", "legs", "feet", "hands", "belt", "bag", "neck", "accessory"]
PIECES = ["straw hat", "cap", "bandana", "headwrap", "helmet", "beanie", "visor", "wide brim", "hard hat", "fedora", "hood", "cloche", "beret", "kufi", "turban",
          "t-shirt", "guayabera", "tank", "suit jacket", "rain poncho", "hoodie", "robe", "uniform shirt", "apron", "overalls", "dress", "kaftan", "polo", "vest",
          "leather jacket", "shorts", "cargo pants", "jeans", "skirt", "slacks", "wetsuit", "sarong", "sandals", "boots", "sneakers", "flip-flops", "loafers",
          "work boots", "gloves", "fingerless gloves", "bracelet", "watch", "belt", "holster belt", "backpack", "satchel", "tool bag", "fanny pack", "scarf",
          "necklace", "sunglasses", "earrings", "mask", "lanyard", "umbrella", "walking cane"]

def gen_144():
    out = sec(144, "CHARACTER LEDGER — NPC OUTFIT STATES, CROWD ARCHETYPES, COSTUME PIECES", [
        "Hero-lite NPCs (MetaHuman + custom costume) per 107.2; crowd archetypes compose 32 bodies x 120 heads x 12 slots (107.2).",
        "CLIP-SWEEP column is the automated 20-animation pose sweep that must show zero clipping (107.6)."])
    out += ["", "## 144.A — NAMED NPC x OUTFIT x STATE (25 x 8 x 5 = 1,000)"]
    rows = []
    i = 0
    for nid, nn in NPCS:
        for o in OUTFITS:
            for st in CSTATE:
                i += 1
                rows.append(["NC-%04d" % i, nid, nn, o, st, "LOD0 %dk" % RI(90, 140, nid, "l"), "hair:%s" % PK(["strand", "strand+cards", "cards"], nid, o),
                             RF(0.1, 1.0, nid, o, st, "wet"), RF(0.0, 1.0, nid, o, st, "dirt"), "MHA-face:%d" % RI(1, 4, nid, o, "f"), "20/20 pending"])
    out += table(["ID", "NPC", "NAME", "OUTFIT", "STATE", "LOD", "HAIR", "WET", "DIRT", "FACE_RIG", "CLIP-SWEEP"], rows)
    out += ["", "## 144.B — CROWD ARCHETYPES (20 occupations x 12 areas = 240; x 6 situations = 1,440 rows)"]
    rows = []
    i = 0
    for a in AREAS:
        for oc in OCC:
            for cv in CROWD_VAR:
                i += 1
                rows.append(["CA-%04d" % i, "%s/%s" % (a, oc), cv, "body:%02d" % RI(1, 32, a, oc, "b"), "head:%03d" % RI(1, 120, a, oc, cv, "h"),
                             ",".join(PK(PIECES, a, oc, cv, s) for s in SLOTS[:4]), "Mass-weight:%.2f" % RF(0.05, 1.0, a, oc, cv, "w"),
                             "sched:%s" % PK(["7-16", "9-18", "12-23", "18-03", "6-14", "all-day", "5-13"], oc, a),
                             "%d-%d" % (RI(5, 20, a, oc, "s1"), RI(21, 90, a, oc, "s2")) + "yo", "LOD3 1.5k"])
    out += table(["ID", "AREA/OCC", "SITUATION", "BODY", "HEAD", "KEY_PIECES", "WEIGHT", "SCHEDULE", "AGE", "FAR_LOD"], rows)
    out += ["", "## 144.C — COSTUME PIECE KIT (12 slots; 60 pieces)"]
    rows = []
    for i, p in enumerate(PIECES, 1):
        rows.append(["CP-%03d" % i, p, SLOTS[min(len(SLOTS)-1, (i - 1) * 12 // len(PIECES))], "%d" % RI(3, 18, p, "tri"), "%d" % RI(3, 6, p, "mask"), "cloth-sim" if p in ("rain poncho", "robe", "kaftan", "dress", "scarf", "hood") else "static",
                     "wet-darken" if H(p) % 2 else "wet-shine", "32 bodies fit", "pending"])
    out += table(["ID", "PIECE", "SLOT", "TRIS_K", "COLOR_MASKS", "SIM", "WET_RESPONSE", "FIT", "STATUS"], rows)
    return out

# ------------------------------------------------------------------ SURFACES & FOLEY
SURF = ["asphalt-new","asphalt-worn","concrete","brick","cobble","pavers","tile-ceramic","marble","wood-planks","wood-decking","parquet","corrugated-metal",
        "steel-plate","steel-grating","cast-iron","aluminum","glass","glass-broken","plastic","rubber","carpet","linoleum","sand-dry","sand-wet","gravel",
        "dirt-packed","mud","clay","grass-short","grass-tall","leaf-litter","moss","roots","jungle-floor","rock-bare","rock-wet","scree","lava-rock",
        "snow-N/A","water-shallow","water-deep","river-stones","reef-coral","shell-beach","cane-debris","straw-thatch","canvas","cloth-heavy","leather",
        "drywall","plaster","stucco","chain-link","barbed-wire","sandbags","hay","ice-block","bone","ceramic-pot","paper-pile"]
SURF = [s for s in SURF if not s.endswith("N/A")] + ["cardboard", "foam", "cork"][:60 - len([s for s in SURF if not s.endswith("N/A")])]
WETS = ["dry", "damp", "wet", "flooded"]
SHOE = ["boots", "sneakers", "sandals", "barefoot", "dress shoes", "paws (dog)"]
GAIT = ["walk", "jog", "sprint", "crouch"]

def gen_145():
    out = sec(145, "SURFACE, PHYSICAL-MATERIAL & FOLEY LEDGER", [
        "One Chaos Physical Material per surface drives: friction (tyres+feet), restitution, bullet decal set, impact VFX/SFX, AI noise radius,",
        "audio surface tag, wetness response (puddle/film/sheen). FOLEY: footsteps by surface x footwear x gait (127.4)."])
    out += ["", "## 145.A — PHYSICAL MATERIAL x WETNESS (%d x 4)" % len(SURF)]
    rows = []
    i = 0
    hard = {"asphalt-new","asphalt-worn","concrete","brick","cobble","pavers","tile-ceramic","marble","steel-plate","steel-grating","cast-iron","aluminum","glass","rock-bare","rock-wet","lava-rock","river-stones","reef-coral","shell-beach"}
    for s in SURF:
        for w in WETS:
            i += 1
            base = 0.35 if s in ("glass", "tile-ceramic", "marble", "rubber") else (0.9 if s in ("asphalt-new", "rubber", "carpet", "grass-tall", "gravel") else RF(0.5, 0.85, s, "f"))
            fr = max(0.05, base * {"dry": 1.0, "damp": 0.92, "wet": 0.68 if s in hard else 0.8, "flooded": 0.5}[w])
            rows.append(["PM-%04d" % i, s, w, round(fr, 2), RF(0.02, 0.7, s, "e"), "bh-%s-%02d" % (s.split("-")[0], RI(1, 16, s, w, "d")),
                         "imp.%s.%s" % (s.split("-")[0], w), "noise-r:%d m" % (RI(2, 14, s, "nr") + (3 if s in hard else 0) - (2 if w != "dry" and s not in hard else 0)),
                         "puddle-mask" if w in ("wet", "flooded") and s in hard else "sheen" if w != "dry" else "-",
                         "pen:%s" % ("hard" if s in hard else ("soft" if s in ("drywall","plaster","stucco","hay","cloth-heavy","canvas","grass-tall","straw-thatch","wood-planks","foam") else "medium"))])
    out += table(["ID", "SURFACE", "WETNESS", "FRICTION", "RESTITUTION", "DECAL_SET", "IMPACT_FX", "AI_NOISE", "WET_LOOK", "PEN_CLASS"], rows)
    out += ["", "## 145.B — FOOTSTEP FOLEY (%d x 6 x 4)" % len(SURF)]
    rows = []
    i = 0
    for s in SURF:
        for sh in SHOE:
            for g in GAIT:
                i += 1
                rows.append(["FS-%05d" % i, s, sh, g, "sfx.fs.%s.%s.%s" % (s.split("-")[0], sh.split()[0], g), RI(4, 8, s, sh, g, "v"),
                             "%d-%d dB" % (RI(38, 48, s, sh, g, "a") - (6 if g == "crouch" else 0) + (8 if g == "sprint" else 0), RI(52, 66, s, sh, g, "b") + (6 if g == "sprint" else 0)),
                             "dust" if s in ("sand-dry", "dirt-packed", "gravel", "asphalt-worn") and g != "crouch" else "-",
                             "splash" if s in ("water-shallow", "mud", "sand-wet") else "-"])
    out += table(["ID", "SURFACE", "FOOTWEAR", "GAIT", "AUDIO_EVENT", "VARIATIONS", "LEVEL", "PARTICLE", "WATER"], rows)
    return out

# ------------------------------------------------------------------ VFX
def gen_146():
    out = sec(146, "NIAGARA VFX LEDGER — %d effects x 4 quality tiers", [
        "Tiers: Q0 sprite-only (T0-T1), Q1 GPU sprites+mesh (T2), Q2 + volumetrics-lite (T3), Q3 heterogeneous volumes/fluids (T4-T5).",
        "BUDGET = GPU ms at T3 worst-case overlap; PARTICLES = peak alive; LIGHT = dynamic light injected into Lumen (Y/N)."])
    fx = []
    for wid, wn, wc, amm in WEAPONS:
        if wc in ("melee", "unarmed"): continue
        for k in ("muzzle flash", "shell eject", "tracer", "barrel heat shimmer"):
            fx.append("%s %s (%s)" % (wn, k, wid))
    for s in ["concrete","brick","metal","wood","glass","dirt","sand","water","foliage","rock","flesh","cloth"]:
        for k in ("bullet impact", "ricochet spark", "debris burst"):
            fx.append("%s %s" % (s, k))
    for e in ["grenade","frag","flashbang","smoke","molotov","C4","rocket","vehicle","barrel","fuel depot","gunship missile","mortar","dynamite","demolition"]:
        for k in ("fireball", "shockwave+dust", "smoke column"):
            fx.append("%s %s" % (e, k))
    for m in ["dry brush","palm frond","wood structure","grass field","canopy","oil spill","vehicle wreck","straw roof","cane field","crate stack"]:
        for k in ("flame", "embers", "smoke"):
            fx.append("fire: %s %s" % (m, k))
    for k in ["rain drops","rain splashes","rain ripples","puddle splash","squall sheet","lightning bolt","lightning flash glow","fog bank","mist valley","heat haze",
              "wind-blown leaves","wind-blown sand","storm surf spray","wave crest foam","shore foam","boat wake","bow spray","jetski roost","underwater bubbles","caustics"]:
        fx.append("env: " + k)
    for wid, wn, vc in VEHICLES:
        for k in ("tire smoke", "dust trail", "engine smoke/damage", "exhaust/backfire"):
            fx.append("%s %s" % (wn, k))
    for k in ["dust motes in light shafts","pollen drift","insect swarm","fireflies","steam vents","street steam","chimney smoke","campfire","cigarette smoke","neon sparks",
              "welding sparks","electrical arc","cooking steam","incense","candle flame","torch flame","flare","signal fire","seagull flock feathers","bat swarm",
              "blood splatter","blood drip","wound mist","sweat drops","breath vapour (cool)","footstep dust","footstep splash","landing dust","bullet casing bounce","glass shards"]:
        fx.append("amb: " + k)
    out[1] = "# SECTION 146 — NIAGARA VFX LEDGER — %d effects x 4 quality tiers = %d rows" % (len(fx), len(fx) * 4)
    rows = []
    i = 0
    for n in fx:
        for q in ("Q0", "Q1", "Q2", "Q3"):
            i += 1
            qi = int(q[1])
            rows.append(["FX-%05d" % i, n, q, int(RI(40, 400, n, "p") * (0.4 + 0.5*qi)), "%.2f" % (RF(0.02, 0.35, n, "b") * (0.5 + 0.35*qi)),
                         "Y" if (("flash" in n or "fireball" in n or "flame" in n or "lightning" in n or "neon" in n or "arc" in n or "sparks" in n) and qi >= 1) else "N",
                         ["sprite", "sprite+mesh", "sprite+mesh+vol-lite", "heterogeneous-volume/fluid"][qi] if ("smoke" in n or "fire" in n or "fireball" in n or "fog" in n or "steam" in n) else ["sprite", "sprite", "GPU-mesh", "GPU-mesh+ribbon"][qi],
                         "aud.fx.%d" % RI(100, 999, n), "pool:%d" % RI(4, 64, n)])
    out += table(["ID", "EFFECT", "TIER", "PARTICLES", "BUDGET_MS", "LIGHT", "TECH", "AUDIO", "POOL"], rows)
    return out

# ------------------------------------------------------------------ SHADER PERMUTATIONS
MASTERS = ["M_Master_Opaque","M_Master_Foliage","M_Master_Landscape","M_Master_Skin","M_Master_Hair","M_Master_Cloth","M_Master_Glass","M_Master_Water",
           "M_Master_Decal","M_Master_Emissive","M_Master_VehiclePaint","M_Master_Metal","M_Master_Rock","M_Master_Road","M_Master_Wood","M_Master_Plastic",
           "M_Master_Fabric_Thin","M_Master_Eye","M_Master_Teeth","M_Master_Fire","M_Master_Cloud","M_Master_Weapon","M_Master_Wet_Overlay","M_Master_UI_3D"]
FEATS = ["nanite-compatible","substrate-slab","clear-coat","anisotropy","SSS","fuzz/sheen","wetness-layer","puddle-RVT","WPO-wind","WPO-interaction","vertex-paint-wear",
         "decal-grime","triplanar","stochastic-tiling","parallax/POM","displacement","emissive-lumen","translucency","two-sided","custom-primitive-data"]

def gen_155():
    out = sec(155, "SHADER / MATERIAL PERMUTATION BUDGET — 24 masters x 20 features = 480 rows", [
        "Every feature toggle is a static switch; permutation count must stay under the master's cap (PSO precache bundles, 103.12).",
        "COST = estimated base-pass instruction count delta at T3; CAP_PERMS = maximum shipped permutations for this master."])
    rows = []
    i = 0
    for m in MASTERS:
        cap = PK([64, 96, 128, 192, 256], m)
        for f in FEATS:
            i += 1
            used = "Y" if H(m, f) % 5 else "N"
            rows.append(["SP-%04d" % i, m, f, used, RI(2, 140, m, f, "c") if used == "Y" else 0, cap, "nanite-ok" if "WPO" not in f and "translucency" not in f else "non-nanite-whitelist",
                         "T0:off" if f in ("displacement", "SSS", "anisotropy", "fuzz/sheen") else "all-tiers"])
    out += table(["ID", "MASTER", "FEATURE", "ENABLED", "COST_INSTR", "CAP_PERMS", "NANITE", "TIER_RULE"], rows)
    return out
