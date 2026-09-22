"""Original reversible Craft95 architecture. Existing floor/collision envelopes retained."""
from pathlib import Path
import json, math, runpy, hashlib, random
import bpy

HERE=Path(__file__).resolve().parent
source=(HERE/'city_districts.py').read_text(encoding='utf8')
scope={'__file__':str(HERE/'city_districts.py'),'__name__':'craft95_surroundings'}
# Reuse authored construction functions without exporting/replacing old assets.
exec(compile(source[:source.index('for variant in range(')],str(HERE/'city_districts.py'),'exec'),scope)
B=scope['Builder']; box=scope['box']; merge=scope['merge']; export=scope['export']
column=scope['column']; planter=scope['planter']; roof=scope['roof']; OUT=scope['OUT']

def balcony_shrub(b,p,size,seed,trailing=False):
    # Curved leaf surfaces keep an airy silhouette with bounded, shared geometry.
    rng=random.Random(seed)
    for j in range(7):
        a=j*math.tau/7+rng.uniform(-.2,.2)
        pts=[]
        for k in range(8):
            t=k/7
            pts.append((p[0]+math.cos(a)*size*t*.65,p[1]+math.sin(a)*size*t*.65,
                p[2]+size*(math.sin(t*math.pi)*.25-t*.90 if trailing else t*.55)))
        b.tube(pts,[size*(.014-.009*k/7) for k in range(8)],'Bark',6,(.15,.115,.075,1))
        for k in range(2,8):
            for side in [-1,1]:
                ang=a+side*1.0;length=size*rng.uniform(.14,.22);width=length*.43
                start=pts[k];v=[];f=[]
                for n in range(6):
                    t=n/5;w=width*math.sin(math.pi*t)
                    for q in [-1,0,1]:
                        v.append((start[0]+math.cos(ang)*length*t-math.sin(ang)*w*q,
                            start[1]+math.sin(ang)*length*t+math.cos(ang)*w*q,
                            start[2]+length*(.19*math.sin(math.pi*t)-.13*t)-abs(q)*length*.08*math.sin(math.pi*t)))
                for n in range(5):
                    for q in range(2):
                        i=n*3+q;f.append((i,i+1,i+4,i+3))
                b.mesh(v,f,'Foliage',(.08+rng.random()*.04,.22+rng.random()*.06,.09,1))
scope['shrub']=balcony_shrub

for variant in range(3):
    b=B();box(b,(0,0,-.5),(41.5,41.5,1.0))
    for side in range(4):
        for x in [-14,-5,5,14]:
            pp=B();planter(pp,(x,-18.6,0),5.0,variant*79+side*13+x)
            merge(b,pp,yaw=side*math.pi*.5)
    for x in [-12,0]:
        for y in [5,17]:column(b,(x,y,.1),5.2,.22)
    canopy=B();roof(canopy,14,15,5.65);merge(b,canopy,(-6,11,0))
    for x,y in [(11,11),(-12,-10)]:
        box(b,(x,y,.55),(5.4,5.4,1.1),colour=(.59,.63,.59,1))
        # Solid coping and recessed soil catch light at the plant's base.
        for s in [-1,1]:
            box(b,(x+s*2.61,y,1.02),(.20,5.40,.16),colour=(.72,.73,.66,1),bevel=.018)
            box(b,(x,y+s*2.61,1.02),(5.00,.20,.16),colour=(.72,.73,.66,1),bevel=.018)
        box(b,(x,y,1.065),(4.98,4.98,.025),'Bark',(.12,.105,.075,1),.002)
    export('Craft95RoofGarden_'+str(variant),b,'Original roof shell; exact pavilion/planter envelopes; separate branching trees replace old crowns')

# Recessed joinery and warm timber at human scale in the actual cafe hall.
b=B()
for side in [-1,1]:
    x=side*17.80
    for y in [-14,-10,-6,-2,2,6,10,14]:
        box(b,(x,y,.24),(.13,3.70,.48),'Timber',(.23,.14,.075,1),.015)
        box(b,(x-side*.045,y,.50),(.19,3.74,.065),'Bronze',(.38,.29,.15,1),.008)
    for y in [-16,-12,-8,-4,0,4,8,12,16]:
        box(b,(x-side*.05,y,1.0),(.17,.10,2.0),'Timber',(.25,.16,.085,1),.012)
# Entrance archivolt: exterior columns remain clear of the 4m doorway.
for side in [-1,1]:
    x=side*2.28
    column(b,(x,-18.35,0),4.1,.095)
    box(b,(x,-18.28,.12),(.38,.44,.24),colour=(.69,.70,.64,1),bevel=.018)
box(b,(0,-18.35,4.16),(4.9,.34,.13),'Bronze',(.34,.25,.12,1),.012)
# Slender brass inset around the existing water basin; no added walk obstruction.
for x in [3.58,10.42]:box(b,(x,7,.381),(.035,4.84,.019),'Bronze',(.39,.31,.17,1),.006)
for y in [4.58,9.42]:box(b,(7,y,.381),(6.84,.035,.019),'Bronze',(.39,.31,.17,1),.006)
# A quiet frieze above the counter: individual slats give grazing light a rhythm.
for i in range(55):
    box(b,(-12.75+i*.175,17.68,2.2),(.045,.12,3.5),'Timber',(.22+.01*(i%3),.135,.065,1),.008)
export('Craft95CafeJoinery',b,'Cafe doorway, skirting, water-basin rim and counter frieze; clear primary and memory routes')

records=scope['CATALOG']
for item in records:
    lo,hi=item['bounds_m'];item['ue_bounds_cm']=[[lo[0]*100,-hi[1]*100,lo[2]*100],[hi[0]*100,-lo[1]*100,hi[2]*100]]
(OUT/'craft95-surroundings.json').write_text(json.dumps({'revision':95,'assets':records,'collision':'Existing roof and cafe solid envelopes retained; trim stays within structural edges'},indent=2),encoding='utf8')
(OUT/'Blender').mkdir(exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Blender/Craft95Surroundings.blend'))
print('CRAFT95_SURROUNDINGS_DONE',len(records),sum(i['triangles'] for i in records))
