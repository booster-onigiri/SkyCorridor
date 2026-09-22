"""Author layered districts and landforms; run forest_world.py afterward for the current canopy."""
from pathlib import Path
import runpy,math,random,json,time
import bpy
from mathutils import Vector,Matrix
shared=runpy.run_path(str(Path(__file__).with_name("vertical_primitives.py")))
globals().update({key:value for key,value in shared.items() if not key.startswith("__")})
started=time.time()
existing=json.loads((OUT/"kit-catalog.json").read_text(encoding="utf8"))

for variant in range(4):
    b=Builder();rng=random.Random(300+variant)
    island(b,(0,0,-3),18,78+variant*12,21+variant)
    # A stone spine continues down below the arrival paths and above the crowns.
    for j in range(8):
        a=j*math.tau/8
        column(b,(math.cos(a)*6.5,math.sin(a)*6.5,-54),172+variant*9,.75)
    for level,z in enumerate([-38,16,55,92,127+variant*5]):
        r=[26,32,39,33,42][level]+variant*1.8
        start=.15+level*.36+variant*.28;end=start+math.tau*(.68+.055*(level%3))
        band(b,(0,0,z),r,7,start,end,100,.62)
        rail(b,(0,0,z),r+3.5,start,end,spacing=1.8)
        rail(b,(0,0,z),r-3.5,start,end,spacing=2.2)
        # Underside ribs and a cornice give every level a legible thickness.
        for j in range(14):
            a=start+(end-start)*j/13
            endp=(math.cos(a)*(r-1),math.sin(a)*(r-1),z-.9)
            b.tube([(math.cos(a)*6,math.sin(a)*6,z-17),endp],[.32,.22],"Limestone",12,IVORY)
        for j in range(3+level%2):
            a=start+.5+j*(end-start-.85)/(3+level%2)
            p=(math.cos(a)*(r-1),math.sin(a)*(r-1),z+.12)
            pavilion(b,p,9+(j%2)*3,7,1+(j+level+variant)%3,a+math.pi*.5,(j+level+variant)%4)
        for j in range(9):
            a=start+(end-start)*j/8
            p=(math.cos(a)*(r+3.4),math.sin(a)*(r+3.4),z+.1)
            ivy(b,p,5+level*.9,level*16+j)
    # Asymmetric neighboring islets occupy different heights instead of a stack.
    for j in range(3):
        a=.5+j*2.1+variant*.35;rad=58+j*13;z=[-76,-8,67][j]+variant*4
        p=Vector((math.cos(a)*rad,math.sin(a)*rad,z))
        island(b,p,10+j*2,47+j*15,60+variant*11+j)
        pavilion(b,p,13,10,2+j%2,a-.7,j+variant)
        source=Vector((math.cos(a)*(28+j*3),math.sin(a)*(28+j*3),[-38,16,55][j]))
        bridge(b,source,p,4,4)
        for k in range(2):
            off=Vector((math.cos(a+k)*10,math.sin(a+k)*10,0))
            b.tube([p+off,p+off+Vector((1,0,-20)),p+off+Vector((3,-1,-78))],
                   [.26,.50,.85],"Water",18,(.44,.73,.79,1))
    # Fragmented white crown: thin, varied slabs and fine suspended struts.
    crown=166+variant*13
    for j in range(23):
        a=j*2.399;r=13+1.9*j
        p=Vector((math.cos(a)*r,math.sin(a)*r,crown+math.sin(j*.63)*10+j*.44))
        b.box(p,(7+j%5*2,3.3+(j%3)*1.4,.20),"Limestone",.045,a*.35,IVORY)
        b.tube([(math.cos(a)*6,math.sin(a)*6,crown-32),p],[.12,.055],"Bronze",10,GOLD)
        if j%2==0:b.tube([p,p+Vector((0,0,9+j%7))],[.045,.015],"Bronze",8,GOLD)
    export("SkyWard_"+str(variant),b,"Five offset inhabited galleries, lower islands, suspended passages and a fragmented upper crown")


def leaf_cloud(b,p,radius,count,seed,palette,length=1.7):
    rng=random.Random(seed);p=Vector(p)
    for j in range(count):
        a=j*2.399;u=rng.uniform(-1,1);q=math.sqrt(1-u*u);r=radius*rng.uniform(.35,1)**.33
        at=p+Vector((math.cos(a)*q*r,math.sin(a)*q*r,u*r*.32))
        b.leaf(at,length*rng.uniform(.75,1.35),length*.64,a,rng.uniform(-.8,.8),"Foliage",palette[j%len(palette)])


for variant in range(3):
    b=Builder();rng=random.Random(650+variant);height=137+variant*21
    island(b,(0,0,0),24,103+variant*15,330+variant)
    for stem in range(3):
        ps=[(math.sin(t*.095+stem*2.1)*(2+t*.03),math.cos(t*.072+stem*2.1)*(2+t*.025),t*height/55-12) for t in range(56)]
        b.tube(ps,[5.5*(1-t/63)**.72+.25 for t in range(56)],"Bark",48,colour=(.40,.36,.24,1),ribs=.10)
    palette=[(.22,.47,.27,1),(.39,.61,.36,1),(.55,.67,.36,1)] if variant!=1 else [(.31,.53,.48,1),(.42,.64,.57,1),(.58,.70,.62,1)]
    for j in range(18):
        a=j*2.399;z=20+j*height/23;r=21+13*math.sin(j*.37)**2
        ps=[(math.cos(a)*r*t/25,math.sin(a)*r*t/25,z+10*math.sin(t/25*1.7)) for t in range(26)]
        b.tube(ps,[2.8*(1-t/28)**1.4+.06 for t in range(26)],"Bark",26,colour=(.39,.36,.24,1),ribs=.07)
        leaf_cloud(b,ps[-1],8+j%3,420,800+variant*25+j,palette,1.8)
        if j%4==0:
            p=Vector(ps[-1])+Vector((0,0,.25))
            band(b,p,6.7,3,steps=56,thickness=.28,family="Timber",colour=(.55,.41,.25,1))
            rail(b,p,8.2,spacing=1.6,colour=PALE)
            pavilion(b,p+Vector((-3,3,0)),6,5,1,a,j+variant)
            bridge(b,(0,0,z),p,2.8,2,"Timber")
        if j%3==0:
            p=Vector(ps[-1])
            for k in range(3):ivy(b,p+Vector((k*.5,0,0)),16+k*3,j*8+k)
    for j in range(4):
        a=j*1.9;ps=[(math.cos(a)*(3+t*1.3),math.sin(a)*(3+t*.8),1-t*3.3) for t in range(32)]
        b.tube(ps,[3.4*(1-t/36)**1.2+.07 for t in range(32)],"Bark",32,ribs=.1)
    for j in range(2):
        p=(42*(-1 if j else 1),23+j*12,-48-j*38)
        island(b,p,13,48,450+j+variant*7)
        leaf_cloud(b,Vector(p)+Vector((0,0,6)),11,620,900+j,palette,1.4)
        bridge(b,(0,0,12-j*25),p,3,8,"Timber")
    export("CanopyWorld_"+str(variant),b,"Huge braided trunks, occupied canopy decks, hanging vines and lower rooted gardens")


for variant in range(3):
    b=Builder();rng=random.Random(980+variant)
    island(b,(0,0,-8),24,120,510+variant)
    for j in range(11):
        a=j*2.399;r=10+j*1.3;height=38+rng.random()*87
        temp=Builder();temp.crystal((0,0,0),3+rng.random()*5,height,5+j%4,0,
                                   [(.23,.47,.64,1),(.36,.62,.70,1),(.46,.53,.69,1)][j%3])
        rot=Matrix.Rotation(.16*math.sin(a),3,"Y")@Matrix.Rotation(.22*math.cos(a),3,"X")
        merge(b,temp,(math.cos(a)*r,math.sin(a)*r,-4+j*2),rot)
    temp=Builder();temp.arc((0,0,0),36+variant*5,4.4,depth=8,segments=100,family="Crystal")
    merge(b,temp,(0,5,39),Matrix.Rotation(variant*.7,3,"Z"))
    # Interrupted floating shelves reveal the sky through the vertical structure.
    for j in range(4):
        a=j*1.7+variant*.4;r=44+j*5;z=-65+j*48
        p=Vector((math.cos(a)*r,math.sin(a)*r,z))
        island(b,p,10+j%2*5,45+j*11,605+j+variant*5)
        band(b,p,7.5,4,.25,5.7,64,.5,colour=PALE)
        for k in range(5):
            angle=k*1.18
            column(b,p+Vector((math.cos(angle)*7,math.sin(angle)*7,0)),6+k%2*3,.3)
        if j%2==0:bridge(b,(0,0,z+15),p,3.5,2)
        for k in range(5):
            b.crystal(p+Vector((math.cos(k)*9,math.sin(k)*7,.1)),.8,3+k*2,6,k,(.49,.71,.77,1))
    for j in range(16):
        a=j*2.399;r=26+j*1.8;z=65+j*7
        temp=Builder();temp.crystal((0,0,0),.6+j%4*.25,8+j%5*2,5,j,(.49,.66,.77,1))
        merge(b,temp,(math.cos(a)*r,math.sin(a)*r,z),Matrix.Rotation(.5+j*.13,3,"Y"))
    export("CrystalWorld_"+str(variant),b,"Mineral spires, a monumental hollow arch, suspended ruins and ascending fragments")

# Replace the featureless panes in the existing modular buildings.
b=Builder();window(b,(0,0,0),1.15,2.05)
export("Window",b,"Recessed blue glass, stone jambs, sill, lintel and copper mullions")

names={item["name"] for item in CATALOG}
existing["assets"]=[item for item in existing["assets"] if item["name"] not in names]+CATALOG
existing["triangles"]=sum(item["triangles"] for item in existing["assets"])
existing["vertical_art"]={"assets":sorted(names),"authored_seconds":round(time.time()-started,3),
    "blend_source":"VerticalWorld.blend","scale":"metres; scenery extends below and above the walking layers"}
(OUT/"kit-catalog.json").write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"VerticalWorld.blend"))
print("EW_VERTICAL_ART_COMPLETE",len(CATALOG),sum(a["triangles"] for a in CATALOG),round(time.time()-started,3),flush=True)
