"""Original walkable white-spire water district, metres. User reference is visual only.
Stone ribs, real open arches and vegetation are geometry, never a backdrop image.
"""
from pathlib import Path
import bpy, runpy, math, random, json
from mathutils import Matrix, Vector

g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
B=g['Builder'];OUT=g['OUT'];records=[]
for family,colour in [('Cascade86Stone',(.61,.69,.68,1)),('Cascade86Leaf',(.12,.26,.09,1)),
                      ('Cascade86Surface',(.5,.99,.5,1)),('Cascade86Fall',(.5,.5,.6,1)),('Cascade86Foam',(.5,.5,.3,1))]:
    g['FAMILIES'].append(family);g['COLOURS'][family]=colour
    m=bpy.data.materials.new('M_'+family);m.diffuse_color=colour;g['MATERIALS'].append(m)
STONE='Cascade86Stone';LEAF='Cascade86Leaf'
PALE=(.64,.70,.69,1);DARK=(.20,.29,.28,1);EDGE=(.73,.76,.69,1);GOLD=(.24,.31,.27,1)

def box(b,p,d,c=PALE,bevel=.045,family=STONE):b.box(p,d,family,bevel,colour=c)
def beam(b,a,c,r=.12,colour=EDGE,family=STONE):b.tube([a,c],[r,r],family,10,colour)
def merge(b,a,p=(0,0,0),yaw=0):
    n=len(b.v);q=Matrix.Rotation(yaw,3,'Z');v=Vector(p)
    b.v.extend(tuple(q@Vector(x)+v) for x in a.v);b.f.extend(tuple(n+i for i in face) for face in a.f)
    b.m.extend(a.m);b.col.extend(a.col);b.smooth.extend(a.smooth)
def emit(name,b,note):
    bounds=[[min(v[i] for v in b.v)*100 for i in range(3)],[max(v[i] for v in b.v)*100 for i in range(3)]]
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(face)) for face in b.f]
    g['export'](name,b,note);item=g['CATALOG'][-1];item['ue_bounds_cm']=bounds;records.append(item)
def pillar(b,x,y,z,h,r=.32):
    b.lathe([(0,0),(r*1.6,0),(r*1.6,.18),(r,.34),(r*.84,h-.5),(r*1.4,h-.3),(r*1.5,h),(0,h)],(x,y,z),STONE,20,EDGE)
    for a in range(4):
        angle=a*math.pi/2;beam(b,(x+math.cos(angle)*r,y+math.sin(angle)*r,z+.5),(x+math.cos(angle)*r*.85,y+math.sin(angle)*r*.85,z+h-.6),r*.16)
def pointed(b,x,y,z,width,height,r=.17):
    # Gothic rib: two curved sides meet at a real apex, leaving the centre open.
    for sign in [-1,1]:
        pts=[(x+sign*width*.5*(1-t**1.55),y,z+height*t) for t in [i/24 for i in range(25)]]
        b.tube(pts,[r]*len(pts),STONE,10,EDGE)
def window(b,x,y,z,w,h):
    pts=[(x-w/2,y,z),(x+w/2,y,z),(x+w/2,y,z+h*.56)]
    pts += [(x+w*.5*(1-t**1.55),y,z+h*.56+h*.44*t) for t in [i/14 for i in range(1,15)]]
    pts += [(x-w*.5*(1-t**1.55),y,z+h*.56+h*.44*t) for t in [i/14 for i in range(13,-1,-1)]]
    b.mesh(pts,[tuple(range(len(pts)))],STONE,(.115,.25,.28,1),smooth=False)
    for xx in [x-w/2,x+w/2]:pillar(b,xx,y-.09,z,h*.56,.11)
    pointed(b,x,y-.12,z+h*.56,w,h*.44,.12)
    beam(b,(x,y-.13,z),(x,y-.13,z+h*.87),.07,DARK)
    for zz in [.32,.54]:beam(b,(x-w*.44,y-.12,z+h*zz),(x+w*.44,y-.12,z+h*zz),.055,DARK)
def vine(b,p,length,seed,density=1):
    rng=random.Random(seed);x,y,z=p
    pts=[(x+math.sin(i*.9+seed)*.17,y+.10*math.sin(i),z-length*i/10) for i in range(11)]
    b.tube(pts,[.035*(1-i/15) for i in range(11)],'Bark',6,(.12,.16,.065,1))
    for i in range(int(length*5*density)+2):
        t=rng.random();zz=z-t*length;xx=x+math.sin(t*9+seed)*.2
        b.leaf((xx,y-.1,zz),rng.uniform(.28,.62),rng.uniform(.16,.34),rng.random()*math.tau,.32,LEAF,(.10+rng.random()*.07,.22+rng.random()*.13,.055+rng.random()*.055,1))
def shrub(b,p,size,seed):
    rng=random.Random(seed);x,y,z=p
    for j in range(5):
        a=j*2.399;end=(x+math.cos(a)*size*.35,y+math.sin(a)*size*.35,z+size*(.45+rng.random()*.35))
        beam(b,p,end,.035,(.14,.19,.085,1),'Bark')
        for k in range(8):
            t=rng.random();v=Vector(p).lerp(Vector(end),t)
            b.leaf(tuple(v),size*.48,size*.2,a+k*1.2,.35,LEAF,(.13,.25+rng.random()*.10,.07,1))
def rail(b,length,width):
    for side in [-1,1]:
        y=side*width/2
        for z in [.28,1.13]:box(b,(0,y,z),(length,.16,.13),EDGE,.025)
        for i in range(math.ceil(length/1.5)+1):
            x=-length/2+i*length/math.ceil(length/1.5);pillar(b,x,y,0,1.2,.09)

# Three silhouettes, each with readable shaded window reveals and flying ribs.
for variant,height in enumerate([100,138,178]):
    b=B();rng=random.Random(8600+variant)
    stages=[(0,height*.40,12),(height*.40,height*.30,9),(height*.70,height*.20,6)]
    for base,h,half in stages:
        box(b,(0,0,base+h/2),(half*2,half*2,h),PALE,.12)
        for z in [base,base+h*.5,base+h]:
            box(b,(0,0,z),(half*2+1.3,half*2+1.3,.48),EDGE,.08)
        for side in range(4):
            f=B()
            cols=max(2,int(half/2.5));width=half*2/cols
            for c in range(cols):
                x=-half+(c+.5)*width
                for row in range(max(1,int(h/12))):
                    wh=(h-2)/max(1,int(h/12))
                    window(f,x,-half-.06,base+1+row*wh,width*.67,wh*.82)
            for x in [-half+.25,0,half-.25]:pillar(f,x,-half-.35,base,h,.23)
            for j in range(6):vine(f,(-half+(.5+j)*half/3,-half-.55,base+h+.1),2.5+rng.random()*9,variant*300+side*40+j)
            for x in [-half+1,half-1]:shrub(f,(x,-half+.7,base+h+.3),4.2+(side%2)*.5,side*13+variant*97+int(x))
            merge(b,f,yaw=side*math.pi/2)
    crown=height*.90
    for x in [-6,6]:
        for y in [-6,6]:
            pillar(b,x,y,crown, height*.095,.42)
            b.lathe([(0,0),(.7,0),(.4,3),(.12,7),(0,8)],(x,y,crown+height*.095),STONE,12,EDGE)
    for side in range(4):
        f=B();pointed(f,0,-6,crown,12,height*.10,.25)
        for sign in [-1,1]:
            pts=[(sign*(11-5*t),-11+5*t,height*.52+height*.33*math.sin(t*math.pi/2)) for t in [i/25 for i in range(26)]]
            f.tube(pts,[.27]*len(pts),STONE,10,EDGE)
        merge(b,f,yaw=side*math.pi/2)
    emit(f'Cascade86Spire{variant}',b,'Original stepped gothic tower with open crown, shaded lancets and hanging greenery')

# Raised, open arch bridge. A 1 cm recessed walking skin avoids joint flicker.
b=B();box(b,(0,0,-.27),(24,8,.50),PALE,.07);rail(b,20,7.7)
for x in [-12,0,12]:
    for y in [-3,3]:
        pillar(b,x,y,-32,31.8,.45)
        box(b,(x,y,-32),(1.8,1.8,.6),DARK,.06)
for x in [-6,6]:
    for y in [-3.3,3.3]:pointed(b,x,y,-12,10,11,.28)
for x in [-10,-6,2,7]:
    for y in [-4.05,4.05]:vine(b,(x,y,-.1),3.8+(x%4),860+int(x+y*7),.7)
for x in [-8,8]:
    for y in [-4.05,4.05]:shrub(b,(x,y,-.3),1.9,860+int(x+y*7))
emit('Cascade86Bridge',b,'24 m colonnaded span; measured matching independent collision')

# A ring belvedere and an open, roofed sanctuary.
b=B();box(b,(0,0,-.32),(28,24,.64),EDGE,.09)
for x in [-13,13]:
    for y in [-11,11]:pillar(b,x,y,-18,18,.65);shrub(b,(x,y,.15),2.5,int(x+y+86))
for y in [-11.6,11.6]:
    for x in [-9,9]:
        r=B();rail(r,8,0.12);merge(b,r,(x,y,0))
for x in [-13.6,13.6]:
    for y in [-7.7,7.7]:
        r=B();rail(r,7.4,.12);merge(b,r,(x,y,0),math.pi/2)
for x in [-12,-8,8,12]:vine(b,(x,-12,-.2),7,860+int(x));vine(b,(x,12,-.2),5,861+int(x))
emit('Cascade86Belvedere',b,'28 by 24 m upper viewpoint with clear central north/south approaches')
b=B()
for x in [-7,7]:
    for y in [-7,7]:pillar(b,x,y,0,8,.46)
for side in range(4):
    f=B();pointed(f,0,-7,8,14,5,.28)
    for x in [-7,7]:vine(f,(x,-7.4,8),4,860+int(x)+side*17)
    merge(b,f,yaw=side*math.pi/2)
b.lathe([(0,0),(11.2,0),(11.6,.5),(8,2.2),(1.3,4.8),(0,5.0)],(0,0,13),STONE,8,PALE)
for a in range(8):
    t=a*math.pi/4;shrub(b,(math.cos(t)*8,math.sin(t)*8,13.5),1.4,860+a)
emit('Cascade86Sanctum',b,'Open sanctuary: four 12 m wide entrances and genuine 13 m vaulted space')

# An ornamental circular crown reads as a landmark from the old city.
b=B()
for r in [16,17.1]:
    pts=[(math.cos(t)*r,0,math.sin(t)*r+17.6) for t in [i*math.tau/120 for i in range(121)]]
    b.tube(pts,[.22]*len(pts),STONE,10,EDGE)
for a in range(12):
    t=a*math.tau/12;beam(b,(math.cos(t)*15.4,0,math.sin(t)*15.4+17.6),(math.cos(t)*17.6,0,math.sin(t)*17.6+17.6),.16)
for x in [-7,7]:pillar(b,x,0,0,7,.44)
emit('Cascade86Halo',b,'Stone astronomical ring crowning the outer water city')

b=B();box(b,(0,0,-1),(22,18,2),DARK,.10)
for x in [-10.8,10.8]:box(b,(x,0,.16),(.4,18,.5),EDGE,.06)
box(b,(0,-8.8,.16),(22,.4,.5),EDGE,.06)
for x in [-8,8]:box(b,(x,8.8,.16),(5.6,.4,.5),EDGE,.06)
for x in [-10,-7,7,10]:vine(b,(x,9,-.3),9,860+int(x))
emit('Cascade86Cistern',b,'Real elevated source basin, open 10 m spillway on the north side')

# Rebase-stable UV metres and per-vertex fall height parameters match material contract.
for name,width,depth,family in [('Water',128,128,'Cascade86Surface'),('SourceWater',21.4,17.6,'Cascade86Surface')]:
    b=B();n=32 if name=='Water' else 8;v=[(width*i/n,depth*j/n,0) for j in range(n+1) for i in range(n+1)]
    faces=[]
    for j in range(n):
        for i in range(n):a=j*(n+1)+i;faces.append((a,a+1,a+n+2,a+n+1))
    b.mesh(v,faces,family,(.5,.999,.4,1));emit('Cascade86'+name,b,'Animated blue single-layer water, UV phase independent of origin rebasing')
for height in [36,60,4.5]:
    b=B();nx=32;ny=80;v=[];cols=[]
    for j in range(ny+1):
        t=j/ny
        for i in range(nx+1):
            u=i/nx;v.append(((u-.5)*(128 if height==4.5 else 10),.3*math.sin(t*math.pi),height*(1-t)));cols.append((u,t,height/100,1))
    f=[]
    for j in range(ny):
        for i in range(nx):a=j*(nx+1)+i;f.append((a,a+1,a+nx+2,a+nx+1))
    b.mesh(v,f,'Cascade86Fall');b.col=cols;emit('Cascade86Weir' if height==4.5 else f'Cascade86Fall{height}',b,'Downward advected translucent streams; irregular short foam trails, no emissive sheet')
b=B();n=24;v=[];cols=[]
for j in range(n+1):
    for i in range(n+1):u=i/n;w=j/n;v.append(((u-.5)*16,(w-.5)*9,0));cols.append((u,w,.41,1))
f=[]
for j in range(n):
    for i in range(n):a=j*(n+1)+i;f.append((a,a+1,a+n+2,a+n+1))
b.mesh(v,f,'Cascade86Foam');b.col=cols;emit('Cascade86Foam',b,'Soft evolving impact foam around waterfall foot')

b=B()
for j in range(11):
    a=j*2.399;r=2.0*math.sqrt((j+.4)/11);shrub(b,(math.cos(a)*r,math.sin(a)*r,0),1.4+(j%3)*.55,860+j)
emit('Cascade86Garden',b,'Dense mixed evergreen crown; small branches and individual leaves')
b=B();box(b,(0,0,-.5),(32,32,1),PALE,.09)
for i in range(-7,8):
    box(b,(i*2,0,.012),(.013,31.85,.012),DARK,0);box(b,(0,i*2,.012),(31.85,.013,.012),DARK,0)
for x in [-15,15]:
    for y in [-15,15]:shrub(b,(x,y,.02),1.8,860+int(x+y))
emit('Cascade86Square',b,'One non-overlapping central piazza skin with subtle joints')

OUT.joinpath('cascade86-catalog.json').write_text(json.dumps({'assets':records,'reference':'historical visual reference excluded from distribution; no image input','layout':'EWOuterWater.cpp'},ensure_ascii=False,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Blender/Cascade86.blend'))
print('CASCADE86_COMPLETE',len(records),sum(x['triangles'] for x in records),flush=True)
