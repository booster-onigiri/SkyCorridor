"""Original art library: asymmetric trees, weathered landforms and layered architecture.

Run with Blender 4.5 in background mode. Everything is authored here; no online
generation, purchased asset dependency or reference-image pixels are required.
"""
from pathlib import Path
import runpy, math, random, json, time, bpy
from mathutils import Vector, Matrix

g=runpy.run_path(str(Path(__file__).with_name("build_kit.py")))
Builder,export,column,fern=g["Builder"],g["export"],g["column"],g["fern"]
ROOT,OUT,CATALOG=g["ROOT"],g["OUT"],g["CATALOG"]

def rounded_rock(b,p,radius,height,seed=1,colour=(.48,.52,.48,1),family="Limestone",sides=96):
    rng=random.Random(seed); phase=[rng.uniform(0,math.tau) for _ in range(6)]
    v=[];f=[];rings=25
    for j in range(rings):
        t=j/(rings-1); z=-height*(1-t)
        taper=.10+.90*math.sin(t*math.pi*.5)**.72
        for i in range(sides):
            a=i*math.tau/sides
            ridge=(.055*math.sin(a*5+phase[0])+ .04*math.sin(a*11+phase[1]+t*2)+.025*math.sin(a*21+phase[2]))
            shelf=.045*math.sin(t*45+phase[3])*(.3+.7*t)
            r=radius*(taper+ridge+shelf)
            v.append((math.cos(a)*r, math.sin(a)*r*(.75+.08*math.sin(a*3+phase[4])),z))
    for j in range(rings-1):
        for i in range(sides):
            a=j*sides+i;c=j*sides+(i+1)%sides
            f.append((a,c,c+sides,a+sides))
    v.append((0,0,0));mid=len(v)-1
    for i in range(sides):f.append((mid,(rings-1)*sides+i,(rings-1)*sides+(i+1)%sides))
    b.mesh(v,f,family,colour,p)

def foliage_cloud(b,center,radius,count,seed,palette,willow=False):
    rng=random.Random(seed);center=Vector(center)
    for i in range(count):
        a=rng.uniform(0,math.tau);u=rng.uniform(-1,1);q=math.sqrt(max(0,1-u*u))
        r=radius*rng.uniform(.45,1)**.4
        p=center+Vector((math.cos(a)*q*r,math.sin(a)*q*r,u*r*.45))
        colour=palette[i%len(palette)]
        size=rng.uniform(.5,1.25)
        b.leaf(p,size,size*(.12 if willow else .5),a,rng.uniform(-.8,.8),"Foliage",colour)

for variant,name in enumerate(["Tree_Willow","Tree_Fan","Tree_Blossom","Tree_Spiral"]):
    b=Builder();rng=random.Random(7440+variant)
    height=[29,42,25,48][variant];width=[17,13,19,10][variant]
    points=[(math.sin(t*.21+variant)*1.5,math.sin(t*.12)*.9,t) for t in range(height)]
    b.tube(points,[3.1*(1-t/(height+3))**.65+.18 for t in range(height)],"Bark",72,ribs=.10)
    for k in range(12):
        a=k*2.399+variant;z=height*(.34+.042*k);length=width*(.8+.2*math.sin(k*3))
        points=[]
        for j in range(23):
            t=j/22
            points.append((math.cos(a)*length*t,math.sin(a)*length*t,z+6*math.sin(t*1.8)- (5*t*t if variant==0 else 0)))
        b.tube(points,[.95*(1-j/25)**1.6+.035 for j in range(23)],"Bark",32,ribs=.08)
        tip=Vector(points[-1])
        if variant==2:
            palette=[(.55,.65,.37,1),(.69,.72,.47,1),(.48,.61,.32,1)]
        elif variant==3:
            palette=[(.14,.43,.38,1),(.20,.53,.45,1),(.31,.58,.46,1)]
        else:palette=[(.16,.40,.17,1),(.27,.53,.22,1),(.37,.60,.27,1)]
        foliage_cloud(b,tip,4.5+variant%2,470,500+variant*17+k,palette,variant==0)
        if variant==0:
            for j in range(12):
                a2=j*math.tau/12;drop=4.5+rng.random()*5
                start=tip+Vector((math.cos(a2)*3.5,math.sin(a2)*3.5,0))
                ps=[start+Vector((math.sin(t*.35)*.25,math.cos(t*.4)*.2,-t*drop/12)) for t in range(13)]
                b.tube(ps,[.04*(1-t/15) for t in range(13)],"Bark",8)
                for t in range(1,13):
                    for side in [-1,1]:b.leaf(ps[t],.55,.11,a2+side,1.1,"Foliage",palette[j%3])
        if variant==2:
            for j in range(110):
                a2=j*2.399;r=math.sqrt(j/110)*4.3
                p=tip+Vector((math.cos(a2)*r,math.sin(a2)*r,rng.uniform(-1.1,1.1)))
                for k2 in range(5):b.leaf(p,.26,.22,k2*math.tau/5,.3,"Petal",(.82,.57+.05*(j%3),.57+.035*(j%4),1))
    for j in range(12):
        a=j*math.tau/12
        ps=[(math.cos(a)*(2+12*t/14),math.sin(a)*(2+12*t/14),1.8*(1-t/14)**2-1.0) for t in range(15)]
        b.tube(ps,[1.2*(1-t/17)**1.4+.05 for t in range(15)],"Bark",32,ribs=.07)
    export(name,b,"Distinct branching grammar and leaf silhouette; curved surfaces, local colour variation")

for variant in range(4):
    b=Builder()
    rounded_rock(b,(0,0,0),20+variant*1.3,22+variant*7,92+variant,(.42,.48,.39,1))
    for j in range(9):
        a=j*2.399
        pts=[(math.cos(a)*(18-t*.20),math.sin(a)*(16-t*.16),-.3-t*1.2) for t in range(20)]
        b.tube(pts,[.7*(1-t/22)+.06 for t in range(20)],"Bark",24,ribs=.09)
    for j in range(14):
        a=j*2.399;r=16+2*math.sin(j)
        fern(b,(math.cos(a)*r,math.sin(a)*r,.03),1.3,j+variant*51)
    export("RootIsland_"+str(variant),b,"Eroded organic island rim, exposed roots and ferns")

for variant in range(5):
    b=Builder();rng=random.Random(815+variant)
    rounded_rock(b,(0,0,0),3.8+variant*.5,2.5+variant*.4,70+variant,(.35+.015*variant,.39,.44+.02*variant,1))
    for j in range(18+variant*5):
        a=j*2.399;r=math.sqrt(j/(18+variant*5))*4
        h=rng.uniform(1.5,8.5)
        if variant==1:h=2.5+.9*j
        if variant==2:h=4.8-3.4*r/4
        if variant==3:h=2+2.5*(1+math.sin(a*2))
        rad=rng.uniform(.18,.75)
        p=Vector((math.cos(a)*r,math.sin(a)*r,-.1))
        temp=Builder();temp.crystal((0,0,0),rad,h,5+j%5,a,(.23+.015*(j%7),.43+.016*(j%5),.54+.02*(j%4),1))
        rotation=Matrix.Rotation((.12+.035*variant)*math.sin(j),3,"X") @ Matrix.Rotation(.24*math.cos(j),3,"Y")
        b.mesh(temp.v,temp.f,"Crystal",(.23+.015*(j%7),.43+.016*(j%5),.54+.02*(j%4),1),p,rotation,smooth=False)
        # Thin mineral seams, interrupted and offset instead of one regular prism pattern.
        for band in range(3):
            z=h*(.2+band*.16)
            b.tube([(p.x+rad*.9*math.cos(a+t*.1),p.y+rad*.9*math.sin(a+t*.1),z+t*.015) for t in range(30)],
                   [.014]*30,"Bronze",6,colour=(.35,.40,.42,1))
    export("CrystalGarden_"+str(variant),b,"Mineral clusters with distinct growth patterns, inclusions and weathered bases")

for variant in range(5):
    b=Builder();rng=random.Random(491+variant)
    for j in range(3+variant):
        x=rng.uniform(-9,9);y=rng.uniform(-5,5);r=rng.uniform(2,6)
        rounded_rock(b,(x,y,rng.uniform(1,5)),r,rng.uniform(7,19),20+j+variant*20,(.44,.44+.02*variant,.46+.025*variant,1))
    if variant%2==0:
        b.arc((0,0,0),7+variant,2.2,depth=3.3,segments=112)
        for j in range(8):b.crystal((math.cos(j)*7,math.sin(j)*4,1),.3,1+j*.25,7,j,(.34,.49,.57,1))
    else:
        for j in range(12):
            a=j*2.399
            b.tube([(math.cos(a)*2,math.sin(a)*2,-.1),(math.cos(a)*3,math.sin(a)*3,3),(math.cos(a)*4,math.sin(a)*4,8+j*.3)],
                   [.6,.45,.1],"Limestone",24,colour=(.38,.39,.45,1),ribs=.045)
    export("WeatheredRock_"+str(variant),b,"Rounded strata and asymmetrical erosion, different readable skyline")

for variant in range(4):
    b=Builder()
    for j in range(32):
        a=j*2.399;r=math.sqrt(j/32)*3.1;h=.4+(j%7)*.11
        p=(math.cos(a)*r,math.sin(a)*r,0)
        b.tube([p,(p[0]+.1,p[1],h*.7),(p[0]+.14,p[1]+.07,h)],[.018,.012,.007],"Foliage",8)
        for k in range(3):b.leaf((p[0],p[1],h*.25),.6,.17,k*2.1+a,.6,"Foliage",(.23,.46,.27,1))
        for k in range(6+variant):
            b.leaf((p[0]+.14,p[1]+.07,h),.23+.04*variant,.18,k*math.tau/(6+variant),.1,
                   "Petal",[(.76,.62,.53,1),(.64,.63,.80,1),(.82,.74,.51,1),(.65,.77,.74,1)][variant])
    export("Wildflower_"+str(variant),b,"Regional flowers, layered petals and stems")

for variant in range(4):
    b=Builder();height=26+variant*12
    rounded_rock(b,(0,0,-.3),13,32+variant*7,661+variant,(.57,.58,.52,1))
    column(b,(0,0,0),height,2.2)
    for level in range(3+variant):
        z=8+level*8
        b.lathe([(0,z),(7+level*.9,z),(7+level*.9,z+.35),(0,z+.35)],segments=112)
        for k in range(12):
            a=k*math.tau/12
            column(b,(math.cos(a)*(6+level*.9),math.sin(a)*(6+level*.9),z+.35),5.7,.22)
            b.arc((0,0,z+3),8+level*.9,.16,start=a,end=a+.22,family="Bronze",depth=.15,segments=12)
        b.lathe([(0,z+6.1),(8+level*.9,z+6.1),(8+level*.9,z+6.4),(0,z+6.4)],segments=112)
    for j in range(9+variant*3):
        a=j*2.399;r=6+j*.50
        b.box((math.cos(a)*r,math.sin(a)*r,height+3+j*.4),(4.5+(j%4),2.7,.20),"Limestone",.06,yaw=a)
        b.tube([(math.cos(a)*4,math.sin(a)*4,height-4),(math.cos(a)*r,math.sin(a)*r,height+3+j*.4)], [.06,.035],"Bronze",12)
    export("SkyCitadel_"+str(variant),b,"Layered open galleries and fragmented crown, visible from multiple heights")

for variant in range(3):
    b=Builder();rng=random.Random(20+variant)
    rounded_rock(b,(0,0,0),18,44+variant*8,70+variant,(.64,.66,.61,1))
    for ring in range(3):
        radius=8+ring*3.4
        for j in range(16):
            a=j*math.tau/16
            column(b,(math.cos(a)*radius,math.sin(a)*radius,ring*4),6,.22)
        b.lathe([(radius-1.2,ring*4),(radius+1.2,ring*4),(radius+1.2,ring*4+.30),(radius-1.2,ring*4+.30)],segments=128)
    b.lathe([(0,10),(5.2,10),(5.3,10.4),(4.8,10.6),(4.3,13),(3,15),(0,16.5)],family="Paint",segments=112)
    for j in range(6):
        a=j*math.tau/6
        b.tube([(math.cos(a)*18,math.sin(a)*18,0),(math.cos(a)*18,math.sin(a)*18,-5),
                (math.cos(a)*17,math.sin(a)*17,-26),(math.cos(a)*15,math.sin(a)*15,-55)],
               [.55,.70,1.1,1.7],"Water",32,colour=(.45,.68,.72,1))
    export("CascadeTemple_"+str(variant),b,"Concentric hanging gardens and six falling water ribbons")

for variant in range(4):
    b=Builder()
    # Articulated balcony rather than a flat wall decal.
    b.box((0,0,.1),(3.8,1.5,.2),"Limestone",.04)
    for j in range(9):
        x=-1.75+j*.4375
        b.lathe([(0,0),(.07,0),(.055,.2),(.10,.40),(.06,.65),(.075,.9),(0,.94)],(x,-.65,.2),"Limestone",24)
    for z in [.22,1.1]:b.box((0,-.65,z),(3.9,.15,.14),"Limestone",.025)
    for x in [-1.55,1.55]:
        b.arc((x,.35,-.65),.7,.14,start=0,end=math.pi*.5,family="Limestone",depth=.25,segments=40)
    if variant%2:
        for j in range(10):
            b.tube([(-1.5+j*.33,-.65,.30),(-1.55+j*.33,-.72,-.3),(-1.4+j*.33,-.8,-1.5)], [.014,.016,.008],"Bark",8)
            for k in range(5):b.leaf((-1.5+j*.33,-.73,-k*.26),.35,.25,j*2+k,.2,"Foliage",(.27,.52,.27,1))
    if variant>1:
        for j in range(6):
            b.box((-1.5+j*.60,0,2.3+math.sin(j*.3)*.1),(.60,1.7,.025),"Cloth",.01,
                  colour=(.77,.58,.39,1) if j%2 else (.87,.82,.66,1))
    export("Balcony_"+str(variant),b,"Carved balustrade, corbels, climbing plants and cloth canopy")

for variant in range(4):
    b=Builder()
    for j in range(3+variant):
        x=(j-(2+variant)*.5)*2.3
        for y in [-1,1]:column(b,(x,y,0),3.6,.16)
        b.arc((x,0,2.55),.97,.18,depth=2.15,segments=56)
    b.box((0,0,3.8),((3+variant)*2.3,2.8,.20),"Limestone",.045)
    for j in range(7+variant):
        x=(j-(6+variant)*.5)*.72
        b.box((x,-.6,.82),(.62,.9,1.65),"Timber",.05)
        for k in range(12):
            b.box((x-.24+k*.043,-1.06,1.12),(.035,.25,.20+(k%4)*.055),"Paper",.008,
                  colour=(.38+.045*(k%5),.45+.035*(k%3),.48+.025*(k%4),1))
    export("Arcade_"+str(variant),b,"Human scale book and market arcade with open arches and individually bound books")

for variant in range(3):
    b=Builder()
    b.lathe([(0,0),(2.8,0),(2.9,.2),(2.6,.4),(2.35,.45),(2.1,.22),(0,.22)],segments=128)
    b.lathe([(0,.24),(2.2,.24)],family="Water",segments=128)
    for tier in range(3):
        h=tier*.8+.3;r=1.2-tier*.3
        b.lathe([(0,h),(.15,h),(.13,h+.45),(r,h+.55),(r,h+.63),(.10,h+.67)],family="Limestone",segments=96)
        for j in range(8):
            a=j*math.tau/8
            b.tube([(math.cos(a)*r,math.sin(a)*r,h+.63),(math.cos(a)*(r+.1),math.sin(a)*(r+.1),h+.3),
                    (math.cos(a)*(r+.15),math.sin(a)*(r+.15),max(.24,h-.45))],[.024,.036,.05],"Water",12)
    export("Fountain_"+str(variant),b,"Turned stone bowls, water and copper fittings")

catalog={"version":1,"units":"metres","unreal_units":"centimetres","axis":"Blender Z up / Unreal Z up",
         "materials":g["FAMILIES"],"assets":CATALOG,"triangles":sum(a["triangles"] for a in CATALOG),
         "generated_seconds":round(time.time()-g["started"],3),"source_texture_origin":"aerial-city-native/SourceArt/Textures",
         "art_direction":"layered illustrated world; distinct silhouettes; regional palettes; original procedural source"}
(OUT/"kit-catalog.json").write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"EndlessWorldKit.blend"))
print("EW_ART_COMPLETE",len(CATALOG),catalog["triangles"],catalog["generated_seconds"],flush=True)
