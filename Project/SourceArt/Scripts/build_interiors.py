"""Authored, reusable apartment interiors and a full-height reading hall.

Four-sided entrances and ten distinct room types use twenty-nine reusable meshes.
Furniture footprints generate the C++ collision plan from the same definitions.
New geometry is authored in Unreal metres, then compensates FBX's Y reflection.
"""
from pathlib import Path
import ast, json, math, random, runpy, sys
import bpy
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
Builder, export, column=g['Builder'],g['export'],g['column']
OUT,CATALOG=g['OUT'],g['CATALOG']
existing=json.loads((OUT/'kit-catalog.json').read_text(encoding='utf8'))
SELECTOR=next((arg.split('=',1)[1] for arg in sys.argv if arg.startswith('--only=')),None)
ONLY=set(SELECTOR.split(',')) if SELECTOR else None
FINISH={'Timber':'InteriorOak','Paint':'InteriorPlaster','Cloth':'InteriorFabric',
        'Ceramic':'InteriorCeramic','Glow':'InteriorGlow','Bronze':'InteriorBrass',
        'Foliage':'InteriorLeaf','Bark':'InteriorStem','Paper':'InteriorPaper'}
for original,family in FINISH.items():
    g['FAMILIES'].append(family);g['COLOURS'][family]=g['COLOURS'][original]
    mat=bpy.data.materials.new('M_'+family);mat.diffuse_color=g['COLOURS'][family]
    g['MATERIALS'].append(mat)
OAK=(.34,.145,.052,1);LIGHT_OAK=(.59,.32,.125,1);WALNUT=(.105,.041,.018,1)
IVORY=(.89,.81,.66,1);TEAL=(.025,.245,.195,1);LINEN=(.76,.57,.35,1)
GOLD=(.72,.40,.115,1);STONE=(.80,.84,.82,1);TRIM=(.91,.92,.85,1)
SHADOW=(.045,.095,.115,1);GLASS=(.12,.27,.32,1);COPPER=(.21,.44,.42,1)

def merge(b,part,p=(0,0,0),yaw=0):
    offset=len(b.v);r=Matrix.Rotation(yaw,3,'Z');p=Vector(p)
    b.v.extend(tuple(r@Vector(v)+p) for v in part.v)
    b.f.extend(tuple(offset+i for i in f) for f in part.f)
    b.m.extend(part.m);b.col.extend(part.col);b.smooth.extend(part.smooth)

def box(b,p,d,family='Timber',colour=OAK,bevel=.025,yaw=0):
    b.box(p,d,family,bevel,yaw,colour)

def beam(b,a,c,r=.025,family='Bronze',colour=GOLD):
    b.tube([a,c],[r,r],family,10,colour)

def load_functions(filename,names,namespace):
    tree=ast.parse(Path(__file__).with_name(filename).read_text(encoding='utf8'))
    defs=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names]
    assert {n.name for n in defs}==set(names)
    exec(compile(ast.Module(body=defs,type_ignores=[]),filename,'exec'),namespace)

city=dict(globals());load_functions('city_volume.py',('merge','box','balustrade','hanging','window','facade','urban_block'),city)
library=dict(globals());library.update(WHITE=TRIM,BLUE=(.045,.16,.21,1))
load_functions('city_districts.py',('box','merge','beam','rail','roof','shrub','planter','small_window','tall_window','library_facade'),library)

def exported(name,b,notes,blender=False,rays=()):
    if ONLY and name not in ONLY:return
    expected=[(x,-y,z) for x,y,z in b.v] if blender else list(b.v)
    if rays:
        tree=BVHTree.FromPolygons(expected,b.f)
        for label,a,c in rays:
            delta=Vector(c)-Vector(a)
            hit=tree.ray_cast(Vector(a),delta.normalized(),delta.length)[0]
            assert hit is None,(name,label,list(hit) if hit else None)
    if not blender:
        remap={g['FAMILIES'].index(k):g['FAMILIES'].index(v) for k,v in FINISH.items()}
        b.m=[remap.get(n,n) for n in b.m]
        b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(f)) for f in b.f]
    export(name,b,notes)
    item=CATALOG[-1];item['nanite']=True
    item['ue_bounds_cm']=[[min(v[i] for v in expected)*100 for i in range(3)],
                          [max(v[i] for v in expected)*100 for i in range(3)]]
    indices=sorted({0,len(expected)//4,len(expected)//2,len(expected)*3//4,len(expected)-1})
    item['ue_anchor_vertices_cm']=[{'name':'vertex_'+str(i),'position':[v*100 for v in expected[i]]} for i in indices]
    item['coordinate_contract']='interior-unreal-cm-v1';item['source_rays_clear']=[r[0] for r in rays]

def bookcase(b,x,y,w=1.8,h=2.4,seed=0):
    g['craft'].bookcase(b,x,y,w,h,OAK,TEAL,depth=.46,seed=seed)

def plant(b,x,y,size=1,seed=0):
    b.lathe([(0,0),(.23,0),(.28,.08),(.32,.47),(.32,.51),(.28,.51),(.25,.44)],(x,y,0),'Ceramic',32,IVORY)
    rng=random.Random(seed)
    for j in range(9):
        a=j*math.tau/9;z=.65+rng.random()*.6*size
        p=(x+math.cos(a)*.2*size,y+math.sin(a)*.2*size,z)
        beam(b,(x,y,.42),p,.012,'Bark',(.12,.19,.08,1))
        b.leaf(p,.38*size,.18*size,a,.2,'Foliage',(.15,.29+rng.random()*.12,.19,1))

def lamp(b,x,y,z,standing=False):
    base=.035 if standing else z
    b.lathe([(0,0),(.17,0),(.18,.035),(.12,.07),(0,.07)],(x,y,base),'Bronze',32,GOLD)
    beam(b,(x,y,base+.04),(x,y,z+.42),.022)
    g['craft'].shade(b,x,y,z+.28,.29,.38,IVORY)

def vase(b,x,y,z):
    b.lathe([(0,0),(.07,0),(.12,.10),(.08,.25),(.055,.32),(.04,.32),(.06,.22)],(x,y,z),'Ceramic',32,TEAL)
    for j in range(3):
        p=(x+(j-1)*.08,y+.06*j,z+.58+.1*(j%2));beam(b,(x,y,z+.24),p,.005,'Bark',TEAL)
        b.leaf(p,.13,.05,j*1.4,.1,'Foliage',TEAL)

def art(b,x,y,z,w=1.5,h=.85):
    box(b,(x,y,z),(w+.1,.075,h+.1),colour=LIGHT_OAK)
    box(b,(x,y-.045,z),(w,.016,h),'Cloth',IVORY,.002)
    # Original landscape relief: copper sun and overlapping river terraces.
    sun=(x+w*.23,y-.071,z+h*.22);radius=h*.14
    circle=[sun]+[(sun[0]+math.cos(i*math.tau/40)*radius,sun[1],sun[2]+math.sin(i*math.tau/40)*radius) for i in range(40)]
    b.mesh(circle,[(0,1+i,1+(i+1)%40) for i in range(40)],'Paint',(.79,.32,.08,1),smooth=False)
    for j,col in enumerate([(.32,.45,.26,1),TEAL,(.59,.25,.08,1)]):
        pts=[(x-w*.47+i*w*.94/24,y-.085-j*.015,z+math.sin(i/24*math.pi*1.8+j)*h*.15-j*h*.13) for i in range(25)]
        vertices=pts+[(pts[-1][0],pts[-1][1],z-h*.46),(pts[0][0],pts[0][1],z-h*.46)]
        b.mesh(vertices,[tuple(reversed(range(len(vertices))))],'Paint',col,smooth=False)
        b.tube(pts,[h*.012]*len(pts),'Bronze',8,GOLD)

def chair(b,x,y,yaw=0,colour=TEAL):
    g['craft'].chair(b,x,y,yaw,colour,WALNUT)

def rug(b,x,y,w,d):
    box(b,(x,y,.009),(w,d,.018),'Cloth',TEAL,.007)
    box(b,(x,y,.023),(w-.12,d-.12,.012),'Cloth',LINEN,.004)
    for j in range(max(1,int(d/.24))):
        yy=y-d/2+.12+j*.24
        box(b,(x,yy,.032),(w-.24,.065,.005),'Cloth',[TEAL,(.58,.20,.075,1),IVORY][j%3],.001)
    for side in [-1,1]:
        for j in range(int(w/.09)):
            beam(b,(x-w/2+j*.09,y+side*(d/2-.01),.018),(x-w/2+j*.09,y+side*(d/2+.06),.018),.006,'Cloth',LINEN)

collisions={str(k):[] for k in range(10)}
def solid(kind,p,d):collisions[str(kind)].append({'centre':[v*100 for v in p],'extent':[v*50 for v in d]})
details=Path(__file__).with_name('interior_details.py')
exec(compile(details.read_text(encoding='utf8'),str(details),'exec'),globals())

shell=Builder()
for row in range(20):
    y=-17.85+row*.30
    for col in range(6):
        x=.75+col*1.5
        tint=tuple(v*(.95+.012*((row*7+col*3)%7)) if i<3 else 1 for i,v in enumerate(LIGHT_OAK))
        box(shell,(x,y,-.018),(1.497,.297,.036),colour=tint,bevel=.003)
for x in [.06,8.94]:
    box(shell,(x,-14.96,1.70),(.12,5.90,3.40),'Paint',IVORY)
    box(shell,(x+(.025 if x<1 else -.025),-14.96,.055),(.13,5.90,.11),colour=LIGHT_OAK,bevel=.01)
box(shell,(4.5,-12.06,1.70),(8.78,.12,3.40),'Paint',IVORY)
box(shell,(4.5,-12.145,.53),(8.7,.05,1.06),'Paint',TEAL,.01)
for x in [i*.6+.3 for i in range(15)]:box(shell,(x,-12.184,.53),(.035,.026,.97),colour=LIGHT_OAK,bevel=.004)
box(shell,(4.5,-12.2,1.09),(8.82,.11,.055),colour=LIGHT_OAK,bevel=.01)
box(shell,(4.5,-15,3.445),(9,6,.11),'Paint',IVORY)
for x in [0.2,3.0,6.0,8.8]:box(shell,(x,-15,3.30),(.11,5.80,.18),colour=LIGHT_OAK)
for y in [-17.75,-15,-12.25]:box(shell,(4.5,y,3.30),(8.80,.11,.18),colour=LIGHT_OAK)
for x in [1.05,3.45]:box(shell,(x,-17.65,1.425),(.10,.15,2.85),colour=LIGHT_OAK)
box(shell,(2.25,-17.65,2.86),(2.5,.15,.12),colour=LIGHT_OAK)
for x in [5.44,8.07]:
    for j in range(7):
        xx=x+(j-3)*.045
        box(shell,(xx,-17.48+math.sin(j*1.7)*.03,1.99),(.052,.045,2.3),'Cloth',LINEN,.009)
beam(shell,(5.2,-17.49,3.19),(8.35,-17.49,3.19),.026)
for x in [2.2,6.6]:
    beam(shell,(x,-14.6,3.4),(x,-14.6,2.86),.012)
    shell.lathe([(.36,0),(.32,.12),(.19,.31),(0,.34)],(x,-14.6,2.74),'Ceramic',40,IVORY)
    shell.lathe([(0,0),(.30,0),(.30,.018),(0,.018)],(x,-14.6,2.745),'Glow',32,(1,.85,.64,1))
for shell_name,accent in [('InteriorShellRose',(.35,.073,.084,1)),('InteriorShellSage',(.13,.29,.11,1)),
                          ('InteriorShellBlue',(.055,.14,.26,1)),('InteriorShell',TEAL)]:
    themed=Builder();merge(themed,shell)
    themed.col=[accent if tuple(c)==TEAL else c for c in themed.col]
    exported(shell_name,themed,'Warm timber, a room-specific accent wall, pleated curtains and pendant lights',
             rays=[('door_eye',(2.25,-20.9,1.62),(2.25,-15.2,1.62)),('window_eye',(6.75,-16.3,1.62),(6.75,-19,1.62))])
entry=Builder();welcoming_entry(entry)
exported('InteriorEntry',entry,'Open folded doors, carved sunburst, warm lanterns and a woven doorstep',
         rays=[('entry_eye',(2.25,-22.0,1.62),(2.25,-15.2,1.62))])

names=['Living','Bedroom','Study','Dining']
for kind,name in enumerate(names):
    b=Builder();rug(b,5.95,-15.10,4.2,2.2)
    if kind==0:
        box(b,(5.9,-12.81,.23),(2.65,.91,.38),'Cloth',LINEN,.09)
        box(b,(5.9,-12.43,.83),(2.65,.20,.84),'Cloth',LINEN,.10)
        for x in [4.62,7.18]:box(b,(x,-12.81,.70),(.20,.98,.51),'Cloth',LINEN,.09)
        solid(kind,(5.9,-12.81,.55),(2.85,1.03,1.10))
        box(b,(5.95,-13.80,.42),(1.40,.61,.09),colour=LIGHT_OAK,bevel=.065)
        for x in [5.48,6.42]:box(b,(x,-13.8,.20),(.065,.43,.40),colour=WALNUT)
        solid(kind,(5.95,-13.80,.245),(1.40,.61,.49));vase(b,6.35,-13.80,.47)
        chair(b,8.1,-15.6,-math.pi/2);solid(kind,(8.1,-15.6,.50),(.63,.66,1))
        bookcase(b,.90,-14.0,1.4,1.7,3);solid(kind,(.90,-14.15,.90),(1.50,.55,1.8))
        art(b,5.9,-12.23,2.06,2.2,1.0);lamp(b,8.25,-13.15,1.23,True)
    elif kind==1:
        box(b,(6.6,-13.60,.27),(2.12,2.14,.40),colour=WALNUT,bevel=.05)
        box(b,(6.6,-13.60,.52),(2.04,2.08,.24),'Cloth',IVORY,.12)
        box(b,(6.6,-13.92,.655),(2.10,1.50,.14),'Cloth',(.54,.64,.59,1),.095)
        box(b,(6.6,-14.37,.765),(2.12,.35,.045),'Cloth',TEAL,.025)
        box(b,(6.6,-12.48,.80),(2.22,.15,1.35),'Cloth',LINEN,.08)
        solid(kind,(6.6,-13.6,.65),(2.22,2.39,1.3))
        for x in [5.1,8.1]:
            box(b,(x,-12.8,.29),(.58,.59,.58),colour=LIGHT_OAK);lamp(b,x,-12.8,.6)
            solid(kind,(x,-12.8,.32),(.61,.63,.64))
        box(b,(.69,-13.58,1.24),(1.17,2.63,2.48),colour=LIGHT_OAK)
        for y in [-14.1,-13.1]:box(b,(1.29,y,1.24),(.045,.93,2.25),'Paint',IVORY,.01)
        solid(kind,(.69,-13.58,1.24),(1.2,2.66,2.48))
    elif kind==2:
        bookcase(b,2.55,-12.38,3.15,2.55,81);solid(kind,(2.55,-12.50,1.35),(3.30,.65,2.70))
        box(b,(6.35,-12.90,.76),(2.30,.85,.08),colour=LIGHT_OAK)
        for x in [5.35,7.35]:box(b,(x,-12.90,.36),(.09,.68,.72),colour=WALNUT)
        solid(kind,(6.35,-12.9,.40),(2.3,.85,.80));chair(b,6.35,-13.82,0)
        solid(kind,(6.35,-13.82,.50),(.64,.65,1))
        box(b,(6.4,-12.95,.81),(.60,.42,.022),'Paper',IVORY,.002)
        lamp(b,5.58,-12.90,.81);vase(b,7.15,-12.77,.81)
        art(b,6.4,-12.23,2.06,1.95,.95)
        chair(b,8.12,-16.30,-math.pi/2,LINEN);solid(kind,(8.12,-16.3,.5),(.66,.66,1.))
    else:
        box(b,(6.4,-13.73,.78),(2.25,1.02,.11),colour=LIGHT_OAK,bevel=.055)
        for x in [5.52,7.28]:
            for y in [-14.04,-13.42]:box(b,(x,y,.37),(.09,.09,.74),colour=WALNUT)
        solid(kind,(6.4,-13.73,.43),(2.25,1.02,.86))
        for x in [5.80,7.0]:
            for y,yaw in [(-12.74,0),(-14.72,math.pi)]:
                chair(b,x,y,yaw,LINEN);solid(kind,(x,y,.5),(.64,.64,1))
                b.lathe([(0,0),(.16,0),(.18,.012),(.17,.025),(0,.025)],(x,-13.73+(.25 if y>-13 else -.25),.84),'Ceramic',28,IVORY)
        vase(b,6.40,-13.73,.84)
        box(b,(1.1,-13.24,.47),(1.4,1.78,.94),'Paint',TEAL)
        box(b,(1.1,-13.24,.97),(1.46,1.84,.08),'Ceramic',IVORY)
        solid(kind,(1.1,-13.24,.51),(1.46,1.84,1.02))
        bookcase(b,3.35,-12.4,1.30,2.15,191);solid(kind,(3.35,-12.5,1.15),(1.40,.65,2.30))
        art(b,6.45,-12.23,2.2,2.0,.85)
    enhance_room(b,kind)
    plant(b,8.5,-17.0,.85,kind+1);solid(kind,(8.5,-17.,.30),(.64,.64,.60))
    exported('Interior'+name,b,'Human-scale '+name.lower()+' furniture, upholstery, artwork, books, ceramic details and plants')

# The existing authored window seat, valve and safe standing location remain.
b=Builder();rug(b,4.6,-14.45,3.6,1.55)
bookcase(b,2.4,-12.38,2.5,2.45,213);solid(5,(2.4,-12.50,1.3),(2.62,.62,2.60))
box(b,(8.0,-12.75,.78),(1.40,.64,.09),colour=LIGHT_OAK)
for x in [7.44,8.56]:box(b,(x,-12.75,.37),(.08,.53,.74),colour=WALNUT)
solid(5,(8.0,-12.75,.415),(1.40,.64,.83));vase(b,8.20,-12.74,.84);lamp(b,7.55,-12.72,.84)
art(b,5.1,-12.22,2.13,1.95,1.02)
for x in [4.0,5.5,7.,8.5]:box(b,(x,-12.22,.54),(1.4,.05,1.04),'Paint',TEAL,.01)
enhance_room(b,5)
exported('InteriorRain',b,'Additional reading furniture and wall finishes around the retained rain-window seat')

for kind,name in [(6,'Cafe'),(7,'Botanical'),(8,'Atelier'),(9,'Music')]:
    b=Builder();rug(b,5.95,-15.1,4.2,2.2);new_room(b,kind)
    exported('Interior'+name,b,'Distinct '+name.lower()+' with authored furnishings, working circulation and small personal details')

b=Builder()
# A clear axial cross connects all four real entrances; furnishings form islands.
for y in range(-17,18):
    for x in range(-5,6):box(b,(x*3.2,y,.004),(3.196,.996,.028),colour=LIGHT_OAK,bevel=.002)
for x in [-8.2,8.2]:
    for y in [-8.2,8.2]:
        rug(b,x,y,7,5.4)
        for dx in [-1.5,1.5]:
            xx=x+dx;box(b,(xx,y,.76),(2.30,1.05,.10),colour=LIGHT_OAK)
            solid(4,(xx,y,.42),(2.30,1.05,.84))
            for side in [-1,1]:
                chair(b,xx,y+side*1.15,0 if side>0 else math.pi)
                solid(4,(xx,y+side*1.15,.5),(.65,.65,1.))
            lamp(b,xx+.65,y,.82);box(b,(xx-.45,y,.823),(.44,.32,.035),'Paper',IVORY,.005)
        for dy in [-3.6,3.6]:
            bookcase(b,x,y+dy,4.5,2.7,int(x+y+dy+99));solid(4,(x,y+dy-.12,1.42),(4.65,.63,2.84))
        plant(b,x+3.2,y+2.0,1.5,int(x+y+99));solid(4,(x+3.2,y+2,.3),(.65,.65,.6))
for x in [-13.,13.]:
    for y in [-13.,13.]:solid(4,(x,y,8.),(1.45,1.45,16.))
for x in [-12.,0.,12.]:
    for y in [-12.,0.,12.]:
        beam(b,(x,y,17.65),(x,y,7.7),.027)
        b.lathe([(.95,0),(.84,.28),(.32,.7),(0,.73)],(x,y,7.4),'Ceramic',48,IVORY)
        b.lathe([(0,0),(.85,0),(.85,.04),(0,.04)],(x,y,7.405),'Glow',40,(1,.82,.58,1))
for x in [-15.,-9.,-3.,3.,9.,15.]:box(b,(x,0,17.61),(.18,35.5,.26),colour=LIGHT_OAK)
for y in [-15.,-9.,-3.,3.,9.,15.]:box(b,(0,y,17.6),(35.5,.18,.26),colour=LIGHT_OAK)
exported('InteriorLibrary',b,'Open reading hall with eight desks, sixteen chairs, eight real-scale bookcases, rugs and pendant lights')

for variant in range(8):
    b=Builder();city['urban_block'](b,variant,'all',(0.,18.),True)
    rays=[]
    for side in range(4):
        rotation=Matrix.Rotation(side*math.pi*.5,3,'Z')
        for floor in (0.,18.):
            a=rotation@Vector((2.25,-20.9,floor+1.62));c=rotation@Vector((2.25,-14.5,floor+1.62))
            rays.append((f'door_{side}_{floor}',tuple(a),tuple(c)))
    exported('UrbanBlock_'+str(variant),b,'Eight real rooms: an open doorway on every side of both public gallery floors',True,rays)
for variant in range(3):
    b=Builder()
    for side in range(4):
        panel=Builder();library['library_facade'](panel,variant+side,apartment=True)
        merge(b,panel,yaw=side*math.pi*.5)
    for z in [0.,18.,36.]:city['box'](b,(0,0,z-.32),(36,36,.62),colour=STONE)
    for x in [-13.,13.]:
        for y in [-13.,13.]:column(b,(x,y,0),18,.6)
    rays=[]
    for side in range(4):
        r=Matrix.Rotation(side*math.pi*.5,3,'Z')
        rays.append(('upper_door_'+str(side),tuple(r@Vector((2.25,-20.9,19.62))),tuple(r@Vector((2.25,-14.5,19.62)))))
    for side in range(4):
        # Source to UE reverses both side winding and the tangent direction.
        j=(-(variant+side)*17)%3;xx=[-11.5,0,11.5][j]
        r=Matrix.Rotation(side*math.pi*.5,3,'Z')
        a=r@Vector((xx,-21,1.62));c=r@Vector((xx,-15,1.62))
        rays.append(('hall_door_'+str(side),(a.x,-a.y,a.z),(c.x,-c.y,c.z)))
    exported('UrbanLibrary_'+str(variant),b,'Recessed library facade, level entrances, full-height reading hall and upper apartment',True,rays)

for variant in range(3):
    b=Builder();library['box'](b,(0,0,-.5),(41.5,41.5,1.0))
    for side in range(4):
        for i,x in enumerate([-14.8,-6.5,6.5,14.8]):
            pp=Builder();library['planter'](pp,(x,-18.6,0),3.2,variant*79+side*13+i)
            merge(b,pp,yaw=side*math.pi*.5)
    exported('UrbanTerrace_'+str(variant),b,'Planted gallery with clear apartment and library entry approaches',True)

names={item['name'] for item in CATALOG}
assert (names==ONLY) if ONLY else len(names)==29
all_names=set(existing.get('interior_art',{}).get('assets',[]))|names
existing['assets']=[item for item in existing['assets'] if item['name'] not in names]+CATALOG
existing['materials']=list(dict.fromkeys(existing['materials']+list(FINISH.values())))
existing['triangles']=sum(item['triangles'] for item in existing['assets'])
existing['interior_art']={'revision':70,'coordinate_contract':'interior-unreal-cm-v1','assets':sorted(all_names),
    'room_types':['living','bedroom','study','dining','library','rain-window','cafe','botanical','atelier','music'],'source':'Interiors70/Interiors70.blend',
    'apartment_size_m':[9,6,3.4],'public_floor_offsets_cm':[0,1800],
    'entrance_sides':[0,1,2,3],'rooms_per_residential_module':8,'rooms_per_library_module':5,
    'materials':list(FINISH.values()),'furniture_colliders':collisions}
(OUT/'kit-catalog.json').write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding='utf8')
(OUT/'interior-layout.json').write_text(json.dumps(existing['interior_art'],ensure_ascii=False,indent=2),encoding='utf8')
header=['#pragma once','#include "CoreMinimal.h"','// Generated by SourceArt/Scripts/build_interiors.py. Centimetres.',
        'namespace EWInteriorPlan {','struct Box { FVector Centre,Extent; };','inline TArray<Box> Furniture(int32 Kind) {','switch(Kind) {']
def vec(v):return 'FVector('+','.join(f'{n:.6f}' for n in v)+')'
for kind,rows in collisions.items():
    header.append('case '+kind+': return {'+','.join('{'+vec(r['centre'])+','+vec(r['extent'])+'}' for r in rows)+'};')
header+=['default:return {};','}','}','}']
(OUT/'quality93-interior-plan.generated.h').write_text('\n'.join(header)+'\n',encoding='utf8')
(OUT/'Interiors70').mkdir(exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Interiors70'/((SELECTOR+'70.blend') if ONLY and len(ONLY)==1 else 'Interiors70Corrections.blend' if ONLY else 'Interiors70.blend')))
print('EW_INTERIOR_ART_READY',len(names),sum(x['triangles'] for x in CATALOG),flush=True)
