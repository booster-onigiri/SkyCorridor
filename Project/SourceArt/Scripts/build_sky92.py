"""Original walkable architecture and handset, generated in metres.
The three user references guide silhouettes and architectural vocabulary only.
Collision comes from the same authored dimensions as floors, furniture and walls.
"""
from pathlib import Path
import bpy,runpy,math,json,random
from mathutils import Matrix,Vector
g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
(g['OUT']/'Generated').mkdir(exist_ok=True)
B=g['Builder']; OUT=g['OUT']; records=[]; collision={}; active=[]
families={
 'Sky92Stone':(.64,.61,.51,1),'Sky92Edge':(.76,.73,.63,1),
 'Sky92White':(.69,.73,.72,1),'Sky92Metal':(.07,.16,.18,1),
 'Sky92Window':(.08,.28,.34,1),'Sky92Glass':(.12,.32,.38,1),
 'Sky92Copper':(.38,.24,.12,1),'Sky92Roof':(.21,.37,.34,1),
 'Sky92Tile':(.42,.22,.14,1),'Sky92Wood':(.26,.17,.11,1),
 'Sky92Leaf':(.15,.28,.14,1),'Sky92Cloth':(.39,.51,.49,1),
 'Sky92Dark':(.06,.075,.09,1),'Sky92PhoneMetal':(.25,.29,.32,1),
 'Sky92Night':(.68,.51,.24,1)}
for family,c in families.items():
    g['FAMILIES'].append(family);g['COLOURS'][family]=c
    m=bpy.data.materials.new('M_'+family);m.diffuse_color=c;g['MATERIALS'].append(m)
STONE='Sky92Stone';EDGE='Sky92Edge';WHITE='Sky92White';METAL='Sky92Metal';WINDOW='Sky92Window'
GLASS='Sky92Glass';COPPER='Sky92Copper';ROOF='Sky92Roof';TILE='Sky92Tile';WOOD='Sky92Wood'

def box(b,p,d,f=STONE,bevel=.025,yaw=0):b.box(p,d,f,bevel,yaw)
def solid(b,p,d,f=STONE,bevel=.025,yaw=0,floor=False):
    box(b,p,d,f,bevel,yaw);active.append(dict(p=p,d=d,yaw=math.degrees(yaw),floor=floor))
def beam(b,a,c,r=.06,f=EDGE):b.tube([a,c],[r,r],f,8)
def merge(b,a,p=(0,0,0),yaw=0):
    n=len(b.v);q=Matrix.Rotation(yaw,3,'Z');v=Vector(p)
    b.v.extend(tuple(q@Vector(x)+v) for x in a.v);b.f.extend(tuple(n+i for i in face) for face in a.f)
    b.m.extend(a.m);b.col.extend(a.col);b.smooth.extend(a.smooth)
def emit(name,b,note,colliders=None):
    bounds=[[min(v[i] for v in b.v)*100 for i in range(3)],[max(v[i] for v in b.v)*100 for i in range(3)]]
    used=[g['FAMILIES'][i] for i in sorted(set(b.m))]
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(face)) for face in b.f]
    g['export'](name,b,note);a=dict(g['CATALOG'][-1]);a['ue_bounds_cm']=bounds;a['used_materials']=used;records.append(a)
    if colliders is not None:collision[name]=list(colliders)
def column(b,x,y,z,h,r=.22,f=EDGE,collide=False):
    b.lathe([(0,0),(r*1.7,0),(r*1.7,.16),(r*1.18,.32),(r,.43),(r*.88,h-.4),
        (r*1.25,h-.3),(r*1.7,h-.13),(r*1.7,h),(0,h)],(x,y,z),f,16)
    if collide:active.append(dict(p=(x,y,z+h/2),d=(r*3.4,r*3.4,h),yaw=0,floor=False))
def arch(b,p,w,h,yaw=0,f=EDGE,thick=.24):
    a=B();a.arc((0,0,h-w/2),w/2,thick,0,math.pi,f,depth=.5,segments=24)
    for s in [-1,1]:box(a,(s*w/2,0,(h-w/2)/2),(thick,.5,h-w/2),f)
    merge(b,a,p,yaw)
def ring(b,r,z,width=.25,depth=.20,f=EDGE,sides=80):
    b.lathe([(r-width/2,z-depth/2),(r+width/2,z-depth/2),(r+width/2,z+depth/2),
        (r-width/2,z+depth/2),(r-width/2,z-depth/2)],family=f,segments=sides)
def rail(b,a,c,collide=True,f=EDGE):
    A=Vector(a);C=Vector(c);D=C-A;length=D.length;middle=(A+C)/2
    for z in [.17,1.12]:beam(b,A+Vector((0,0,z)),C+Vector((0,0,z)),.07,f)
    for i in range(math.ceil(length/.8)+1):
        p=A+D*i/math.ceil(length/.8);column(b,p.x,p.y,p.z,1.10,.075,f)
    if collide:active.append(dict(p=tuple(middle+Vector((0,0,.57))),d=(length,.14,1.14),yaw=math.degrees(math.atan2(D.y,D.x)),floor=False))
def diskfloor(b,r,z=0,f=STONE):
    b.lathe([(0,z-.5),(r,z-.5),(r,z),(0,z)],family=f,segments=96)
    # Narrow inscribed strips follow the visible circle rather than an invisible square.
    step=.40
    for i in range(math.ceil(r*2/step)):
        y0=-r+i*step;y1=min(r,y0+step);half=math.sqrt(max(0,r*r-max(abs(y0),abs(y1))**2))
        if half>.08:active.append(dict(p=(0,(y0+y1)/2,z-.25),d=(half*2,y1-y0+.004,.5),yaw=0,floor=True))
def furniture(b,p,yaw=0):
    x,y,z=p
    solid(b,(x,y,z+.72),(2.2,.90,.15),WOOD,.05,yaw)
    for dx in [-.85,.85]:box(b,(x+dx,y,z+.34),(.10,.65,.68),METAL)
    for sy in [-1,1]:
        solid(b,(x,y+sy*.95,z+.42),(1.9,.40,.14),WOOD,.05,yaw)
        box(b,(x,y+sy*1.11,z+.76),(1.9,.10,.66),WOOD)
def plant(b,p,seed=0,size=1):
    rng=random.Random(seed);x,y,z=p
    b.lathe([(0,0),(.4*size,0),(.6*size,.7*size),(0,.7*size)],p,STONE,16)
    for i in range(12):
        a=i*math.tau/12
        b.leaf((x,y,z+.7*size),rng.uniform(.6,1.2)*size,.3*size,a,.5,family='Sky92Leaf')
def window(b,x,y,z,w,h,yaw=0,f=EDGE):
    # The backing masonry remains solid; recessed glazing must sit in front of it.
    a=B();r=w/2
    glazing=[(-r,-.10,0),(r,-.10,0)]
    glazing += [(math.cos(j*math.pi/24)*r,-.10,h-r+math.sin(j*math.pi/24)*r) for j in range(25)]
    a.mesh(glazing,[tuple(range(len(glazing)))],WINDOW,smooth=False)
    arch(a,(0,0,0),w,h,0,f,.16)
    for s in [-1,1]:column(a,s*w/2,-.06,0,h-w/2,.09,f)
    for zz in [.22,.55]:box(a,(0,-.18,h*zz),(w,.08,.065),METAL)
    box(a,(0,-.18,h*.47),(.07,.08,h*.94),METAL)
    box(a,(0,-.12,-.10),(w+.4,.65,.2),f);merge(b,a,(x,y,z),yaw)
def roof(b,p,w,d,h,f=TILE):
    x,y,z=p
    b.mesh([(-w/2,-d/2,0),(w/2,-d/2,0),(w/2,d/2,0),(-w/2,d/2,0),(0,-d/2,h),(0,d/2,h)],
        [(0,1,4),(1,2,5,4),(2,3,5),(3,0,4,5)],f,position=p,smooth=False)
    beam(b,(x,y-d/2,z+h),(x,y+d/2,z+h),.15,COPPER)
    for j in range(1,14):
        yy=y-d/2+d*j/14
        for s in [-1,1]:beam(b,(x+s*w/2,yy,z),(x,yy,z+h),.045,f)
def tower(b,p,r,h,f=WHITE,roofmat=ROOF,sides=12):
    x,y,z=p
    b.lathe([(0,0),(r,0),(r,h*.78),(r*1.12,h*.79),(r*1.12,h*.82),(r,h*.83),(r,h),(0,h)],p,f,sides)
    for zz in [h*.25,h*.51,h*.77,h]:
        q=B();ring(q,r*1.10,0,.32,.25,f,sides*3);merge(b,q,(x,y,z+zz))
    for i in range(sides):
        a=i*math.tau/sides;xx=x+math.sin(a)*(r+.02);yy=y-math.cos(a)*(r+.02)
        for j in range(4):window(b,xx,yy,z+1+j*h*.215,r*.78,h*.15,a,f)
        # Fluted ribs and a projecting crown break up the upper masonry bands.
        beam(b,(x+math.sin(a)*r*1.02,y-math.cos(a)*r*1.02,z+h*.82),
            (x+math.sin(a)*r*1.02,y-math.cos(a)*r*1.02,z+h),.075,COPPER)
        column(b,x+math.sin(a)*r,y-math.cos(a)*r,z+h-.25,.9,.12,f)
    b.lathe([(r*1.25,h),(r*.88,h+1),(r*.7,h+3),(0,h+r*3.3)],p,roofmat,sides)
    beam(b,(x,y,z+h+r*3.2),(x,y,z+h+r*3.2+3),.075,COPPER)

def shop_sign(b,label):
    curve=bpy.data.curves.new('Sky92ShopLetters','FONT');curve.body=label
    curve.align_x='CENTER';curve.size=.34;curve.extrude=.006;curve.resolution_u=3
    obj=bpy.data.objects.new('Sky92ShopLetters',curve);bpy.context.collection.objects.link(obj)
    evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
    # Text needs a horizontal pre-flip: the final UE coordinate reflection
    # otherwise reverses the readable face on the outer shop canopy.
    b.mesh([(-v.co.x,v.co.y,v.co.z) for v in mesh.vertices],[tuple(reversed(p.vertices)) for p in mesh.polygons],EDGE,
        position=(0,12.28,3.25),rotation=Matrix.Rotation(math.pi,3,'Z')@Matrix.Rotation(math.pi/2,3,'X'),smooth=False)
    evaluated.to_mesh_clear();bpy.data.objects.remove(obj,do_unlink=True);bpy.data.curves.remove(curve)

# 1. A tapered glazed shopping rotunda, with an open occupied-size arcade.
active=[];b=B();gl=B();diskfloor(b,18)
b.lathe([(0,-11),(7,-11),(15,-2),(18,-.5)],family=WHITE,segments=72)
for i in range(32):
    a=i*math.tau/32;aa=(i+1)*math.tau/32
    if abs((a+aa)/2-math.pi*1.5)>.21:rail(b,(18*math.cos(a),18*math.sin(a),0),(18*math.cos(aa),18*math.sin(aa),0))
for i in range(20):
    a=i*math.tau/20
    if i!=15:column(b,16.5*math.cos(a),16.5*math.sin(a),0,6,.22,WHITE,True)
    solid(b,(9.7*math.cos(a),9.7*math.sin(a),2.8),(3.2,.32,5.6),METAL,yaw=a+math.pi/2)
for i in range(8):
    a=i*math.tau/8;stall=B()
    # Deep shop windows, counters, small canopies and shelving give a human scale.
    box(stall,(0,10.7,3.75),(5.3,3,.12),ROOF)
    for xx in [-2.45,2.45]:box(stall,(xx,10.3,1.8),(.12,1.3,3.6),COPPER)
    for z in [.7,1.5,2.3]:box(stall,(0,10.05,z),(4.8,.65,.12),WOOD)
    for j in range(9):
        x=-2+j*.48
        if i%3==0:box(stall,(x,10.1,1.8),(.25,.38,.55+(.2 if j%2 else 0)),('Sky92Cloth' if j%2 else WHITE))
        elif i%3==1:
            stall.lathe([(0,0),(.12,0),(.17,.22),(.12,.28),(0,.28)],(x,10.1,1.56),WHITE,12)
            if j%2:plant(stall,(x,10.05,2.36),i*10+j,.36)
        else:
            box(stall,(x,10.12,1.56),(.35,.45,.08),'Sky92Cloth')
            box(stall,(x,10.12,1.64),(.31,.42,.07),WHITE)
    box(stall,(0,12.1,.45),(3.8,.7,.9),WOOD)
    box(stall,(0,12.1,.925),(3.95,.85,.05),STONE)
    shop_sign(stall,['ATELIER','FLEURS','CAFE','LIBRAIRIE','MAISON','THE','GALERIE','TISSUS'][i])
    merge(b,stall,yaw=a)
    p=Matrix.Rotation(a,3,'Z')@Vector((0,12.1,.45));active.append(dict(p=tuple(p),d=(3.8,.7,.9),yaw=math.degrees(a),floor=False))
for i in [0,2,4,6]:plant(b,(14*math.cos(i*math.tau/8),14*math.sin(i*math.tau/8),0),i)
for j in range(13):
    z=6+j*5.8;r=16-j*.77;rn=r-.77
    # Thick ledges and alternating recessed glass bays keep highlights and shade separate.
    ring(b,r+.6,z,.85,.65,WHITE);ring(b,r+.58,z+.3,.13,.12,COPPER)
    for i in range(48):
        a=i*math.tau/48;aa=(i+1)*math.tau/48
        verts=[(r*math.cos(a),r*math.sin(a),z+.35),(r*math.cos(aa),r*math.sin(aa),z+.35),
            (rn*math.cos(aa),rn*math.sin(aa),z+5.45),(rn*math.cos(a),rn*math.sin(a),z+5.45)]
        b.mesh(verts,[(0,1,2,3)],WINDOW,colour=(.065+(i%3)*.008,.23+(j%3)*.025,.28+(i%4)*.015,1),smooth=False)
        beam(b,(r*math.cos(a),r*math.sin(a),z),(rn*math.cos(a),rn*math.sin(a),z+5.8),.105,METAL)
        if i%4==0:beam(b,((r+.3)*math.cos(a),(r+.3)*math.sin(a),z),((rn+.3)*math.cos(a),(rn+.3)*math.sin(a),z+5.8),.12,COPPER)
    ring(b,r-.36,z+2.6,.11,.11,METAL)
    if j%3==2:ring(b,r+1.05,z+.75,.11,.18,COPPER)
tower(b,(0,0,81),3.9,9,WHITE,ROOF)
emit('Sky92Market',b,'Tapered glass rotunda, 8 walkable shop bays, ring promenade and deep facade fins.',active)

# 2. Sandstone cloister, arcaded court and a domed hall in the clouds.
active=[];b=B();gl=B()
solid(b,(0,0,-.30),(36,34,.6),STONE,floor=True)
for side in [-1,1]:
    solid(b,(side*17.5,0,3),(.7,34,6),STONE)
    solid(b,(side*12.8,2,6.6),(9,30,.6),EDGE,floor=True)
    for y in range(-12,16,4):
        column(b,side*8.5,y,0,4.2,.3,EDGE,True)
        arch(b,(side*8.5,y+2,0),4,6,math.pi/2,EDGE,.35)
    roof(b,(side*13,2,7),10,31,3.5)
    for y in range(-12,15,5):window(b,side*17.9,y,1.2,2.7,3.7,side*math.pi/2,EDGE)
    for y in [-8,5]:furniture(b,(side*12.4,y,0))
    for y in [-13,13]:plant(b,(side*6.7,y,0),y+20)
solid(b,(0,16.5,5),(36,1,10),STONE)
for x in [-14,-10,10,14]:solid(b,(x,8,4),(3.7,.7,8),STONE)
for x in [-6,-2,2,6]:column(b,x,8,0,5,.29,EDGE,True)
for x in [-4,0,4]:arch(b,(x,8,0),4,7,0,EDGE,.4)
solid(b,(0,12.4,10),(35,9.2,.6),EDGE,floor=True)
for x in range(-14,15,4):window(b,x,16.95,2,2.8,5,math.pi)
# Round dome and lantern above the rear hall.
b.lathe([(0,10.4),(7,10.4),(7,12),(6.6,13),(5.3,16),(2.2,18.8),(0,19.6)],(0,12,0),ROOF,64)
for i in range(16):
    a=i*math.tau/16;pts=[(math.cos(a)*r,12+math.sin(a)*r,z) for r,z in [(7,12),(6.6,13),(5.3,16),(2.2,18.8),(0,19.6)]]
    b.tube(pts,[.07]*len(pts),COPPER,8)
tower(b,(-13,11,10.5),3,23,STONE,TILE,8)
tower(b,(13,11,10.5),2.8,15,STONE,TILE,8)
# Front triple gateway and flanking balustrades leave the central six metres open.
for x in [-14,-10,-6,6,10,14]:column(b,x,-15,0,4.2,.31,EDGE,True)
for x in [-12,-8,8,12]:arch(b,(x,-15,0),4,6,0,EDGE,.3)
for s in [-1,1]:rail(b,(s*3.4,-16.7,0),(s*17.4,-16.7,0))
arch(b,(0,-15,0),12,10,0,EDGE,.55)
for x in [-5.8,5.8]:solid(b,(x,-15,3),(.65,.85,6),STONE)
for s in [-1,1]:
    # Detached buttress shrines hang below the building instead of becoming a giant support pole.
    tower(b,(s*13,9,-20),2.5,13,STONE,TILE,8)
    beam(b,(s*13,9,-8),(s*9,10,-1),.45,STONE)
for y in [-7,-2,3]:
    box(b,(0,y,.012),(.08,4.8,.02),COPPER,.001)
emit('Sky92Cloister',b,'Open sandstone cloister, vaulted walkways, reading benches, copper dome and hanging shrines.',active)

# 3. A floating castle, with a real gateway, open court and accessible vaulted hall.
active=[];b=B();gl=B();diskfloor(b,38,f=WHITE)
b.lathe([(0,-43),(5,-38),(11,-29),(19,-23),(26,-13),(34,-7),(38,-.5)],family=WHITE,segments=32)
for i in range(24):
    a=i*math.tau/24
    beam(b,(35*math.cos(a),35*math.sin(a),-.4),(19*math.cos(a),19*math.sin(a),-27),.35,EDGE)
    if i%3==0:tower(b,(27*math.cos(a),27*math.sin(a),-22),2,12,WHITE,ROOF,8)
for i in range(64):
    a=i*math.tau/64;aa=(i+1)*math.tau/64
    if abs((a+aa)/2-math.pi*1.5)>.14:rail(b,(37.8*math.cos(a),37.8*math.sin(a),0),(37.8*math.cos(aa),37.8*math.sin(aa),0))
# Gateway is a physical opening, not a door image on a solid wall.
for s in [-1,1]:
    solid(b,(s*12,-25,4),(15,1.4,8),WHITE)
    tower(b,(s*21,-23,0),4.1,31,WHITE,ROOF)
    active.append(dict(p=(s*21,-23,15.5),d=(6.4,6.4,31),yaw=0,floor=False))
    solid(b,(s*6,-25,5),(1.0,1.4,10),WHITE)
    solid(b,(s*29,0,6.5),(1.2,35,13),WHITE)
    for y in range(-14,17,5):window(b,s*29.8,y,3,3,6,s*math.pi/2,WHITE)
    for y in [-16,-5,6,17]:
        column(b,s*23,y,0,7,.4,WHITE,True)
        if y<17:arch(b,(s*23,y+5.5,0),11,12,math.pi/2,WHITE,.5)
    solid(b,(s*26.2,0,12.6),(7.6,37,.7),WHITE,floor=True)
    roof(b,(s*26,0,13),9,39,5,ROOF)
    for y in [-10,8]:furniture(b,(s*25,y,0))
    for y in [-20,20]:plant(b,(s*12,y,0),int(y+25),2)
arch(b,(0,-25,0),12,14,0,WHITE,.65)
solid(b,(0,-25,13),(12,1.6,2.2),WHITE)
for x in [-7,7]:tower(b,(x,-25,13.8),1.15,8,WHITE,ROOF,8)
# Central great hall: separate side walls, high ceiling, open door and glazed apse.
for s in [-1,1]:
    solid(b,(s*10,12,8),(.9,20,16),WHITE)
    solid(b,(s*6.8,2,7),(5.5,.8,14),WHITE)
    for y in [5,11,17]:window(b,s*10.55,y,3,4,9,s*math.pi/2,WHITE)
    for y in [4,10,16,21]:column(b,s*8,y,0,10,.32,WHITE,True)
arch(b,(0,2,0),8,13,0,WHITE,.45)
solid(b,(0,2,15),(20,1,4),WHITE)
solid(b,(0,22,8),(20,.8,16),WHITE)
for x in [-6,0,6]:window(b,x,21.5,2,4,10,0,WHITE)
solid(b,(0,12,17),(21,22,.6),WHITE,floor=True)
roof(b,(0,12,17.3),22,23,13,ROOF)
for x in [-11,11]:tower(b,(x,22,0),3.4,44,WHITE,ROOF,12)
tower(b,(0,17,28),4.3,29,WHITE,ROOF,12)
for s in [-1,1]:
    # Detached satellite turrets are visibly separated from the inhabited castle.
    tower(b,(s*40,13,12),2.3,26,WHITE,ROOF,10)
    b.lathe([(0,-8),(2,-3),(2.6,0)],(s*40,13,12),WHITE,10)
    beam(b,(s*29,13,16),(s*38,13,15),.18,COPPER)
for x in [-4,4]:furniture(b,(x,14,0))
for y in [-19,-9,0,10]:box(b,(0,y,.012),(.11,8,.024),COPPER,.001)
emit('Sky92Castle',b,'Floating ribbed foundation, detached towers, open gateway, court, colonnades and great hall.',active)

# Slender conical pavilions diversify the rest of the skyline.
active=[];b=B();diskfloor(b,8,f=WHITE)
for i in range(12):
    a=i*math.tau/12;column(b,6.7*math.cos(a),6.7*math.sin(a),0,7,.25,WHITE)
    a2=(i+1)*math.tau/12
    b.mesh([(6.5*math.cos(a),6.5*math.sin(a),1),(6.5*math.cos(a2),6.5*math.sin(a2),1),
        (6.5*math.cos(a2),6.5*math.sin(a2),6.5),(6.5*math.cos(a),6.5*math.sin(a),6.5)],[(0,1,2,3)],WINDOW,smooth=False)
for j in range(7):
    z=7+j*5;r=8-j*.76
    ring(b,r,z,.5,.35,WHITE);b.lathe([(r,z),(r-.76,z+5)],family=WINDOW,segments=48)
    for i in range(12):
        a=i*math.tau/12;beam(b,(r*math.cos(a),r*math.sin(a),z),((r-.76)*math.cos(a),(r-.76)*math.sin(a),z+5),.09,COPPER)
b.lathe([(3,42),(0,55)],family=ROOF,segments=24)
emit('Sky92ConePavilion',b,'Slender copper and glass conical pavilion; skyline companion to the market.')

# New phone shell, leaving a slim graphite bezel around the 720 x 1440 screen.
b=B()
def handset_layer(b,w,h,depth,y,r,f):
    # Corner radius is independent of thickness, unlike a cube bevel.
    outline=[]
    for x,z,start in [(w/2-r,h/2-r,0),(-w/2+r,h/2-r,90),
                       (-w/2+r,-h/2+r,180),(w/2-r,-h/2+r,270)]:
        for j in range(13):
            a=math.radians(start+j*90/12);outline.append((x+math.cos(a)*r,z+math.sin(a)*r))
    n=len(outline);v=[(x,y+dy,z) for dy in [-depth/2,depth/2] for x,z in outline]
    faces=[tuple(range(n)),tuple(reversed(range(n,2*n)))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    b.mesh(v,faces,f,smooth=False)
handset_layer(b,.094,.184,.012,0,.007,'Sky92PhoneMetal')
handset_layer(b,.093,.183,.005,.0045,.0068,'Sky92Dark')
handset_layer(b,.088,.176,.001,.0075,.0062,'Sky92Dark')
for z in [.03,.05]:box(b,(.048,0,z),(.002,.007,.011),'Sky92PhoneMetal',.0006)
box(b,(-.048,0,.04),(.002,.007,.019),'Sky92PhoneMetal',.0006)
emit('Sky92Phone',b,'Rounded graphite/titanium handset, original unbranded design.')

(OUT/'sky92-catalog.json').write_text(json.dumps(dict(assets=records,collision=collision),ensure_ascii=False,indent=2),encoding='utf8')
# Native runtime uses these same box dimensions; no auto-generated convex hull over doorways.
lines=['#pragma once','#include "EWWorld.h"','namespace EWSky92Data {','struct Shape {FVector P,Half; double Yaw; bool Floor;};']
for name,items in collision.items():
    lines.append('inline const Shape '+name+'[] = {')
    for c in items:
        def v(a):return 'FVector('+','.join(f'{x:.5f}' for x in a)+')'
        lines.append('{'+v([x*100 for x in c['p']])+','+v([x*50 for x in c['d']])+f",{c['yaw']:.5f},"+str(c['floor']).lower()+'},')
    lines.append('};')
lines.append('}')
(OUT/'Generated/EWSky92Data.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'sky92-originals.blend'))
print('SKY92_READY',len(records),sum(len(a) for a in collision.values()),flush=True)
