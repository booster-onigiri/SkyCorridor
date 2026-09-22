"""Create only the five assets for the reference city's rain-window residence.

Run the EWResidenceAudit commandlet first, then pass its report after Blender's
`--`: --layout <new report.json>. No original kit mesh is re-exported. The source
block's actual variant, opening side and floor come from the C++ recipe report.
"""
from pathlib import Path
import ast, argparse, json, math, random, runpy, sys
import bpy
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

parser=argparse.ArgumentParser()
parser.add_argument("--layout",type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else [])
report=json.loads(args.layout.read_text(encoding="utf8"))
if not report.get("success"):raise RuntimeError("Residence recipe audit must pass before creating its meshes")
layout=report["layout"]
if layout.get("format")!="rain-window-layout-v1":raise RuntimeError("Unsupported residence layout")
side=int(layout["opening_side"]);floor=float(layout["opening_floor_m"])
variant=int(layout["block_variant"])
if side not in range(4) or floor not in (0.,18.) or variant not in range(8):raise RuntimeError("Invalid residence opening")

g=runpy.run_path(str(Path(__file__).with_name("mesh_primitives.py")))
Builder,export,column=g["Builder"],g["export"],g["column"]
OUT,CATALOG=g["OUT"],g["CATALOG"]
existing=json.loads((OUT/"kit-catalog.json").read_text(encoding="utf8"))
STONE=(.80,.84,.82,1);TRIM=(.91,.92,.85,1);SHADOW=(.045,.095,.115,1)
GLASS=(.12,.27,.32,1);COPPER=(.21,.44,.42,1);GOLD=(.55,.36,.14,1)
TIMBER=(.78,.58,.36,1);WATER=(.11,.39,.38,1)
# These slots belong only to newly authored residence surfaces. The inherited
# block keeps every original family index and shared material untouched.
ROOM_FINISHES={"Timber":"RainTimber","Paint":"RainPlaster","Ceramic":"RainCeramic","Cloth":"RainCloth"}
for original,family in ROOM_FINISHES.items():
    g["FAMILIES"].append(family)
    g["COLOURS"][family]=g["COLOURS"][original]
    mat=bpy.data.materials.new("M_"+family);mat.diffuse_color=g["COLOURS"][family]
    mat.use_nodes=True;mat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value=g["COLOURS"][family]
    g["MATERIALS"].append(mat)

def residence_finishes(part):
    remap={g["FAMILIES"].index(source):g["FAMILIES"].index(target) for source,target in ROOM_FINISHES.items()}
    part.m=[remap.get(index,index) for index in part.m]
    return part
# Import just the pure builders. Running city_volume.py itself would re-export
# every building and overwrite source assets unrelated to this one room.
source=Path(__file__).with_name("city_volume.py")
tree=ast.parse(source.read_text(encoding="utf8"),filename=str(source))
names={"merge","box","balustrade","hanging","window","facade","urban_block"}
defs=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in names]
if {node.name for node in defs}!=names:raise RuntimeError("City builder definitions changed; update this explicit import")
exec(compile(ast.Module(body=defs,type_ignores=[]),str(source),"exec"),globals())

def unreal_to_blender(part):
    """Precompensate the measured FBX import (x,y,z)m -> (x,-y,z)*100cm.

    Only newly authored UE-coordinate geometry uses this function. Reflecting
    an existing complete city block would alter its inherited facades.
    """
    part.v=[(x,-y,z) for x,y,z in part.v]
    part.f=[tuple(reversed(face)) for face in part.f]
    return part

def tagged_export(name,part,notes,anchors=None,already_blender=False,clear_segments=None):
    expected=list(part.v) if not already_blender else [(x,-y,z) for x,y,z in part.v]
    if not already_blender:unreal_to_blender(part)
    checks=[]
    if clear_segments:
        # Test the actual source polygons in the intended UE asset frame, not
        # just the simpler collision boxes. This catches an uncleared pane,
        # facade trim, arcade column or room wall before importing.
        tree=BVHTree.FromPolygons(expected,part.f,all_triangles=False)
        for label,start,end in clear_segments:
            a=Vector(start);delta=Vector(end)-a
            hit,normal,face,distance=tree.ray_cast(a,delta.normalized(),delta.length)
            check={"name":label,"start_ue_cm":[v*100 for v in start],"end_ue_cm":[v*100 for v in end],
                   "clear":hit is None,"hit_ue_cm":[float(v)*100 for v in hit] if hit is not None else None,
                   "hit_face":face,"distance_cm":float(distance)*100 if distance is not None else None}
            checks.append(check)
        if not all(c["clear"] for c in checks):
            print("EW_RAIN_WINDOW_SOURCE_RAY_FAILURE",json.dumps(checks),flush=True)
            raise RuntimeError("Actual residence polygons block a required eye/entry ray")
    export(name,part,notes)
    item=CATALOG[-1]
    item["ue_bounds_cm"]=[[min(v[i] for v in expected)*100 for i in range(3)],
                          [max(v[i] for v in expected)*100 for i in range(3)]]
    if anchors is None:
        indices=sorted({0,len(expected)//4,len(expected)//2,len(expected)*3//4,len(expected)-1})
        anchors=[("surface_vertex_"+str(i),expected[i]) for i in indices]
    item["ue_anchor_vertices_cm"]=[{"name":label,"position":[float(v)*100 for v in p]} for label,p in anchors]
    item["coordinate_contract"]="rain-window-unreal-cm-v2"
    item["source_clear_segment_checks"]=checks

def pipe(b,points,r=.065,family="Paint",colour=COPPER):
    b.tube(points,[r]*len(points),family,12,colour)
def trough(b,points,width=.34,water_radius=.075):
    """Open U section whose low lips leave the water visible from the gallery.

    Points follow the water center. The floor meets the stream bottom, while
    each lip stops 2.5 cm below the center instead of enclosing the flow.
    """
    pts=[Vector(p) for p in points];half=width*.5;vertices=[];faces=[]
    profile=[(-half,-water_radius-.06),(half,-water_radius-.06),(half,-.025),
             (half-.04,-.025),(half-.04,-water_radius),(-half+.04,-water_radius),
             (-half+.04,-.025),(-half,-.025)]
    for index,p in enumerate(pts):
        tangent=pts[min(len(pts)-1,index+1)]-pts[max(0,index-1)]
        tangent.z=0
        if tangent.length<.001:raise RuntimeError("A trough segment cannot be vertical")
        tangent.normalize();across=Vector((-tangent.y,tangent.x,0))
        vertices.extend(tuple(p+across*q+Vector((0,0,z))) for q,z in profile)
    for segment in range(len(pts)-1):
        for j in range(8):
            a=segment*8+j;c=segment*8+(j+1)%8
            faces.append((a,c,c+8,a+8))
    faces.extend([tuple(reversed(range(8))),tuple((len(pts)-1)*8+j for j in range(8))])
    b.mesh(vertices,faces,"Ceramic",COPPER,smooth=False)
def pot(b,p,seed):
    b.lathe([(.22,0),(.30,.12),(.30,.52),(.34,.57),(.29,.64),(.22,.64)],p,"Ceramic",32,COPPER)
    box(b,(p[0],p[1],p[2]+.60),(.40,.40,.045),"Soil",(.09,.07,.035,1))
    rng=random.Random(seed)
    for j in range(9):
        a=j*math.tau/9
        tip=(p[0]+math.cos(a)*.40,p[1]+math.sin(a)*.40,p[2]+.88+rng.random()*.42)
        pipe(b,[(p[0],p[1],p[2]+.60),tip],.012,"Bark",(.12,.17,.06,1))
        b.leaf(tip,.42,.20,a,-.25,"Foliage",(.15,.34+rng.random()*.09,.18,1))

room=Builder()
box(room,(4.5,-15,-.08),(9,6,.16),"Timber",TIMBER)
box(room,(4.5,-15,3.48),(9,6,.16))
box(room,(.07,-14.3,1.70),(.14,3.4,3.4),"Paint",(.72,.76,.66,1))
box(room,(8.93,-14.3,1.70),(.14,3.4,3.4),"Paint",(.72,.76,.66,1))
box(room,(4.5,-12.04,1.70),(9,.16,3.4),"Paint",(.72,.76,.66,1))
for x in [1.2,3.4,5.6,7.8]:box(room,(x,-14.3,3.34),(.13,3.4,.16),"Timber",TIMBER)
# One seat, deliberately without loose books, cups or fake pickup objects.
box(room,(6.50,-17.10,.815),(2.2,.76,.13),"Timber",TIMBER)
for x in [5.65,7.35]:
    for y in [-17.36,-16.84]:box(room,(x,y,.40),(.13,.13,.80),"Timber",TIMBER)
box(room,(6.50,-17.12,.935),(1.7,.62,.10),"Cloth",(.63,.42,.23,1))
box(room,(6.50,-16.77,1.105),(2.15,.13,.56),"Timber",TIMBER)
pot(room,(.85,-13,0),194);pot(room,(8.55,-16.20,0),752)
box(room,(3.4,-12.16,2.15),(.70,.20,.16),"Paint",COPPER)
box(room,(3.4,-12.30,2.35),(.46,.18,.40),"Glow",(.68,.40,.12,1))
for x in [1.0,3.5]:
    box(room,(x,-18.24,1.45),(.15,.18,2.9),"Timber",TIMBER)
box(room,(2.25,-18.24,2.90),(2.7,.18,.15),"Timber",TIMBER)
box(room,(2.25,-18.39,2.61),(.40,.12,.27),"Glow",(.65,.39,.12,1))

# Visible source, selector body, and two separate routes. The selector's
# handle is a runtime component; neither branch controls an existing lift.
box(room,(-2.25,-19.80,.14),(1.5,.80,.28))
box(room,(-2.25,-19.80,.78),(1.03,.65,1.12),"Paint",COPPER)
pipe(room,[(-2.25,-19.80,1.15),(-2.25,-19.80,3.42)],.07,"Bronze",GOLD)
pipe(room,[(-2.25,-19.80,1.15),(-2.25,-20.40,1.15)],.08,"Bronze",GOLD)
for x in [-2.68,-1.82]:box(room,(x,-20.15,1.37),(.15,.12,.15),"Ceramic",(.80,.78,.61,1))
box(room,(-2.25,-19.50,3.29),(1.0,.75,.16),"Ceramic",COPPER)
for y in [-19.86,-19.14]:box(room,(-2.25,y,3.45),(1.0,.045,.28),"Ceramic",COPPER,.01)
pipe(room,[(-2.25,-17.0,4.08),(-2.25,-19.50,4.08),(-2.25,-19.50,3.90)],.13)
trough(room,[(-2.25,-19.50,3.37),(8.70,-19.50,3.37),(8.70,-18.55,3.37)])
# The elevated window seat occupies y=-17.40. Keep the water 65 cm forward,
# immediately behind the external guard, so it never cuts through the cushion.
# The folded right casement reaches y=-18.34. Bring only the open fall 50 cm
# ahead of the unchanged window basin, so the gallery can see past that leaf.
pipe(room,[(8.70,-18.55,3.33),(8.70,-18.55,3.20)],.115)
pipe(room,[(8.70,-18.55,1.04),(8.70,-18.55,1.17)],.115)
trough(room,[(8.70,-18.55,1.04),(8.70,-18.05,1.04)])
trough(room,[(5.55,-18.05,1.01),(8.70,-18.05,1.01)],.34,.035)
pipe(room,[(5.55,-18.05,.9),(5.55,-18.05,.30),(5.55,-18.44,.30),(5.55,-18.44,-17.4)],.12)
trough(room,[(-2.25,-19.50,3.37),(-2.25,-24.1,3.27),(-2.25,-25.1,2.70)],.36,.085)
for x in [-2.7,-1.8]:
    pipe(room,[(x,-18.2,0),(x,-24.4,-1.6)],.065,"Bronze",GOLD)
box(room,(-2.25,-25.1,-17.63),(1.6,1.2,.30),"Ceramic",COPPER)
for s in [-1,1]:box(room,(-2.25+s*.76,-25.1,-17.36),(.12,1.2,.38),"Ceramic",COPPER)
pipe(room,[(-2.25,-25.1,-17.6),(-2.25,-25.1,-18.0),(-2.25,-18.44,-18.0),(5.55,-18.44,-18.0)],.10)

# The source facade tangent reverses at import. Choose the source side whose
# outward normal maps to the requested UE side, and mirror only its two new
# opening bays. Every untouched facade/window keeps the original source mesh.
source_side=(2-side)%4
b=Builder();urban_block(b,variant,source_side,floor,opening_mirrored=True)
references=[("entry_left_post",(1.,-18.24,1.45)),("entry_right_post",(3.5,-18.24,1.45)),
            ("seat_cushion",(6.5,-17.12,.985)),("valve_body",(-2.25,-19.8,.78)),
            ("lamp",(3.4,-12.3,2.35)),("planter",(.85,-13,.64)),
            ("window_trough",(7.1,-18.05,.94)),("lower_basin",(-2.25,-25.1,-17.36))]
rotation=Matrix.Rotation(side*math.pi*.5,3,"Z")
anchors=[]
for label,reference in references:
    p=min(room.v,key=lambda p:sum((p[i]-reference[i])**2 for i in range(3)))
    anchors.append((label,tuple(rotation@Vector(p)+Vector((0,0,floor)))))
placed_room=Builder();merge(placed_room,residence_finishes(room),p=(0,0,floor),yaw=side*math.pi*.5)
unreal_to_blender(placed_room);merge(b,placed_room)
def chunk_to_asset(point):
    yaw=math.radians(float(layout["block_rotation_euler"][2]))
    p=Vector(point)-Vector(layout["block_position_chunk_cm"])
    p=Matrix.Rotation(-yaw,3,"Z")@p
    return tuple(p[i]/float(layout["block_scale"][i])/100 for i in range(3))
seat_eye=Vector(layout["seat_chunk_cm"])+Vector((0,0,47))
start_eye=(6400,5150,float(layout["public_floor_z_cm"])+162)
entry_eye=rotation@Vector((2.25,-20.90,1.62))+Vector((0,0,floor))
inside_eye=rotation@Vector((2.25,-14.50,1.62))+Vector((0,0,floor))
gallery_eye=rotation@Vector((-11.,-23.,1.62))+Vector((0,0,floor))
eaves_water=rotation@Vector((-2.25,-24.5,3.1185))+Vector((0,0,floor))
window_water=rotation@Vector((8.70,-18.615,2.20))+Vector((0,0,floor))
clear_segments=[("seat_eye_to_clock_hand",chunk_to_asset(seat_eye),chunk_to_asset(layout["view_target_chunk_cm"])),
                ("seat_eye_to_arrival_eye",chunk_to_asset(seat_eye),chunk_to_asset(start_eye)),
                ("entry_eye_to_room",tuple(entry_eye),tuple(inside_eye)),
                ("gallery_eye_to_eaves_water",tuple(gallery_eye),tuple(eaves_water)),
                ("gallery_eye_to_window_fall",tuple(gallery_eye),tuple(window_water))]
tagged_export("UrbanBlock_RainWindow",b,"Original reference-city block with one open room, one seat and the rain-water selector; feature revision 1",
              anchors=anchors,already_blender=True,clear_segments=clear_segments)

b=Builder()
pipe(b,[(0,0,-.47),(0,0,.47)],.055,"Bronze",GOLD)
box(b,(0,0,.47),(.45,.13,.14),"Timber",TIMBER)
box(b,(0,0,-.47),(.18,.16,.17),"Ceramic",(.83,.79,.61,1))
tagged_export("RainValveHandle",residence_finishes(b),"Two-stop selector handle around local Y; pivot placement is owned by AEWResidence")

b=Builder();pipe(b,[(-2.25,-19.50,3.90),(-2.25,-19.50,3.45)],.075,"Water",WATER)
tagged_export("RainWaterCommon",b,"Visible water entering the selector, in the residence floor frame")
b=Builder()
pipe(b,[(-2.25,-19.50,3.37),(-2.25,-24.1,3.27),(-2.25,-25.1,2.7),(-2.25,-25.1,-17.12)],.085,"Water",WATER)
box(b,(-2.25,-25.1,-17.22),(1.30,.92,.055),"Water",WATER,.01)
tagged_export("RainWaterEaves",b,"Outside branch and its lower catch basin; visible only in flow mode zero")
b=Builder()
pipe(b,[(-2.25,-19.50,3.37),(8.70,-19.50,3.37),(8.70,-18.55,3.37),
        (8.70,-18.55,1.04),(8.70,-18.05,1.04)],.075,"Water",WATER)
box(b,(7.125,-18.05,1.01),(3.15,.23,.04),"Water",WATER,.008)
tagged_export("RainWaterWindow",b,"Window-side branch and the indoor water surface; visible only in flow mode one")

new_names={item["name"] for item in CATALOG}
expected={"UrbanBlock_RainWindow","RainValveHandle","RainWaterCommon","RainWaterEaves","RainWaterWindow"}
if new_names!=expected:raise RuntimeError("Unexpected asset set in residence-only generation")
existing["assets"]=[item for item in existing["assets"] if item["name"] not in new_names]+CATALOG
existing["materials"]=list(dict.fromkeys(existing["materials"]+list(ROOM_FINISHES.values())))
existing["triangles"]=sum(item["triangles"] for item in existing["assets"])
existing["rain_window_art"]={"assets":sorted(new_names),"source":"RainWindow.blend","layout":layout,"revision":1,
                            "coordinate_contract":"rain-window-unreal-cm-v2","source_opening_side":source_side,
                            "source_opening_mirrored":True,"window_trough_y_cm":-1805,
                            "residence_materials":list(ROOM_FINISHES.values()),"finish_contract":"rain-window-finish-v1",
                            "open_troughs":True,"window_exposed_fall_cm":[117,320],"window_exposed_fall_y_cm":-1855}
(OUT/"kit-catalog.json").write_text(json.dumps(existing,ensure_ascii=False,indent=2),encoding="utf8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"RainWindow.blend"))
print("EW_RAIN_WINDOW_KIT_COMPLETE",len(CATALOG),sum(item["triangles"] for item in CATALOG),flush=True)
