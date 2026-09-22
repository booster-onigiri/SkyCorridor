"""Author connected trees, eroded roots, quiet water and irregular forest verges.

Run after expand_art.py. Existing buildings and the large occupied canopies stay
in the catalog unchanged. Geometry is original, editable and measured in metres.
"""
from pathlib import Path
import runpy,math,random,json,time,sys
import bpy
from mathutils import Vector,Matrix

shared=runpy.run_path(str(Path(__file__).with_name("forest_primitives.py")))
globals().update({key:value for key,value in shared.items() if not key.startswith("__")})
started=time.time()
ROOT_SHELVES_ONLY="--root-shelves-only" in sys.argv
existing=json.loads((OUT/"kit-catalog.json").read_text(encoding="utf8"))
BARK=(.44,.30,.17,1)
PALETTES=[[(.10,.27,.12,1),(.17,.36,.15,1),(.25,.43,.19,1),(.32,.48,.23,1)],
          [(.10,.28,.21,1),(.17,.37,.25,1),(.25,.45,.30,1),(.35,.51,.34,1)],
          [(.16,.28,.12,1),(.24,.36,.16,1),(.34,.44,.22,1),(.43,.51,.28,1)]]


def leaf_cluster(b,p,radii,seed,palette,count=560,leaf_size=.6):
    rng=random.Random(seed);p=Vector(p);phase=rng.uniform(0,TAU)
    rx,ry,rz=radii
    # Short twigs spread through the foliage instead of terminating below it.
    for j in range(7):
        a=j*2.399963+phase
        end=p+Vector((math.cos(a)*rx*.82,math.sin(a)*ry*.82,rz*rng.uniform(-.3,.5)))
        ps=bezier(p-Vector((0,0,.6)),p+Vector((0,0,.3)),end-Vector((0,0,.2)),end,8)
        b.tube(ps,[.09*(1-k/8)**1.2+.006 for k in range(9)],"Bark",7,BARK)
    for j in range(count):
        a=j*2.399963+phase;u=rng.uniform(-.91,.98);q=math.sqrt(1-u*u)
        r=rng.uniform(.24,1.12)**.62*(1+.1*math.sin(a*5+phase))
        at=p+Vector((math.cos(a)*q*rx*r,math.sin(a)*q*ry*r,u*rz*r))
        blade(b,at,leaf_size*rng.uniform(.72,1.25),leaf_size*rng.uniform(.39,.66),
              a,rng.uniform(-1.0,1.0),palette[rng.randrange(len(palette))])


def moss(b,p,rx,ry,seed,height=.18):
    rng=random.Random(seed);v=[(0,0,height)];faces=[];sides=40
    for j in range(1,8):
        t=j/7
        for k in range(sides):
            a=k*TAU/sides;r=t*(1+.14*math.sin(a*3+seed)+.06*math.cos(a*7))
            v.append((math.cos(a)*rx*r,math.sin(a)*ry*r,height*(1-t*t)+.018*math.sin(a*7+t*9)))
    faces.extend((0,1+k,1+(k+1)%sides) for k in range(sides))
    for j in range(6):
        for k in range(sides):
            a=1+j*sides+k;c=1+j*sides+(k+1)%sides
            faces.append((a,a+sides,c+sides,c))
    offset=len(b.v);b.mesh(v,faces,"Soil",(.23,.34,.14,1),p)
    for j in range(offset,len(b.v)):
        x,y,z=b.v[j];shade=.035*math.sin(x*1.7+y*.9)+.018*math.sin(y*4-x*3)
        b.col[j]=(.23+shade,.34+shade,.14+shade*.6,1)
    for j in range(42):
        a=rng.uniform(0,TAU);r=rng.random()**.5
        at=Vector(p)+Vector((math.cos(a)*rx*r,math.sin(a)*ry*r,height*(1-r*r)+.025))
        blade(b,at,rng.uniform(.08,.20),rng.uniform(.03,.06),a,rng.uniform(.3,1.0),PALETTES[0][j%4])


def root_land(b,radius,depth,seed):
    rng=random.Random(seed);sides=96;rings=30;v=[];f=[];plant_scale=min(1.,radius/20)
    for j in range(rings):
        t=j/(rings-1)
        for k in range(sides):
            a=k*TAU/sides
            outline=1+.09*math.sin(a*3+seed)+.065*math.cos(a*7+seed*.3)
            strata=.025*math.sin(t*57+a*.5)+.016*math.cos(t*89+a*2)
            r=radius*outline*(.035+.965*t**.59+strata*math.sin(t*math.pi))
            v.append((math.cos(a)*r,math.sin(a)*r*.83,-depth*(1-t)))
    for j in range(rings-1):
        for k in range(sides):
            a=j*sides+k;c=j*sides+(k+1)%sides;f.append((a,c,c+sides,a+sides))
    offset=len(b.v);b.mesh(v,f,"Limestone",(.32,.35,.28,1))
    for j in range(offset,len(b.v)):
        x,y,z=b.v[j];shade=.025*math.sin(z*1.3)+.018*math.cos(x*.6+y*.4)
        b.col[j]=(.32+shade,.35+shade,.28+shade,1)
    rim=v[-sides:]
    b.mesh([(0,0,.015)]+[(x,y,.015) for x,y,z in rim],
           [(0,k+1,(k+1)%sides+1) for k in range(sides)],"Soil",(.33,.37,.22,1))
    for j in range(11):
        a=j*2.399963+seed*.13;reach=radius*(.82+.11*math.sin(a*3+seed))
        start=Vector((math.cos(a)*reach*.55,math.sin(a)*reach*.45,-.8))
        end=Vector((math.cos(a)*(reach*.52),math.sin(a)*(reach*.41),-depth*rng.uniform(.60,.96)))
        ps=bezier(start,(math.cos(a)*reach,math.sin(a)*reach*.83,-.65),
                  (math.cos(a)*reach*.80,math.sin(a)*reach*.67,-depth*.47),end,29)
        b.tube(ps,[(1.12*(1-k/29)**1.4+.018)*plant_scale for k in range(30)],"Bark",20,BARK,ribs=.05)
        moss(b,(start.x,start.y,.04),2.3*plant_scale,1.6*plant_scale,seed+j,.22*plant_scale)
        if j%2==0:
            fork=bezier(ps[15],ps[18]+Vector((2,1,0)),end+Vector((3,1,5)),end+Vector((3,1,1)),12)
            b.tube(fork,[(.34*(1-k/12)**1.5+.009)*plant_scale for k in range(13)],"Bark",12,BARK)
            ivy(b,ps[3],min(19,depth*.6),seed*10+j)


names=["GiantTree","Tree_Willow","Tree_Fan","Tree_Blossom","Tree_Spiral"]
for variant,name in ([] if ROOT_SHELVES_ONLY else enumerate(names)):
    b=Builder();rng=random.Random(24800+variant*53);height=[39,29,42,25,48][variant]
    palette=PALETTES[[0,1,0,2,1][variant]];lean=[4,-5,7,-3,6][variant]
    trunk=bezier((0,0,-.5),(lean,-2,height*.30),(-lean*.6,4,height*.70),(lean*.8,2,height),56)
    # The last ring is a fine living leader; there is no exposed cylindrical cap.
    b.tube(trunk,[3.0*(1-k/56)**1.1+.022 for k in range(57)],"Bark",48,BARK,ribs=.052)
    def at_z(z):
        i=min(range(len(trunk)),key=lambda i:abs(trunk[i].z-z));return trunk[i]
    for j in range(10):
        a=j*TAU/10+rng.uniform(-.15,.15);reach=rng.uniform(7.5,12.5)
        ps=bezier(at_z(rng.uniform(2.5,5)),(math.cos(a)*3.5,math.sin(a)*3.5,.9),
                  (math.cos(a)*reach*.75,math.sin(a)*reach*.75,.16),(math.cos(a)*reach,math.sin(a)*reach,-.35),22)
        b.tube(ps,[1.12*(1-k/22)**1.25+.018 for k in range(23)],"Bark",22,BARK,ribs=.05)
        if j%2==0:moss(b,(math.cos(a)*6,math.sin(a)*6,.015),2.1,1.5,230+variant*20+j,.16)
    for j in range(11):
        t=.32+j*.054;start=at_z(height*t);a=j*2.399963+variant*.73
        if variant==2:a=-1.3+(j%6)*.49+(j//6)*math.pi
        direction=Vector((math.cos(a),math.sin(a),0));side=Vector((-direction.y,direction.x,0))
        reach=rng.uniform(9,16)*(1-.38*max(0,(t-.55)/.45))
        end=start+direction*reach+side*rng.uniform(-3,3)+Vector((0,0,3 if variant==1 else rng.uniform(4,8)))
        ps=bezier(start,start+direction*reach*.24+Vector((0,0,4)),
                  end-direction*2+Vector((0,0,2 if variant==1 else -1)),end,22)
        b.tube(ps,[1.0*(1-k/22)**1.25+.018 for k in range(23)],"Bark",20,BARK,ribs=.035)
        for branch in range(3):
            stem=ps[10+branch*5];turn=(-1 if branch%2 else 1)*rng.uniform(.4,.9)
            heading=direction*math.cos(turn)+side*math.sin(turn)
            tip=stem+heading*rng.uniform(4.0,7.2)+Vector((0,0,rng.uniform(1,3)))
            twigs=bezier(stem,stem+heading*1.9+Vector((0,0,2)),tip+Vector((0,0,1)),tip,14)
            b.tube(twigs,[.32*(1-k/14)**1.3+.01 for k in range(15)],"Bark",12,BARK)
            leaf_cluster(b,tip+Vector((0,0,.5)),(3.3,2.5,1.45),variant*1000+j*19+branch,palette,540,.65)
            if variant==1:
                for hanging in range(9):
                    a2=hanging*2.399963;top=tip+Vector((math.cos(a2)*2.2,math.sin(a2)*1.7,.5))
                    length=rng.uniform(2.5,6.2)
                    drop=bezier(top,top+Vector((.8,.5,-length*.3)),top+Vector((1.1,.4,-length*.8)),top+Vector((.4,.9,-length)),13)
                    b.tube(drop,[.043*(1-k/13)+.004 for k in range(14)],"Bark",6,BARK)
                    for k in range(2,14):
                        for sign in [-1,1]:blade(b,drop[k],.48,.12,a2+sign,1.15,palette[k%4])
            if variant==3:
                for blossom in range(34):
                    a2=blossom*2.399963;r=rng.random()**.5*2.6
                    at=tip+Vector((math.cos(a2)*r,math.sin(a2)*r,.5+rng.uniform(-.5,.9)))
                    for petal in range(5):b.leaf(at,.27,.23,petal*TAU/5,.2,"Petal",(.68,.36+.03*(blossom%3),.37,1))
        leaf_cluster(b,end+Vector((0,0,.8)),(3.6,2.7,1.6),8000+variant*20+j,palette,650,.66)
    # An asymmetrical crown covers and continues the tapered central leader.
    for j in range(3):
        end=trunk[-1]+Vector((math.cos(j*2.4)*3,math.sin(j*2.4)*3,-1+j*.6))
        ps=bezier(trunk[46],trunk[50],end-Vector((0,0,1)),end,15)
        b.tube(ps,[.43*(1-k/15)**1.3+.008 for k in range(16)],"Bark",12,BARK)
        leaf_cluster(b,end,(3.8,3.1,1.9),9300+variant*10+j,palette,710,.65)
    export(name,b,"Connected tapering leaders, asymmetrical boughs, layered leaves and buttress roots; no exposed cut trunk")

for variant in range(4):
    b=Builder();root_land(b,20+variant*1.3,22+variant*7,92+variant)
    export("RootIsland_"+str(variant),b,"Layered grey-green earth, connected hanging roots, moss and trailing vines")

for variant in range(3):
    b=Builder();rng=random.Random(10240+variant)
    # A soil shelf supports every plant and tapers below the path edge.
    root_land(b,3.4,3.2+variant*.6,420+variant)
    for j in range(13):
        a=j*2.399963;r=rng.uniform(.3,2.6)
        moss(b,(math.cos(a)*r,math.sin(a)*r*.7,.03),rng.uniform(.6,1.3),rng.uniform(.5,1),600+variant*20+j,.12)
        if j%3!=0:fern(b,(math.cos(a)*r,math.sin(a)*r*.7,.09),rng.uniform(.4,.95),seed=120+variant*20+j)
    for j in range(32):
        a=j*2.399963;r=rng.uniform(.7,2.8);p=Vector((math.cos(a)*r,math.sin(a)*r*.73,.03))
        for k in range(4):blade(b,p,.35+rng.random()*.3,.035,a+k*.6,1.05,PALETTES[variant][k])
    export("ForestVerge_"+str(variant),b,"Irregular supported moss shelves, uneven fern heights, fine grasses and trailing roots")

if not ROOT_SHELVES_ONLY:
    # Unit footprint matches AddFloor's existing scaling and its unchanged collider.
    b=Builder();steps=48;v=[];faces=[]
    for y in range(steps+1):
        for x in range(steps+1):v.append((x/steps-.5,y/steps-.5,0))
    for y in range(steps):
        for x in range(steps):
            k=y*(steps+1)+x;faces.append((k,k+1,k+steps+2,k+steps+1))
    b.mesh(v,faces,"Soil",(.36,.39,.24,1))
    for i,(x,y,z) in enumerate(b.v):
        patch=.5+.5*math.sin(x*29+math.sin(y*18)*1.5)*math.cos(y*21-x*5)
        b.col[i]=mix((.28,.36,.16,1),(.49,.41,.28,1),patch)
    b.box((0,0,-.20),(1,1,.39),"Soil",0,colour=(.29,.27,.20,1))
    export("GardenEarthDeck",b,"Quiet mottled earth and moss, one metre footprint preserving the existing walk surface")

    # The pond's shore is irregular and dark enough to read beside a sunlit path.
    b=Builder();sides=128;rng=random.Random(812);v=[];faces=[]
    for ring,scale in enumerate([.0,.52,.90,1.,1.06,1.14]):
        for k in range(sides):
            a=k*TAU/sides;r=7.1*(1+.09*math.sin(a*3)+.045*math.cos(a*5))
            z=[.13,.13,.13,.12,.27,.02][ring]
            v.append((math.cos(a)*r*scale,math.sin(a)*r*.84*scale,z))
    for j in range(3):
        for k in range(sides):
            a=j*sides+k;c=j*sides+(k+1)%sides;faces.append((a,a+sides,c+sides,c))
    b.mesh(v[:4*sides],faces,"Water",(.045,.19,.17,1))
    shore_faces=[]
    for j in [3,4]:
        for k in range(sides):
            a=j*sides+k;c=j*sides+(k+1)%sides;shore_faces.append((a,a+sides,c+sides,c))
    b.mesh(v,shore_faces,"Limestone",(.30,.36,.28,1))
    for j in range(16):
        a=j*2.399963;r=7.1*(1+.09*math.sin(a*3)+.045*math.cos(a*5));p=(math.cos(a)*r*1.09,math.sin(a)*r*.84*1.09,.19)
        if j%3:moss(b,p,.85,.48,850+j,.10)
        if j%4==0:fern(b,p,.48,j)
        if j%2==0:
            at=(math.cos(a)*r*.78,math.sin(a)*r*.84*.78,.155)
            b.leaf(at,.70,.56,a,0,"Foliage",(.19,.38,.17,1))
    export("LuminousPond",b,"Asymmetrical teal pool, muted stone shore, moss, water leaves and small bank ferns")

changed={item['name'] for item in CATALOG}
existing['assets']=[item for item in existing['assets'] if item['name'] not in changed]+CATALOG
existing['triangles']=sum(item['triangles'] for item in existing['assets'])
existing['understory_art']={'assets':sorted(set(existing.get('understory_art',{}).get('assets',[]))|changed),'blend_source':'UnderstoryWorld.blend','last_update_source':'RootShelves.blend' if ROOT_SHELVES_ONLY else 'UnderstoryWorld.blend','last_update_assets':sorted(changed),'authored_seconds':round(time.time()-started,3)}
(OUT/'kit-catalog.json').write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/('RootShelves.blend' if ROOT_SHELVES_ONLY else 'UnderstoryWorld.blend')))
print('EW_UNDERSTORY_ART_COMPLETE',len(CATALOG),sum(a['triangles'] for a in CATALOG),round(time.time()-started,3),flush=True)
