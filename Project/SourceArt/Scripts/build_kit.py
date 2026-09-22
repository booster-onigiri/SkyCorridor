"""Rebuild the base modular kit from shared authoring primitives."""
from pathlib import Path
import runpy
globals().update(runpy.run_path(str(Path(__file__).with_name("mesh_primitives.py"))))

started=time.time()
for name,family in [("StoneDeck","Limestone"),("WoodDeck","Timber"),("RockDeck","Soil")]:
    b=Builder();b.box((0,0,-.14),(1,1,.28),family,.015)
    export(name,b,"1 x 1 metre top at Z=0; dedicated runtime floor collider")
b=Builder();b.box((0,0,0),(1,1,1),"Limestone",.012);export("Wall",b,"unit cube; positive dimensions only")
b=Builder();b.box((0,0,0),(1,1,.16),"Limestone",.02);export("Cornice",b)
b=Builder();column(b);export("Column",b,"base at Z=0; nominal height 4m")

for name,wood in [("StoneRail",False),("WoodRail",True)]:
    b=Builder();fam="Timber" if wood else "Limestone"
    for x in [-2,0,2]:
        b.box((x,0,.49),(.16,.18,.98),fam,.018)
        b.lathe([(0,0),(.13,0),(.13,.12),(0,.12)],(x,0,1),"Bronze" if wood else fam,24)
    for h in [.24,.94]:
        b.box((0,0,h),(4,.13,.13),fam,.015)
    for x in [-1.5,-1,-.5,.5,1,1.5]:
        if wood:b.box((x,0,.59),(.06,.06,.62),"Timber",.008)
        else:b.lathe([(.085,0),(.10,.05),(.045,.18),(.08,.3),(.05,.55),(.085,.63)],(x,0,.28),segments=24)
    export(name,b,"4m long rail with 1.1m top")

b=Builder()
for x in [-1.4,1.4]:column(b,(x,0,0),2.7,.13)
b.arc((0,0,2.55),1.37,.24,depth=.45)
for a in [math.pi*.1+i*math.pi*.8/8 for i in range(9)]:
    b.box((math.cos(a)*1.38,-.235,2.55+math.sin(a)*1.38),(.20,.08,.20),"Bronze",.014,yaw=a)
export("DoorArch",b)

b=Builder()
b.box((0,0,1.10),(1.65,.10,2.2),"Ceramic",.006,colour=(.18,.30,.31,1))
for x in [-.86,.86]:b.box((x,-.09,1.10),(.12,.2,2.4),"Limestone",.018)
for z in [0,1.1,2.2]:b.box((0,-.1,z),(1.83,.2,.10),"Limestone",.018)
for x in [-.4,0,.4]:b.box((x,-.11,1.1),(.035,.12,2.2),"Bronze",.005)
b.box((0,-.18,-.05),(2.05,.48,.18),"Limestone",.022)
export("Window",b)

for name in ["GableRoof","DomeRoof","FlatRoof"]:
    b=Builder()
    b.box((0,0,.05),(10.4,10.4,.24),"Limestone",.04)
    if name=="GableRoof":
        verts=[(-5.4,-5.4,0),(5.4,-5.4,0),(-5.4,5.4,0),(5.4,5.4,0),(0,-5.4,3),(0,5.4,3)]
        b.mesh(verts,[(0,1,4),(2,5,3),(0,4,5,2),(1,3,5,4),(0,2,3,1)],"Paint")
        for sign in [-1,1]:
            for row in range(16):
                x=sign*(row+.5)*5.4/16
                z=3*(1-abs(x)/5.4)+.045
                for col in range(25):
                    y=-5.25+col*.43
                    b.lathe([(0,0),(.075,0),(.09,.30),(.03,.36)],(x,y,z),"Paint",12)
        b.box((0,0,3.1),(.18,10.8,.18),"Bronze",.03)
    elif name=="DomeRoof":
        profile=[(0,0),(5.1,0)]
        for i in range(33):
            a=i*math.pi/2/32;profile.append((max(.01,5.1*math.cos(a)),.15+3.4*math.sin(a)))
        b.lathe(profile,family="Paint",segments=96)
        for i in range(16):
            a=i*math.tau/16
            pts=[(math.cos(a)*5.13*math.cos(k*math.pi/2/24),math.sin(a)*5.13*math.cos(k*math.pi/2/24),
                  .16+3.44*math.sin(k*math.pi/2/24)) for k in range(25)]
            b.tube(pts,[.035]*25,"Bronze",8)
        b.lathe([(0,0),(.18,0),(.18,.3),(.03,.8),(0,.83)],(0,0,3.55),"Bronze",32)
    else:
        for y in [-5,5]:b.box((0,y,.45),(10.2,.18,.75),"Limestone",.04)
        for x in [-5,5]:b.box((x,0,.45),(.18,10.2,.75),"Limestone",.04)
        b.box((0,0,.3),(4,4,.35),"Paint",.05)
    export(name,b,"10m roof footprint; Z=0 at cornice")

b=Builder()
for x in [-1.2,1.2]:b.box((x,0,1.2),(.10,.6,2.4),"Timber",.014)
for z in [0,.6,1.2,1.8,2.4]:b.box((0,0,z),(2.5,.6,.08),"Timber",.014)
b.box((0,.28,1.2),(2.5,.045,2.4),"Timber",.008)
for shelf in range(4):
    for i in range(16):
        w=.08+.018*(i%4);h=.36+.035*((i+shelf)%5);x=-1.09+i*.14
        b.box((x,-.04,shelf*.6+.07+h/2),(w,.35,h),"Paper",.004)
        c=[(.20,.32,.30,1),(.38,.19,.12,1),(.19,.23,.39,1),(.45,.36,.17,1)][(i+shelf)%4]
        for side in [-1,1]:b.box((x+side*(w/2+.006),-.04,shelf*.6+.07+h/2),(.012,.38,h+.03),"Leather",.003,colour=c)
        b.box((x,-.23,shelf*.6+.07+h/2),(w+.02,.026,h+.03),"Leather",.004,colour=c)
        for zz in [.12,.18,h-.07]:b.box((x,-.248,shelf*.6+.07+zz),(w*.75,.006,.009),"Bronze",.001)
export("Bookshelf",b)

b=Builder()
b.lathe([(0,0),(.30,0),(.30,.1),(.10,.2),(.07,.63),(.65,.7),(.67,.77),(0,.77)],family="Timber",segments=64)
for x in [-1,1]:
    b.box((x,0,.44),(.52,.52,.08),"Timber",.02)
    for xx in [-.21,.21]:
        for yy in [-.21,.21]:b.box((x+xx,yy,.22),(.055,.055,.44),"Timber",.009)
    b.box((x,.23,.69),(.55,.055,.55),"Timber",.015)
for x in [-.22,.22]:
    b.lathe([(0,0),(.13,0),(.14,.02),(.10,.035),(0,.035)],(x,0,.785),"Ceramic",48)
    b.lathe([(.045,0),(.055,.02),(.064,.13),(.061,.145),(.052,.14),(.046,.035)],(x,0,.82),"Ceramic",48)
b.box((0,.25,.80),(.32,.22,.03),"Paper",.005)
export("TableSet",b)

b=Builder()
b.lathe([(0,0),(.35,0),(.39,.1),(.45,.7),(.48,.72),(.48,.83),(.39,.84),(.37,.72),(0,.69)],
        family="Ceramic",segments=64)
b.lathe([(0,.70),(.37,.70)],family="Soil",segments=40)
fern(b,(0,0,.72),.8)
export("Planter",b)

b=Builder()
for x in [-1,1]:
    b.box((x,0,.25),(.15,.65,.5),"Bronze",.018)
    b.box((x,.25,.68),(.1,.1,.87),"Bronze",.012)
for y in [-.24,-.08,.08,.24]:b.box((0,y,.53),(2.5,.125,.085),"Timber",.012)
for z in [.75,.93,1.11]:b.box((0,.28,z),(2.5,.10,.13),"Timber",.012)
export("Bench",b)

b=Builder()
b.lathe([(0,0),(.23,0),(.24,.12),(.14,.2),(.07,.4),(.05,3.7),(.13,3.8),(.13,4.1)],family="Bronze",segments=40)
for x in [-.22,.22]:
    for y in [-.22,.22]:b.box((x,y,4.18),(.025,.025,.7),"Bronze",.004)
b.box((0,0,4.18),(.35,.35,.55),"Glow",.018,colour=(.91,.71,.33,1))
b.lathe([(0,0),(.39,0),(.42,.07),(.18,.35),(0,.4)],(0,0,4.56),"Bronze",40)
export("Lamp",b)

b=Builder();rng=random.Random(191)
segments=128
# A rounded, layered underside. Descending rings need outward winding.
profile=[]
for j in range(33):
    t=j/32
    radius=18+2.5*math.sin(math.pi*t)**.8*(1-.7*t)-13.5*t**1.2
    profile.append((-25*t,radius))
v=[];f=[]
for j,(z,r) in enumerate(profile):
    for i in range(segments):
        a=i*math.tau/segments
        ripple=1+.065*math.sin(a*7)+.038*math.sin(a*13+j*.8)+.014*math.sin(a*31+j*2)
        v.append((math.cos(a)*r*ripple,math.sin(a)*r*ripple,z+(.3*math.sin(a*11) if j>0 else 0)))
for j in range(len(profile)-1):
    for i in range(segments):
        a=j*segments+i;c=j*segments+(i+1)%segments;f.append((a,a+segments,c+segments,c))
f.extend([tuple(range(segments)),tuple(reversed([(len(profile)-1)*segments+i for i in range(segments)]))])
b.mesh(v,f,"Limestone",colour=(.63,.61,.52,1))
export("Island",b,"40m floating rock base; top Z=0")

b=Builder()
for level in range(5):
    b.box((0,0,level*4+2),(5.4-.3*level,5.4-.3*level,4),"Limestone",.09)
    b.box((0,0,(level+1)*4),(5.8-.3*level,5.8-.3*level,.25),"Limestone",.05)
    for x in [-1,1]:
        for y in [-1,1]:column(b,(x*(2.5-.15*level),y*(2.5-.15*level),level*4),4,.13)
for side in range(4):
    a=side*math.pi/2
    centre=Vector((0,-2.05,17))
    rot=Matrix.Rotation(a,3,"Z")
    # Clock discs and hands are real geometry, rotated to the four tower faces.
    disc=Builder();disc.lathe([(0,0),(1.4,0),(1.44,.06),(1.28,.12),(0,.13)],family="Bronze",segments=64)
    rr=rot @ Matrix.Rotation(math.pi/2,3,"X")
    b.mesh(disc.v,disc.f,"Bronze",position=rot@centre,rotation=rr)
    for i in range(12):
        t=i*math.tau/12
        pos=rot@Vector((math.sin(t)*1.12,-2.2,17+math.cos(t)*1.12))
        b.box(pos,(.10,.08,.25),"Bronze",.01,yaw=a)
    b.box(rot@Vector((0,-2.23,17.42)),(.085,.08,.85),"Bronze",.01,yaw=a)
    b.box(rot@Vector((.31,-2.25,17)),(.63,.08,.085),"Bronze",.01,yaw=a)
b.lathe([(0,0),(3.1,0),(3.1,.3),(2.8,.5),(2.4,1.5),(1.5,2.5),(.15,3.4),(0,3.5)],(0,0,20),"Paint",64)
export("ClockTower",b)

b=Builder()
for x in [-12,-8,-4,0,4,8,12]:
    column(b,(x,0,0),5,.25);column(b,(x,6,0),5,.25)
for y in [0,6]:b.box((0,y,5.2),(25,.8,.5),"Limestone",.06)
b.box((0,3,5.65),(26,8,.5),"Paint",.08)
for x in [-10,-6,-2,2,6,10]:b.arc((x,0,3.4),1.85,.3,depth=.6)
export("LibraryPavilion",b)

b=Builder()
v=[];f=[];segs=80;levels=64
for j in range(levels+1):
    t=j/levels;z=(t-.5)*30;r=max(.01,math.sin(math.pi*t)**.65)*4.4
    for i in range(segs):
        a=i*math.tau/segs
        v.append((z,math.cos(a)*r,math.sin(a)*r+7))
for j in range(levels):
    for i in range(segs):
        a=j*segs+i;c=j*segs+(i+1)%segs;f.append((a,c,c+segs,a+segs))
b.mesh(v,f,"Cloth",colour=(.89,.87,.72,1))
for t in [.15,.3,.5,.7,.85]:
    x=(t-.5)*30;r=math.sin(math.pi*t)**.65*4.43
    pts=[(x,math.cos(i*math.tau/80)*r,math.sin(i*math.tau/80)*r+7) for i in range(81)]
    b.tube(pts,[.035]*81,"Bronze",8)
b.box((0,0,.9),(11,2.4,2),"Timber",.2)
b.box((0,0,2),(12,3,.3),"Paint",.07)
for x in [-4,-2,0,2,4]:
    for y in [-1.23,1.23]:b.box((x,y,1.05),(1.2,.05,.7),"Ceramic",.025,colour=(.12,.30,.36,1))
for x in [-4,4]:
    for y in [-1,1]:b.tube([(x,y,2.1),(x,y*2.1,4.3)],[.035,.035],"Bronze",8)
for y in [-1,1]:b.box((12,y*3.2,6),(4,.18,3),"Paint",.1)
b.box((12,0,9.3),(4,4,.15),"Paint",.1)
export("Airship",b)
b=Builder();b.lathe([(0,0),(.65,0),(.65,.3),(.35,.5),(.18,7),(.22,7.2),(0,7.3)],family="Bronze",segments=48)
b.tube([(0,0,6.7),(1.8,0,6.9),(3.8,0,5.5)],[.05,.05,.04],"Bronze",12)
export("MooringMast",b)

b=Builder();rng=random.Random(64017)
pts=[(.65*math.sin(t*.13),.35*math.sin(t*.24),t) for t in range(39)]
b.tube(pts,[3.8*(1-t/43)**.74+.13 for t in range(39)],"Bark",64,ribs=.055)
for j in range(14):
    a=j*2.399;z=12+j*1.55;length=13+(j%4)*2.3;pts=[]
    for k in range(18):
        t=k/17
        pts.append((math.cos(a)*length*t,math.sin(a)*length*t,z+5*math.sin(t*1.7)+t*t*3))
    b.tube(pts,[max(.04,1.2*(1-k/18)**1.2) for k in range(18)],"Bark",24,ribs=.05)
    tip=Vector(pts[-1])
    for leaf in range(230):
        direction=Vector((rng.gauss(0,1),rng.gauss(0,1),rng.gauss(0,.45)))
        if direction.length<.01:direction=Vector((1,0,0))
        direction.normalize()
        c=tip+direction*rng.uniform(1,6.0)
        size=rng.uniform(.72,1.55)
        b.leaf(c,size,size*.48,rng.random()*math.tau,rng.uniform(-1.1,1.1),
               colour=(rng.uniform(.10,.25),rng.uniform(.32,.54),rng.uniform(.07,.16),1))
for j in range(10):
    a=j*math.tau/10;pts=[]
    for k in range(12):
        t=k/11;pts.append((math.cos(a)*(2+10*t),math.sin(a)*(2+10*t),2*(1-t)**2-.7*t))
    b.tube(pts,[1.3*(1-k/12)**1.1+.06 for k in range(12)],"Bark",24,ribs=.05)
export("GiantTree",b,"organic trunk, 14 curved limbs, 3,220 curved leaves")

b=Builder()
for x in [-5,5]:
    for y in [-3,3]:column(b,(x,y,0),5,.2)
b.box((0,0,5.3),(12,8,.3),"Timber",.06)
for y in [-3.8,3.8]:b.box((0,y,5.45),(12,.2,.45),"Paint",.03)
b.lathe([(0,0),(.3,0),(.13,.2),(.09,1),(.12,1.1)],(0,0,0),"Bronze",32)
b.tube([(0,0,1.1),(0,1,1.6),(0,1.5,1.85)],[.20,.20,.22],"Bronze",32)
export("CanopyLookout",b)
b=Builder()
for x in [-2.4,2.4]:column(b,(x,0,0),3.3,.2)
b.arc((0,0,3),2.4,.42,depth=.7,family="Timber")
b.lathe([(0,0),(.8,0),(.9,.2),(.45,.5),(.25,1),(.65,1.2),(0,1.25)],(0,.8,0),"Limestone",48)
for j in range(8):fern(b,(math.cos(j)*3,math.sin(j)*3,0),.6,j)
export("TreeShrine",b)
b=Builder()
b.lathe([(0,-.06),(8,-.06),(8.3,.08),(8.1,.24),(7.6,.3),(7.4,.12),(0,.12)],family="Limestone",segments=128)
b.lathe([(0,.14),(7.45,.14)],family="Water",segments=128)
for j in range(24):
    a=j*math.tau/24
    b.leaf((math.cos(a)*7.6,math.sin(a)*7.6,.3),.6,.4,a,family="Glow",colour=(.17,.68,.39,1))
export("LuminousPond",b)
b=Builder()
for j in range(6):fern(b,(math.cos(j)*.7,math.sin(j)*.7,0),.75,j)
export("FernPatch",b)
b=Builder()
for j in range(18):
    a=j*2.399;r=math.sqrt(j/18)*1.2;x=math.cos(a)*r;y=math.sin(a)*r
    b.tube([(x,y,0),(x+.06,y,.3),(x+.04,y,.65)],[.018,.016,.009],"Foliage",8)
    for k in range(5):b.leaf((x+.04,y,.65),.2,.15,k*math.tau/5,.3,"Glow",(.32,.72,.55,1))
export("GlowFlowers",b)

b=Builder()
b.crystal((0,0,0),4,28,7,.1)
for j in range(9):
    a=j*2.399
    b.crystal((math.cos(a)*4,math.sin(a)*4,0),1.1+(.3*(j%3)),4+j,6,a,
              (.17+.02*(j%4),.38+.035*(j%5),.57+.025*(j%4),1))
export("CrystalSpire",b)
for name,n,scale in [("CrystalCluster",16,1),("CrystalShards",23,.18)]:
    b=Builder();rng=random.Random(331)
    for j in range(n):
        a=j*2.399;r=math.sqrt(j/max(1,n-1))*3.8
        b.crystal((math.cos(a)*r*scale,math.sin(a)*r*scale,0),rng.uniform(.4,1)*scale,rng.uniform(1.4,6)*scale,
                  5+j%3,a,(.15+.015*(j%5),.36+.025*(j%6),.50+.03*(j%4),1))
    export(name,b)
b=Builder()
b.arc((0,0,0),7.2,2.6,depth=11,start=0,end=math.pi,segments=48)
for j in range(18):
    a=j*math.pi/18
    x=math.cos(a)*6.2;z=math.sin(a)*6.2
    b.crystal((x,-3+j%4,z),.3+(j%3)*.16,1.0+(j%4)*.7,6,a)
export("CrystalCave",b,"open-ended cave; visual roof with dedicated runtime side colliders")
b=Builder();b.arc((0,0,0),7,1.5,depth=2.5,segments=48)
for j in range(11):
    a=j*math.pi/10
    b.crystal((math.cos(a)*7.7,0,math.sin(a)*7.7),.45,1.8,6,a)
export("RockArch",b)
b=Builder()
b.tube([(.1,0,0),(-.1,.03,1),(.03,.04,2.8),(.05,0,4.8)],[.70,.78,.61,.30],"Limestone",9,
       colour=(.42,.44,.40,1),ribs=.03)
for z in [.5,1.7,2.9]:b.box((0,-.64,z),(.60,.05,.08),"Bronze",.012)
export("StandingStone",b)

b=Builder()
b.lathe([(0,0),(.4,0),(.42,.12),(.2,.25),(.14,1.15),(.23,1.2),(0,1.22)],family="Bronze",segments=48)
b.box((0,0,1.27),(.8,.65,.10),"Limestone",.035)
b.box((0,-.04,1.335),(.65,.47,.018),"Bronze",.012)
b.lathe([(0,0),(.10,0),(.1,.04),(0,.055)],(0,-.04,1.35),"Glow",32)
export("Wayfinder",b)

catalog={"version":1,"units":"metres","unreal_units":"centimetres","axis":"Blender Z up / Unreal Z up",
         "materials":FAMILIES,"assets":CATALOG,"triangles":sum(a["triangles"] for a in CATALOG),
         "generated_seconds":round(time.time()-started,3),"source_texture_origin":"aerial-city-native/SourceArt/Textures"}
(OUT/"kit-catalog.json").write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"EndlessWorldKit.blend"))
print("EW_KIT_COMPLETE",len(CATALOG),catalog["triangles"],catalog["generated_seconds"],flush=True)
