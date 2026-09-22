"""Author the reference water city's shared geometry and collision definition.

This is an ordinary Python source step: it does not run Blender or Unreal and
does not touch assets or player data. Its JSON drives Blender; the generated
C++ header embeds exactly the same JSON for streamed recipes and audits.
All positions are centimetres relative to reference chunk (0,0).
"""
from pathlib import Path
import argparse, hashlib, json, math

FORMAT="water-city-plan-v1"
CHUNK=12800
ROOT=Path(__file__).resolve().parents[2]


def rect(x0,y0,x1,y1):
    return [[x0,y0],[x1,y0],[x1,y1],[x0,y1]]


def local(point,box):
    """Inverse the recipe's recorded yaw/pitch/roll transform, without scale.

    Collider extents already include scale in EW::BoxAt; applying it again
    would silently enlarge the old galleries during clearance checking.
    """
    roll,pitch,yaw=map(math.radians,box["rotation"])
    cy,sy=math.cos(yaw),math.sin(yaw);cp,sp=math.cos(pitch),math.sin(pitch)
    cr,sr=math.cos(roll),math.sin(roll)
    # Unreal's positive pitch points forward upward.
    axes=((cp*cy,cp*sy,sp),(sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp),
          (-(cr*sp*cy+sr*sy),cy*sr-cr*sp*sy,cr*cp))
    delta=[point[i]-box["position"][i] for i in range(3)]
    return [sum(delta[i]*axis[i] for i in range(3)) for axis in axes]


def old_boxes(snapshot):
    result=[]
    for district in snapshot["districts"]:
        cx,cy=map(int,district["coord"].split(","))
        for index,source in enumerate(district["colliders"]):
            box=dict(source);box["position"]=list(source["position"])
            box["position"][0]+=cx*CHUNK;box["position"][1]+=cy*CHUNK
            box["source"]=[district["coord"],index]
            result.append(box)
    return result


def guard_box(box):
    extent=box["extent"]
    return not box["floor"] and abs(extent[2]-62)<.01 and min(extent[:2])<=13


def overlap(point,box,radius=38,half_height=88):
    p=local(point,box)
    # Conservative upright capsule envelope, matching the recipe audit.
    return (abs(p[0])<box["extent"][0]+radius-.05 and
            abs(p[1])<box["extent"][1]+radius-.05 and
            abs(p[2])<box["extent"][2]+half_height-.05)


def sample(a,b,spacing=65):
    count=max(1,math.ceil(math.dist(a,b)/spacing))
    return [[a[k]+(b[k]-a[k])*i/count for k in range(3)] for i in range(count+1)]


def build(snapshot,source_sha):
    if not snapshot.get("success") or snapshot.get("format")!="water-city-existing-layout-v1":
        raise ValueError("A successful native existing-layout audit is required")
    if {d["coord"] for d in snapshot["districts"]}!={"0,-1","0,0","0,1"}:
        raise ValueError("The baseline must contain exactly the reference three districts")
    if any(d["hub"]!=[6400,6400,2250] for d in snapshot["districts"]):
        raise ValueError("The measured water-city datum changed")
    plan={"format":FORMAT,"revision":1,"baseline_sha256":source_sha,
          "baseline_digests":{d["coord"]:d["digest"] for d in snapshot["districts"]},
          "bodies":[],"banks":[],"falls":[],"walks":[],"walk_rails":[],"supports":[],"views":[],
          "materials":["WaterCitySurface","WaterCityFall","WaterCityFoam","WaterCityStone"],
          "meshes":["WaterCityCentralStone","WaterCityCentralWater","WaterCityCentralFoam",
                    "WaterCityNorthStone","WaterCityNorthWater","WaterCityNorthFall","WaterCityNorthFoam",
                    "WaterCitySouthStone","WaterCitySouthWater","WaterCitySouthFall","WaterCitySouthFoam",
                    "WaterCityWalks","WaterCityDrop"]}
    def body(name,group,polygon,z,flow=(0,-1),depth=260):
        plan["bodies"].append({"id":name,"group":group,"polygon":polygon,"water_z":z,
            "depth":depth,"flow":list(flow),"wall_height":65,"wall_width":55})
    body("clock-water-square","Central",[[5500,4400],[7300,4400],[8400,5500],[8400,7300],
         [7300,8400],[5500,8400],[4400,7300],[4400,5500]],1950)
    body("main-south-canal","Central",rect(5500,-1100,7300,4400),1950)
    body("main-north-canal","Central",rect(5500,8400,7300,11700),1950)
    body("north-main-catchment","Central",rect(5200,11700,7600,12000),1950)
    body("west-canal","Central",rect(0,5500,4400,7300),1950,(1,0))
    body("east-canal","Central",rect(8400,5500,12800,7300),1950,(-1,0))
    body("middle-water-garden","North",rect(5200,12200,7600,14300),3850)
    body("upper-water-garden","North",rect(5200,13000,7600,15800),7450)
    body("high-north-aqueduct","North",rect(5500,15800,7300,24800),7450)
    body("south-catchment","South",rect(5200,-3900,7600,-1350),150)
    body("south-tail-canal","South",rect(5500,-11200,7300,-3900),150)
    def fall(name,group,x0,x1,y0,y1,top,bottom):
        plan["falls"].append({"id":name,"group":group,"x0":x0,"x1":x1,
            "start_y":y0,"end_y":y1,"top_z":top,"bottom_z":bottom,
            "foam_depth":380,"foam_width":x1-x0+120})
    for label,x0,x1 in (("west",5200,5800),("east",7000,7600)):
        fall("upper-"+label,"North",x0,x1,13000,12800,7450,3850)
        fall("middle-"+label,"North",x0,x1,12200,12000,3850,1950)
    for label,x0,x1 in (("west",5550,6150),("east",6650,7250)):
        fall("south-"+label,"South",x0,x1,-1100,-1350,1950,150)
    def walk(name,a,b,width=280,access=False):
        plan["walks"].append({"id":name,"a":list(a),"b":list(b),"width":width,
                              "thickness":45,"end_pad":width*.5,"access":access,"rails":True})
    # A small bridge circuit within the large water square avoids all four
    # actual lift shafts. Its branches join the current unrailed street cross.
    circuit=[(5500,5200,2250),(7250,5200,2250),(7600,5550,2250),(7600,7300,2250),
             (7250,7650,2250),(5500,7650,2250),(5150,7300,2250),(5150,5550,2250)]
    for i,a in enumerate(circuit):walk("clock-quay-%02d"%i,a,circuit[(i+1)%len(circuit)],260)
    walk("clock-quay-south-entry",(6400,5700,2250),(6400,5200,2250),320)
    walk("clock-quay-north-entry",(6400,7000,2250),(6400,7650,2250),320)
    # The NW tower's east face has a runtime central rail opening. These
    # bridges use that central bay; no fixed UrbanWalkRing mesh is cut.
    for label,z,y in (("middle",4050,12100),("upper",7650,12850)):
        walk(label+"-gallery-entry",(5300,9571,z),(6400,9571,z),300,True)
        walk(label+"-view-bridge",(6400,9571,z),(6400,y,z),320)
        # This front is beyond the complete north ends of the old galleries,
        # clearing both the fixed corner rails and the real library shelves.
        walk(label+"-water-front",(5170,y,z),(7630,y,z),240 if label=="middle" else 320)
    # Retaining walls are cut at same-level joins and at actual spill openings.
    # Water must have a visible continuous path through its physical banks.
    for basin in plan["bodies"]:
        polygon=basin["polygon"]
        for edge,a in enumerate(polygon):
            b=polygon[(edge+1)%len(polygon)];dx=b[0]-a[0];dy=b[1]-a[1]
            length=math.hypot(dx,dy);cuts=[]
            def project(p):return ((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(length*length)
            def aligned(p):return abs((p[0]-a[0])*dy-(p[1]-a[1])*dx)<.1
            for other in plan["bodies"]:
                if other is basin or other["water_z"]!=basin["water_z"]:continue
                for index,c in enumerate(other["polygon"]):
                    d=other["polygon"][(index+1)%len(other["polygon"])]
                    if aligned(c) and aligned(d):cuts.append(sorted((project(c),project(d))))
            if abs(dy)<.01:
                for fall in plan["falls"]:
                    if ((fall["top_z"]==basin["water_z"] and abs(fall["start_y"]-a[1])<.01) or
                        (fall["bottom_z"]==basin["water_z"] and abs(fall["end_y"]-a[1])<.01)):
                        cuts.append(sorted((project((fall["x0"],a[1])),project((fall["x1"],a[1])))))
            cursor=0
            for lo,hi in sorted(cuts)+[(1,1)]:
                lo=max(0,min(1,lo));hi=max(0,min(1,hi))
                if lo>cursor+.001:
                    plan["banks"].append({"body":basin["id"],"group":basin["group"],
                        "a":[a[0]+dx*cursor,a[1]+dy*cursor,basin["water_z"]],
                        "b":[a[0]+dx*lo,a[1]+dy*lo,basin["water_z"]],
                        "width":basin["wall_width"],"top_z":basin["water_z"]+basin["wall_height"],
                        "bottom_z":basin["water_z"]-basin["depth"]-55})
                cursor=max(cursor,hi)
    boxes=old_boxes(snapshot)
    def connected_floor(point,current,margin=25):
        for old in boxes:
            if not old["floor"] or abs(old["position"][2]+old["extent"][2]-point[2])>1:continue
            q=local(point,old)
            if abs(q[0])<old["extent"][0]-margin and abs(q[1])<old["extent"][1]-margin:return True
        for other in plan["walks"]:
            if other is current or abs(other["a"][2]-point[2])>1:continue
            a,b=other["a"],other["b"];dx=b[0]-a[0];dy=b[1]-a[1]
            length2=dx*dx+dy*dy
            t=((point[0]-a[0])*dx+(point[1]-a[1])*dy)/length2
            extra=(other["end_pad"]-margin)/math.sqrt(length2)
            if -extra<t<1+extra and math.hypot(point[0]-a[0]-t*dx,point[1]-a[1]-t*dy)<other["width"]*.5-margin:return True
        return False
    def rail_edge(path,a,b,outward):
        pieces=sample(a,b,28);start=None
        for i,p in enumerate(pieces):
            probe=[p[0]+outward[0]*85,p[1]+outward[1]*85,p[2]]
            # An end rail cannot occupy the adjacent deck itself, even if the
            # probe beyond that rail has already crossed the other deck edge.
            needed=not connected_floor(probe,path) and not connected_floor(p,path,-12)
            if needed and start is None:start=p
            if start is not None and (not needed or i==len(pieces)-1):
                end=pieces[max(0,i-1)] if not needed else p
                if math.dist(start,end)>25:
                    plan["walk_rails"].append({"walk":path["id"],"a":start,"b":end,"height":118,"width":18})
                start=None
    for path in plan["walks"]:
        a,b=path["a"],path["b"];length=math.dist(a,b)
        dx=(b[0]-a[0])/length;dy=(b[1]-a[1])/length
        a=[a[0]-dx*path["end_pad"],a[1]-dy*path["end_pad"],a[2]]
        b=[b[0]+dx*path["end_pad"],b[1]+dy*path["end_pad"],b[2]]
        for sign in (-1,1):
            n=(-dy*sign,dx*sign);offset=path["width"]*.5-12
            rail_edge(path,[a[0]+n[0]*offset,a[1]+n[1]*offset,a[2]],
                      [b[0]+n[0]*offset,b[1]+n[1]*offset,b[2]],n)
        for end,outward in ((a,(-dx,-dy)),(b,(dx,dy))):
            n=(-dy,dx);offset=path["width"]*.5-12
            rail_edge(path,[end[0]-n[0]*offset,end[1]-n[1]*offset,end[2]],
                      [end[0]+n[0]*offset,end[1]+n[1]*offset,end[2]],outward)
    errors=[];openings=set();route_points=0
    def footprint_hits_guard(path,box):
        if abs(box["position"][2]-path["a"][2]-62)>.1:return False
        a,b=path["a"],path["b"];length=math.dist(a,b)
        axis=((b[0]-a[0])/length,(b[1]-a[1])/length);side=(-axis[1],axis[0])
        yaw=math.radians(box["rotation"][2]);old_axis=(math.cos(yaw),math.sin(yaw));old_side=(-old_axis[1],old_axis[0])
        delta=(box["position"][0]-(a[0]+b[0])*.5,box["position"][1]-(a[1]+b[1])*.5)
        dot=lambda u,v:u[0]*v[0]+u[1]*v[1]
        for test in (axis,side,old_axis,old_side):
            extent=(length*.5+path["end_pad"])*abs(dot(axis,test))+path["width"]*.5*abs(dot(side,test))
            extent+=box["extent"][0]*abs(dot(old_axis,test))+box["extent"][1]*abs(dot(old_side,test))
            # GroundGuard trims 15 local cm beyond an intersecting deck edge.
            # The measured tower scales are below 1.2; include that existing
            # 18 cm join tolerance along the old rail, not a broad exemption.
            extent+=18*abs(dot(old_axis,test))
            if abs(dot(delta,test))>extent+.1:return False
        return True
    for box in boxes:
        if guard_box(box) and any(footprint_hits_guard(path,box) for path in plan["walks"]):openings.add(tuple(box["source"]))
    rail_boxes=[]
    for rail in plan["walk_rails"]:
        a,b=rail["a"],rail["b"]
        rail_boxes.append({"walk":rail["walk"],"position":[(a[0]+b[0])*.5,(a[1]+b[1])*.5,a[2]+rail["height"]*.5],
            "rotation":[0,0,math.degrees(math.atan2(b[1]-a[1],b[0]-a[0]))],
            "extent":[math.dist(a,b)*.5+1,rail["width"]*.5,rail["height"]*.5]})
    shafts=[]
    for district in snapshot["districts"]:
        cy=int(district["coord"].split(",")[1])
        for lift in district["lifts"]:
            shafts.append((lift["id"],lift["cabin"][0],lift["cabin"][1]+cy*CHUNK))
    for path in plan["walks"]:
        for p in sample(path["a"],path["b"]):
            route_points+=1;eye=[p[0],p[1],p[2]+88]
            for box in boxes:
                if abs(box["position"][2]-eye[2])>box["extent"][2]+100:continue
                if not overlap(eye,box):continue
                if guard_box(box):openings.add(tuple(box["source"]));continue
                # A floor at the same level is support, not an obstruction.
                if box["floor"] and box["position"][2]+box["extent"][2]<=p[2]+1:continue
                errors.append({"kind":"new-walk-hits-existing","walk":path["id"],"point":p,
                               "source":box["source"],"extent":box["extent"]})
                break
            for name,x,y in shafts:
                if math.hypot(p[0]-x,p[1]-y)<285:
                    errors.append({"kind":"new-walk-hits-lift-envelope","walk":path["id"],"lift":name,"point":p})
            for rail in rail_boxes:
                if overlap(eye,rail):errors.append({"kind":"new-walk-hits-new-rail","walk":path["id"],"rail":rail["walk"],"point":p})
    # Stone piers must avoid every old walkable floor, including those well
    # below the camera. Search a bounded longitudinal offset for each pier.
    for group,top,ys in (("North",3560,[12400,13900]),("North",7160,[13400,15200,18300,20600,24100])):
        for requested_y in ys:
            for preferred_x in (5750,7050):
                chosen=None
                for x_offset in (0,125,-125,250,-250):
                    if chosen:break
                    x=preferred_x+x_offset
                    for offset in (0,180,-180,360,-360,540,-540,720,-720):
                        y=requested_y+offset;clear=True
                        for box in boxes:
                            if not box["floor"]:continue
                            z=box["position"][2]+box["extent"][2]
                            if z<1450 or z>top+170:continue
                            q=local([x,y,z],box)
                            if abs(q[0])<box["extent"][0]+105 and abs(q[1])<box["extent"][1]+140:
                                clear=False;break
                        if clear and all(math.hypot(x-sx,y-sy)>365 for _,sx,sy in shafts):chosen=(x,y);break
                if chosen is None:errors.append({"kind":"pier-cannot-clear-old-floors","x":preferred_x,"y":requested_y});continue
                plan["supports"].append({"group":group,"x":chosen[0],"y":chosen[1],"bottom_z":1470,"top_z":top,"half_width":65,"half_depth":100})
    plan["views"]=[
        {"id":"arrival-wide-waterfall","eye":[6400,5150,2412],"target":[7300,12100,3000]},
        {"id":"clock-west-water","eye":[5700,5800,2412],"target":[4850,6400,1950]},
        {"id":"middle-garden-close","eye":[6400,12040,4212],"target":[6780,12700,3850]},
        {"id":"upper-garden-close","eye":[6200,12780,7812],"target":[6700,14200,7450]},
        {"id":"south-cascade","eye":[6950,50,2412],"target":[6950,-1260,1000]},
        {"id":"window-clock-water","eye":[3805,7857.1,2427],"target":[5650,6950,1950]}]
    plan["checks"]={"route_points":route_points,"expected_guard_openings":[list(p) for p in sorted(openings)],
                    "old_paths_unchanged":True,"old_lifts_unchanged":True,"failure_count":len(errors),"failures":errors[:100]}
    plan["success"]=not errors
    return plan


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--existing",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--cpp",type=Path)
    args=parser.parse_args()
    raw=args.existing.read_bytes();snapshot=json.loads(raw)
    plan=build(snapshot,hashlib.sha256(raw).hexdigest())
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    if not plan["success"]:
        print(json.dumps({"success":False,"checks":plan["checks"]},ensure_ascii=False,indent=2))
        raise SystemExit(1)
    if args.cpp:
        compact=json.dumps(plan,ensure_ascii=False,separators=(",",":"))
        chunks=[];cursor=0
        while cursor<len(compact):
            size=min(3000,len(compact)-cursor)
            while len(json.dumps(compact[cursor:cursor+size],ensure_ascii=False))>4000:size//=2
            chunks.append(json.dumps(compact[cursor:cursor+size],ensure_ascii=False));cursor+=size
        # Distinct array entries, not adjacent literals: MSVC also limits the
        # combined size of adjacent TEXT strings, including a single raw JSON.
        content='// Generated only by SourceArt/Scripts/water_city_layout.py.\n#pragma once\n#include "CoreMinimal.h"\nnamespace EW::WaterCityData\n{\ninline FString GetJson()\n{\n    static const TCHAR* const Chunks[] = {\n'
        content+=''.join('        TEXT('+piece+'),\n' for piece in chunks)
        content+='    };\n    FString Result;Result.Reserve('+str(len(compact))+');\n    for(const TCHAR* Chunk:Chunks)Result.Append(Chunk);\n    return Result;\n}\n}\n'
        args.cpp.parent.mkdir(parents=True,exist_ok=True);args.cpp.write_text(content,encoding="utf8")
    print(json.dumps({"success":True,"bodies":len(plan["bodies"]),"falls":len(plan["falls"]),
        "walks":len(plan["walks"]),"supports":len(plan["supports"]),"route_points":plan["checks"]["route_points"],
        "guard_openings":len(plan["checks"]["expected_guard_openings"]),"out":str(args.out)},ensure_ascii=False))


if __name__=="__main__":main()
