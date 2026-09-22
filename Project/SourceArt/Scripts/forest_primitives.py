"""Shared original 3D forest geometry; importing does not export named assets."""
from pathlib import Path
import runpy,math,random,json,time
import bpy
from mathutils import Vector,Matrix

shared=runpy.run_path(str(Path(__file__).with_name("vertical_primitives.py")))
globals().update({key:value for key,value in shared.items() if not key.startswith("__")})
TAU=math.tau
BARK=(.56,.46,.31,1)
WOOD=(.60,.43,.23,1)


def bezier(a,b,c,d,steps=24):
    a,b,c,d=map(Vector,(a,b,c,d))
    return [a*(1-t)**3+b*(3*(1-t)**2*t)+c*(3*(1-t)*t*t)+d*t**3
            for t in [j/steps for j in range(steps+1)]]


def mix(a,b,t):return tuple(a[i]*(1-t)+b[i]*t for i in range(4))


def blade(b,p,length,width,yaw,tilt,colour):
    """Small folded leaf with a curved midrib; actual surface, no billboard."""
    v=[];f=[]
    for j in range(5):
        t=j/4;w=max(.008,math.sin(t*math.pi)**.8)*width*.5
        for s in [-1,0,1]:
            v.append((s*w,t*length,length*(.12*math.sin(t*math.pi)+abs(s)*.055)))
    for j in range(4):
        k=j*3
        f.extend([(k,k+1,k+4,k+3),(k+1,k+2,k+5,k+4)])
    b.mesh(v,f,"Foliage",colour,p,Matrix.Rotation(yaw,3,"Z")@Matrix.Rotation(tilt,3,"X"))


def crown(b,p,rx,ry,rz,seed,palette,count=290):
    """Layer individual leaves through a loose crown with gaps and fine twigs."""
    rng=random.Random(seed);p=Vector(p);yaw=rng.uniform(0,TAU);rotation=Matrix.Rotation(yaw,3,"Z")
    phases=[rng.uniform(0,TAU) for _ in range(3)]
    if rz>1:
        for j in range(9):
            a=j*2.399963+phases[0]
            tip=p+rotation@Vector((math.cos(a)*rx*.78,math.sin(a)*ry*.78,rng.uniform(-.15,.5)*rz))
            stem=bezier(p-Vector((0,0,.4)),p+(tip-p)*.25+Vector((0,0,.7)),
                        tip-Vector((.3,.5,.2)),tip,8)
            b.tube(stem,[.09*(1-k/10)+.009 for k in range(9)],"Bark",7,BARK)
    # No solid ellipsoid under the canopy: both its silhouette and interior
    # shading come from overlapping curved leaves, including at grazing angles.
    for j in range(max(180,count*4)):
        a=j*2.399963+phases[1];u=rng.uniform(-.92,1);rad=math.sqrt(1-u*u)
        r=rng.uniform(.24,1.12)**.6*(1+.13*math.sin(a*3+phases[0])+.05*math.cos(a*5+phases[2]))
        at=p+rotation@Vector((math.cos(a)*rx*rad*r,math.sin(a)*ry*rad*r,u*rz*r))
        colour=palette[rng.randrange(len(palette))]
        blade(b,at,rng.uniform(.82,1.38),rng.uniform(.43,.76),a+yaw+rng.uniform(-.5,.5),rng.uniform(-1.05,1.05),colour)


def earth_island(b,p,radius,depth,seed,palette):
    p=Vector(p);rng=random.Random(seed);sides=72;rings=20;phase=rng.random()*TAU
    vertices=[];faces=[]
    for j in range(rings):
        t=j/(rings-1)
        for k in range(sides):
            a=k*TAU/sides
            r=radius*(.04+.96*t**.55)*(1+.09*math.sin(a*5+phase)+.035*math.cos(a*11+t*9))
            vertices.append((math.cos(a)*r,math.sin(a)*r*.86,-depth*(1-t)))
    for j in range(rings-1):
        for k in range(sides):
            a=j*sides+k;c=j*sides+(k+1)%sides;faces.append((a,c,c+sides,a+sides))
    b.mesh(vertices,faces,"Limestone",(.42,.48,.38,1),p)
    rim=vertices[-sides:];top=[(0,0,.06)]+[(v[0],v[1],.06) for v in rim]
    b.mesh(top,[(0,k+1,(k+1)%sides+1) for k in range(sides)],"Soil",(.59,.54,.35,1),p)
    for j in range(15):
        a=j*2.399963;r=radius*rng.uniform(.36,.91)
        at=p+Vector((math.cos(a)*r,math.sin(a)*r*.86,.15))
        crown(b,at,rng.uniform(1.9,3.4),rng.uniform(1.4,2.6),.28,seed*30+j,palette,50)
        if j%2==0:fern(b,at,1.7+rng.random(),seed+j)


def walkway(b,points,width=2.6):
    """Continuous boardwalk, edge rails and posts on both sides."""
    ps=[Vector(p) for p in points];v=[];f=[];sides=[]
    for i,p in enumerate(ps):
        tangent=ps[min(len(ps)-1,i+1)]-ps[max(0,i-1)]
        side=Vector((-tangent.y,tangent.x,0)).normalized();sides.append(side)
        for z in [-.24,0]:
            for sign in [-1,1]:v.append(tuple(p+side*width*.5*sign+Vector((0,0,z))))
    for i in range(len(ps)-1):
        k=i*4;n=k+4
        f.extend([(k+2,k+3,n+3,n+2),(k,n,n+1,k+1),(k,k+2,n+2,n),(k+1,n+1,n+3,k+3)])
    b.mesh(v,f,"Timber",WOOD)
    for sign in [-1,1]:
        hand=[p+side*(width*.5-.05)*sign+Vector((0,0,1.13)) for p,side in zip(ps,sides)]
        b.tube(hand,[.075]*len(hand),"Timber",8,WOOD)
        for i in range(0,len(ps),3):
            foot=ps[i]+sides[i]*(width*.5-.08)*sign
            b.tube([foot,foot+Vector((0,0,1.13))],[.065,.048],"Timber",8,WOOD)
    for i in range(0,len(ps),2):
        a=ps[i]-sides[i]*width*.48+Vector((0,0,.008))
        c=ps[i]+sides[i]*width*.48+Vector((0,0,.008))
        b.tube([a,c],[.018,.018],"Bark",5,(.25,.20,.13,1))


