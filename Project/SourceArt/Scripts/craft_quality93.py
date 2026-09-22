"""Shared original furniture craft. Metres, local axes; no collision/layout changes.

Small details are baked into the existing Nanite meshes, never separate actors.
All shapes and decorative marks are authored here, with deterministic seeds.
"""
import math,random
from mathutils import Vector,Matrix

def tint(c,gain):return tuple(max(0,min(.92,x*gain)) for x in c[:3])+(1,)
def merge(b,part,p=(0,0,0),yaw=0):
    n=len(b.v);q=Matrix.Rotation(yaw,3,'Z');v=Vector(p)
    b.v.extend(tuple(q@Vector(x)+v) for x in part.v)
    b.f.extend(tuple(n+i for i in face) for face in part.f)
    b.m.extend(part.m);b.col.extend(part.col);b.smooth.extend(part.smooth)

def cushion(b,p,d,c,family='Cloth',power=.30):
    """Rounded, lightly compressed upholstery with inset stitched welt seams."""
    rings=16;segments=40;v=[];f=[]
    short=min(range(3),key=lambda k:d[k]);other=[k for k in range(3) if k!=short]
    def cv(x):return math.copysign(abs(x)**power,x)
    def point(t,a):
        q=[0.,0.,0.];q[other[0]]=cv(math.cos(t))*cv(math.cos(a))
        q[other[1]]=cv(math.cos(t))*cv(math.sin(a));q[short]=cv(math.sin(t))
        # Subtle compression folds converge at the edge without crossing bounds.
        q[short]*=1-.035*(1-abs(math.sin(t)))*(.5+.5*math.sin(a*11+.7))
        return tuple(p[k]+q[k]*d[k]*.5 for k in range(3))
    for j in range(rings+1):
        for i in range(segments):v.append(point(-math.pi/2+math.pi*j/rings,i*math.tau/segments))
    # Axis remapping may reverse handedness; keep outward-facing normals.
    reverse=(other+[short]) in ([0,2,1],[1,0,2],[2,1,0])
    for j in range(rings):
        for i in range(segments):
            a=j*segments+i;k=j*segments+(i+1)%segments
            face=(a,k,k+segments,a+segments);f.append(tuple(reversed(face)) if reverse else face)
    b.mesh(v,f,family,c)
    r=min(.0045,min(d)*.024)
    for side in [-1,1]:
        points=[point(side*.18,i*math.tau/80) for i in range(81)]
        b.tube(points,[r]*len(points),family,6,tint(c,.77))

def drape(b,p,w,depth,c,family='Cloth',drop=.25):
    """Thin quilt with a folded edge and hanging skirts, rather than a pillow."""
    nx=72;ny=40;v=[];f=[]
    def at(u,t):
        x=w*(u-.5);y=depth*(t-.5)
        edge=max(0,(abs(u-.5)-.46)/.04)
        front=max(0,(.04-t)/.04)
        z=-drop*edge**.8-.085*front+.006*math.sin(u*27+t*7)+.008*math.sin(u*11-t*5)
        z+=.007*math.sin(u*29)*max(edge,front)
        # Head end folds over itself as a gentle roll.
        z+=.035*math.exp(-((t-.92)/.065)**2)
        return (p[0]+x,p[1]+y,p[2]+z)
    for j in range(ny+1):
        for i in range(nx+1):v.append(at(i/nx,j/ny))
    for j in range(ny):
        for i in range(nx):
            a=j*(nx+1)+i;f.append((a,a+1,a+nx+2,a+nx+1))
    # Close the thin underside and hem, so looking under a fold reveals fabric
    # rather than culled triangles or isolated welt tubes.
    n=len(v);v.extend((x,y,z-.010) for x,y,z in list(v))
    f.extend(tuple(n+i for i in reversed(face)) for face in list(f))
    perimeter=list(range(nx+1))+[j*(nx+1)+nx for j in range(1,ny+1)]
    perimeter += [ny*(nx+1)+i for i in range(nx-1,-1,-1)]
    perimeter += [j*(nx+1) for j in range(ny-1,0,-1)]
    for a,k in zip(perimeter,perimeter[1:]+perimeter[:1]):f.append((a,n+a,n+k,k))
    b.mesh(v,f,family,c)
    for u in [.015,.985]:
        pts=[at(u,j/60) for j in range(61)];b.tube(pts,[.0035]*61,family,6,tint(c,.77))
    pts=[at(i/88,.03) for i in range(89)];b.tube(pts,[.0035]*89,family,6,tint(c,.77))
    # Quiet woven bands on the folded edge; no decorative text or logos.
    for t in [.865,.892]:
        pts=[at(i/88,t) for i in range(89)];b.tube(pts,[.002]*89,family,6,tint(c,1.12))

def cup(b,x,y,z,c=(.73,.75,.68,1),family='Ceramic',metal='Bronze'):
    b.lathe([(0,0),(.064,0),(.069,.009),(.074,.018),(.086,.105),(.087,.135),(.081,.142),(.075,.137),(.070,.040),(.051,.025),(0,.025)],(x,y,z),family,40,c)
    pts=[(x+.078+math.sin(i*math.pi/24)*.048,y,z+.079+math.cos(i*math.pi/24)*.044) for i in range(25)]
    b.tube(pts,[.010]*25,family,10,c)
    b.lathe([(0,0),(.12,0),(.139,.010),(.144,.018),(.134,.028),(.095,.017),(0,.017)],(x,y,z-.017),family,40,c)
    b.lathe([(0,0),(.077,0),(.077,.002),(0,.002)],(x,y,z+.117),family,36,(.052,.027,.011,1))
    # Fine glaze lip catches a highlight; spoon sits on the saucer.
    b.tube([(x-.104,y-.074,z+.017),(x-.005,y-.092,z+.021)],[.005,.004],metal,8,(.51,.42,.28,1))

def chair(b,x,y,yaw=0,c=(.05,.22,.19,1),wood=(.26,.12,.045,1),arm=False):
    a=type(b)();w=.62 if not arm else .68;dep=.64 if not arm else .76
    for side in [-1,1]:
        for front in [-1,1]:
            xx=side*(w/2-.075);yy=front*(dep/2-.095)
            a.tube([(xx*1.05,yy*1.06,.025),(xx,yy,.43)],[.025,.035],'Timber',12,wood)
        a.tube([(side*(w/2-.09),-dep*.31,.30),(side*(w/2-.09),dep*.33,.30)],[.013]*2,'Timber',10,wood)
        a.tube([(side*(w/2-.10),dep*.30,.35),(side*(w/2-.08),dep*.34,.91)],[.026,.023],'Timber',12,wood)
    a.box((0,0,.41),(w-.045,dep-.04,.065),'Timber',.025,colour=wood)
    cushion(a,(0,-.01,.48),(w-.05,dep-.06,.15),c)
    cushion(a,(0,dep*.32,.76),(w-.045,.13,.40),c)
    # Exposed shaped timber edge underneath the upholstered back.
    pts=[(-w*.45+i*w*.9/20,dep*.36+.03*math.sin(i*math.pi/20),.958-.035*abs(i/10-1)) for i in range(21)]
    a.tube(pts,[.018]*21,'Timber',10,wood)
    if arm:
        for side in [-1,1]:
            a.tube([(side*.295,-.25,.48),(side*.295,-.25,.66),(side*.295,.23,.71)],[.022,.026,.024],'Timber',12,wood)
            cushion(a,(side*.285,-.015,.695),(.09,.51,.09),c)
    merge(b,a,(x,y,0),yaw)

def book(b,x,y,z,w,h,c,depth=.21,seed=0):
    """A page block, two covers, rounded spine and discreet foil bands."""
    paper=(.64,.60,.48,1);cover=.005
    b.box((x,y+.005,z+h*.5),(w-.008,depth-.012,h-.014),'Paper',.003,colour=paper)
    for side in [-1,1]:b.box((x+side*(w/2-cover/2),y,z+h/2),(cover,depth,h),'Cloth',.0015,colour=c)
    b.box((x,y-depth/2+.004,z+h/2),(w,.012,h),'Cloth',.004,colour=c)
    for t in [.12,.82]:b.box((x,y-depth/2-.003,z+h*t),(w*.82,.004,.005),'Bronze',.0005,colour=(.45,.32,.15,1))
    for j in range(2+(seed%3)):
        b.box((x,y-depth/2-.003,z+h*.61-j*.013),(w*.52,.003,.0025),'Paper',.0005,colour=(.64,.57,.39,1))

def bookcase(b,x,y,w,h,wood,accent,depth=.48,seed=0):
    rng=random.Random(seed);front=y-depth/2;back=y+depth/2
    b.box((x,back-.02,h/2),(w,.04,h),'Timber',.007,colour=tint(wood,.55))
    for side in [-1,1]:
        b.box((x+side*(w/2-.03),y,h/2),(.06,depth,h),'Timber',.009,colour=wood)
        b.box((x+side*(w/2-.015),front-.015,h/2),(.035,.045,h),'Timber',.006,colour=tint(wood,1.12))
    b.box((x,y,.07),(w,depth,.14),'Timber',.013,colour=tint(wood,.62))
    b.box((x,y,h-.038),(w+.04,depth+.025,.075),'Timber',.013,colour=wood)
    count=max(2,int(h/.46));pitch=(h-.25)/count
    palette=[accent,(.24,.13,.075,1),(.12,.17,.21,1),(.38,.31,.18,1),(.46,.44,.35,1)]
    for row in range(count):
        z=.17+row*pitch
        b.box((x,y,z),(w-.08,depth-.014,.045),'Timber',.008,colour=wood)
        # Vertical dividers break up large cases into actual joinery bays.
        for j in range(1,max(1,int(w/1.2))):
            xx=x-w/2+j*w/max(1,int(w/1.2));b.box((xx,y,z+pitch/2),(.033,depth-.014,pitch),'Timber',.004,colour=wood)
        num=max(1,int((w-.20)/.105));start=x-w/2+.105
        for i in range(num):
            if i%19 in [15,16,17]:continue
            ww=.055+rng.random()*.030;hh=min(pitch-.075,.235+rng.random()*.12)
            yy=front+.112+rng.uniform(.004,.025)
            book(b,start+i*.105,yy,z+.025,ww,hh,palette[(i+row+seed)%5],seed=i+row)
        if num>18:
            xx=start+16*.105
            for j in range(3):
                b.box((xx,front+.12,z+.047+j*.035),(.26,.20,.030),'Cloth',.005,colour=palette[(row+j)%5])

def cabinet(b,x,y,z,w,d,h,wood,metal=(.48,.34,.17,1),drawers=2):
    """Adds recessed front panels and handles inside the cabinet footprint."""
    front=y-d/2-.003
    for j in range(drawers):
        zz=z-h/2+.07+(j+.5)*(h-.14)/drawers
        b.box((x,front,zz),(w-.09,.013,(h-.14)/drawers-.022),'Timber',.006,colour=tint(wood,.9))
        for side in [-1,1]:b.tube([(x+side*.11,front-.009,zz),(x+side*.11,front-.036,zz)],[.006]*2,'Bronze',8,metal)
        b.tube([(x-.11,front-.036,zz),(x+.11,front-.036,zz)],[.007]*2,'Bronze',10,metal)

def shade(b,x,y,z,r=.30,h=.38,c=(.73,.73,.63,1)):
    # Pleated linen shade, rolled rims, three supports and a recessed diffuser.
    n=96;v=[];f=[]
    for zz,rr in [(0,r),(h,r*.75)]:
        for i in range(n):
            a=i*math.tau/n;rad=rr+(.006 if i%2==0 else -.006)
            v.append((x+math.cos(a)*rad,y+math.sin(a)*rad,z+zz))
    for i in range(n):f.append((i,(i+1)%n,(i+1)%n+n,i+n))
    b.mesh(v,f,'Paper',c)
    for zz,rr in [(0,r),(h,r*.75)]:
        pts=[(x+math.cos(i*math.tau/64)*rr,y+math.sin(i*math.tau/64)*rr,z+zz) for i in range(65)]
        b.tube(pts,[.007]*65,'Cloth',8,tint(c,.75))
    for i in range(3):
        a=i*math.tau/3;b.tube([(x,y,z+.09),(x+math.cos(a)*r*.94,y+math.sin(a)*r*.94,z+.025)],[.0035]*2,'Bronze',8,(.38,.28,.15,1))
    b.lathe([(0,0),(r*.72,0),(r*.72,.008),(0,.008)],(x,y,z+.035),'Glow',40,(.65,.47,.24,1))

def leaf(b,p,length,width,yaw,tilt,family,c):
    """Curved leaf with a thinner serrated edge, midrib and fine branching veins."""
    steps=12;across=6;v=[];f=[]
    q=Matrix.Rotation(yaw,3,'Z')@Matrix.Rotation(tilt,3,'X');origin=Vector(p)
    def at(t,s,raise_z=0):
        w=max(.002,math.sin(math.pi*t)**.72)*width*.5
        w*=1+.026*math.sin(t*math.pi*18)
        return tuple(q@Vector((s*w,t*length,length*(.09*math.sin(math.pi*t)+s*s*.035+.018*math.sin(t*8)*abs(s))+raise_z))+origin)
    for j in range(steps+1):
        for i in range(across+1):v.append(at(j/steps,i/(across/2)-1))
    for j in range(steps):
        for i in range(across):
            a=j*(across+1)+i;f.append((a,a+1,a+across+2,a+across+1))
    b.mesh(v,f,family,c)
    if length>=.22:
        pts=[at(j/20,0,.0015) for j in range(21)]
        b.tube(pts,[min(.003,length*.004)]*21,family,5,tint(c,1.22))
        for t in [.22,.40,.58,.76]:
            for side in [-1,1]:
                pts=[at(t+j*.11/5,side*j*.87/5,.0014) for j in range(6)]
                b.tube(pts,[min(.0015,length*.0017)]*6,family,4,tint(c,1.1))
