"""Shared original 3D architecture and landscape primitives for vertical worlds."""
from pathlib import Path
import runpy, math, random, json, time
import bpy
from mathutils import Vector, Matrix

g=runpy.run_path(str(Path(__file__).with_name("mesh_primitives.py")))
Builder,export,column,fern=g["Builder"],g["export"],g["column"],g["fern"]
OUT,CATALOG=g["OUT"],g["CATALOG"]
IVORY=(.94,.94,.88,1)
PALE=(.82,.89,.89,1)
BLUE=(.28,.47,.60,1)
TEAL=(.20,.48,.45,1)
GREEN=(.32,.55,.29,1)
GOLD=(.58,.42,.21,1)


def band(b,center,radius,width,start=0,end=math.tau,steps=100,thickness=.45,family="Limestone",colour=IVORY):
    """A horizontal curved terrace with a real top, underside, and inner edge."""
    v=[];f=[]
    for i in range(steps+1):
        a=start+(end-start)*i/steps
        for z in [-thickness,0]:
            for r in [radius-width*.5,radius+width*.5]:
                v.append((math.cos(a)*r,math.sin(a)*r,z))
    for i in range(steps):
        p=i*4;q=p+4
        f.extend([(p+2,p+3,q+3,q+2),(p,q,q+1,p+1),(p,p+2,q+2,q),(p+1,q+1,q+3,p+3)])
    f.extend([(0,1,3,2),(steps*4,steps*4+2,steps*4+3,steps*4+1)])
    b.mesh(v,f,family,colour,center)


def rail(b,center,radius,start=0,end=math.tau,height=1.1,spacing=1.3,colour=IVORY):
    x,y,z=center
    count=max(2,int((end-start)*radius/spacing))
    for h in [.18,height]:
        ps=[(x+math.cos(start+(end-start)*i/count)*radius,y+math.sin(start+(end-start)*i/count)*radius,z+h) for i in range(count+1)]
        b.tube(ps,[.07 if h<.3 else .1]*(count+1),"Limestone",8,colour)
    for i in range(count+1):
        a=start+(end-start)*i/count
        b.lathe([(.09,0),(.09,.16),(.045,.3),(.075,.58),(.045,.88),(.07,height)],
                (x+math.cos(a)*radius,y+math.sin(a)*radius,z),segments=12,colour=colour)


def island(b,p,radius,depth,seed=0):
    rng=random.Random(seed);phases=[rng.random()*math.tau for _ in range(4)]
    v=[];f=[];sides=80;rings=23
    for j in range(rings):
        t=j/(rings-1);z=-depth*(1-t)
        for i in range(sides):
            a=i*math.tau/sides
            r=radius*(.055+.945*t**.48)*(1+.07*math.sin(a*5+phases[0])+.045*math.cos(a*9+t*5+phases[1]))
            v.append((math.cos(a)*r,math.sin(a)*r*.82,z))
    for j in range(rings-1):
        for i in range(sides):
            a=j*sides+i;c=j*sides+(i+1)%sides
            f.append((a,c,c+sides,a+sides))
    v.append((0,0,0));mid=len(v)-1
    for i in range(sides):f.append((mid,(rings-1)*sides+i,(rings-1)*sides+(i+1)%sides))
    b.mesh(v,f,"Limestone",(.58,.64,.62,1),p)
    for k in range(7):
        a=k*2.399+phases[2]
        ps=[(p[0]+math.cos(a)*(radius*.8-t*.11),p[1]+math.sin(a)*(radius*.66-t*.09),p[2]-t*depth/22) for t in range(20)]
        b.tube(ps,[.32*(1-t/23)+.045 for t in range(20)],"Limestone",12,(.68,.72,.70,1))


def ivy(b,p,length,seed=0,flower=False):
    rng=random.Random(seed)
    for j in range(6):
        x=p[0]+(j-2.5)*.30;y=p[1];h=length*rng.uniform(.55,1)
        ps=[(x+.16*math.sin(t*.6+j),y+.07*math.cos(t*.3),p[2]-t*h/16) for t in range(17)]
        b.tube(ps,[.012]*17,"Bark",6)
        for t in range(1,16):
            for side in [-1,1]:
                b.leaf(ps[t],.35,.24,side*1.3+j*.45,-.3,"Foliage",(.25+.04*(j%3),.48+.035*(t%3),.29,1))
            if flower and t%3==0:
                for k in range(5):b.leaf(ps[t],.20,.14,k*math.tau/5,.3,"Petal",(.86,.65,.54,1))


def window(b,p,width=1.1,height=2.0,yaw=0,tint=BLUE):
    x,y,z=p
    def box(q,d,fam="Limestone",colour=IVORY,bevel=.025):
        b.box(Vector(p)+Matrix.Rotation(yaw,3,"Z")@Vector(q),d,fam,bevel,yaw,colour)
    box((0,.04,height*.5),(width+.22,.16,height+.24),"Limestone",(.72,.79,.78,1))
    box((0,-.07,height*.5),(width,.10,height),"Paint",tint)
    for s in [-1,1]:box((s*(width*.5+.1),-.14,height*.5),(.16,.25,height+.32))
    for h in [0,height*.48,height]:box((0,-.145,h),(width+.36,.27,.10))
    box((0,-.16,height*.5),(.055,.12,height),"Bronze",(.48,.56,.55,1),.008)
    box((0,-.15,-.14),(width+.56,.42,.16))
    box((0,-.12,height+.20),(width+.6,.34,.12))


def pavilion(b,p,width=10,depth=8,floors=2,yaw=0,variant=0):
    """Narrow terraces, recessed windows, roofline and open ground arcade."""
    temp=Builder();H=floors*4.4
    temp.box((0,depth*.16,H*.5),(width,depth*.66,H),"Limestone",.10,colour=IVORY)
    for level in range(floors):
        z=level*4.4
        temp.box((0,0,z-.12),(width+.7,depth+.7,.26),"Limestone",.045,colour=IVORY)
        count=max(2,int(width/2.7));step=width/count
        for j in range(count):
            x=-width*.5+(j+.5)*step
            window(temp,(x,-depth*.175-.1,z+.75),min(1.4,step*.5),2.1,tint=BLUE if (j+variant)%2 else TEAL)
            column(temp,(x-step*.5,-depth*.5+.1,z),3.5,.15)
            temp.arc((x,-depth*.5,z+2.45),step*.45,.16,depth=.36,segments=24)
        column(temp,(width*.5,-depth*.5+.1,z),3.5,.15)
        temp.box((0,-depth*.38,z+3.9),(width+.8,depth*.40,.28),"Limestone",.045,colour=IVORY)
        for j in range(count*2+1):
            temp.box((-width*.5+j*width/(count*2),-depth*.60,z+.65),(.055,.065,1.15),"Bronze",.008,colour=GOLD)
        for h in [.15,1.22]:temp.box((0,-depth*.60,z+h),(width+.15,.10,.10),"Limestone",.015,colour=IVORY)
        if (level+variant)%2==0:
            for j in range(3):ivy(temp,((j-1)*width*.28,-depth*.60,z+.26),2.8,j+variant*10,True)
    temp.box((0,0,H+.03),(width+1.2,depth+1.2,.4),"Limestone",.07,colour=IVORY)
    if variant%3==0:
        for i in range(14):
            x=-width*.53+i*width*1.06/13
            z=H+.5+1.1*(1-abs(x)/(width*.57))
            temp.box((x,0,z),(width*1.06/13+.05,depth+1.5,.16),"Paint",.025,colour=TEAL)
    elif variant%3==1:
        temp.lathe([(0,0),(2.2,0),(2.25,.25),(2.1,.5),(1.8,1.7),(1.2,2.6),(0,3.0)],(0,0,H+.2),"Paint",64,TEAL)
        for x in [-width*.4,width*.4]:temp.box((x,0,H+1),(.55,depth+.5,1.7),"Limestone",.04,colour=IVORY)
    else:
        for x in [-width*.35,width*.35]:
            for y in [-depth*.35,depth*.35]:column(temp,(x,y,H+.2),3,.12)
        temp.box((0,0,H+3.3),(width+1,depth+1,.25),"Limestone",.05,colour=IVORY)
    rotation=Matrix.Rotation(yaw,3,"Z")
    # Keep material families and vertex colors when placing a compound object.
    merge(b,temp,p,rotation)


def merge(b,temp,p=(0,0,0),rotation=None):
    offset=len(b.v);anchor=Vector(p)
    b.v.extend(tuple((rotation@Vector(v) if rotation else Vector(v))+anchor) for v in temp.v)
    b.f.extend(tuple(offset+i for i in f) for f in temp.f)
    b.m.extend(temp.m);b.col.extend(temp.col);b.smooth.extend(temp.smooth)


def bridge(b,a,c,width=3.5,sag=3.0,family="Limestone"):
    a,c=Vector(a),Vector(c);axis=(c-a).normalized();side=Vector((-axis.y,axis.x,0)).normalized()
    steps=max(10,int((c-a).length/1.4));v=[];f=[];ps=[]
    for i in range(steps+1):
        t=i/steps;p=a.lerp(c,t)-Vector((0,0,math.sin(math.pi*t)*sag));ps.append(p)
        for h in [-.35,0]:
            for s in [-1,1]:v.append(tuple(p+side*width*.5*s+Vector((0,0,h))))
    for i in range(steps):
        k=i*4;n=k+4
        f.extend([(k+2,k+3,n+3,n+2),(k,n,n+1,k+1),(k,k+2,n+2,n),(k+1,n+1,n+3,k+3)])
    b.mesh(v,f,family,IVORY if family=="Limestone" else (.52,.40,.24,1))
    for s in [-1,1]:
        hand=[p+side*(width*.5-.06)*s+Vector((0,0,1.2)) for p in ps]
        b.tube(hand,[.085]*len(hand),"Bronze",8,GOLD)
        for i in range(0,len(ps),2):
            p=ps[i]+side*(width*.5-.06)*s
            b.tube([p,p+Vector((0,0,1.2))],[.06,.045],"Limestone",8,IVORY)


