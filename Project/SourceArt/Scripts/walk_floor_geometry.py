"""Disjoint convex stone/inlay patches for the shared water-city walk plan.

Centimetres in, centimetres out. Pure Python so Blender and offline audits use
the same deterministic authoring operation. Collision paths are not changed.
"""
import math

EPS = 1e-7
SURFACE_LIFT_CM = 1.0


def area(poly):
    return sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(poly,poly[1:]+poly[:1]))*.5


def clean(poly):
    out=[]
    for p in poly:
        if not out or math.dist(out[-1],p)>EPS:out.append(p)
    if len(out)>1 and math.dist(out[0],out[-1])<=EPS:out.pop()
    if len(out)<3 or abs(area(out))<1e-5:return []
    return out if area(out)>0 else list(reversed(out))


def clip(poly,a,b,inside=True):
    sign=1 if inside else -1
    def side(p):return sign*((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]))
    out=[]
    for p,q in zip(poly,poly[1:]+poly[:1]):
        dp,dq=side(p),side(q)
        pin,qin=dp>=0,dq>=0
        if pin:out.append(p)
        if pin!=qin:
            t=dp/(dp-dq)
            out.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
    return clean(out)


def subtract(poly,cut):
    """Partition poly minus a convex CCW cut into non-overlapping patches."""
    remaining=poly;out=[]
    for a,b in zip(cut,cut[1:]+cut[:1]):
        if not remaining:break
        outside=clip(remaining,a,b,False)
        if outside:out.append(outside)
        remaining=clip(remaining,a,b)
    return out


def subtract_all(polys,cuts):
    result=polys
    for cut in cuts:
        result=[p for poly in result for p in subtract(poly,cut)]
    return result


def union_patches(polys):
    out=[];occupied=[]
    for poly in polys:
        out.extend(subtract_all([poly],occupied));occupied.append(poly)
    return out


def rectangle(a,b,width,end_pad=0,offset=0):
    dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
    ux,uy=dx/length,dy/length;nx,ny=-uy,ux
    return [(a[0]+ux*t+nx*s,a[1]+uy*t+ny*s)
            for t,s in [(-end_pad,offset-width/2),(length+end_pad,offset-width/2),
                        (length+end_pad,offset+width/2),(-end_pad,offset+width/2)]]


def floor_patches(walks):
    levels=[]
    for z in sorted({w['a'][2] for w in walks}):
        group=[w for w in walks if w['a'][2]==z]
        assert all(w['b'][2]==z and w['thickness']==group[0]['thickness'] for w in group)
        decks=[rectangle(w['a'],w['b'],w['width'],w['end_pad']) for w in group]
        inlays=[rectangle(w['a'],w['b'],20,w['end_pad'],side*(w['width']/2-16))
                for w in group for side in (-1,1)]
        floor=union_patches(decks)
        levels.append({'floor_z':z,'top_z':z+SURFACE_LIFT_CM,'bottom_z':z-group[0]['thickness'],
                       'decks':decks,'inlays':inlays,'stone':subtract_all(floor,inlays),
                       'copper':union_patches(inlays),'union_area_cm2':sum(map(area,floor))})
    return levels


def contains(poly,point,tolerance=EPS):
    return all((b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0])>=-tolerance
               for a,b in zip(poly,poly[1:]+poly[:1]))
