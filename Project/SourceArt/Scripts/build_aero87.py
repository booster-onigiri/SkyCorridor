"""Original walkable air yachts. Coordinates: metres, bow +X, boarding -Y.
Public release redesign: terraced observatory barge, segmented teal chines,
faceted forward pavilion and twin horizontal stern ring thrusters.
All geometry is procedural; no external image is loaded or embedded.
Colliders are emitted from the same dimensions as the visible walking surfaces.
"""
from pathlib import Path
import bpy, runpy, math, json, random
from mathutils import Vector, Matrix
g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
B=g['Builder'];OUT=g['OUT'];records=[];bodies=[]
IVORY=(.64,.70,.73,1);DARK=(.018,.040,.058,1);GOLD=(.49,.37,.20,1)
TEAK=(.39,.235,.115,1);NAVY=(.027,.09,.13,1);CREAM=(.68,.68,.57,1)
for family,c in [('Aero87Hull',IVORY),('Aero87Metal',GOLD),('Aero87Teak',TEAK),
                 ('Aero87Fabric',CREAM),('Aero87Glass',(.13,.29,.34,1)),
                 ('Aero87Pool',(.06,.36,.46,1)),('Aero87Glow',(1,.69,.35,1))]:
    g['FAMILIES'].append(family);g['COLOURS'][family]=c
    m=bpy.data.materials.new('M_'+family);m.diffuse_color=c;g['MATERIALS'].append(m)

def box(b,p,d,f='Aero87Hull',c=IVORY,bevel=.04,yaw=0):b.box(p,d,f,bevel,yaw,colour=c)
def beam(b,a,c,r=.04,f='Aero87Metal',col=GOLD):b.tube([a,c],[r,r],f,12,col)
def solid(p,e,floor=False,yaw=0,category='hull'):
    bodies.append(dict(p=[v*100 for v in p],e=[v*100 for v in e],yaw=yaw*180/math.pi,floor=floor,kind=category))
def merge(b,a,p=(0,0,0),yaw=0):
    n=len(b.v);q=Matrix.Rotation(yaw,3,'Z');v=Vector(p)
    b.v.extend(tuple(q@Vector(x)+v) for x in a.v);b.f.extend(tuple(n+i for i in face) for face in a.f)
    b.m.extend(a.m);b.col.extend(a.col);b.smooth.extend(a.smooth)
def emit(name,b,note):
    bounds=[[min(v[i] for v in b.v)*100 for i in range(3)],[max(v[i] for v in b.v)*100 for i in range(3)]]
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(f)) for f in b.f]
    g['export'](name,b,note);row=g['CATALOG'][-1];row['ue_bounds_cm']=bounds
    row['used_materials']=[g['FAMILIES'][i] for i in sorted(set(b.m))];records.append(row)
def label(b,text,p,size=.30,col=GOLD):
    curve=bpy.data.curves.new('lettering','FONT');curve.body=text;curve.align_x='CENTER';curve.size=size;curve.extrude=.003
    obj=bpy.data.objects.new('lettering',curve);bpy.context.collection.objects.link(obj)
    obj.rotation_euler=(math.pi/2,0,0);obj.location=p;bpy.context.view_layer.update()
    mesh=bpy.data.meshes.new_from_object(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    # Unreal's +Y-facing viewer has screen-right along -X. Reflect lettering
    # around its own origin so the front, not its mirrored back, reads correctly.
    vs=[tuple(obj.matrix_world@v.co) for v in mesh.vertices]
    b.mesh([(2*p[0]-x,y,z) for x,y,z in vs],[tuple(reversed(f.vertices)) for f in mesh.polygons],'Aero87Metal',col,smooth=False)
    bpy.data.objects.remove(obj,do_unlink=True);bpy.data.meshes.remove(mesh)
def half(x):return 22*math.sqrt(max(.0001,1-(x/58)**2))

hull=B();glass=B();deck=B();interior=B();furn=B();garden=B();water=B();fins=B()
# The open top is a true opening: no ellipsoid covers the pool or guests.
xs=sorted(set([-57.9+i*115.8/128 for i in range(129)]+[-25.8,-22.2]))
levels=[(-14.0,.04),(-13.2,.38),(-10.1,.72),(-5.0,.94),(-.05,1),(.7,1),(3.65,.98),(5.6,.94),(6.5,.91)]
def zshape(z,x):return z*math.sqrt(max(.001,1-(x/58)**2)) if z<0 else z
for j in range(len(xs)-1):
    a,c=xs[j:j+2]
    for side in [-1,1]:
        opening=side==-1 and a>=-25.80001 and c<=-22.19999
        for k in range(len(levels)-1):
            if opening and k in [4,5]:continue
            (z0,w0),(z1,w1)=levels[k:k+2]
            pts=[(a,side*half(a)*w0,zshape(z0,a)),(c,side*half(c)*w0,zshape(z0,c)),(c,side*half(c)*w1,zshape(z1,c)),(a,side*half(a)*w1,zshape(z1,a))]
            isglass=k==5;target=glass if isglass else hull
            colour=DARK if k==4 else IVORY
            target.mesh(pts,[(0,1,2,3) if side<0 else (3,2,1,0)],'Aero87Glass' if isglass else 'Aero87Hull',None if isglass else colour)
    # Closed, gently curving keel.
    hull.mesh([(a,-half(a)*.04,zshape(-14,a)),(a,half(a)*.04,zshape(-14,a)),(c,half(c)*.04,zshape(-14,c)),(c,-half(c)*.04,zshape(-14,c))],[(0,1,2,3)],'Aero87Hull',IVORY)
# Pinched bow and tail caps join every shell band.
for x in [-57.9,57.9]:
    verts=[(x,side*half(x)*w,zshape(z,x)) for side in [-1,1] for z,w in (levels if side<0 else list(reversed(levels)))]
    hull.mesh(verts,[tuple(range(len(verts))) if x>0 else tuple(reversed(range(len(verts))))],'Aero87Hull',IVORY)

for side in [-1,1]:
    for z,w,r,f,col in [(-.05,1.002,.11,'Aero87Metal',GOLD),(.73,1.002,.07,'Aero87Hull',DARK),
                          (3.65,.983,.095,'Aero87Hull',DARK),(5.65,.946,.10,'Aero87Metal',GOLD),
                          (6.47,.919,.13,'Aero87Hull',IVORY)]:
        groups=[xs] if z>3.7 else [[x for x in xs if x<=-25.8],[x for x in xs if x>=-22.2]] if side<0 else [xs]
        for group in groups:hull.tube([(x,side*half(x)*w,z) for x in group],[r]*len(group),f,12,col)
    for x in range(-54,55,4):
        if side<0 and -26<=x<=-22:continue
        y=side*half(x)
        beam(hull,(x,y,.72),(x+.35,side*half(x+.35)*.981,3.68),.075,'Aero87Hull',DARK)
    # Segmented side-wall collision follows the physical transparent glazing.
    for a,c in zip(xs[::4],xs[4::4]):
        if side<0 and c>-25.8 and a<-22.2:continue
        p=Vector((a,side*half(a)*.983,2.0));q=Vector((c,side*half(c)*.983,2.0));diff=q-p
        solid(tuple((p+q)*.5),(diff.length*.5,.13,2.0),False,math.atan2(diff.y,diff.x),'glazing')
# Entry portal has a 3.6 m opening and a level threshold, with no hidden wall.
for x in [-25.85,-22.15]:
    box(hull,(x,-half(x)*.989,1.7),(.16,.28,3.4),c=DARK)
    solid((x,-half(x)*.983,1.7),(.08,.20,1.7),category='entry jamb')
box(hull,(-24,-20,3.51),(4.0,.4,.20),c=DARK)
label(hull,'A U R E L I A   /   S K Y   C L U B',(-2,-22.03,4.48),.68)
label(hull,'B O A R D I N G',(-24,-20.22,3.93),.35)

# Elliptical decks are made from strips; the staircase and pool are real holes.
def floor(z,rx,ry,holes,target,category):
    xx=sorted(set([-rx+.25+i*(2*rx-.5)/100 for i in range(101)]+[v for h in holes for v in h[:2]]))
    xx=[x for x in xx if -rx<x<rx]
    cuts=sorted(set([v for h in holes for v in h[2:]]))
    for a,c in zip(xx,xx[1:]):
        if c-a<.002:continue
        wa=ry*math.sqrt(max(0,1-(a/rx)**2));wc=ry*math.sqrt(max(0,1-(c/rx)**2));w=min(wa,wc)
        bands=sorted(set([-w]+[y for y in cuts if -w<y<w]+[w]))
        for low,high in zip(bands,bands[1:]):
            mid=((a+c)*.5,(low+high)*.5)
            if any(h[0]<mid[0]<h[1] and h[2]<mid[1]<h[3] for h in holes):continue
            ya0=-wa if low==-w else low;ya1=wa if high==w else high
            yc0=-wc if low==-w else low;yc1=wc if high==w else high
            verts=[(a,ya0,z),(c,yc0,z),(c,yc1,z),(a,ya1,z)]
            target.mesh(verts,[(0,1,2,3)],'Aero87Teak',TEAK,smooth=False)
            target.mesh([(x,y,z-.22) for x,y,_ in verts],[(3,2,1,0)],'Aero87Hull',CREAM,smooth=False)
        # Collision needs only the occupied spans, not every material strip.
        spans=[(-w,w)]
        for h in holes:
            if h[0]<(a+c)/2<h[1]:
                spans=[s for lo,hi in spans for s in [(lo,min(hi,h[2])),(max(lo,h[3]),hi)] if s[1]-s[0]>.002]
        for lo,hi in spans:solid(((a+c)/2,(lo+hi)/2,z-.11),((c-a)/2+.003,(hi-lo)/2,.11),True,category=category)
    # Fine parallel planks, with restrained caulking, are geometric along the deck.
    for y in [i*.26 for i in range(-int(ry/.26),int(ry/.26)+1)]:
        xlim=rx*math.sqrt(max(0,1-(y/ry)**2))-.3
        spans=[(-xlim,xlim)]
        for h in holes:
            if h[2]-.015<y<h[3]+.015:
                spans=[s for a,c in spans for s in [(a,min(c,h[0])),(max(a,h[1]),c)] if s[1]-s[0]>.03]
        for a,c in spans:
            box(target,((a+c)/2,y,z+.003),(c-a,.012,.004),c=DARK,bevel=0)

floor(0,56.8,21.65,[],interior,'salon floor')
holes=[(-32,-11,-15.4,-11.6),(-32,-11,11.6,15.4),(-18,26,-7.0,7.0)]
floor(6,53,19.55,holes,deck,'sun deck')
# Plinths close the curved rim without a broad opaque wall obscuring the view.
for i in range(180):
    a=i*math.tau/180;c=(i+1)*math.tau/180
    p=Vector((53*math.cos(a),19.55*math.sin(a),6));q=Vector((53*math.cos(c),19.55*math.sin(c),6));v=q-p
    beam(deck,tuple(p+Vector((0,0,1.12))),tuple(q+Vector((0,0,1.12))),.045)
    beam(deck,tuple(p),tuple(p+Vector((0,0,1.12))),.026)
    glass.mesh([tuple(p+Vector((0,0,.12))),tuple(q+Vector((0,0,.12))),tuple(q+Vector((0,0,1.05))),tuple(p+Vector((0,0,1.05)))],[(0,1,2,3)],'Aero87Glass')
    solid(tuple((p+q)*.5+Vector((0,0,.56))),(v.length*.5,.09,.56),False,math.atan2(v.y,v.x),'deck rail')
# Twin broad staircases: thirty 20 cm risers, brass nosings and continuous handrails.
for side in [-1,1]:
    y=side*13.5
    for n in range(30):
        x=-32+(n+.5)*.7;z=(n+1)*.2
        box(interior,(x,y,z-.1),(.7,3.7,.2),'Aero87Teak',TEAK,.01)
        box(interior,(x-.345,y,z+.006),(.022,3.7,.015),'Aero87Metal',GOLD,.003)
        solid((x,y,z-.1),(.351,1.85,.1),True,category='stair')
    for yy in [y-1.89,y+1.89]:
        beam(interior,(-32,yy,1.0),(-11,yy,7.0),.04)
        for n in range(11):
            x=-32+n*2.1;z=n*.6
            beam(interior,(x,yy,z),(x,yy,z+1),.025)
        # Colliding side panels stop sideways drops through the stairwell.
        for n in range(10):solid((-30.95+n*2.1,yy,1.05+n*.6),(1.05,.055,.85),category='stair rail')
    # At the top, the return side is protected, while both ends remain open.
    for yy in [y-1.91,y+1.91]:
        beam(deck,(-31.9,yy,7.1),(-11,yy,7.1),.038)
    label(interior,'SUN DECK  /  OBSERVATION',(-33.8,y-.2,2.5),.32)

# Pool basin is recessed into the top deck, with a walkable submerged floor and steps.
box(deck,(4,0,4.72),(44.6,14.6,.42),'Aero87Hull',(.21,.40,.43,1),.1)
solid((4,0,4.72),(22.3,7.3,.21),True,category='pool bottom')
# Finished underside of the pool becomes a deliberate coffer in the lower salon.
box(interior,(4,0,4.48),(44.7,14.7,.08),'Aero87Hull',CREAM,.035)
for y in [-7.0,7.0]:
    box(deck,(4,y,5.32),(44,.25,1.4),'Aero87Hull',(.15,.38,.45,1),.03)
    solid((4,y,5.32),(22,.125,.70),category='pool wall')
    box(interior,(4,y,4.41),(44,.06,.07),'Aero87Glow',(1,.70,.36,1),.015)
box(deck,(-18,0,5.31),(.24,14,1.4),'Aero87Hull',(.15,.38,.45,1),.03)
solid((-18,0,5.31),(.12,7,.7),category='pool wall')
for y in [-4.25,4.25]:
    box(deck,(26,y,5.31),(.24,5.5,1.4),'Aero87Hull',(.15,.38,.45,1),.03)
    solid((26,y,5.31),(.12,2.75,.7),category='pool wall')
for n in range(6):
    x=26-(n+.5)*.48;z=6-(n+1)*.18
    box(deck,(x,0,z-.10),(.48,3.0,.20),'Aero87Hull',(.5,.67,.67,1),.025)
    solid((x,0,z-.10),(.245,1.5,.10),True,category='pool step')
water.mesh([(-17.9,-6.88,5.78),(25.9,-6.88,5.78),(25.9,6.88,5.78),(-17.9,6.88,5.78)],[(0,1,2,3)],'Aero87Pool')
for y in [-7.33,7.33]:box(deck,(4,y,6.015),(44.6,.09,.035),'Aero87Glow',(.46,.80,1,1),.015)

# PUBLIC v0.1.0: stepped architectural ribs replace the two sweeping arches.
# Attachments sit outside the walkable rail or above the 2.7 m head clearance.
for side in [-1,1]:
    for x in [-37,-20,0,20,37]:
        yy=side*half(x)*.985
        h=7.6 if abs(x)>30 else 9.0
        hull.tube([(x,yy,-7.0),(x,yy,-.3),(x,yy,5.4),(x,yy*.95,h)], [.22,.20,.16,.09], 'Aero87Metal',8,GOLD)
        box(hull,(x,yy*.95,h),(.85,.7,.28),'Aero87Hull',IVORY,.05)
    # Recessed teal cladding panels create a horizontal, architectural waist.
    for j in range(23):
        x0=-46+j*4;x1=x0+3.75
        pts=[(x0,side*half(x0)*.943,-4.9),(x1,side*half(x1)*.943,-4.9),
             (x1,side*half(x1)*1.003,-.12),(x0,side*half(x0)*1.003,-.12)]
        hull.mesh(pts,[(3,2,1,0) if side>0 else (0,1,2,3)],'Aero87Hull',(.055,.22,.24,1),smooth=False)
        beam(hull,pts[0],pts[3],.05,'Aero87Metal',GOLD)
# Low faceted forward observatory replaces the dark swept nose cap.
for side in [-1,1]:
    y=side*7.1
    for x in [38.0,47.0]:
        beam(hull,(x,y,6.4),(x,y,9.4),.12,'Aero87Hull',IVORY)
    hull.mesh([(36,y-1.3*side,9.4),(49,y*.7,9.4),(49,0,10.3),(36,0,10.3)],
              [(0,1,2,3) if side>0 else (3,2,1,0)],'Aero87Hull',(.07,.25,.26,1),smooth=False)
    beam(hull,(36,y-1.3*side,9.4),(49,y*.7,9.4),.09,'Aero87Metal',GOLD)

def lounge(b,p,yaw=0,cloth=CREAM):
    a=B();box(a,(0,0,.25),(2.35,.96,.44),'Aero87Teak',TEAK,.09)
    box(a,(0,0,.55),(2.23,.92,.22),'Aero87Fabric',cloth,.09)
    box(a,(0,.39,.91),(2.37,.25,.75),'Aero87Fabric',cloth,.08)
    for x in [-1.09,1.09]:box(a,(x,0,.79),(.20,.99,.21),'Aero87Teak',TEAK,.06)
    for x in [-.63,0,.63]:box(a,(x,.22,.87),(.51,.23,.41),'Aero87Fabric',NAVY,.065)
    merge(b,a,p,yaw)
def table(b,p,large=False):
    x,y,z=p;r=.85 if large else .46
    b.lathe([(0,0),(.29,0),(.20,.04),(.13,.50),(r,.54),(r,.60),(0,.60)],p,'Aero87Metal',32,GOLD)
    b.lathe([(0,.60),(r*.94,.60),(r*.94,.62),(0,.62)],p,'Aero87Hull',32,IVORY)
    for xx in [-.16,.16]:
        b.lathe([(0,0),(.055,0),(.06,.11),(.052,.12),(.045,.10)],(x+xx,y,z+.62),'Ceramic',16,CREAM)
def plant(b,p,size=1,seed=0):
    x,y,z=p;b.lathe([(0,0),(.34,0),(.47,.72),(.45,.80),(.36,.80)],p,'Aero87Hull',32,IVORY)
    rng=random.Random(seed)
    for j in range(9):
        a=j*2.399;end=(x+math.cos(a)*.35*size,y+math.sin(a)*.35*size,z+.78+size*(1.1+rng.random()*.7))
        beam(b,(x,y,z+.70),end,.018,'Bark',(.15,.2,.08,1))
        for n in range(5):
            t=.30+n*.13;v=Vector((x,y,z+.7)).lerp(Vector(end),t)
            b.leaf(tuple(v),size*.75,size*.32,a+n*.7,.38,'Foliage',(.07,.22+rng.random()*.12,.08,1))

# Panoramic salon: a clear 4 m central promenade remains from entry to bow.
for x in [-14,-3,9,23,35,45]:
    for side in [-1,1]:
        y=side*min(10.0,half(x)-4.0)
        lounge(furn,(x,y,0),0 if side>0 else math.pi)
        table(furn,(x,y-side*2.15,0),True)
        solid((x,y,.6),(1.2,.55,.6),category='salon sofa')
        solid((x,y-side*2.15,.3),(.84,.84,.3),category='salon table')
        plant(garden,(x+2.2,y,0),1.0,int(x+side*100))
# Private library and a curved lounge bar beneath the stern.
for x in [-47,-43,-39]:
    box(furn,(x,5,1.45),(3.2,.40,2.9),'Aero87Teak',TEAK,.035)
    for zz in [.4,1.05,1.7,2.35]:
        box(furn,(x,4.7,zz),(3.10,.52,.055),'Aero87Metal',GOLD,.01)
        for n in range(16):box(furn,(x-1.4+n*.18,4.7,zz+.20),(.10,.24,.37),'Aero87Fabric',[(.22,.30,.31,1),(.42,.24,.14,1),CREAM][n%3],.003)
    solid((x,4.8,1.45),(1.6,.45,1.45),category='library')
box(furn,(-40,-6,.58),(9,1.7,1.16),'Aero87Teak',TEAK,.15)
box(furn,(-40,-6,1.18),(9.3,1.85,.10),'Aero87Hull',CREAM,.06)
solid((-40,-6,.62),(4.65,.925,.62),category='bar')
for x in [-43,-41,-39,-37]:
    box(furn,(x,-8,.46),(.58,.58,.92),'Aero87Teak',TEAK,.08)
    box(furn,(x,-8,.95),(.63,.63,.12),'Aero87Fabric',NAVY,.07)
    # Translucent bottles belong to the glass mesh. A single translucent slot
    # forces an entire Nanite furniture mesh onto its simplified fallback.
    for n in range(3):glass.lathe([(0,0),(.055,0),(.055,.24),(.035,.27),(.035,.35),(0,.35)],(x+n*.16,-6,1.24),'Aero87Glass',16)
label(interior,'THE PANORAMA SALON',(-4,-.02,5.40),.40)
label(interior,'LIBRARY  /  LOUNGE',(-41,4.45,3.08),.36)
for x in [-40,-20,0,20,39]:
    # Ceiling coffers and indirect light keep a tangible indoor atmosphere.
    cz=4.25 if -25<x<30 else 5.73
    box(interior,(x,0,cz),(7.2,16,.18),'Aero87Hull',CREAM,.06)
    for y in [-7.8,7.8]:box(interior,(x,y,cz-.11),(7,.07,.07),'Aero87Glow',(1,.74,.46,1),.01)
    for y in [-4,4]:
        beam(interior,(x,y,cz-.03),(x,y,cz-1.13),.025)
        interior.lathe([(0,0),(.55,0),(.66,.10),(.55,.18),(0,.18)],(x,y,cz-1.23),'Aero87Metal',32,GOLD)
        box(interior,(x,y,cz-1.23),(.85,.85,.025),'Aero87Glow',(1,.74,.46,1),.10)

# Eight pergolas line the pool, with dining chairs and sun loungers.
for x in [-4,5,14,29]:
    for side in [-1,1]:
        y=side*(11.4 if x<25 else 9.0)
        for xx in [-2.0,2.0]:
            for yy in [-1.8,1.8]:
                beam(furn,(x+xx,y+yy,6),(x+xx,y+yy,9.2),.065)
                solid((x+xx,y+yy,7.6),(.075,.075,1.6),category='pergola post')
        for n in range(15):box(furn,(x-2.15+n*.307,y,9.2),(.10,4,.12),'Aero87Teak',TEAK,.02)
        beam(furn,(x,y,9.2),(x,y,8.92),.018)
        box(furn,(x,y,8.94),(.45,.45,.09),'Aero87Metal',GOLD,.025)
        box(furn,(x,y,8.86),(.34,.34,.10),'Aero87Glow',(1,.67,.34,1),.035)
        table(furn,(x,y,6),True)
        for yy in [-1.45,1.45]:lounge(furn,(x,y+yy,6),0 if yy>0 else math.pi)
        for yy in [-1.45,1.45]:solid((x,y+yy,6.6),(1.2,.55,.6),category='deck seating')
for x in [-39,-34,38,43]:
    for side in [-1,1]:
        y=side*6.6
        box(furn,(x,y,6.30),(2.6,.85,.30),'Aero87Teak',TEAK,.08)
        box(furn,(x,y,6.52),(2.5,.80,.16),'Aero87Fabric',CREAM,.085)
        box(furn,(x-.9,y,6.73),(.58,.78,.23),'Aero87Fabric',NAVY,.085)
        solid((x,y,6.5),(1.3,.45,.5),category='sun lounger')
for x,y in [(-40,-11),(-40,11),(-8,-17),(-8,17),(23,-16),(23,16),(38,-10),(38,10)]:plant(garden,(x,y,6),1.7,int(x+y))

# PUBLIC v0.1.0: a squared stern bridge and twin halo drives replace fins.
for side in [-1,1]:
    yy=side*20.0
    beam(fins,(-42,side*9,-2.3),(-49.5,yy,-2.3),.44,'Aero87Metal',GOLD)
    beam(fins,(-49.5,yy,-2.3),(-56.0,side*13,-2.3),.35,'Aero87Hull',IVORY)
    for radius,thick,fam,col in [(4.6,.38,'Aero87Hull',IVORY),(4.05,.10,'Aero87Metal',GOLD),(3.7,.045,'Aero87Glow',(.25,.62,.64,1))]:
        pts=[(-49.5,yy+radius*math.cos(i*math.tau/80),-.7+radius*math.sin(i*math.tau/80)) for i in range(81)]
        fins.tube(pts,[thick]*81,fam,12,col)
    for i in range(12):
        angle=i*math.tau/12
        beam(fins,(-49.5,yy+.9*math.cos(angle),-.7+.9*math.sin(angle)),
             (-49.5,yy+3.7*math.cos(angle+.22),-.7+3.7*math.sin(angle+.22)),.075,'Aero87Metal',GOLD)
    for x in [-31,27]:
        y=side*(half(x)+2.2)
        beam(fins,(x,side*(half(x)-1),-4),(x,y,-4.6),.45,'Aero87Hull',IVORY)
        # Rotated hollow nacelle, real inner cavity and concentric turbine rims.
        a=B();a.lathe([(1.5,-1.8),(1.75,-1.55),(1.85,-1.0),(1.80,1.1),(1.55,1.5),(1.28,1.5),(1.23,-1.7),(1.5,-1.8)],family='Aero87Hull',segments=64,colour=IVORY)
        rot=Matrix.Rotation(math.pi/2,3,'Y');a.v=[tuple(rot@Vector(v)) for v in a.v];merge(fins,a,(x,y,-4.6))
        a=B();a.lathe([(1.26,-1.4),(1.32,-1.4),(1.32,-1.3),(1.26,-1.3)],family='Aero87Glow',segments=48,colour=(.30,.65,1,1));a.v=[tuple(rot@Vector(v)) for v in a.v];merge(fins,a,(x,y,-4.6))
        solid((x,y,-4.6),(1.8,1.9,1.9),category='engine')

# Only replace the exterior. Collision, furniture, interior, pool and terminal
# FBXs remain the exact copied originals, so dock/boarding data is untouched.
import hashlib
previous=json.loads((OUT/'aero87-catalog.json').read_text(encoding='utf8'))
for name,b,note in [('Aero87Hull',hull,'Public observatory barge: segmented teal waist, stepped ribs and faceted pavilion'),
                    ('Aero87Fins',fins,'Public observatory barge: twin stern halo drives and four retained nacelles')]:
    emit(name,b,note)
updates={row['name']:row for row in records}
previous['assets']=[updates.get(row['name'],row) for row in previous['assets']]
previous['public_design_revision']='observatory-barge-v1'
previous['source']='Scripts/build_aero87.py'
(OUT/'aero87-catalog.json').write_text(json.dumps(previous,indent=2),encoding='utf8')
print('PUBLIC_AERO_EXTERIOR_COMPLETE',[(r['name'],r['triangles']) for r in records],flush=True)
