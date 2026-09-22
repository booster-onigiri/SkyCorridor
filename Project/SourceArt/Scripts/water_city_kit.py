"""Build only the 13 reference-water-city assets from a passing native audit.

Blender invocation: --background --python <this file> -- --layout <audit.json>.
The embedded plan is shared with runtime colliders. Existing FBX files, shared
material definitions and player data are never changed by this source step.
"""
from pathlib import Path
import argparse, hashlib, json, math, runpy, sys
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--layout",type=Path,required=True)
parser.add_argument("--walks-only",action="store_true",help="Rebuild only the walkway mesh and its catalog record")
parser.add_argument("--water-design-only",action="store_true",help="Rebuild water architecture, falls and foam while preserving the repaired floor")
args=parser.parse_args(sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else [])
if args.walks_only and args.water_design_only:raise ValueError("Choose one asset scope")
raw=args.layout.read_bytes();audit=json.loads(raw)
if audit.get("format")!="water-city-plan-audit-v1" or not audit.get("success"):
    raise RuntimeError("The complete native water-city plan audit must pass first")
plan=audit["plan"]
if plan.get("format")!="water-city-plan-v1" or not plan.get("success"):
    raise RuntimeError("Unsupported or incomplete shared water-city plan")
required={"all_new_walk_capsules_clear","old_clear_floor_samples_remain_clear",
          "0,0/all_other_old_colliders_preserved","other_world_not_modified"}
passed={check["name"] for check in audit["checks"] if check["pass"]}
if not required.issubset(passed):raise RuntimeError("Missing required native plan checks")

g=runpy.run_path(str(Path(__file__).with_name("mesh_primitives.py")))
Builder,export=g["Builder"],g["export"]
OUT,CATALOG=g["OUT"],g["CATALOG"]
existing=json.loads((OUT/"kit-catalog.json").read_text(encoding="utf8"))
families=["WaterCitySurface","WaterCityFall","WaterCityFoam","WaterCityStone"]
if plan["materials"]!=families:raise RuntimeError("Water-city material contract changed")
palette={"WaterCitySurface":(.07,.34,.31,1),"WaterCityFall":(.55,.82,.83,1),
         "WaterCityFoam":(.86,.94,.93,1),"WaterCityStone":(.77,.82,.74,1)}
# Replace the lists in place because Builder/Export retain those same objects.
# This Blender process has no old kit objects and exports only the new names.
g["FAMILIES"][:]=families;g["MATERIALS"][:]=[]
for family in families:
    g["COLOURS"][family]=palette[family]
    material=bpy.data.materials.new("M_"+family);material.diffuse_color=palette[family]
    material.use_nodes=True
    material.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value=palette[family]
    g["MATERIALS"].append(material)

groups={name:Builder() for name in plan["meshes"]}
origins={"Central":(0,0,0),"North":(0,12800,0),"South":(0,-12800,0)}
STONE=(.77,.82,.74,1);TRIM=(.88,.89,.78,1);BED=(.43,.56,.48,1)
COPPER=(.24,.43,.37,1)

def mesh(name,vertices,faces,family,colour=None,colours=None,smooth=False):
    """All input geometry is authored in the plan's global centimetres."""
    part=groups[name];start=len(part.v)
    part.mesh([tuple(float(v)/100 for v in point) for point in vertices],faces,family,colour,smooth=smooth)
    if colours is not None:
        if len(colours)!=len(vertices):raise ValueError("Colour/vertex count mismatch")
        part.col[start:]=colours

def box(name,position,dimensions,colour=STONE,bevel=2,yaw=0):
    groups[name].box(tuple(v/100 for v in position),tuple(v/100 for v in dimensions),
                     "WaterCityStone",bevel/100,yaw,colour)

def beam(name,a,b,width,bottom,top,colour=STONE,end_pad=0):
    dx=b[0]-a[0];dy=b[1]-a[1];length=math.hypot(dx,dy)
    if length<.01:raise ValueError("Empty bank, bridge or rail")
    box(name,((a[0]+b[0])*.5,(a[1]+b[1])*.5,(bottom+top)*.5),
        (length+2*end_pad,width,top-bottom),colour,2,math.atan2(dy,dx))

def polygon_prism(name,polygon,bottom,top,colour):
    count=len(polygon)
    vertices=[(x,y,z) for z in (bottom,top) for x,y in polygon]
    faces=[tuple(reversed(range(count))),tuple(range(count,2*count))]
    faces.extend((i,(i+1)%count,(i+1)%count+count,i+count) for i in range(count))
    mesh(name,vertices,faces,"WaterCityStone",colour)

def clip_half(poly,a,b):
    def distance(p):return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
    result=[]
    for i,p in enumerate(poly):
        q=poly[(i+1)%len(poly)];dp=distance(p);dq=distance(q)
        if dp>=-.001:result.append(p)
        if (dp<0)!=(dq<0):
            t=dp/(dp-dq);result.append((p[0]+(q[0]-p[0])*t,p[1]+(q[1]-p[1])*t))
    return result

def surface(body):
    poly=body["polygon"];step=145
    lo=[min(p[i] for p in poly) for i in range(2)];hi=[max(p[i] for p in poly) for i in range(2)]
    fx,fy=body["flow"];encoded=(fx*.5+.5,fy*.5+.5,.45,1)
    vertices=[];faces=[]
    for ix in range(math.floor(lo[0]/step),math.ceil(hi[0]/step)):
        for iy in range(math.floor(lo[1]/step),math.ceil(hi[1]/step)):
            patch=[(ix*step,iy*step),((ix+1)*step,iy*step),((ix+1)*step,(iy+1)*step),(ix*step,(iy+1)*step)]
            for index,a in enumerate(poly):
                if not patch:break
                patch=clip_half(patch,a,poly[(index+1)%len(poly)])
            # The same clipping can produce coincident corners on grid edges.
            clean=[]
            for p in patch:
                if not clean or math.dist(clean[-1],p)>.005:clean.append(p)
            if len(clean)>1 and math.dist(clean[0],clean[-1])<.005:clean.pop()
            if len(clean)<3:continue
            area=sum(clean[i][0]*clean[(i+1)%len(clean)][1]-clean[(i+1)%len(clean)][0]*clean[i][1] for i in range(len(clean)))
            if area<.1:continue
            first=len(vertices);vertices.extend((x,y,body["water_z"]) for x,y in clean)
            faces.append(tuple(first+i for i in range(len(clean))))
    mesh("WaterCity"+body["group"]+"Water",vertices,faces,"WaterCitySurface",encoded,smooth=True)

def foam_patch(group,cx,cy,z,width,depth,angle=0,phase=0):
    """A transparent oval carrying local coordinates, never a solid white slab."""
    count=48;vertices=[(cx,cy,z)];colours=[(.5,.5,phase,1)]
    for i in range(count):
        a=i*math.tau/count;u=math.cos(a);v=math.sin(a)
        x=u*width*.5;y=v*depth*.5
        vertices.append((cx+x*math.cos(angle)-y*math.sin(angle),cy+x*math.sin(angle)+y*math.cos(angle),z))
        colours.append((u*.5+.5,v*.5+.5,phase,1))
    mesh("WaterCity"+group+"Foam",vertices,[(0,1+i,1+(i+1)%count) for i in range(count)],
         "WaterCityFoam",colours=colours,smooth=True)

for body in plan["bodies"]:
    stone="WaterCity"+body["group"]+"Stone"
    polygon_prism(stone,body["polygon"],body["water_z"]-body["depth"]-55,body["water_z"]-body["depth"],BED)
    surface(body)
    # Shallow edge ripples make scale and current legible without covering the
    # whole water body in foam. Fall impact patches are authored separately.
    poly=body["polygon"];cx=sum(p[0] for p in poly)/len(poly);cy=sum(p[1] for p in poly)/len(poly)
    for side in (-1,1):
        foam_patch(body["group"],cx+side*610,cy,body["water_z"]+5,330,145,side*.17,.12)

for bank in plan["banks"]:
    name="WaterCity"+bank["group"]+"Stone";a,b=bank["a"],bank["b"]
    beam(name,a,b,bank["width"],bank["bottom_z"],bank["top_z"]-20,STONE)
    beam(name,a,b,bank["width"]+9,bank["top_z"]-20,bank["top_z"],TRIM)
    # Long water walls have real masonry joints and a narrow wet tide line.
    length=math.dist(a,b);count=max(1,math.ceil(length/210))
    for i in range(1,count):
        p=[a[j]+(b[j]-a[j])*i/count for j in range(3)]
        box(name,(p[0],p[1],bank["top_z"]-95),(12,bank["width"]+1,110),COPPER,1,
            math.atan2(b[1]-a[1],b[0]-a[0]))

for index,fall in enumerate(plan["falls"]):
    name="WaterCity"+fall["group"]+"Fall";x0,x1=fall["x0"],fall["x1"]
    top,bottom=fall["top_z"],fall["bottom_z"]
    width=x1-x0;nx=24;nz=max(16,math.ceil((top-bottom)/95))
    vertices=[];colours=[];faces=[]
    for iz in range(nz+1):
        t=iz/nz
        # The lip bends out into a free sheet, leaving its dry backing clear.
        y=fall["start_y"]+(fall["end_y"]-fall["start_y"])*(t**.34)
        for ix in range(nx+1):
            u=ix/nx;x=x0+width*u
            ripple=math.sin(u*math.tau*4+index)*2.5*math.sin(math.pi*t)
            vertices.append((x,y+ripple,top+(bottom-top)*t))
            colours.append((u,t,(top-bottom)/10000,1))
    for iz in range(nz):
        for ix in range(nx):
            a=iz*(nx+1)+ix;faces.append((a,a+nx+1,a+nx+2,a+1))
    mesh(name,vertices,faces,"WaterCityFall",colours=colours,smooth=True)
    stone="WaterCity"+fall["group"]+"Stone"
    # An open spillway replaces the full-height backing slab. Only a low
    # footing meets the receiving basin; the water has real air behind it.
    box(stone,((x0+x1)*.5,fall["start_y"]+110,bottom-100),
        (width,140,320),(.42,.54,.47,1),4)
    # Above-water piers remain outside the clear spill width.
    for x in (x0-42,x1+42):
        box(stone,(x,fall["start_y"]+50,top+28),(50,100,85),TRIM,3)
    foam_patch(fall["group"],(x0+x1)*.5,fall["end_y"]-120,bottom+7,
               fall["foam_width"],fall["foam_depth"],0,.50+index*.065)
    for side in (-1,1):
        foam_patch(fall["group"],(x0+x1)*.5+side*145,fall["end_y"]-320,bottom+8,
                   330,220,side*.25,.32+index*.07)

for support in plan["supports"]:
    name="WaterCity"+support["group"]+"Stone";x,y=support["x"],support["y"]
    box(name,(x,y,(support["top_z"]+support["bottom_z"])*.5),
        (support["half_width"]*2,support["half_depth"]*2,support["top_z"]-support["bottom_z"]),STONE,5)
    # Profiles stay inside the matching solid collision envelope.
    for z in (support["bottom_z"]+20,support["top_z"]-20):
        box(name,(x,y,z),(support["half_width"]*2,support["half_depth"]*2,38),TRIM,2)

# Supported water gardens and framed spillways share their solid envelopes
# and planting positions with native streaming/collision through one header.
garden=runpy.run_path(str(Path(__file__).with_name("water_city_gardens.py")))
garden_report=garden["build"](plan,groups,mesh,box,beam,foam_patch,OUT) if not args.walks_only else {}

# The old coloured boxes shared their top with the white deck and other walks.
# Partition the complete top at each level, including all crossing end pads.
# Its single 1 cm offset also separates it from the existing plaza deck beneath.
walk_geometry=runpy.run_path(str(Path(__file__).with_name("walk_floor_geometry.py")))
walk_levels=walk_geometry["floor_patches"](plan["walks"])
for level in walk_levels:
    for family,colour in (("stone",TRIM),("copper",COPPER)):
        for poly in level[family]:
            count=len(poly)
            vertices=[(x,y,z) for z in (level["bottom_z"],level["top_z"]) for x,y in poly]
            faces=[tuple(reversed(range(count))),tuple(range(count,2*count))]
            # Only the perimeter needs vertical walls. Inlay and intersection
            # seams have one continuous top and no coincident internal walls.
            for i,a in enumerate(poly):
                j=(i+1)%count;b=poly[j];dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
                outside=((a[0]+b[0])/2+dy/length*.001,(a[1]+b[1])/2-dx/length*.001)
                if not any(walk_geometry["contains"](d,outside) for d in level["decks"]):
                    faces.append((i,j,j+count,i+count))
            mesh("WaterCityWalks",vertices,faces,"WaterCityStone",colour)

for rail in plan["walk_rails"]:
    a,b=rail["a"],rail["b"];z=a[2];h=rail["height"]
    beam("WaterCityWalks",a,b,rail["width"],z+h-16,z+h,COPPER)
    beam("WaterCityWalks",a,b,rail["width"]*.65,z+27,z+38,COPPER)
    count=max(1,math.ceil(math.dist(a,b)/110))
    for i in range(count+1):
        t=i/count
        box("WaterCityWalks",(a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,z+h*.5),
            (rail["width"]*.65,rail["width"]*.65,h),STONE,1.2)

# A crossed soft droplet, instanced by AEWWaterCity at most 72 times. Red/green
# hold patch coordinates; the shader makes its edges transparent in both views.
for axis in (0,1):
    vertices=[(-5,0,-9),(5,0,-9),(5,0,9),(-5,0,9)]
    if axis:vertices=[(y,x,z) for x,y,z in vertices]
    mesh("WaterCityDrop",vertices,[(0,1,2,3)],"WaterCityFoam",
         colours=[(0,0,.8,1),(1,0,.8,1),(1,1,.8,1),(0,1,.8,1)],smooth=True)

# Test actual new stone polygons before any FBX is exported. These viewpoints
# avoid the clock itself and the dry back of a weir. Old-world visual occlusion
# remains a runtime acceptance check, distinct from native capsule clearance.
views=[
    {"id":"wide-east-waterfall","eye":[7100,5200,2412],"target":[7300,12100,3000]},
    {"id":"clock-west-water","eye":[5700,5800,2412],"target":[4850,6400,1950]},
    {"id":"middle-garden-water","eye":[6400,12200,4212],"target":[6780,12900,3850]},
    {"id":"upper-garden-water","eye":[6200,12780,7812],"target":[6700,14200,7450]},
    {"id":"south-cascade-front","eye":[7100,-1850,2412],"target":[6950,-1240,1350]}]
opaque_v=[];opaque_f=[]
for name,part in groups.items():
    if not name.endswith("Stone") and name!="WaterCityWalks":continue
    first=len(opaque_v);opaque_v.extend(part.v)
    opaque_f.extend(tuple(first+i for i in face) for face in part.f)
tree=BVHTree.FromPolygons(opaque_v,opaque_f,all_triangles=False)
checks=[]
for view in views:
    start=Vector(tuple(v/100 for v in view["eye"]));end=Vector(tuple(v/100 for v in view["target"]));delta=end-start
    hit,normal,face,distance=tree.ray_cast(start,delta.normalized(),delta.length-.001)
    checks.append({**view,"clear":hit is None,"hit_cm":[float(v)*100 for v in hit] if hit is not None else None})
source_report={"format":"water-city-source-audit-v1","success":all(c["clear"] for c in checks),
    "native_audit":str(args.layout.resolve()),"native_audit_sha256":hashlib.sha256(raw).hexdigest(),
    "opaque_polygon_rays":checks,"old_visual_occlusion":"RUNTIME_NOT_YET_CHECKED"}
source_report["walk_surfaces"]=[{k:v for k,v in level.items() if k not in ("decks","inlays")} for level in walk_levels]
(OUT/"water-city-source-audit.json").write_text(json.dumps(source_report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
if not source_report["success"]:
    print("EW_WATER_CITY_SOURCE_RAY_FAILURE",json.dumps(checks),flush=True)
    raise RuntimeError("Actual new water-city stone blocks a required viewing ray")

for name,part in groups.items():
    if args.walks_only and name!="WaterCityWalks":continue
    if args.water_design_only and (name=="WaterCityWalks" or name=="WaterCityDrop" or name.endswith("Water")):continue
    if not part.v or not part.f:raise RuntimeError("Empty water-city asset: "+name)
    group=next((p for p in origins if name.startswith("WaterCity"+p)),None)
    origin=origins[group] if group else (0,0,0)
    expected=[tuple(v[i]-origin[i]/100 for i in range(3)) for v in part.v]
    part.v=[(x,-y,z) for x,y,z in expected];part.f=[tuple(reversed(face)) for face in part.f]
    export(name,part,"Shared water-city plan v1; authored water, exposed falls, stone and accessible quays")
    item=CATALOG[-1];item["nanite"]=name.endswith("Stone") or name=="WaterCityWalks"
    item["coordinate_contract"]="water-city-unreal-cm-v1"
    item["ue_bounds_cm"]=[[min(v[i] for v in expected)*100 for i in range(3)],
                          [max(v[i] for v in expected)*100 for i in range(3)]]
    indices=sorted({0,len(expected)//4,len(expected)//2,len(expected)*3//4,len(expected)-1})
    item["ue_anchor_vertices_cm"]=[{"name":"source_vertex_"+str(i),"position":[float(v)*100 for v in expected[i]]} for i in indices]
    item["global_origin_cm"]=origin
    # The Blender source itself shows its intended complete world placement.
    obj=bpy.data.objects["SM_"+name];obj.location=(origin[0]/100,-origin[1]/100,origin[2]/100)

new_names={item["name"] for item in CATALOG}
expected_names={"WaterCityWalks"} if args.walks_only else {n for n in plan["meshes"] if n!="WaterCityWalks" and n!="WaterCityDrop" and not n.endswith("Water")} if args.water_design_only else set(plan["meshes"])
if new_names!=expected_names:raise RuntimeError("Unexpected water-city asset set")
existing["assets"]=[item for item in existing["assets"] if item["name"] not in new_names]+CATALOG
existing["materials"]=list(dict.fromkeys(existing["materials"]+families))
existing["triangles"]=sum(item["triangles"] for item in existing["assets"])
water_art={"revision":1,"source":"WaterCity.blend","assets":sorted(plan["meshes"]),
    "materials":families,"coordinate_contract":"water-city-unreal-cm-v1","material_contract":"water-city-material-v1",
    "native_audit_sha256":hashlib.sha256(raw).hexdigest(),"baseline_sha256":plan["baseline_sha256"],
    "plan_sha256":hashlib.sha256(json.dumps(plan,sort_keys=True,separators=(",",":"),ensure_ascii=False).encode("utf8")).hexdigest(),
    "native_route_points":audit["new_route_points"],"protected_floor_points":audit["protected_floor_points"],
    "source_opaque_polygon_rays":checks,"drop_instance_limit":72,"drop_hz":30}
if not args.walks_only and not args.water_design_only:existing["water_city_art"]=water_art
if args.water_design_only:
    existing["water_city_art"]["water_design_revision"]=3
    existing["water_city_art"]["water_design_source"]="WaterCityGardens.blend"
    existing["water_city_art"]["gardens"]=garden_report
existing["water_city_art"]["walk_floor_revision"]=2
if not args.water_design_only:existing["water_city_art"]["walk_floor_source"]="WaterCityWalks.blend" if args.walks_only else "WaterCity.blend"
existing["water_city_art"]["walk_floor_surface_lift_cm"]=walk_geometry["SURFACE_LIFT_CM"]
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/("WaterCityWalks.blend" if args.walks_only else "WaterCityGardens.blend" if args.water_design_only else "WaterCity.blend")))
(OUT/"kit-catalog.json").write_text(json.dumps(existing,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
print("EW_WATER_CITY_COMPLETE",json.dumps({"meshes":len(CATALOG),"triangles":sum(i["triangles"] for i in CATALOG),
    "source_rays":len(checks),"native_route_points":audit["new_route_points"],"protected_floor_points":audit["protected_floor_points"]}),flush=True)
