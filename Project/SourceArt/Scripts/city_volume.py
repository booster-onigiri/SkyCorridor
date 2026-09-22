"""An inhabited city volume: adjoining masonry, repeated storeys and sky streets.

Each building segment is exactly 36 metres high. The runtime joins its full
facades vertically, and joins neighbouring buildings with inhabited bridges.
These are editable 3D meshes, not image planes or isolated skyline ornaments.
"""
from pathlib import Path
import runpy, math, random, json, time, sys
import bpy
from mathutils import Vector, Matrix

g=runpy.run_path(str(Path(__file__).with_name("mesh_primitives.py")))
Builder,export,column=g["Builder"],g["export"],g["column"]
OUT,CATALOG=g["OUT"],g["CATALOG"]
existing=json.loads((OUT/"kit-catalog.json").read_text(encoding="utf8"))
started=time.time()
BRIDGES_ONLY="--bridges-only" in sys.argv
WALK_ACCESS_ONLY="--walk-access-only" in sys.argv
if BRIDGES_ONLY and WALK_ACCESS_ONLY:raise ValueError("Choose one partial export mode")
STONE=(.80,.84,.82,1)
TRIM=(.91,.92,.85,1)
SHADOW=(.045,.095,.115,1)
GLASS=(.12,.27,.32,1)
COPPER=(.21,.44,.42,1)
GOLD=(.55,.36,.14,1)

def merge(b,part,p=(0,0,0),yaw=0):
    offset=len(b.v);r=Matrix.Rotation(yaw,3,"Z");p=Vector(p)
    b.v.extend(tuple(r@Vector(v)+p) for v in part.v)
    b.f.extend(tuple(offset+i for i in f) for f in part.f)
    b.m.extend(part.m);b.col.extend(part.col);b.smooth.extend(part.smooth)

def box(b,p,d,family="Limestone",colour=TRIM,bevel=.035):
    b.box(p,d,family,bevel,colour=colour)

def balustrade(b,width,y,z):
    for h in [.16,1.10]:box(b,(0,y,z+h),(width,.18,.13),bevel=.02)
    n=max(2,int(width/.72))
    for i in range(n+1):
        x=-width*.5+i*width/n
        b.lathe([(.095,0),(.095,.1),(.06,.22),(.085,.5),(.05,.76),(.085,1.1)],
                (x,y,z),segments=10,colour=TRIM)

def hanging(b,p,seed=0):
    rng=random.Random(seed)
    for j in range(4):
        q=Vector(p)+Vector(((j-1.5)*.38,0,0));length=2.2+rng.random()*3
        ps=[q+Vector((.18*math.sin(t*.8+j),0,-length*t/9)) for t in range(10)]
        b.tube(ps,[.018]*10,"Bark",6)
        for i in range(1,10):
            for s in [-1,1]:b.leaf(ps[i],.43,.30,s*1.2,.2,"Foliage",(.16,.39+.04*(i%3),.19,1))

def window(b,x,y,z,seed,arched=True):
    # The dark room sits behind the front edge of the wall by 30 cm.
    box(b,(x,y+.28,z+1.45),(2.15,.07,2.5),"Paint",SHADOW,.012)
    box(b,(x,y+.23,z+1.5),(1.8,.04,2.15),"Glow" if seed%11==0 else "Paint",
        (.72,.42,.13,1) if seed%11==0 else GLASS,.01)
    for dx in [-1.18,1.18]:box(b,(x+dx,y-.08,z+1.5),(.22,.4,3.1))
    box(b,(x,y-.12,z-.10),(2.95,.78,.22))
    box(b,(x,y-.04,z+3.1),(2.92,.5,.18))
    for h in [.82,1.86]:box(b,(x,y+.12,z+h),(2.2,.08,.065),"Bronze",GOLD,.01)
    box(b,(x,y+.1,z+1.47),(.07,.11,2.85),"Bronze",GOLD,.01)
    if arched:
        b.arc((x,y-.08,z+2.25),1.16,.16,depth=.32,segments=22)
        box(b,(x,y-.20,z+3.48),(.34,.47,.30))
    if seed%7==0:
        box(b,(x,y-.62,z-.23),(3.1,1.8,.26))
        temp=Builder();balustrade(temp,3.1,-1.40,0);merge(b,temp,(x,y,z-.08))
        for s in [-1,1]:box(b,(x+s*1.46,y-.73,z+.42),(.13,1.4,.12),"Bronze",GOLD)
        if seed%3==0:hanging(b,(x,y-1.49,z),seed)

def facade(b,variant,opening_floor=None,opening_mirrored=False):
    # Continuous 36-m wall; masonry piers leave real window recesses.
    # All four sides use this same datum, so stacked floors meet exactly.
    width=36.;y=-18.;bay=4.5
    tint=[STONE,(.78,.83,.86,1),(.83,.83,.76,1),(.77,.82,.79,1)][variant%4]
    opening_floors=[] if opening_floor is None else list(opening_floor) if isinstance(opening_floor,(list,tuple)) else [opening_floor]
    if not opening_floors:
        box(b,(0,y+1.30,18),(width,.20,36),colour=tint)
    else:
        # Preserve the original backing wall except the two selected bays.
        def opening_span(a,c):return (-c,-a) if opening_mirrored else (a,c)
        def backing(a,c,low,high):
            a,c=opening_span(a,c)
            if c>a and high>low:box(b,((a+c)/2,y+1.30,(low+high)/2),(c-a,.20,high-low),colour=tint)
        cursor=0.
        for opened_floor in sorted(opening_floors):
            backing(-18,18,cursor,opened_floor)
            for a,c in [(-18,1),(3.5,5.5),(8,18)]:backing(a,c,opened_floor,opened_floor+3.4)
            backing(5.5,8,opened_floor,opened_floor+.55)
            backing(1,3.5,opened_floor+2.85,opened_floor+3.4)
            backing(5.5,8,opened_floor+3.15,opened_floor+3.4)
            cursor=opened_floor+3.4
        backing(-18,18,cursor,36)
    for level in range(8):
        z=level*4.5
        opened=any(abs(z-opened_floor)<.001 for opened_floor in opening_floors)
        if opened:
            for a,c in [(-18.24,1),(3.5,18.24)]:
                a,c=opening_span(a,c);box(b,((a+c)/2,y-.05,z+.1),(c-a,.52,.22))
        else:box(b,(0,y-.05,z+.1),(width+.48,.52,.22))
        box(b,(0,y-.08,z+4.20),(width+.64,.64,.28))
        for j in range(8):
            x=-18+(j+.5)*bay
            opening_bay=7-j if opening_mirrored else j
            for s in [-1,1]:
                box(b,(x+s*1.735,y+.50,z+2.25),(1.03,1.0,4.5),colour=tint)
            if not (opened and opening_bay==4):box(b,(x,y+.50,z+.26),(2.5,1.0,.52),colour=tint)
            box(b,(x,y+.50,z+4.16),(2.5,1.0,.68),colour=tint)
            # Individual relief panels and quoins keep scale readable at WQHD.
            for row in range(4):
                box(b,(x-2.02,y-.055,z+.57+row*.83),(.34,.18,.65),colour=tint,bevel=.012)
            if opened and opening_bay in [4,5]:
                height=2.85 if opening_bay==4 else 3.15
                box(b,(x,y+.50,z+(height+4.5)/2),(2.5,1.0,4.5-height),colour=tint)
                if opening_bay==5:
                    box(b,(x,y-.10,z+.48),(2.75,1.05,.14))
                    for h in [.58,1.10]:box(b,(x,y-.10,z+h),(2.5,.08,.065),"Bronze",GOLD,.01)
                    for dx in [-1.12,-.56,0,.56,1.12]:box(b,(x+dx,y-.10,z+.82),(.045,.065,.55),"Bronze",GOLD,.008)
                    # Casements folded onto the piers leave an actual open view.
                    for side in [-1,1]:box(b,(x+side*1.45,y-.24,z+1.8),(.40,.20,2.45),"Paint",COPPER)
            else:window(b,x,y-.01,z+.60,j+level*9+variant*31,(j+level+variant)%3!=0)
        if level in [0,4]:
            # A projecting public arcade carries the sky street along the facade.
            box(b,(0,y-1.60,z-.15),(36.9,3.6,.38),colour=TRIM)
            for j in range(9):
                x=-18+j*4.5
                column(b,(x,y-2.75,z),3.3,.13)
            for j in range(8):b.arc((-15.75+j*4.5,y-2.75,z+2.18),2.10,.23,depth=.34,segments=24)
            box(b,(0,y-1.4,z+4.1),(37.4,3.7,.32))
            # Public guards now belong to UrbanWalkRing. Its openings match
            # actual connecting bridges and lift landings in the runtime.
    for s in [-1,1]:
        box(b,(s*17.75,y-.40,18),(.65,1.25,36),colour=TRIM)
        for z in range(0,36,3):box(b,(s*17.75,y-.67,z+.3),(1.1,.65,.44))

def urban_block(b,variant,opening_side=None,opening_floor=None,opening_mirrored=False):
    for side in range(4):
        panel=Builder();facade(panel,(variant+side)%8,opening_floor if side==opening_side or opening_side=='all' else None,
                              opening_mirrored if side==opening_side or opening_side=='all' else False);merge(b,panel,yaw=side*math.pi*.5)
    for z in [0,18,36]:
        box(b,(0,0,z-.32),(36,36,.62),colour=(.72,.76,.73,1))
        box(b,(0,0,z-.72),(37.25,37.25,.22))
    if variant%3==0:
        # Occupied rounded oriel, with its own repeated windows and balcony.
        for z in [4.5,9.,22.5,27.]:
            for side in [-1,1]:
                p=(side*18.8,-10,z)
                b.lathe([(0,0),(2.2,0),(2.3,.2),(2.2,3.7),(2.5,4.05),(0,4.1)],p,segments=36,colour=TRIM)
                for a in [-.65,0,.65]:
                    q=(p[0]+math.sin(a)*2.26,p[1]-math.cos(a)*2.26,z+.6)
                    small=Builder();window(small,0,0,0,variant+int(z),False);merge(b,small,q,a)
    if variant%3==1:
        for side in [-1,1]:
            for z in [11.,29.]:
                box(b,(side*10,-20.5,z),(7.1,5.6,3.2),"Paint",COPPER)
                box(b,(side*10,-20.6,z+1.8),(8,6.2,.34))
                for dx in [-2.2,0,2.2]:window(b,side*10+dx,-23.34,z-.9,variant+int(dx))

for variant in range(0 if BRIDGES_ONLY else 8):
    b=Builder();urban_block(b,variant)
    export("UrbanBlock_"+str(variant),b,"Eight continuous inhabited storeys, four recessed facades, public arcades and projecting rooms; exact 36m vertical joint")

for variant in range(0 if WALK_ACCESS_ONLY else 4):
    b=Builder();w=5.2 if variant%2 else 7.4
    box(b,(0,0,-.28),(64,w,.55),colour=TRIM)
    for side in [-1,1]:
        rail=Builder();balustrade(rail,64,0,0);merge(b,rail,(0,side*(w*.5-.1),0))
        box(b,(0,side*w*.5,-.82),(64,.75,.47),colour=STONE)
    # Structural arches run under the visible span between building faces.
    for y in [-w*.36,w*.36]:
        # Both feet reach the adjoining buildings even when the runtime fits
        # the span. An isolated narrow semicircle would hang in the street.
        ps=[(-32+i*64/56,y,-1.35-12.5*((i-28)/28)**2) for i in range(57)]
        b.tube(ps,[.55]*57,"Limestone",16,TRIM)
    if variant in [1,3]:
        for x in range(-30,31,5):
            for side in [-1,1]:column(b,(x,side*(w*.5-.28),0),3.55,.12)
        box(b,(0,0,3.95),(64,w+.8,.38),colour=TRIM)
        for j in range(16):
            y=-w*.56+j*w*1.12/15
            box(b,(0,y,4.22+.9*(1-abs(y)/(w*.58))),(64.4,w*1.12/15+.035,.16),"Paint",COPPER)
    if variant>=2:
        for x in [-10,10]:
            box(b,(x,-w*.5-.4,.4),(5.2,2.0,.8),colour=STONE)
            hanging(b,(x,-w*.5-1.4,.9),variant+int(x))
    export("UrbanBridge_"+str(variant),b,"64m adjoining street bridge with balustrades, supporting arch and optional covered colonnade")

for variant in range(0 if BRIDGES_ONLY else 4):
    b=Builder()
    box(b,(0,0,0),(38,38,.9))
    for x in [-17,17]:
        for y in [-17,17]:
            column(b,(x,y,.3),6,.40)
            b.lathe([(0,0),(1.8,0),(1.8,.25),(1.5,1.2),(.4,3),(0,3.4)],(x,y,6.3),"Paint",36,COPPER)
    # The accessible roof shares the runtime's continuous outer walk and guards.
    for x in [-9,9]:
        for y in [-9,9]:
            box(b,(x,y,3.2),(11,10,5.9),colour=STONE)
            for dx in [-3,0,3]:window(b,x+dx,y-5.03,1.1,variant+int(dx))
            b.lathe([(0,0),(6.2,0),(6.2,.2),(5.7,1.7),(4.4,3.5),(2.0,5),(0,5.6)],(x,y,6.4),"Paint",48,COPPER)
            hanging(b,(x,y-5.3,.8),variant+int(x+y))
    export("UrbanCrown_"+str(variant),b,"Inhabited roof district with cupolas, four corner turrets and hanging gardens")

names={item["name"] for item in CATALOG}
existing["assets"]=[item for item in existing["assets"] if item["name"] not in names]+CATALOG
existing["triangles"]=sum(item["triangles"] for item in existing["assets"])
if WALK_ACCESS_ONLY:
    existing["city_volume_art"]["walk_access_source"]="CityWalkAccess.blend"
elif BRIDGES_ONLY:
    existing["city_volume_art"]["bridge_adjustment_source"]="CityBridges.blend"
    existing["city_volume_art"]["bridge_adjustment_seconds"]=round(time.time()-started,3)
else:
    existing["city_volume_art"]={"assets":sorted(names),"segment_height_m":36,"storeys_per_segment":8,
        "footprint_m":36,"street_bridge_span_m":64,"blend_source":"CityVolume.blend",
        "authored_seconds":round(time.time()-started,3)}
(OUT/"kit-catalog.json").write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/("CityWalkAccess.blend" if WALK_ACCESS_ONLY else "CityBridges.blend" if BRIDGES_ONLY else "CityVolume.blend")))
print("EW_CITY_VOLUME_COMPLETE",len(CATALOG),sum(a["triangles"] for a in CATALOG),round(time.time()-started,3),flush=True)
