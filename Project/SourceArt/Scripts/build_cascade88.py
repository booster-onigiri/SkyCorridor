"""Original neo-Gothic glazed palaces. Reference background informs architecture only.
Metres, reproducible geometry; inset glazing, actual reveals, galleries and roof ribs.
"""
from pathlib import Path
import bpy, runpy, math, random, json
from mathutils import Matrix, Vector

g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
B=g['Builder']; OUT=g['OUT']; records=[]
families={'Cascade88Stone':(.58,.62,.63,1),'Cascade88Edge':(.72,.75,.72,1),
 'Cascade88Metal':(.075,.15,.16,1),'Cascade88Window':(.035,.17,.23,1),
 'Cascade88Roof':(.018,.075,.105,1),'Cascade88Glass':(.045,.19,.23,1),
 'Cascade88Interior':(.075,.115,.12,1),'Cascade88Gold':(.36,.24,.095,1),'Cascade88LitWindow':(.055,.16,.20,1)}
for family,c in families.items():
    g['FAMILIES'].append(family);g['COLOURS'][family]=c
    m=bpy.data.materials.new('M_'+family);m.diffuse_color=c;g['MATERIALS'].append(m)
STONE='Cascade88Stone';EDGE='Cascade88Edge';METAL='Cascade88Metal'
WINDOW='Cascade88Window';ROOF='Cascade88Roof';GLASS='Cascade88Glass';INNER='Cascade88Interior';GOLD='Cascade88Gold'

def box(b,p,d,f=STONE,bevel=.025,c=None):b.box(p,d,f,bevel,colour=c)
def beam(b,a,c,r=.08,f=EDGE):b.tube([a,c],[r,r],f,8)
def merge(b,a,p=(0,0,0),yaw=0):
    n=len(b.v);q=Matrix.Rotation(yaw,3,'Z');v=Vector(p)
    b.v.extend(tuple(q@Vector(x)+v) for x in a.v);b.f.extend(tuple(n+i for i in face) for face in a.f)
    b.m.extend(a.m);b.col.extend(a.col);b.smooth.extend(a.smooth)
def emit(name,b,note):
    bounds=[[min(v[i] for v in b.v)*100 for i in range(3)],[max(v[i] for v in b.v)*100 for i in range(3)]]
    used=[g['FAMILIES'][i] for i in sorted(set(b.m))]
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(face)) for face in b.f]
    g['export'](name,b,note);a=dict(g['CATALOG'][-1]);a['ue_bounds_cm']=bounds;a['used_materials']=used;records.append(a)

def column(b,x,y,z,h,r=.15,f=EDGE):
    b.lathe([(0,0),(r*1.7,0),(r*1.7,.13),(r*1.25,.24),(r,.36),(r*.9,h-.36),
       (r*1.35,h-.25),(r*1.65,h-.13),(r*1.65,h),(0,h)],(x,y,z),f,12)
def arch(b,x,y,z,w,h,r=.095,f=EDGE):
    for sign in [-1,1]:
        pts=[(x+sign*w*.5*(1-t**1.5),y,z+h*t) for t in [i/12 for i in range(13)]]
        b.tube(pts,[r]*len(pts),f,8)
def finial(b,x,y,z,h=4,r=.38):
    b.lathe([(0,0),(r*1.6,0),(r*1.6,.2),(r,.35),(r,h*.56),(r*.46,h*.76),(0,h)],(x,y,z),EDGE,8)
    beam(b,(x,y,z+h*.78),(x,y,z+h+1.2),.038,GOLD)
def window(b,x,y,z,w,h,seed=0):
    # Glass sits 38 cm behind the stone hood. Side reveals are geometry, and
    # glazing has separate metallic/roughness response from the limestone.
    rng=random.Random(seed);glass_y=y+.32;head=z+h*.72
    verts=[(x-w/2,glass_y,z),(x+w/2,glass_y,z),(x+w/2,glass_y,head)]
    verts += [(x+w*.5*(1-t**1.5),glass_y,head+h*.28*t) for t in [i/12 for i in range(1,13)]]
    verts += [(x-w*.5*(1-t**1.5),glass_y,head+h*.28*t) for t in [i/12 for i in range(11,-1,-1)]]
    c=(.026+rng.random()*.018,.12+rng.random()*.07,.18+rng.random()*.075,1)
    b.mesh(verts,[tuple(range(len(verts)))],'Cascade88LitWindow' if seed%11==3 else WINDOW,c,smooth=False)
    for s in [-1,1]:
        box(b,(x+s*(w*.5+.075),y+.13,z+h*.36),(.15,.42,h*.72),EDGE,.025)
        column(b,x+s*(w*.5+.12),y-.13,z-.12,h*.72+.12,.095)
    arch(b,x,y-.07,head,w+.20,h*.28+.1,.13)
    arch(b,x,glass_y-.04,head,w-.12,h*.28-.04,.052,METAL)
    box(b,(x,y-.07,z-.09),(w+.48,.70,.18),EDGE,.028)
    beam(b,(x,glass_y-.075,z),(x,glass_y-.075,z+h-.06),.041,METAL)
    for s in [-1,1]:
        arch(b,x+s*w*.245,glass_y-.08,z+h*.57,w*.45,h*.20,.035,METAL)
    for zz in [.33,.58]:box(b,(x,glass_y-.08,z+h*zz),(w,.075,.065),METAL,.006)
    # A sculpted keystone and small framed stone panel above the window.
    box(b,(x,y-.16,z+h+.15),(.25,.24,.30),EDGE,.03)

def balustrade(b,x,y,z,w):
    box(b,(x,y,z-.15),(w,1.12,.3),EDGE,.05)
    for h in [.16,1.0]:box(b,(x,y-.48,z+h),(w,.14,.13),EDGE,.018)
    for j in range(max(2,int(w/.42))+1):
        xx=x-w*.5+j*w/max(2,int(w/.42));column(b,xx,y-.48,z+.17,.78,.045)
    for s in [-1,1]:box(b,(x+s*w*.49,y,z+.58),(.12,1,.86),EDGE,.02)
def rose(b,x,y,z,r):
    pts=[(x+math.cos(a)*r,y,z+math.sin(a)*r) for a in [i*math.tau/48 for i in range(49)]]
    b.tube(pts,[.11]*len(pts),EDGE,8)
    for j in range(8):
        a=j*math.tau/8;beam(b,(x,y,z),(x+math.cos(a)*r,y,z+math.sin(a)*r),.045,METAL)
    for rr in [.35,.63]:
        pts=[(x+math.cos(a)*r*rr,y,z+math.sin(a)*r*rr) for a in [i*math.tau/32 for i in range(33)]]
        b.tube(pts,[.045]*len(pts),GOLD,8)

def roof(b,glass,base,half,height,style):
    # A glazed octagonal lantern with visible floor and setback interior core.
    sides=8;radius=half*.87;lantern=5.5+style
    b.lathe([(0,0),(radius+.42,0),(radius+.42,.3),(radius,.5)],(0,0,base),EDGE,8)
    box(b,(0,0,base+lantern*.46),(radius*.85,radius*.85,lantern*.88),INNER,.03)
    for i in range(sides):
        a=(i+.5)*math.tau/sides;c=math.cos(a)*radius;s=math.sin(a)*radius
        column(b,c,s,base+.3,lantern,.13)
        na=(i+1.5)*math.tau/sides;other=(math.cos(na)*radius,math.sin(na)*radius)
        glass.mesh([(c,s,base+.6),(other[0],other[1],base+.6),(other[0],other[1],base+lantern),(c,s,base+lantern)],[(0,1,2,3)],GLASS,smooth=False)
        beam(b,(c,s,base+lantern),(other[0],other[1],base+lantern),.18)
    tip=base+height
    rings=[(radius+1,base+lantern),(radius*.64,base+lantern+height*.14),(radius*.46,tip-height*.22),(.05,tip)]
    for j in range(len(rings)-1):
        r0,z0=rings[j];r1,z1=rings[j+1]
        for i in range(sides):
            a=(i+.5)*math.tau/sides;aa=(i+1.5)*math.tau/sides
            v=[(math.cos(a)*r0,math.sin(a)*r0,z0),(math.cos(aa)*r0,math.sin(aa)*r0,z0),
               (math.cos(aa)*r1,math.sin(aa)*r1,z1),(math.cos(a)*r1,math.sin(a)*r1,z1)]
            b.mesh(v,[(0,1,2,3)],WINDOW if j==1 else ROOF,smooth=False)
            beam(b,v[0],v[3],.075,METAL)
            for t in [.25,.5,.75]:beam(b,tuple(Vector(v[0]).lerp(Vector(v[3]),t)),tuple(Vector(v[1]).lerp(Vector(v[2]),t)),.035,METAL)
    finial(b,0,0,tip,3,.30)

for variant,height in enumerate([100.,138.,178.]):
    b=B();glass=B();bay_count=0
    stages=[(0,height*.42,11.8),(height*.42,height*.31,8.5),(height*.73,height*.16,5.55)]
    for stage,(base,h,half) in enumerate(stages):
        rows=max(2,round(h/4.7));rh=h/rows
        # Setback solid core and storey slabs behind the window line. No giant
        # white box immediately behind glazing: every bay has real depth.
        box(b,(0,0,base+h*.5),((half-1.15)*2,(half-1.15)*2,h),INNER,.03)
        for row in range(rows+1):
            z=base+row*rh
            box(b,(0,0,z-.18),(half*2+.52,half*2+.52,.36),STONE,.035)
            if row%3==0 or row==rows:
                box(b,(0,0,z+.08),(half*2+1.12,half*2+1.12,.20),EDGE,.04)
                box(b,(0,0,z-.4),(half*2+.78,half*2+.78,.16),EDGE,.025)
        for side in range(4):
            f=B();cols=6 if stage==0 else 4 if stage==1 else 3;pitch=(half*2-1.0)/cols
            for col in range(cols+1):
                x=-half+.5+col*pitch
                box(f,(x,-half-.03,base+h*.5),(.42,.68,h),STONE,.035)
                # Half columns cast longer shadows than a flat line or texture.
                column(f,x,-half-.45,base+.18,h-.3,.13)
                for row in range(1,rows,3):box(f,(x,-half-.51,base+row*rh-.56),(.62,.75,.64),EDGE,.05)
            for row in range(rows):
                z=base+row*rh+.52
                for col in range(cols):
                    x=-half+.5+(col+.5)*pitch
                    window(f,x,-half,z,pitch-.67,rh-.98,variant*10000+side*1000+row*10+col);bay_count+=1
                    if row%4==2 and col in [1,cols-2]:balustrade(f,x,-half-.7,z-.45,pitch-.15)
                    elif row%3==0:
                        box(f,(x,-half-.27,base+row*rh+.20),(pitch*.58,.22,.17),EDGE,.02)
            # Heavy corner buttresses terminate in miniature roof spires.
            for sign in [-1,1]:
                x=sign*(half+.18)
                box(f,(x,-half+.15,base+h*.5),(.76,1.42,h),STONE,.07)
                column(f,x,-half-.62,base+.2,h-.2,.18)
                finial(f,x,-half-.06,base+h+.25,3.8+stage*.5,.39)
            # A dentilled cornice is visible in silhouette at terrace level.
            for j in range(int(half*2/.48)):
                box(f,(-half+.24+j*.48,-half-.57,base+h-.52),(.22,.46,.28),EDGE,.024)
            merge(b,f,yaw=side*math.pi/2)
        if stage<2:
            next_half=stages[stage+1][2]
            for side in range(4):
                f=B();y=-half-.3
                for j in range(9):
                    x=-half+1.2+j*(half*2-2.4)/8
                    column(f,x,y,base+h+.28,1.2,.07)
                for z in [.45,1.52]:box(f,(0,y,base+h+z),(half*2-1.4,.16,.12),EDGE,.022)
                # Arched roof dormers set against the exposed sloping glass wing.
                for x in [-half*.59,half*.59]:
                    window(f,x,-half+.28,base+h+.4,2.05,3.35,variant*100+side)
                    beam(f,(x-1.35,-half+.16,base+h+3.2),(x,-half+.16,base+h+4.8),.15)
                    beam(f,(x,-half+.16,base+h+4.8),(x+1.35,-half+.16,base+h+3.2),.15)
                    finial(f,x,-half+.16,base+h+4.8,1.0,.13)
                v=[(-half+.25,-half+.45,base+h+.35),(half-.25,-half+.45,base+h+.35),
                   (next_half,-next_half-.5,base+h+3.2),(-next_half,-next_half-.5,base+h+3.2)]
                f.mesh(v,[(0,1,2,3)],WINDOW,smooth=False)
                for j in range(15):
                    t=j/14;beam(f,tuple(Vector(v[0]).lerp(Vector(v[1]),t)),tuple(Vector(v[3]).lerp(Vector(v[2]),t)),.065,METAL)
                merge(b,f,yaw=side*math.pi/2)
    if variant==1:
        # Paired lantern spires and a steep glazed connecting roof distinguish
        # this palace from the tall single-needle observatory.
        crown=B();crown_glass=B();roof(crown,crown_glass,0,3.55,18,0)
        for sign in [-1,1]:merge(b,crown,(sign*3.05,0,height*.89));merge(glass,crown_glass,(sign*3.05,0,height*.89))
    else:
        roof(b,glass,height*.89,5.55,height*.11+3,variant)
    if variant==2:
        for side in range(4):
            f=B();rose(f,0,-5.75,height*.89-3.2,1.55);merge(b,f,yaw=side*math.pi/2)
    # Flying supports catch warm dawn/side light outside the narrow top stage.
    for side in range(4):
        f=B()
        for sign in [-1,1]:
            pts=[(sign*(8.4-3.0*t),-8.4+3*t,height*.63+height*.15*math.sin(t*math.pi/2)) for t in [i/20 for i in range(21)]]
            f.tube(pts,[.22]*len(pts),EDGE,10)
        merge(b,f,yaw=side*math.pi/2)
    emit(f'Cascade88Palace{variant}',b,f'{bay_count} recessed lancet bays, storey cornices, balconies, dormers, octagonal glazed crown; {height:g}m design')
    emit(f'Cascade88Glass{variant}',glass,'Separate real translucent lantern glazing; opaque Nanite architecture remains independent')

# Station-specific glass canopy. Platform geometry/collision remains the proven
# station kit; this thin umbrella adds an identity without obstructing the doors.
b=B();glass=B()
for x in [-12,-6,0,6,12]:
    for y in [2.7,6.0]:column(b,x,y,2.95,1.4,.06,METAL)
    pts=[(x,2.5+i*3.8/20,4.05+.5*math.sin(i*math.pi/20)) for i in range(21)]
    b.tube(pts,[.065]*len(pts),METAL,8)
for j in range(8):
    y0=2.5+j*3.8/8;y1=2.5+(j+1)*3.8/8
    z0=4.05+.5*math.sin(j*math.pi/8);z1=4.05+.5*math.sin((j+1)*math.pi/8)
    glass.mesh([(-13,y0,z0),(13,y0,z0),(13,y1,z1),(-13,y1,z1)],[(0,1,2,3)],GLASS,smooth=False)
    beam(b,(-13,y0,z0),(13,y0,z0),.035,METAL)
emit('Cascade88StationRibs',b,'Delicate green bronze barrel-vault station canopy above the passenger clearance')
emit('Cascade88StationGlass',glass,'Separate curved glass station canopy')

# A carved source balcony replaces the plain two-metre band across the towers.
# Its exact water-bed and ten-metre north spill opening retain the 86 flow contract.
b=B();box(b,(0,0,-.75),(22,18,1.5),STONE,.08)
for z,w,d in [(-1.52,22.45,18.45),(-1.32,22.2,18.2),(-.18,22.4,18.4)]:
    box(b,(0,0,z),(w,d,.20),EDGE,.035)
for x in [-10.8,10.8]:box(b,(x,0,.16),(.4,18,.5),EDGE,.035)
box(b,(0,-8.8,.16),(22,.4,.5),EDGE,.035)
for x in [-8,8]:box(b,(x,8.8,.16),(5.6,.4,.5),EDGE,.035)
for side in range(4):
    f=B();half=11 if side%2==0 else 9;y=-9 if side%2==0 else -11
    for j in range(int((half*2-.8)/.65)):
        x=-half+.4+j*.65;box(f,(x,y-.18,-.43),(.25,.36,.48),EDGE,.023)
    for x in [-half*.74,half*.74]:
        beam(f,(x,y+.18,-1.45),(x,y+2.3,-4.25),.24,STONE)
        rose(f,x,y-.24,-.83,.37)
    merge(b,f,yaw=side*math.pi/2)
emit('Cascade88Cistern',b,'Carved stone cornices, bracket supports and bronze rosettes; original basin and spillway dimensions retained')

(OUT/'cascade88-catalog.json').write_text(json.dumps({'assets':records,'source':'build_cascade88.py','reference':'historical visual reference excluded from distribution; no image input'},indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'cascade88.blend'))
print('EW_CASCADE88_COMPLETE',len(records),sum(a['triangles'] for a in records),flush=True)
