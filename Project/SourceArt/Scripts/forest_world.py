"""Sculpt inhabited forest canopies. Run after vertical_world.py to replace only CanopyWorld_0..2."""
from pathlib import Path
import runpy,math,random,json,time
import bpy
from mathutils import Vector,Matrix
shared=runpy.run_path(str(Path(__file__).with_name("forest_primitives.py")))
globals().update({key:value for key,value in shared.items() if not key.startswith("__")})
started=time.time()
existing=json.loads((OUT/"kit-catalog.json").read_text(encoding="utf8"))

for variant in range(3):
    b=Builder();rng=random.Random(22400+variant*113);height=[128,146,159][variant]
    palettes=[[(.095,.29,.13,1),(.16,.39,.17,1),(.23,.47,.20,1),(.31,.50,.20,1)],
              [(.08,.28,.23,1),(.13,.37,.29,1),(.22,.44,.32,1),(.30,.49,.31,1)],
              [(.12,.29,.12,1),(.20,.38,.14,1),(.31,.47,.18,1),(.40,.50,.23,1)]]
    palette=palettes[variant]
    earth_island(b,(0,0,-.35),29,96+variant*11,720+variant,palette)
    trunk=bezier((0,0,-7),(13-variant*5,-8,height*.35),
                 (-13+variant*8,10,height*.76),(14+variant*4,7-variant*4,height),64)
    b.tube(trunk,[8.4*(1-t/68)**.92+.24 for t in range(65)],"Bark",64,BARK,ribs=.07)
    def centre_at(z):
        idx=max(0,min(63,int((z+7)/(height+7)*64)))
        while idx<63 and trunk[idx+1].z<z:idx+=1
        while idx>0 and trunk[idx].z>z:idx-=1
        return trunk[idx].lerp(trunk[idx+1],max(0,min(1,(z-trunk[idx].z)/(trunk[idx+1].z-trunk[idx].z))))
    # Buttresses widen into the earth before tapering down its eroded sides.
    for j in range(11):
        a=j*TAU/11+rng.uniform(-.1,.1);r=rng.uniform(18,29)
        start=centre_at(rng.uniform(10,19))+Vector((math.cos(a)*3,math.sin(a)*3,0))
        end=Vector((math.cos(a)*r,math.sin(a)*r*.84,-5-rng.random()*13))
        ps=bezier(start,(math.cos(a)*8,math.sin(a)*8,2),
                   (math.cos(a)*r*.87,math.sin(a)*r*.73,1),end,26)
        b.tube(ps,[3.4*(1-t/29)**1.4+.12 for t in range(27)],"Bark",28,BARK,ribs=.08)
        if j%2==0:
            tip=end+Vector((math.cos(a)*-rng.uniform(5,10),math.sin(a)*-rng.uniform(3,7),-52-rng.random()*20))
            root=bezier(end,end+Vector((2,3,-19)),tip+Vector((4,-1,18)),tip,23)
            b.tube(root,[1.05*(1-t/25)**1.35+.025 for t in range(24)],"Bark",16,BARK,ribs=.05)
    spiral=[]
    for j in range(241):
        t=j/240;z=5+t*height*.72;a=t*TAU*3.25+variant*.6
        spiral.append(centre_at(z)+Vector((math.cos(a)*11.4,math.sin(a)*11.4,0)))
    walkway(b,spiral,2.7)
    for j in range(0,len(spiral),12):
        p=spiral[j];origin=centre_at(p.z-4)
        b.tube([origin,p-Vector((0,0,.34))],[.37,.22],"Timber",12,WOOD)
    homes=[]
    for branch in range(13):
        t=.22+branch*.052;z=height*t;a=branch*2.399963+variant*.78
        direction=Vector((math.cos(a),math.sin(a),0));side=Vector((-direction.y,direction.x,0))
        start=centre_at(z)
        reach=rng.uniform(33,57)*(1-.27*max(0,(t-.7)/.3))
        tip=start+direction*reach+side*rng.uniform(-9,9)+Vector((0,0,rng.uniform(13,24)))
        ps=bezier(start,start+direction*reach*.24+Vector((0,0,12)),
                   tip-direction*reach*.27+Vector((0,0,-3)),tip,30)
        radius=3.8*(1-t*.58)
        b.tube(ps,[radius*(1-j/33)**1.22+.06 for j in range(31)],"Bark",28,BARK,ribs=.055)
        for twig in range(4):
            u=.49+twig*.15;index=min(29,int(u*30));at=ps[index]
            turn=(-1 if twig%2 else 1)*rng.uniform(.38,1.08)
            heading=direction*math.cos(turn)+side*math.sin(turn)
            extent=rng.uniform(10,19)
            end=at+heading*extent+Vector((0,0,rng.uniform(2,10)))
            twig_ps=bezier(at,at+heading*extent*.28+Vector((0,0,5)),
                            end-heading*2+Vector((0,0,2)),end,16)
            b.tube(twig_ps,[.83*(1-j/18)**1.2+.025 for j in range(17)],"Bark",14,BARK,ribs=.025)
            for lobe,fraction in enumerate([.60,1.0]):
                anchor=twig_ps[min(16,int(16*fraction))]
                rx=rng.uniform(5.5,8.0);ry=rng.uniform(3.8,5.7);rz=rng.uniform(1.8,3.1)
                crown(b,anchor+Vector((0,0,1.4)),rx,ry,rz,variant*1000+branch*33+twig*4+lobe,palette)
        crown(b,tip+Vector((0,0,1.8)),7.5,5.8,2.7,5000+variant*100+branch,palette,360)
        if branch in [1,4,7,10]:
            # Solid decks are supported by the bough and carry the small rooms.
            deck=ps[20]+Vector((0,0,.5));deck.z=ps[20].z+.5
            b.box(deck-Vector((0,0,.23)),(12.8,10.6,.48),"Timber",.08,a,WOOD)
            frame=Matrix.Rotation(a,3,"Z")
            for x in [-5.9,5.9]:
                for y in [-4.8,4.8]:
                    corner=deck+frame@Vector((x,y,-.3))
                    b.tube([ps[13]-Vector((0,0,1.0)),corner],[.25,.16],"Timber",10,WOOD)
            pavilion(b,deck+frame@Vector((1.1,1.4,.1)),7.5,6,1,a,branch+variant)
            for k in range(5):ivy(b,deck+frame@Vector((-5+k*2.3,-5.3,.15)),7+k%3*2,700+branch*5+k)
            target_index=min(240,max(0,int((z-5)/(height*.72)*240)))
            source=spiral[target_index];hub=centre_at(source.z)
            first=math.atan2(source.y-hub.y,source.x-hub.x);last=math.atan2(deck.y-hub.y,deck.x-hub.x)
            delta=(last-first+math.pi)%TAU-math.pi
            bend=[hub+Vector((math.cos(first+delta*i/18)*11.4,math.sin(first+delta*i/18)*11.4,0)) for i in range(19)]
            walkway(b,bend,2.7)
            bridge(b,bend[-1],deck-frame@Vector((5.6,0,0)),2.7,1.3,"Timber")
            homes.append(tuple(deck))
    # The crown closes around a curved leader, with open spaces between boughs.
    for j in range(5):
        a=j*TAU/5+.4;start=trunk[47];end=trunk[-1]+Vector((math.cos(a)*rng.uniform(16,26),math.sin(a)*rng.uniform(16,26),rng.uniform(-6,5)))
        ps=bezier(start,start+Vector((math.cos(a)*9,math.sin(a)*9,18)),end-Vector((4,3,6)),end,23)
        b.tube(ps,[1.5*(1-k/26)**1.2+.04 for k in range(24)],"Bark",20,BARK,ribs=.03)
        crown(b,end,9,7,3.1,8800+variant*30+j,palette,430)
    # Lower gardens are rooted shelves with tree cover, rather than bare white discs.
    for j in range(2):
        a=1.0+j*2.8+variant*.4;p=Vector((math.cos(a)*(44+j*14),math.sin(a)*(44+j*14),-36-j*32))
        earth_island(b,p,14+j*2,43+j*9,1900+variant*9+j,palette)
        base=Vector((p.x,p.y,p.z));top=base+Vector((-3,2,15))
        ps=bezier(base,base+Vector((3,0,6)),top-Vector((1,1,4)),top,18)
        b.tube(ps,[1.55*(1-k/20)**1.3+.06 for k in range(19)],"Bark",24,BARK,ribs=.05)
        for k in range(4):
            a2=k*2.399963;end=top+Vector((math.cos(a2)*6,math.sin(a2)*6,-2+k))
            b.tube([ps[10],ps[14],end],[.7,.4,.08],"Bark",12,BARK)
            crown(b,end,5.7,4.9,2.2,9900+variant*10+j*4+k,palette,280)
        bridge(b,spiral[0] if j==0 else Vector((0,0,-12)),p+Vector((0,0,.2)),3,5,"Timber")
    export("CanopyWorld_"+str(variant),b,
           "Curved living trunk, thirteen branching boughs, layered emerald foliage, continuous spiral boardwalk, supported canopy rooms and rooted lower gardens")

names={item["name"] for item in CATALOG}
existing["assets"]=[item for item in existing["assets"] if item["name"] not in names]+CATALOG
existing["triangles"]=sum(item["triangles"] for item in existing["assets"])
existing["forest_art"]={"assets":sorted(names),"authored_seconds":round(time.time()-started,3),
    "blend_source":"ForestWorld.blend","walkway_collisions":"not yet authored; world art pass"}
(OUT/"kit-catalog.json").write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"ForestWorld.blend"))
print("EW_FOREST_ART_COMPLETE",len(CATALOG),sum(a["triangles"] for a in CATALOG),round(time.time()-started,3),flush=True)
