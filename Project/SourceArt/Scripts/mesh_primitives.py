"""Build original modular assets in metres, retaining a reproducible Blender source.

Textures are the original PBR surfaces from the preceding AerialCity project.
No network, model inference, reference-image pixels or external art packages are used.
"""
from pathlib import Path
import bpy, bmesh, math, random, json, time, hashlib, sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import craft_quality93 as craft
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "SourceArt"
MESHES = OUT / "Meshes"
MESHES.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
for block in list(bpy.data.meshes):
    if block.users == 0:
        bpy.data.meshes.remove(block)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 1.0

FAMILIES = ["Limestone", "Bronze", "Timber", "Paint", "Foliage", "Bark",
            "Cloth", "Ceramic", "Leather", "Paper", "Soil", "Fruit", "Petal",
            "Pollen", "Crystal", "Water", "Glow"]
COLOURS = {
    "Limestone": (.87, .84, .74, 1), "Bronze": (.55, .34, .13, 1),
    "Timber": (.45, .27, .13, 1), "Paint": (.60, .74, .69, 1),
    "Foliage": (.22, .45, .12, 1), "Bark": (.26, .17, .09, 1),
    "Cloth": (.89, .85, .65, 1), "Ceramic": (.25, .58, .53, 1),
    "Leather": (.21, .31, .30, 1), "Paper": (.93, .87, .65, 1),
    "Soil": (.25, .23, .18, 1), "Fruit": (.78, .22, .06, 1),
    "Petal": (.81, .52, .61, 1), "Pollen": (.87, .67, .24, 1),
    "Crystal": (.23, .52, .64, 1), "Water": (.07, .38, .35, 1),
    "Glow": (.45, .86, .62, 1),
}
MATERIALS = []
for family in FAMILIES:
    mat = bpy.data.materials.new("M_" + family)
    mat.diffuse_color = COLOURS[family]
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = COLOURS[family]
    bsdf.inputs["Roughness"].default_value = .24 if family in ("Bronze", "Ceramic", "Crystal", "Water") else .65
    if family == "Bronze":
        bsdf.inputs["Metallic"].default_value = .75
    MATERIALS.append(mat)

BOX_CACHE = {}
def beveled_box(dimensions, bevel):
    key = tuple(round(float(v), 5) for v in dimensions) + (round(bevel, 5),)
    if key not in BOX_CACHE:
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1)
        for v in bm.verts:
            v.co.x *= dimensions[0]; v.co.y *= dimensions[1]; v.co.z *= dimensions[2]
        if bevel > .0001:
            bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(bevel, min(dimensions) * .2),
                           segments=3, affect="EDGES", clamp_overlap=True)
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        bm.verts.ensure_lookup_table(); bm.verts.index_update()
        BOX_CACHE[key] = ([tuple(v.co) for v in bm.verts], [tuple(v.index for v in f.verts) for f in bm.faces])
        bm.free()
    return BOX_CACHE[key]

class Builder:
    def __init__(self):
        self.v=[]; self.f=[]; self.m=[]; self.col=[]; self.smooth=[]
    def mesh(self, vertices, faces, family="Limestone", colour=None, position=(0,0,0), rotation=None, smooth=True):
        offset=len(self.v); p=Vector(position); c=colour or COLOURS[family]
        for v in vertices:
            q=Vector(v)
            if rotation is not None: q=rotation @ q
            self.v.append(tuple(q+p)); self.col.append(c)
        self.f.extend(tuple(offset+i for i in face) for face in faces)
        self.m.extend([FAMILIES.index(family)]*len(faces)); self.smooth.extend([smooth]*len(faces))
    def box(self, p, d, family="Limestone", bevel=.02, yaw=0, colour=None):
        if family in ('Cloth','InteriorFabric','Aero87Fabric','Sky92Cloth') and min(d)>=.075 and max(d)<6 and max(d)/min(d)<=12:
            part=Builder();craft.cushion(part,(0,0,0),d,colour or COLOURS[family],family)
            craft.merge(self,part,p,yaw);return
        v,f=beveled_box(d,bevel)
        self.mesh(v,f,family,colour,p,Matrix.Rotation(yaw,3,"Z"))
    def lathe(self, profile, p=(0,0,0), family="Limestone", segments=48, colour=None):
        v=[]; f=[]
        for radius,z in profile:
            for i in range(segments):
                a=i*math.tau/segments
                v.append((math.cos(a)*radius,math.sin(a)*radius,z))
        for j in range(len(profile)-1):
            for i in range(segments):
                a=j*segments+i; b=j*segments+(i+1)%segments
                f.append((a,b,b+segments,a+segments))
        self.mesh(v,f,family,colour,p)
    def tube(self, points, radii, family="Bark", segments=20, colour=None, ribs=0):
        pts=[Vector(p) for p in points]; v=[]; f=[]
        for j,p in enumerate(pts):
            tangent=(pts[min(len(pts)-1,j+1)]-pts[max(0,j-1)]).normalized()
            ref=Vector((0,0,1)) if abs(tangent.z)<.95 else Vector((1,0,0))
            x=tangent.cross(ref).normalized(); y=tangent.cross(x).normalized()
            for i in range(segments):
                a=i*math.tau/segments
                r=radii[j]*(1+ribs*math.sin(a*13+j*.35))
                v.append(tuple(p+(math.cos(a)*x+math.sin(a)*y)*r))
        for j in range(len(pts)-1):
            for i in range(segments):
                a=j*segments+i; b=j*segments+(i+1)%segments
                f.append((a,b,b+segments,a+segments))
        f.extend([tuple(reversed(range(segments))),tuple((len(pts)-1)*segments+i for i in range(segments))])
        self.mesh(v,f,family,colour)
    def leaf(self,p,length,width,yaw=0,tilt=0,family="Foliage",colour=None):
        craft.leaf(self,p,length,width,yaw,tilt,family,colour or COLOURS[family]);return
        v=[]; f=[]; steps=7
        for j in range(steps+1):
            t=j/steps; w=max(.007,math.sin(math.pi*t)**.72)*width*.5
            for i in range(5):
                s=i/2-1
                v.append((s*w,t*length,length*(.09*math.sin(math.pi*t)+s*s*.035)))
        for j in range(steps):
            for i in range(4):
                a=j*5+i; f.extend([(a,a+1,a+5),(a+1,a+6,a+5)])
        r=Matrix.Rotation(yaw,3,"Z") @ Matrix.Rotation(tilt,3,"X")
        self.mesh(v,f,family,colour,p,r)
    def crystal(self,p,radius,height,sides=6,yaw=0,colour=None):
        rng=random.Random(int(radius*137+height*953)); v=[]; f=[]
        for z,r in [(0,.85),(height*.12,1),(height*.76,.8),(height,.02)]:
            for i in range(sides):
                a=i*math.tau/sides+yaw
                v.append((math.cos(a)*radius*r,math.sin(a)*radius*r,z))
        for j in range(3):
            for i in range(sides):
                a=j*sides+i;b=j*sides+(i+1)%sides;f.append((a,b,b+sides,a+sides))
        f.append(tuple(reversed(range(sides))))
        self.mesh(v,f,"Crystal",colour,p,smooth=False)
    def arc(self,p,radius,thickness,start=0,end=math.pi,family="Limestone",depth=.45,segments=32):
        v=[];f=[]
        for i in range(segments+1):
            a=start+(end-start)*i/segments
            for y,r in [(-depth*.5,radius-thickness*.5),(-depth*.5,radius+thickness*.5),
                        (depth*.5,radius-thickness*.5),(depth*.5,radius+thickness*.5)]:
                v.append((math.cos(a)*r,y,math.sin(a)*r))
        for i in range(segments):
            a=i*4;b=a+4
            f.extend([(a,b,b+1,a+1),(a+2,a+3,b+3,b+2),(a,a+2,b+2,b),(a+1,b+1,b+3,a+3)])
        f.extend([(0,1,3,2),(segments*4,segments*4+2,segments*4+3,segments*4+1)])
        self.mesh(v,f,family,position=p)

CATALOG=[]
def export(name,b,notes=""):
    mesh=bpy.data.meshes.new("SM_"+name); mesh.from_pydata(b.v,[],b.f); mesh.update()
    for m in MATERIALS:mesh.materials.append(m)
    for i,poly in enumerate(mesh.polygons):
        poly.material_index=b.m[i];poly.use_smooth=b.smooth[i]
    colors=mesh.color_attributes.new(name="Color",type="FLOAT_COLOR",domain="POINT")
    colors.data.foreach_set("color",[n for c in b.col for n in c])
    # All runtime surfaces use triplanar PBR; retain a deterministic UV set for normal and fallback tools.
    uv=mesh.uv_layers.new(name="UVMap")
    for poly in mesh.polygons:
        normal=poly.normal
        axis=max(range(3),key=lambda i:abs(normal[i]))
        for loop in poly.loop_indices:
            p=mesh.vertices[mesh.loops[loop].vertex_index].co
            uv.data[loop].uv=(p.y,p.z) if axis==0 else (p.x,p.z) if axis==1 else (p.x,p.y)
    obj=bpy.data.objects.new("SM_"+name,mesh);bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT");obj.select_set(True);bpy.context.view_layer.objects.active=obj
    try:
        mod=obj.modifiers.new("Weighted surface normals","WEIGHTED_NORMAL");mod.keep_sharp=True;mod.weight=50
        bpy.ops.object.modifier_apply(modifier=mod.name)
    except RuntimeError: pass
    mesh=obj.data;mesh.calc_loop_triangles()
    filename=MESHES/(obj.name+".fbx")
    bpy.ops.export_scene.fbx(filepath=str(filename),use_selection=True,object_types={"MESH"},add_leaf_bones=False,
        bake_anim=False,axis_forward="-Y",axis_up="Z",apply_unit_scale=True,global_scale=1,mesh_smooth_type="FACE",
        use_mesh_modifiers=True,use_custom_props=False,path_mode="AUTO")
    lo=[min(v.co[i] for v in mesh.vertices) for i in range(3)];hi=[max(v.co[i] for v in mesh.vertices) for i in range(3)]
    CATALOG.append({"name":name,"file":filename.name,"triangles":len(mesh.loop_triangles),"vertices":len(mesh.vertices),
                    "bounds_m":[lo,hi],"materials":FAMILIES,"notes":notes,
                    "sha256":hashlib.sha256(filename.read_bytes()).hexdigest()})
    obj.location=(len(CATALOG)%8*55,len(CATALOG)//8*65,0)
    print("EW_KIT_READY",name,len(mesh.loop_triangles),flush=True)

def column(b,p=(0,0,0),height=4,radius=.22):
    x,y,z=p
    profile=[(0,0),(radius*1.7,0),(radius*1.7,.12),(radius*1.45,.18),(radius*1.35,.31),
             (radius,.38),(radius*.87,height-.42),(radius*1.25,height-.32),(radius*1.6,height-.18),
             (radius*1.6,height),(0,height)]
    b.lathe(profile,(x,y,z),segments=40)
    for h in [.28,height-.3]:
        b.lathe([(radius*1.36,h),(radius*1.4,h+.035)],(x,y,z),"Bronze",40)

def fern(b,p=(0,0,0),size=1,seed=4):
    rng=random.Random(seed);x,y,z=p
    for j in range(8):
        a=j*math.tau/8+rng.uniform(-.1,.1)
        direction=Vector((math.cos(a),math.sin(a),0))
        pts=[]
        for k in range(13):
            t=k/12
            pts.append((x+direction.x*t*size,y+direction.y*t*size,z+math.sin(t*math.pi*.8)*size*.55))
        b.tube(pts,[.016*size*(1-k/15) for k in range(13)],"Bark",8)
        for k in range(1,12):
            t=k/12;anchor=pts[k]
            for side in [-1,1]:
                b.leaf(anchor,size*.34*math.sin(math.pi*t)**.7,size*.12,a+side*1.1,.25,colour=(.12,.35+.05*(j%3),.10,1))

