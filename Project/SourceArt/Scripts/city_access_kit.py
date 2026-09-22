"""Public floor rings and open-view lift hardware; all dimensions are metres.

The ring's two apertures on each side are filled or connected by the runtime.
The runtime owns matching floor, wall and guard collision in centimetres.
"""
from pathlib import Path
import runpy, math, json, time
import bpy
from mathutils import Vector, Matrix
g=runpy.run_path(str(Path(__file__).with_name("mesh_primitives.py")))
Builder,export,column=g["Builder"],g["export"],g["column"]
OUT,CATALOG=g["OUT"],g["CATALOG"]
existing=json.loads((OUT/"kit-catalog.json").read_text(encoding="utf8"))
STONE=(.78,.83,.79,1); COPPER=(.08,.29,.27,1); GOLD=(.48,.31,.12,1)
def box(b,p,d,f="Limestone",c=STONE):b.box(p,d,f,.025,colour=c)
def merge(b,part,p=(0,0,0),yaw=0):
    offset=len(b.v);r=Matrix.Rotation(yaw,3,"Z");p=Vector(p)
    b.v.extend(tuple(r@Vector(v)+p) for v in part.v)
    b.f.extend(tuple(offset+i for i in f) for f in part.f)
    b.m.extend(part.m);b.col.extend(part.col);b.smooth.extend(part.smooth)
def guard(b,a,c,y,z=0):
    for h in [.15,1.16]:box(b,((a+c)/2,y,z+h),(c-a,.20,.15))
    n=max(1,math.ceil((c-a)/.8))
    for j in range(n+1):
        x=a+(c-a)*j/n
        b.lathe([(.085,0),(.10,.15),(.06,.60),(.085,1.12)],(x,y,z),"Limestone",10,STONE)
b=Builder()
for side in range(4):
    p=Builder()
    box(p,(0,-21,-.18),(48 if side%2==0 else 36,6,.36))
    box(p,(0,-23.92,-.45),(48,.25,.22),"Paint",COPPER)
    merge(b,p,yaw=side*math.pi/2)
export("UrbanWalkRingFloor",b,"Public floor without fixed guards; ground junction guards are fitted to crossing streets at runtime")
b=Builder()
for side in range(4):
    p=Builder()
    box(p,(0,-21,-.18),(48 if side%2==0 else 36,6,.36))
    box(p,(0,-23.92,-.45),(48,.25,.22),"Paint",COPPER)
    for a,c in [(-24,-17.8),(-14.2,-8),(8,14.2),(17.8,24)]:guard(p,a,c,-23.85)
    merge(b,p,yaw=side*math.pi/2)
export("UrbanWalkRing",b,"48m outer public floor ring; 16m centre bridge apertures and 3.6m lift apertures at x +/-16; guards 1.24m high")
b=Builder()
box(b,(0,0,-.18),(4,4,.36),"Timber",(.28,.18,.08,1))
for x in [-1.9,1.9]:
    for y in [-1.9,1.9]:column(b,(x,y,0),3.1,.07)
for side in [0,1,3]:
    p=Builder();guard(p,-1.9,1.9,-1.9);merge(b,p,yaw=side*math.pi/2)
box(b,(0,0,3.2),(4.3,4.3,.25),"Paint",COPPER)
b.lathe([(0,0),(2.85,0),(2.5,.3),(1.2,.8),(0,1.15)],(0,0,3.32),"Paint",48,COPPER)
for x in [-1.7,1.7]:box(b,(x,1.65,2.65),(.16,.16,.45),"Glow",(.44,.28,.08,1))
export("UrbanLiftCabin",b,"4m open-view lift car with 3.2m clear roof and open boarding side at +Y")
b=Builder()
for x in [-2.3,2.3]:
    box(b,(x,0,18),(.22,.22,36),"Paint",COPPER)
    for z in range(0,37,6):box(b,(x,0,z),(.5,.5,.18),"Bronze",GOLD)
export("UrbanLiftMast",b,"36m paired copper guide rails for continuous lift shaft")
b=Builder()
box(b,(0,0,.07),(1.15,.7,.14))
box(b,(0,0,.65),(.7,.4,1.1),"Paint",COPPER)
box(b,(0,-.225,.9),(.44,.025,.48),"Glow",(.40,.27,.10,1))
for s in [-1,1]:
    b.tube([(s*.16,-.255,.85),(0,-.255,1.02),(s*.16,-.255,1.18)],[.025]*3,"Bronze",8,GOLD)
export("UrbanLiftSign",b,"Warm brass lift call marker; use prompt and floor labels are native game text")
names={a["name"] for a in CATALOG}
existing["assets"]=[a for a in existing["assets"] if a["name"] not in names]+CATALOG
existing["triangles"]=sum(a["triangles"] for a in existing["assets"])
existing["city_access_art"]={"assets":sorted(names),"source":"CityAccessKit.blend","outer_ring_m":48,"public_level_m":18}
(OUT/"kit-catalog.json").write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"CityAccessKit.blend"))
print("CITY_ACCESS_KIT_COMPLETE",len(CATALOG),sum(a["triangles"] for a in CATALOG))
