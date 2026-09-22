"""Three quiet rooftop pools. Visible solids and collision share centimetre data.
Coordinates are Unreal-oriented metres; only export reflects Blender Y.
"""
from pathlib import Path
import bpy,runpy,json,math,hashlib
g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
(g['OUT']/'Generated').mkdir(exist_ok=True)
B=g['Builder'];OUT=g['OUT'];records=[];bodies=[]
families={'Pool90Stone':(.66,.71,.68,1),'Pool90Tile':(.19,.40,.44,1),
          'Pool90Water':(.035,.27,.32,1),'Pool90Glass':(.10,.29,.33,1),
          'Pool90Metal':(.43,.31,.15,1),'Pool90Fabric':(.53,.59,.55,1),'Pool90Light':(.40,.76,.81,1)}
for family,c in families.items():
    g['FAMILIES'].append(family);g['COLOURS'][family]=c
    m=bpy.data.materials.new('M_'+family);m.diffuse_color=c;g['MATERIALS'].append(m)
stone=B();details=B();water=B();glass=B()
def box(b,p,d,f='Pool90Stone',floor=False,solid=True,bevel=.015,kind='structure'):
    b.box(p,d,f,bevel,colour=families[f])
    if solid:bodies.append(dict(p=[v*100 for v in p],e=[v*50 for v in d],yaw=0,floor=floor,kind=kind))
def beam(a,b,r=.035):details.tube([a,b],[r,r],'Pool90Metal',12,families['Pool90Metal'])
def emit(name,b):
    bounds=[[min(v[i] for v in b.v)*100 for i in range(3)],[max(v[i] for v in b.v)*100 for i in range(3)]]
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(f)) for f in b.f]
    g['export'](name,b,'Reversible raised rooftop basin with paired stairs and matching collision')
    row=g['CATALOG'][-1];row['ue_bounds_cm']=bounds;row['used_materials']=[g['FAMILIES'][i] for i in sorted(set(b.m))];records.append(row)
# The old roof stays below the basin. Nobody falls through a cut-out or stands on water.
box(stone,(0,0,.10),(16,9,.16),'Pool90Tile',True,kind='basin floor')
for y in [-4.68,4.68]:
    box(stone,(0,y,1.22),(16.8,.36,2.44),'Pool90Stone',kind='basin side')
    box(stone,(0,y-.205*math.copysign(1,y),1.2),(16.0,.04,2.04),'Pool90Tile',solid=False)
    box(stone,(0,y,2.45),(16.9,.64,.12),floor=True,kind='coping')
    for x in [-5,0,5]:
        box(details,(x,y-.21*math.copysign(1,y),.9),(.34,.05,.11),'Pool90Metal',solid=False)
        box(details,(x,y-.24*math.copysign(1,y),.9),(.24,.025,.055),'Pool90Light',solid=False)
    # Dark drainage slot and closely spaced metal grates are geometry, not a flat decal.
    box(details,(0,y+.35*math.copysign(1,y),2.445),(16.3,.13,.025),'Pool90Metal',solid=False)
    for x in range(-80,81):box(details,(x*.1,y+.35*math.copysign(1,y),2.46),(.025,.15,.018),'Pool90Stone',solid=False,bevel=0)
# The distant glazed end reveals water depth against the skyline.
box(glass,(8.06,0,1.3),(.12,9.0,2.24),'Pool90Glass',kind='glazed end wall')
box(stone,(8.15,0,.15),(.44,9.75,.30),floor=True,kind='end plinth')
box(stone,(8.15,0,2.45),(.58,9.75,.12),floor=True,kind='end coping')
for y in [-3,-1.5,0,1.5,3]:box(details,(8.14,y,1.3),(.06,.05,2.24),'Pool90Metal',solid=False)
# West entrance: the rim is open only at a real two-metre stair.
for y in [-2.875,2.875]:
    box(stone,(-8.18,y,1.22),(.36,3.25,2.44),kind='entry wall')
    box(stone,(-8.18,y,2.45),(.70,3.25,.12),floor=True,kind='entry coping')
box(stone,(-8.58,0,1.2),(.84,2.4,2.4),floor=True,kind='entry landing')
for i in range(12):
    z=(i+1)*.20
    box(stone,(-13.60+(i+.5)*.40,0,z*.5),(.40,2.4,z),floor=True,kind='outside stair')
    top=2.4-(i+1)*.185
    box(stone,(-8.16+(i+.5)*.38,0,(top+.02)*.5),(.38,2.4,top-.02),'Pool90Tile',True,kind='inside stair')
    for x,t in [(-13.60+(i+.5)*.40,z),(-8.16+(i+.5)*.38,top)]:
        box(details,(x-.17,0,t+.004),(.026,1.87,.008),'Pool90Stone',solid=False,bevel=0)
for y in [-1.16,1.16]:
    points=[(-13.6,y,1.05),(-8.8,y,3.45),(-8.15,y,3.45),(-3.6,y,1.23)]
    for a,c in zip(points,points[1:]):beam(a,c)
    for x in [-13.4,-11.2,-8.9,-6.1,-3.8]:
        if x < -8.8:
            step=min(11,max(0,math.floor((x+13.6)/.4)));z=(step+1)*.2
            hand=1.05+(x+13.6)*.5
        else:
            step=min(11,max(0,math.floor((x+8.16)/.38)));z=2.4-(step+1)*.185
            hand=3.45+(x+8.15)*(1.23-3.45)/4.55
        beam((x,y,z-.02),(x,y,hand),.027)
    # Follow each tread and its sloping handrail; no tall invisible wall above low steps.
    for i in range(12):
        for x,top,run in [(-13.6+(i+.5)*.4,(i+1)*.2,.4),(-8.16+(i+.5)*.38,2.4-(i+1)*.185,.38)]:
            bodies.append(dict(p=[x*100,y*100,(top+.525)*100],e=[run*50,4,52.5],yaw=0,floor=False,kind='stair guard'))
    bodies.append(dict(p=[-858,y*100,292.5],e=[42,4,52.5],yaw=0,floor=False,kind='landing guard'))
water.mesh([(-8,-4.5,2.38),(8,-4.5,2.38),(8,4.5,2.38),(-8,4.5,2.38)],[(0,1,2,3)],'Pool90Water')
# Quiet signs of past use: two linen loungers and a folded towel, away from circulation.
for x in [-1.5,1.5]:
    box(details,(x,-6.1,.20),(.82,2.05,.32),'Pool90Stone',True,kind='lounger base')
    box(details,(x,-6.1,.395),(.76,1.98,.09),'Pool90Fabric',solid=False)
    box(details,(x,-6.78,.52),(.76,.40,.25),'Pool90Fabric',solid=False)
box(details,(-1.5,-5.8,.48),(.50,.40,.08),'Pool90Fabric',solid=False)
for x in [-2.15,2.15]:
    box(details,(x,-6.9,.22),(.32,.32,.44),'Pool90Metal',kind='lantern')
    box(details,(x,-6.9,.42),(.27,.27,.06),'Pool90Light',solid=False)
for name,b in [('Pool90Stone',stone),('Pool90Details',details),('Pool90Water',water),('Pool90Glass',glass)]:emit(name,b)
assert all(min(b['e'])>0 for b in bodies)
data=dict(assets=records,bodies=bodies,surface_cm=238,bottom_cm=18,depth_cm=220)
(OUT/'pool90-catalog.json').write_text(json.dumps(data,indent=2),encoding='utf8')
header='#pragma once\n#include "CoreMinimal.h"\nnamespace EWPool90Data {\nstruct Body { FVector P,E; double Yaw; bool Floor; };\ninline const TArray<Body>& Bodies(){ static const TArray<Body> B={\n'
for b in bodies:header+=' {FVector(%s),FVector(%s),%s,%s},\n'%(','.join(f'{v:.6f}' for v in b['p']),','.join(f'{v:.6f}' for v in b['e']),b['yaw'],'true' if b['floor'] else 'false')
header+='};return B;}\n}\n'
(OUT/'Generated/EWPool90Data.h').write_text(header,encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'pool90.blend'))
print('POOL90_GEOMETRY',len(records),len(bodies),flush=True)
