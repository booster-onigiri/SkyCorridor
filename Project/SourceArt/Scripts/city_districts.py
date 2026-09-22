"""Inhabited variations for the continuous city: libraries, gardens and bridges.

Library modules retain the exact 36m structural joint of CityVolume. Garden
terraces attach to those buildings, and new bridge silhouettes join real
neighbouring facades. All artwork remains editable three-dimensional geometry.
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
STRUCTURES_ONLY="--structures-only" in sys.argv
GARDENS_ONLY="--gardens-only" in sys.argv
WALK_ACCESS_ONLY="--walk-access-only" in sys.argv
if STRUCTURES_ONLY and GARDENS_ONLY:raise ValueError("Choose one partial export mode")
WHITE=(.85,.87,.78,1)
STONE=(.60,.69,.65,1)
TEAL=(.045,.22,.20,1)
GOLD=(.52,.30,.075,1)
BLUE=(.045,.16,.21,1)

def box(b,p,d,family="Limestone",colour=WHITE,bevel=.028):
    b.box(p,d,family,bevel,colour=colour)

def merge(b,part,p=(0,0,0),yaw=0):
    offset=len(b.v);r=Matrix.Rotation(yaw,3,"Z");p=Vector(p)
    b.v.extend(tuple(r@Vector(v)+p) for v in part.v)
    b.f.extend(tuple(offset+i for i in f) for f in part.f)
    b.m.extend(part.m);b.col.extend(part.col);b.smooth.extend(part.smooth)

def beam(b,a,c,r=.07,family="Bronze",colour=GOLD):
    b.tube([a,c],[r,r],family,10,colour)

def rail(b,w,y,z,colour=TEAL):
    for h in [.12,1.12]:box(b,(0,y,z+h),(w,.16,.14),"Paint",colour)
    n=max(2,int(w/.8))
    for i in range(n+1):
        x=-w*.5+w*i/n
        box(b,(x,y,z+.59),(.10,.12,1.03),"Paint",colour)
        if i<n:
            beam(b,(x,y,z+.3),(x+w/n,y,z+.95),.028)
            beam(b,(x,y,z+.95),(x+w/n,y,z+.3),.028)

def roof(b,w,d,z):
    # A curved copper roof, assembled as ribs and real overlapping metal strips.
    for i in range(25):
        t=(i-12)/12.; x=t*w*.5
        h=1.6*(1-t*t)
        box(b,(x,0,z+h),(w/24+.055,d,.20),"Paint",TEAL)
    for y in [-d*.5,0,d*.5]:
        ps=[((i-16)*w/32,y,z+1.6*(1-((i-16)/16)**2)+.16) for i in range(33)]
        b.tube(ps,[.065]*33,"Bronze",8,GOLD)

def shrub(b,p,size,seed,trailing=False):
    rng=random.Random(seed);p=Vector(p)
    for j in range(8 if trailing else 11):
        a=rng.random()*math.tau
        end=p+Vector((math.cos(a)*size*.62,math.sin(a)*size*.62,
                      -size*(1.0+rng.random()) if trailing else size*(.4+rng.random()*.5)))
        ps=[p.lerp(end,k/6)+Vector((0,0,math.sin(k/6*math.pi)*size*.22)) for k in range(7)]
        b.tube(ps,[.03*size*(1-k/8) for k in range(7)],"Bark",6)
        for k in range(2,7):
            for s in [-1,1]:
                for n in range(2):
                    b.leaf(ps[k]+Vector((n*.05,n*.1,0)),size*.35,size*.17,
                           a+s*1.1+n*.5,.1+s*.3,"Foliage",
                           (.025+rng.random()*.015,.115+rng.random()*.09,.035+rng.random()*.025,1))

def planter(b,p,length,seed,trailing=True):
    x,y,z=p
    box(b,(x,y,z+.55),(length,1.8,1.1),colour=STONE)
    box(b,(x,y,z+1.1),(length+.3,2.05,.25))
    box(b,(x,y,z+1.18),(length-.35,1.40,.1),"Soil",(.14,.10,.045,1))
    for j in range(max(1,int(length/1.3))):
        q=(x-length*.36+j*1.3,y,z+1.3)
        shrub(b,q,1.15,seed+j)
        if trailing and j%2==0:shrub(b,(q[0],y-.7,z+.9),2.15,seed+50+j,True)

def small_window(b,x,y,z,seed):
    box(b,(x,y+.32,z+1.5),(2.0,.08,2.8),"Paint",BLUE)
    for dx in [-1.12,1.12]:box(b,(x+dx,y,z+1.5),(.20,.50,3.2))
    for dz in [0,3.0]:box(b,(x,y-.02,z+dz),(2.7,.58,.20))
    box(b,(x,y+.15,z+1.5),(.095,.12,2.9),"Bronze",GOLD)
    box(b,(x,y+.13,z+1.2),(2.2,.12,.085),"Bronze",GOLD)
    if seed%3==0:
        box(b,(x-1.55,y-.09,z+1.4),(.63,.20,2.7),"Paint",TEAL)
        box(b,(x+1.55,y-.09,z+1.4),(.63,.20,2.7),"Paint",TEAL)

def tall_window(b,x,y,z,seed):
    # A tall recessed arch with coloured glass and radial tracery.
    radius=4.12;spring=z+9.7
    open_gallery=seed%3==0
    if not open_gallery:
        box(b,(x,y+1.35,z+7.3),(9.1,.30,15.4),"Paint",(.075,.14,.135,1))
    colours=[(.10,.31,.28,1),(.38,.24,.06,1),(.035,.17,.25,1),(.15,.35,.31,1)]
    for j in range(0 if open_gallery else 7):
        xx=(j-3)*1.10
        top=spring+math.sqrt(max(0,radius*radius-xx*xx))
        for k in range(4):
            bot=z+.65+k*3.3;hi=min(bot+3.17,top)
            if hi>bot:
                box(b,(x+xx,y+.96,(bot+hi)*.5),(1.025,.055,hi-bot),
                    "Glow" if (j+k+seed)%13==0 else "Paint",colours[(j+k+seed)%4],.006)
        beam(b,(x+xx,y+.73,z+.55),(x+xx,y+.73,top),.055)
    if open_gallery:
        # This arch is now a real entrance at the public floor datum.
        box(b,(x,y-1.0,-.16),(9.3,4.5,.32))
    else:
        for h in [3.8,7.1,10.4]:box(b,(x,y+.71,z+h),(8.2,.13,.12),"Bronze",GOLD)
    for s in [-1,1]:
        box(b,(x+s*4.4,y-.12,z+4.9),(.65,1.05,10.0))
        column(b,(x+s*4.85,y-.56,z+.12),9.5,.25)
    for r,t,d in [(4.42,.62,1.1),(4.92,.18,.58),(5.18,.14,.38)]:
        b.arc((x,y-.06,spring),r,t,depth=d,segments=48)
    for a in range(15,180,30 if open_gallery else 15):
        a=math.radians(a)
        beam(b,(x,y+.66,spring),(x+math.cos(a)*radius,y+.66,spring+math.sin(a)*radius),.055)
    if not open_gallery:box(b,(x,y-.32,z+.12),(10.2,1.75,.35))
    # Sculpted, circular keystone medallion gives the large opening a readable centre.
    med=Builder()
    med.arc((0,0,0),.74,.14,0,math.tau,"Bronze",.20,48)
    for a in range(0,360,45):
        t=math.radians(a);beam(med,(0,0,0),(math.cos(t)*.59,0,math.sin(t)*.59),.045)
    merge(b,med,(x,y-.72,spring+4.9))

def library_facade(b,variant,apartment=False):
    y=-18.;tint=[STONE,(.68,.66,.53,1),(.56,.69,.66,1)][variant%3]
    # The lower half is a monumental reading gallery; upper floors remain homes.
    for x in [-17.4,-5.7,5.7,17.4]:
        box(b,(x,y+1.0,9),(1.15,2.1,18),colour=tint)
        box(b,(x,y-.42,9),(.42,.50,18))
    box(b,(0,y+.95,16.65),(36,2.0,2.7),colour=tint)
    for j,x in enumerate([-11.5,0,11.5]):tall_window(b,x,y,.65,variant*17+j)
    for z in [0,18,36]:
        gap=([-11.5,0,11.5][(-variant*17)%3]-4.15,[-11.5,0,11.5][(-variant*17)%3]+4.15) if z==0 else (-3.5,-1.) if z==18 and apartment else None
        for a,c in ([(-18.6,gap[0]),(gap[1],18.6)] if gap else [(-18.6,18.6)]):
            box(b,((a+c)/2,y-.15,z),(c-a,.9,.50))
        box(b,(0,y-.12,z-.43),(36.9,.60,.16),"Paint",TEAL)
    for lev in range(4):
        z=18+lev*4.5
        if apartment and lev==0:
            for a,c in [(-18,-8),(-5.5,-3.5),(-1,18)]:box(b,((a+c)/2,y+1,z+1.7),(c-a,.3,3.4),colour=tint)
            box(b,(-6.75,y+1,z+.275),(2.5,.3,.55),colour=tint)
            box(b,(-2.25,y+1,z+3.125),(2.5,.3,.55),colour=tint)
            box(b,(-6.75,y+1,z+3.275),(2.5,.3,.25),colour=tint)
            box(b,(0,y+1,z+3.95),(36,.3,1.1),colour=tint)
        else:box(b,(0,y+1.0,z+2.25),(36,.3,4.5),colour=tint)
        for j in range(8):
            x=-15.75+j*4.5
            for s in [-1,1]:box(b,(x+s*1.68,y+.48,z+2.25),(1.05,1.0,4.5),colour=tint)
            opened=apartment and lev==0 and j in (2,3)
            if not opened:box(b,(x,y+.46,z+.4),(2.4,1,.8),colour=tint)
            box(b,(x,y+.46,z+4.08),(2.4,1,.85),colour=tint)
            if opened:
                height=2.85 if j==3 else 3.15
                box(b,(x,y+.46,z+(height+4.5)/2),(2.4,1,4.5-height),colour=tint)
                if j==2:
                    box(b,(x,y+.46,z+.275),(2.4,1,.55),colour=tint)
                    for h in [.58,1.1]:box(b,(x,y-.1,z+h),(2.5,.08,.065),'Bronze',GOLD,.01)
                    for dx in [-1.12,-.56,0,.56,1.12]:box(b,(x+dx,y-.1,z+.82),(.045,.065,.55),'Bronze',GOLD,.008)
            else:small_window(b,x,y-.12,z+.75,variant+j+lev)
        box(b,(0,y-.05,z+4.36),(36.7,.70,.28))
    # Deep, inhabited balcony at the gallery roof; integral to the building.
    box(b,(0,y-2.5,17.72),(38.0,5.7,.5))
    for x in [-17,-10,10,17]:column(b,(x,y-4.7,18),3.1,.15)
    for side in [-1,1]:
        # The runtime adds exterior guards with verified bridge/lift openings.
        planter(b,(side*11,y-4.1,18),6.5,variant*19+int(side))
    # A canopy shelters the middle entrance without covering the entire facade.
    canopy=Builder();roof(canopy,12,5.8,0);merge(b,canopy,(0,y-2.7,21.35))

for variant in range(0 if GARDENS_ONLY else 3):
    b=Builder()
    for side in range(4):
        f=Builder();library_facade(f,variant+side);merge(b,f,yaw=side*math.pi*.5)
    for z in [0,18,36]:box(b,(0,0,z-.32),(36,36,.62),colour=STONE)
    for x in [-13,13]:
        for y in [-13,13]:column(b,(x,y,0),18,.6)
    # Two levels of open reading galleries look into a real room with shelves,
    # differently sized books and warm practical lights.
    for side in range(4):
        interior=Builder();rng=random.Random(170+variant*7+side)
        for x in [-11,0,11]:
            box(interior,(x,10,7.2),(8.7,.55,13.6),"Timber",(.20,.12,.055,1))
            for dx in [-4.45,4.45]:box(interior,(x+dx,8.9,7.2),(.25,2.6,13.9),"Timber",(.24,.15,.06,1))
            for level in range(8):
                z=.5+level*1.75
                box(interior,(x,8.85,z),(9.05,2.7,.18),"Timber",(.31,.20,.075,1))
                for j in range(16):
                    h=.65+rng.random()*.8;xx=x-4.1+j*.52
                    colour=[(.12,.23,.21,1),(.31,.11,.06,1),(.34,.26,.085,1),(.12,.16,.25,1)][(j+level)%4]
                    box(interior,(xx,8.0,z+.12+h*.5),(.32+rng.random()*.14,.62,h),"Paint",colour,.012)
                    box(interior,(xx,7.66,z+.12+h*.70),(.31,.06,.035),"Bronze",GOLD,.004)
            box(interior,(x,7.3,14.3),(6.5,.35,.55),"Glow",(.30,.17,.055,1))
        for z in [0,7.0]:
            box(interior,(0,-11,z-.18),(31,9,.35),"Timber",(.33,.21,.09,1))
            r=Builder();rail(r,30,0,0);merge(interior,r,(0,-6.7,z))
            for x in [-11,0,11]:
                box(interior,(x,-9,z+1.2),(2.8,1.7,.15),"Timber",(.32,.18,.07,1))
                for dx in [-1.1,1.1]:box(interior,(x+dx,-9,z+.6),(.16,1.3,1.2),"Timber",(.21,.12,.05,1))
        merge(b,interior,yaw=side*math.pi*.5)
    export("UrbanLibrary_"+str(variant),b,"36m continuous library and residence with 16m recessed stained-glass arches, tracery and planted gallery")

for variant in range(0 if STRUCTURES_ONLY else 3):
    b=Builder()
    # Wraparound garden is supported at a real structural floor of a city block.
    box(b,(0,0,-.5),(41.5,41.5,1.0))
    for side in range(4):
        for x in [-14,-5,5,14]:
            pp=Builder();planter(pp,(x,-18.6,0),5.0,variant*79+side*13+x)
            merge(b,pp,yaw=side*math.pi*.5)
    export("UrbanTerrace_"+str(variant),b,"Attached garden balcony with hanging plants; no hidden trees or pavilion inside the next storey")
    # A corner pavilion and a smaller tree distinguish the roof silhouette.
    for x in [-12,0]:
        for y in [5,17]:column(b,(x,y,.1),5.2,.22)
    canopy=Builder();roof(canopy,14,15,5.65);merge(b,canopy,(-6,11,0))
    for x,y,size in [(11,11,4.4),(-12,-10,3.4)]:
        box(b,(x,y,.55),(5.4,5.4,1.1),colour=STONE)
        b.tube([(x,y,1),(x+.3,y,3),(x+.7,y+.3,5)],[.3,.20,.08],"Bark",12)
        for j in range(5):shrub(b,(x+math.cos(j*1.4)*1.6,y+math.sin(j*1.4)*1.6,4.0+(j%2)),size,variant*99+j)
    export("UrbanGarden_"+str(variant),b,"Supported wraparound garden with dense hanging plants, small trees, brass rails and copper-roof pavilion")

for variant in range(0 if GARDENS_ONLY or WALK_ACCESS_ONLY else 2):
    b=Builder();w=7.0
    box(b,(0,0,-.4),(64,w,.8))
    for y in [-w*.5,w*.5]:
        rail(b,64,y,0)
        # Two shallow asymmetric supporting ribs leave more open sky than deep repeated arches.
        ps=[(-32+i*64/40,y,-1.3-6.0*((i-20)/20)**2) for i in range(41)]
        b.tube(ps,[.42]*41,"Limestone",12,STONE)
    for x in range(-28,29,7):
        for y in [-w*.5,w*.5]:column(b,(x,y,0),4.4,.16)
    if variant==0:
        cover=Builder();roof(cover,w+1,64,4.7);merge(b,cover,yaw=math.pi*.5)
    else:
        for x in [-21,-7,7,21]:
            pp=Builder();planter(pp,(0,0,0),5,120+x);merge(b,pp,(x,-4.0,.05))
    # A central suspended lantern announces a destination in the street canyon.
    box(b,(0,0,-3.1),(2.2,2.2,3.0),"Paint",TEAL)
    for s in [-1,1]:
        box(b,(s*1.13,0,-3.1),(.06,1.7,2.3),"Glow",(.42,.22,.055,1))
        box(b,(0,s*1.13,-3.1),(1.7,.06,2.3),"Glow",(.42,.22,.055,1))
    beam(b,(0,0,-.65),(0,0,-1.65),.14)
    export("UrbanPromenade_"+str(variant),b,"64m planted or copper-roof promenade with shallow ribs and a suspended amber lantern")

names={item["name"] for item in CATALOG}
existing["assets"]=[item for item in existing["assets"] if item["name"] not in names]+CATALOG
existing["triangles"]=sum(item["triangles"] for item in existing["assets"])
if WALK_ACCESS_ONLY:
    existing["city_district_art"]["walk_access_source"]="CityGardenAccess.blend"
elif STRUCTURES_ONLY or GARDENS_ONLY:
    existing["city_district_art"]["assets"]=sorted(set(existing["city_district_art"]["assets"])|names)
    key="garden" if GARDENS_ONLY else "structure"
    existing["city_district_art"][key+"_adjustment_source"]="CityGardens.blend" if GARDENS_ONLY else "CityLibraries.blend"
    existing["city_district_art"][key+"_adjustment_seconds"]=round(time.time()-started,3)
else:
    existing["city_district_art"]={"assets":sorted(names),"segment_height_m":36,
        "blend_source":"CityDistricts.blend","authored_seconds":round(time.time()-started,3)}
(OUT/"kit-catalog.json").write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/("CityGardenAccess.blend" if WALK_ACCESS_ONLY else "CityGardens.blend" if GARDENS_ONLY else "CityLibraries.blend" if STRUCTURES_ONLY else "CityDistricts.blend")))
print("EW_CITY_DISTRICTS_COMPLETE",len(CATALOG),sum(a["triangles"] for a in CATALOG),round(time.time()-started,3),flush=True)
