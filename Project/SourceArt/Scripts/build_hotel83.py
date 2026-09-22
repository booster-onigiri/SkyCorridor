"""Eight original sky suites, inspired by the sourced design study in Design.
Metres; shell, furniture, collision, lighting and route share the same coordinates.
No reference photographs or external meshes are incorporated into the assets.
"""
from pathlib import Path
import bpy,runpy,math,json,random
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree

g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
(g['OUT']/'Generated').mkdir(exist_ok=True)
B,export,OUT=g['Builder'],g['export'],g['OUT'];records=[];plans=[]
FINISH={'Timber':'InteriorOak','Paint':'InteriorPlaster','Cloth':'InteriorFabric','Ceramic':'InteriorCeramic',
        'Glow':'InteriorGlow','Bronze':'InteriorBrass','Foliage':'InteriorLeaf','Bark':'InteriorStem','Paper':'InteriorPaper'}
for original,family in list(FINISH.items())+[('Ceramic','Hotel83Glass')]:
    g['FAMILIES'].append(family);g['COLOURS'][family]=g['COLOURS'][original]
    m=bpy.data.materials.new('M_'+family);m.diffuse_color=g['COLOURS'][family];g['MATERIALS'].append(m)
PALE=(.76,.73,.65,1);GOLD=(.45,.28,.10,1);INK=(.035,.05,.052,1)
THEMES=[
 ('NAGI','凪の和邸',(.62,.47,.28,1),(.81,.77,.65,1),(.17,.27,.21,1)),
 ('GALLERY','雲のギャラリー',(.40,.25,.13,1),(.82,.80,.72,1),(.12,.26,.31,1)),
 ('WATER GARDEN','水庭のサロン',(.38,.20,.09,1),(.79,.75,.61,1),(.07,.29,.21,1)),
 ('AMBER','琥珀のオアシス',(.30,.14,.06,1),(.66,.45,.28,1),(.57,.18,.07,1)),
 ('PEARL','真珠のサロン',(.48,.34,.21,1),(.84,.78,.73,1),(.38,.15,.19,1)),
 ('NOCTURNE','夜景の書斎',(.17,.075,.025,1),(.53,.48,.41,1),(.045,.13,.16,1)),
 ('PRISM','光彩のロフト',(.27,.17,.09,1),(.71,.73,.70,1),(.02,.25,.30,1)),
 ('ALPINE','湖雲のロッジ',(.31,.17,.07,1),(.71,.64,.51,1),(.19,.24,.13,1)),
]

def merge(b,part,p=(0,0,0),yaw=0):
    n=len(b.v);q=Matrix.Rotation(yaw,3,'Z');v=Vector(p)
    b.v.extend(tuple(q@Vector(x)+v) for x in part.v);b.f.extend(tuple(n+i for i in f) for f in part.f)
    b.m.extend(part.m);b.col.extend(part.col);b.smooth.extend(part.smooth)
def box(b,p,d,f='Timber',c=PALE,bevel=.025,yaw=0):b.box(p,d,f,bevel,yaw,c)
def beam(b,a,c,r=.025,f='Bronze',colour=GOLD):b.tube([a,c],[r,r],f,10,colour)
def solid(rows,p,d,floor=False):rows.append({'p':[round(x*100,4) for x in p],'e':[round(x*50,4) for x in d],'floor':floor})
def body(b,rows,p,d,f='Timber',c=PALE,bevel=.025):box(b,p,d,f,c,bevel);solid(rows,p,d)
def soft(b,p,d,c):
    g['craft'].cushion(b,p,d,c)
def plant(b,x,y,size=1,seed=1,z=0):
    rng=random.Random(seed);b.lathe([(0,0),(.24,0),(.33,.12),(.36,.57),(.31,.59),(.29,.49)],(x,y,z),'Ceramic',24,PALE)
    for j in range(13):
        a=j*2.4;t=z+.7+rng.random()*size
        tip=(x+math.cos(a)*.3*size,y+math.sin(a)*.3*size,t)
        beam(b,(x,y,z+.5),tip,.014,'Bark',(.10,.16,.055,1))
        b.leaf(tip,.62*size,.28*size,a,.45,'Foliage',(.06,.19+rng.random()*.1,.09,1))
def lamp(b,x,y,z,wood):
    b.lathe([(0,0),(.20,0),(.21,.05),(.08,.09),(.035,.42)],(x,y,z),'Bronze',24,GOLD)
    g['craft'].shade(b,x,y,z+.38)
def cup(b,x,y,z):
    g['craft'].cup(b,x,y,z)
def art(b,x,y,z,w,h,accent):
    box(b,(x,y,z),(w+.12,.09,h+.12),'Bronze',GOLD,.015);box(b,(x,y-.055,z),(w,.025,h),'Paper',PALE,.006)
    for j,c in enumerate([accent,(.59,.29,.11,1),(.19,.29,.27,1)]):
        pts=[(x-w*.47+i*w*.94/30,y-.080-j*.008,z+math.sin(i/30*5+j)*h*.16-j*h*.17) for i in range(31)]
        pts.extend([(x+w*.47,y-.080-j*.008,z-h*.47),(x-w*.47,y-.080-j*.008,z-h*.47)])
        b.mesh(pts,[tuple(reversed(range(len(pts))))],'Paint',c,smooth=False)
    b.lathe([(0,0),(.16,0),(.16,.025),(0,.025)],(x+w*.22,y-.10,z+h*.24),'Bronze',24,GOLD)
def letters(b,text,p,size=.16,c=GOLD):
    curve=bpy.data.curves.new('suite lettering','FONT');curve.body=text;curve.align_x='CENTER';curve.size=size;curve.extrude=.001
    obj=bpy.data.objects.new('suite lettering',curve);bpy.context.collection.objects.link(obj);obj.location=p;obj.rotation_euler=(math.pi/2,0,0)
    bpy.context.view_layer.update();mesh=bpy.data.meshes.new_from_object(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    b.mesh([tuple(obj.matrix_world@v.co) for v in mesh.vertices],[tuple(f.vertices) for f in mesh.polygons],'Bronze',c,smooth=False)
    bpy.data.objects.remove(obj,do_unlink=True);bpy.data.meshes.remove(mesh)
def table(b,rows,x,y,w,d,h,c):
    b.lathe([(0,0),(.43,0),(.45,.06),(.18,.12),(.14,h),(0,h)],(x,y,0),'Bronze',32,GOLD)
    box(b,(x,y,h),(w,d,.09),'Ceramic',c,.07);solid(rows,(x,y,h/2),(w,d,h+.09));cup(b,x-.25,y,h+.08);cup(b,x+.25,y+.12,h+.08)
def books(b,rows,x,y,w,wood,accent):
    solid(rows,(x,y,1.5),(w,.48,3))
    g['craft'].bookcase(b,x,y,w,3,wood,accent,seed=int(abs(x*13+y*7)))
def tub(b,rows,x,y,wood,stone,rect=False):
    if rect:
        body(b,rows,(x,y,.34),(2.25,1.4,.68),'Ceramic',stone,.08)
        box(b,(x,y,.685),(1.95,1.10,.012),'Water',(.14,.33,.29,1),.08)
        for dx in [-1.1,1.1]:box(b,(x+dx,y,.72),(.15,1.47,.12),'Timber',wood,.035)
    else:
        t=B();t.lathe([(0,0),(.59,0),(.70,.08),(.80,.51),(.84,.65),(.75,.67),(.70,.51),(.53,.12),(0,.12)],family='Ceramic',segments=48,colour=stone)
        t.v=[(a*1.5,c*.83,z) for a,c,z in t.v];merge(b,t,(x,y,0));solid(rows,(x,y,.36),(2.54,1.43,.72))
        w=B();w.lathe([(0,0),(.62,0),(.62,.008),(0,.008)],family='Water',segments=48,colour=(.16,.34,.31,1));w.v=[(a*1.5,c*.83,z) for a,c,z in w.v];merge(b,w,(x,y,.4))
    beam(b,(x+1.30,y,0),(x+1.30,y,.97),.035);beam(b,(x+1.3,y,.97),(x+1.05,y,.97),.035)
    box(b,(x-.95,y-.96,.28),(.66,.46,.56),'Timber',wood,.04)
    for j in range(3):soft(b,(x-.95,y-.96,.59+j*.055),(.48,.28,.075),PALE)
def emit(name,b,notes,rays=()):
    if rays:
        tree=BVHTree.FromPolygons(b.v,b.f)
        for label,a,c in rays:
            d=Vector(c)-Vector(a);hit=tree.ray_cast(Vector(a),d.normalized(),d.length)[0]
            assert hit is None,(name,label,tuple(hit) if hit else None)
    bounds=[[min(v[i] for v in b.v)*100 for i in range(3)],[max(v[i] for v in b.v)*100 for i in range(3)]]
    remap={g['FAMILIES'].index(a):g['FAMILIES'].index(c) for a,c in FINISH.items()};b.m=[remap.get(i,i) for i in b.m]
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(f)) for f in b.f];export(name,b,notes)
    item=g['CATALOG'][-1];item['ue_bounds_cm']=bounds;item['source_rays_clear']=[r[0] for r in rays];records.append(item)

for k,(name,jp,wood,wall,accent) in enumerate(THEMES):
    s=B();f=B();rows=[]
    # The front door and rear garden door share a continuous 2 m central aisle.
    for x in [-7.5,7.5]:body(s,rows,(x,0,1.95),(.16,12.2,3.9),'Paint',wall)
    for x in [-4.25,4.25]:body(s,rows,(x,-6,1.95),(6.5,.16,3.9),'Paint',wall)
    body(s,rows,(0,-6,3.51),(2,.16,.78),'Paint',wall)
    for x in [-4.32,4.32]:
        body(s,rows,(x,6,.24),(6.36,.14,.48),'Paint',wall)
        body(s,rows,(x,6,3.70),(6.36,.14,.40),'Paint',wall)
        # Glazing is a separate translucent mesh; these bodies are its physical pane.
        solid(rows,(x,6,1.985),(6.36,.055,3.03))
    for x in [-7.35,-5.3,-3.25,-1.15,1.15,3.25,5.3,7.35]:
        box(s,(x,5.965,1.98),(.065,.10,3.1),'Bronze',INK,.012)
    body(s,rows,(0,0,3.96),(15.15,12.15,.12),'Paint',wall)
    # Floor joints are geometry, with no coplanar overlaid slabs.
    for iy in range(40):
        for ix in range(10):
            tint=tuple(c*(.91+.018*((ix*3+iy*7)%6)) if j<3 else 1 for j,c in enumerate(wood))
            box(s,(-6.75+ix*1.5,-5.85+iy*.3,-.018),(1.496,.296,.036),'Timber',tint,.002)
    for x in [-7.37,7.37]:box(s,(x,0,.075),(.08,11.85,.15),'Timber',wood,.01)
    for x in [-3.4,3.4]:box(s,(x,-5.87,.075),(6.45,.08,.15),'Timber',wood,.01)
    # Recessed panels and slim rails give light a physical edge to graze.
    for side in [-1,1]:
        xx=side*7.395
        for yy in [-4.1,-.65,2.8]:
            for zz in [.35,3.30]:box(s,(xx,yy,zz),(.025,2.85,.027),'Timber',wood,.005)
            for dy in [-1.425,1.425]:box(s,(xx,yy+dy,1.825),(.025,.027,2.95),'Timber',wood,.005)
    # Folds, cove lighting and the entrance plaque distinguish the finished shell.
    for x in [-7.12,-1.47,1.47,7.12]:
        for j in range(7):box(s,(x+(j-3)*.043,5.68+math.sin(j*1.5)*.035,1.98),(.05,.07,3.22),'Cloth',wall,.01)
    for x in [-4.1,4.1]:
        box(s,(x,0,3.84),(5.85,10.65,.06),'Timber',wood,.01)
        for yy in [-5.33,5.33]:box(s,(x,yy,3.825),(5.85,.025,.014),'Glow',(1,.72,.40,1),.004)
    for x in [-1.01,1.01]:box(s,(x,-6.12,1.52),(.08,.10,3.04),'Timber',wood,.018)
    box(s,(1.78,-6.12,1.62),(1.18,.055,.57),'Paint',INK,.06)
    letters(s,f'{101+k}  {name}',(1.78,-6.156,1.58),.105)
    for x in [-2.45,2.45]:
        box(s,(x,-6.16,2.06),(.21,.12,.70),'Timber',wood,.05);box(s,(x,-6.23,2.06),(.12,.025,.51),'Glow',(1,.69,.35,1),.02)
    # Balcony: deep enough to walk around a pair of loungers, without view-height walls.
    for iy in range(13):box(s,(0,6.2+iy*.3,-.028),(15.2,.292,.045),'Timber',wood,.002)
    for x in [-7.45,7.45]:
        box(s,(x,8,.48),(.18,4,.96),'Paint',wall,.03);solid(rows,(x,8,.55),(.18,4,1.1))
    for x in [-6,-2,2,6]:plant(s,x,9.65,.75,k*20+int(x)+8)
    box(s,(0,9.98,.45),(15.2,.25,.90),'Paint',wall,.03);solid(rows,(0,10,.6),(15.2,.25,1.2))
    for x in [-5.6,5.6]:
        body(f,rows,(x,8,.31),(1.04,2.2,.36),'Timber',wood,.05);soft(f,(x,8,.56),(.98,2.14,.22),wall)
    # A composed bedroom with a broad headboard, layered bedding and bedside objects.
    bedx=4.5;bedy=2.15
    body(f,rows,(bedx,bedy,.28),(2.54,2.65,.56),'Timber',wood,.07)
    soft(f,(bedx,bedy,.63),(2.38,2.48,.28),PALE)
    box(f,(bedx,3.50,.88),(3.90,.20,1.76),'Timber',wood,.06);solid(rows,(bedx,3.50,.88),(3.90,.20,1.76))
    for j in range(17):box(f,(bedx-1.76+j*.22,3.37,.95),(.17,.035,1.25),'Cloth',accent,.018)
    for x in [bedx-.61,bedx+.61]:soft(f,(x,2.95,.86),(1.05,.50,.23),PALE)
    g['craft'].drape(f,(bedx,1.77,.795),2.66,1.82,accent,drop=.23)
    for x in [2.45,6.55]:
        body(f,rows,(x,3.18,.32),(1.0,.75,.64),'Timber',wood,.05);lamp(f,x,3.2,.65,wood)
        g['craft'].cabinet(f,x,3.18,.32,1.,.75,.64,wood)
    body(f,rows,(4.5,.15,.29),(2.65,.62,.58),'Cloth',accent,.08)
    # Living cluster leaves the main axis open even with sofa arms and a wide coffee table.
    living_vertex=len(f.v);living_body=len(rows)
    body(f,rows,(-4.30,.1,.25),(3.8,1.02,.50),'Timber',wood,.08)
    for x in [-5.52,-4.3,-3.08]:soft(f,(x,.12,.54),(1.16,.95,.22),wall)
    solid(rows,(-4.3,-.39,.79),(3.84,.25,.9))
    for x in [-5.57,-4.3,-3.03]:soft(f,(x,-.39,.79),(1.27,.25,.9),accent)
    for x in [-6.24,-2.36]:body(f,rows,(x,.04,.69),(.22,1.16,.58),'Cloth',accent,.06)
    for x in [-5.45,-3.10]:soft(f,(x,-.13,.9),(.67,.23,.58),accent)
    box(f,(-4.3,1.9,.02),(5.3,4.25,.034),'Cloth',accent,.015)
    box(f,(-4.3,1.9,.046),(4.92,3.87,.018),'Cloth',wall,.009)
    table(f,rows,-4.3,1.90,2.1,1.0,.48,PALE)
    for x in [-6.45,-2.15]:
        body(f,rows,(x,3.48,.38),(.95,.92,.76),'Cloth',accent,.12)
        soft(f,(x,3.70,.93),(.96,.28,.58),wall)
    # Bath and kitchen are distinct from the sleeping zone, with a generous door into each.
    if k in (1,4,6):
        # Art lofts turn toward the interior; the Paris-inspired salon faces its guests.
        angle=math.pi if k==4 else math.pi/2;q=Matrix.Rotation(angle,3,'Z');pivot=Vector((-4.3,1.9,0))
        f.v[living_vertex:]=[tuple(q@(Vector(v)-pivot)+pivot) for v in f.v[living_vertex:]]
        for row in rows[living_body:]:
            row['p']=[round(n*100,4) for n in q@(Vector(row['p'])/100-pivot)+pivot]
            if k!=4:row['e'][0],row['e'][1]=row['e'][1],row['e'][0]
    body(s,rows,(3.35,-2.00,1.35),(2.85,.13,2.70),'Paint',wall,.025)
    body(s,rows,(6.95,-2.00,1.35),(.95,.13,2.70),'Paint',wall,.025)
    tub(f,rows,4.55,-4.05,wood,wall,k==0)
    body(f,rows,(6.85,-4.10,.47),(.75,2.55,.94),'Timber',wood,.045)
    box(f,(6.85,-4.10,.98),(.80,2.65,.08),'Ceramic',PALE,.035)
    body(f,rows,(-4.45,-5.45,.48),(4.70,.8,.96),'Timber',wood,.04)
    box(f,(-4.45,-5.43,1.02),(4.82,.94,.12),'Ceramic',PALE,.045)
    for x in [-6.22,-5.31,-4.40,-3.49,-2.58]:box(f,(x,-5.867,.47),(.8,.025,.78),'Paint',wall,.015)
    for x in [-5.85,-3.05]:cup(f,x,-5.45,1.12)
    box(f,(-4.35,-5.46,1.37),(.64,.42,.59),'Bronze',INK,.065)
    # A tea/dining table, books, personal objects, a plant and a reading light.
    table(f,rows,-4.3,-2.85,2.1,.95,.77,wood)
    for x in [-5.80,-2.80]:
        solid(rows,(x,-2.85,.45),(.68,.76,.90))
        dining=B();g['craft'].chair(dining,0,0,0,accent,wood,arm=True)
        dining.v=[(a,b*.875,z*.94) for a,b,z in dining.v]
        merge(f,dining,(x,-2.85,0),math.pi/2 if x<-4 else -math.pi/2)
    plant(f,-6.92,4.9,1.45,k+3);solid(rows,(-6.92,4.9,.30),(.75,.75,.6))
    a=B();art(a,0,0,0,3.9,1.28,accent);merge(f,a,(-4.45,-5.70,2.35),math.pi)
    # Authored details are structural and compositional, not just palette swaps.
    if k==0:
        for x in [1.60,7.12]:
            box(s,(x,2.0,1.75),(.06,3.2,3.45),'Paper',PALE,.005)
            solid(rows,(x,2,1.75),(.06,3.2,3.45))
            for z in [.25,.70,1.15,1.6,2.05,2.5,2.95,3.4]:box(s,(x-.04,2,z),(.045,3.2,.025),'Timber',wood,.004)
            for y in [.45,1.25,2.05,2.85,3.55]:box(s,(x-.04,y,1.75),(.045,.024,3.45),'Timber',wood,.004)
        for xx in range(5):
            for yy in range(3):box(f,(-6.0+xx*.85,4.22+yy*.44,.035),(.83,.42,.045),'Cloth',(.43,.43,.22,1),.004)
        for x in [-5.0,-3.65]:soft(f,(x,4.7,.13),(.68,.66,.20),accent)
    elif k==1:
        for x in [-6.6,-4.3,-2.0]:
            box(s,(x,-5.79,2.6),(1.8,.035,2.20),'Timber',wood,.01)
        body(f,rows,(-1.82,4.50,.63),(.56,.56,1.26),'Ceramic',PALE,.04)
        for j in range(3):f.tube([(-1.98,4.5,1.25+j*.15),(-1.62,4.5,1.65+j*.15),(-1.90,4.5,2.08+j*.15)],[.04]*3,'Bronze',12,GOLD)
    elif k==2:
        for x in [-6.8,6.8]:
            body(f,rows,(x,7.80,.3),(.6,2.3,.6),'Ceramic',wood,.045)
            for y in [7.2,8.4]:plant(f,x,y,1.55,k*30+int(y),.30)
        body(f,rows,(-4.30,9.28,.26),(3.6,.8,.52),'Ceramic',PALE,.04)
        box(f,(-4.30,9.28,.53),(3.34,.58,.018),'Water',(.06,.3,.25,1),.01)
        for j in range(11):box(s,(-7.10+j*1.42,0,3.66),(.07,11.4,.15),'Timber',wood,.018)
    elif k==3:
        for x in [-7.08,7.08]:
            for y in [-4.75,0,4.75]:
                for j in range(9):beam(s,(x,y-.6+j*.15,2.35),(x,y+.55-j*.04,3.43),.018,'Timber',wood)
        for x in [-5.8,-2.8]:
            for y in [0.1,3.7]:beam(s,(x,y,0),(x,y,3.3),.045,'Timber',wood)
        for x in [-6.0,-4.3,-2.6]:
            s.lathe([(.28,0),(.37,.20),(.20,.50),(0,.62)],(x,1.9,3.12),'Bronze',24,GOLD)
            box(s,(x,1.9,3.13),(.3,.3,.018),'Glow',(1,.39,.11,1),.01)
    elif k==4:
        for x in [-7.37,7.37]:
            for y in [-3.4,0,3.4]:
                for z in [.35,3.2]:box(s,(x,y,z),(.045,2.9,.045),'Bronze',GOLD,.007)
                for dy in [-1.45,1.45]:box(s,(x,y+dy,1.775),(.045,.045,2.85),'Bronze',GOLD,.007)
        for rr,z in [(.72,3.1),(.49,2.77)]:
            pts=[(-4.3+rr*math.cos(j*math.tau/48),1.7+rr*math.sin(j*math.tau/48),z) for j in range(49)]
            f.tube(pts,[.024]*len(pts),'Bronze',10,GOLD)
            for j in range(10):beam(f,pts[j*4],(pts[j*4][0],pts[j*4][1],z-.23),.028,'Crystal',PALE)
    elif k==5:
        books(f,rows,-6.25,-4.95,1.55,wood,accent);books(f,rows,-2.15,4.72,1.05,wood,accent)
        body(f,rows,(-4.3,4.80,.70),(3.8,.42,1.4),'Ceramic',(.27,.29,.28,1),.035)
        box(f,(-4.3,4.56,.67),(2.9,.025,.58),'Paint',INK,.02)
        for j in range(14):box(f,(-5.62+j*.20,4.536,.41),(.06,.017,.12+((j*3)%5)*.06),'Glow',(1,.21,.035,1),.016)
        for x in [-6.9,6.9]:box(s,(x,0,3.58),(.28,11.6,.38),'Timber',wood,.015)
    elif k==6:
        for x in [-7.25,-3.5,3.5,7.25]:
            beam(s,(x,5.80,.48),(x+.38,5.30,2.1),.045,'Bronze',INK)
            beam(s,(x+.38,5.30,2.1),(x,5.8,3.5),.045,'Bronze',INK)
        for j in range(9):
            box(f,(-6.7+j*.6,2.1,.062),(.18,3.5,.015),'Cloth',[(.44,.075,.19,1),(.04,.32,.37,1),(.63,.38,.07,1)][j%3],.002,yaw=.24)
        art(f,4.5,3.35,2.72,3.35,1.38,(.48,.09,.22,1))
    elif k==7:
        for y in [-5,-2.5,0,2.5,5]:box(s,(0,y,3.62),(14.7,.21,.45),'Timber',wood,.025)
        for j in range(5):
            for i in range(10):box(f,(-5.7+i*.31,4.8,.16+j*.24),(.30,.37,.23),'Limestone',(.34+.03*((i+j)%3),.34,.31,1),.025)
        box(f,(-4.3,4.59,.7),(1.55,.015,.63),'Paint',INK,.01)
        for j in range(8):box(f,(-4.95+j*.17,4.572,.45),(.04,.025,.18+((j*3)%4)*.07),'Glow',(1,.24,.035,1),.018)
        soft(f,(-4.9,.30,.68),(1.3,.96,.10),(.51,.43,.31,1))
    clear=[('entrance_axis',(0,-7.1,1.64),(0,7.2,1.64))]
    emit(f'Hotel83Shell{k}',s,jp+' shell and balcony',clear);emit(f'Hotel83Furniture{k}',f,jp+' original furniture and details',clear)
    plans.append({'kind':11+k,'name':jp,'english':name,'bodies':rows,
                  'lights':[[-425,135,329],[430,125,329],[-430,-360,329],[440,-425,285]],
                  'route':[[0,-740,0],[0,-300,0],[0,100,0],[0,490,0],[0,750,0],[0,490,0],[0,-300,0],[0,-740,0]],
                  'eye':[-110,-425,165],'aim':[-350,180,145]})

glass=B()
for x in [-4.32,4.32]:box(glass,(x,6,1.985),(6.20,.02,2.99),'Hotel83Glass',(.8,.93,.95,1),.0)
emit('Hotel83Glass',glass,'Clear view glazing; central garden door remains open')
hall=B()
for x in [-20.9,20.9]:
    for y in [-19.5,19.5]:plant(hall,x,y,2.3,int(x+y)+100)
for y in [-19.8,19.8]:
    box(hall,(0,y,.3),(6.0,.80,.6),'Timber',(.30,.19,.095,1),.07)
    soft(hall,(0,y,.67),(5.9,.77,.22),(.55,.63,.56,1))
for x in [-2.0,2.0]:box(hall,(x,0,.012),(.028,36,.016),'Bronze',GOLD,.002)
emit('Hotel83Hall',hall,'Shared open-air sky gallery with benches and planted corners')
OUT.joinpath('hotel83-catalog.json').write_text(json.dumps({'assets':records,'rooms':plans},ensure_ascii=False,indent=2),encoding='utf-8')

# Generate the coordinate-only contract consumed by the world generator.
header=['#pragma once','#include "CoreMinimal.h"','namespace EWHotel83Data {','struct Body { FVector P,E; bool Floor; };',
        'inline TArray<Body> Bodies(int32 Kind) { switch(Kind) {']
def vec(v):return 'FVector('+','.join(f'{x:.4f}' for x in v)+')'
for p in plans:
    header.append('case '+str(p['kind'])+': return {'+','.join('{'+vec(b['p'])+','+vec(b['e'])+','+str(b['floor']).lower()+'}' for b in p['bodies'])+'};')
header.extend(['default:return {};}}','inline TArray<FVector> Lights(int32 Kind) { switch(Kind) {'])
for p in plans:header.append('case '+str(p['kind'])+': return {'+','.join(vec(v) for v in p['lights'])+'};')
header.extend(['default:return {};}}','}'])
(OUT/'Generated/EWHotel83Data.h').write_text('\n'.join(header)+'\n',encoding='utf-8')
OUT.joinpath('Blender').mkdir(exist_ok=True);bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Blender/Hotel83.blend'))
print('HOTEL83_COMPLETE',len(records),'assets',sum(x['triangles'] for x in records),'triangles',flush=True)
