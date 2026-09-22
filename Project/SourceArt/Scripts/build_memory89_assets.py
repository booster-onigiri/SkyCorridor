from pathlib import Path
import bpy, math, json, hashlib
R=Path(__file__).resolve().parents[3]
OUT=R/'Project/SourceArt/Memory89';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
materials={}
for name,color,metal,rough in [('Ceramic',(.59,.57,.47,1),.12,.32),('Edge',(.19,.24,.23,1),.84,.24),('Glass',(.015,.027,.027,1),.35,.16),('Copper',(.48,.30,.12,1),.8,.26)]:
    m=bpy.data.materials.new(name);m.diffuse_color=color;m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=color;p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough
    materials[name]=m
objects=[]
def block(name,loc,scale,mat,bevel):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.data.materials.append(materials[mat]);objects.append(o)
    if bevel:
        mod=o.modifiers.new('Machined round edge','BEVEL');mod.width=bevel;mod.segments=5
        bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
        for p in o.data.polygons:p.use_smooth=True
        mod=o.modifiers.new('Weighted broad faces','WEIGHTED_NORMAL');bpy.ops.object.modifier_apply(modifier=mod.name)
    return o
# Centimetres expressed as Blender metres. Blender +Y maps to Unreal +X;
# the screen faces -X in Unreal and its panel is added at runtime.
block('Ceramic_back',(0,0,0),(.107,.012,.196),'Ceramic',.007)
block('Dark_metal_bezel',(0,-.0055,0),(.105,.005,.193),'Edge',.007)
block('Glass_under_screen',(0,-.0085,0),(.101,.002,.181),'Glass',.004)
for x in [-.052,.052]:
    block('Copper_seam',(x,.001,0),(.0015,.010,.174),'Copper',.0005)
for z in [.043,.058]:block('Side_key',(.055,.001,z),(.002,.008,.012),'Copper',.001)
block('Quiet_scan_switch',(-.055,.001,.03),(.002,.008,.022),'Edge',.001)
for x in [-.011,-.007,-.003,.003,.007,.011]:block('Lower_speaker',(x,-.0088,-.093),(.002,.0006,.001),'Glass',.0003)
bpy.ops.object.select_all(action='DESELECT')
for o in objects:o.select_set(True)
bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();phone=bpy.context.object
phone.name='Terminal89Body';bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
def export(o,name):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    path=OUT/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,bake_anim=False)
    return {'name':name,'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
records=[export(phone,'Terminal89Body')]
# One face, no body or face geometry; a blurred material supplies a barely
# visible trace. Its UV origin is at the head/top of the shape.
mesh=bpy.data.meshes.new('MemoryTracePlane');mesh.from_pydata([(-.48,0,0),(.48,0,0),(.48,0,1.85),(-.48,0,1.85)],[],[(0,1,2,3)])
uv=mesh.uv_layers.new(name='UVMap')
for i,p in enumerate([(0,1),(1,1),(1,0),(0,0)]):uv.data[i].uv=p
o=bpy.data.objects.new('Memory89Trace',mesh);bpy.context.collection.objects.link(o)
records.append(export(o,'Memory89Trace'))
(OUT/'catalog.json').write_text(json.dumps(records,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'memory89.blend'))
print('MEMORY89_ASSETS_READY',flush=True)
