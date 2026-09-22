"""Give the outdoor canopy a deeper leaf palette and curved-leaf lighting.

The city planting keeps its separately authored colors. This material is applied
only to the named nature assets; mesh slots and geometry remain in their order.
"""
from pathlib import Path
import json, unreal

root=Path(__file__).resolve().parents[2]
assets=unreal.EditorAssetLibrary
ml=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
path="/Game/EndlessWorld/Materials/M_FoliageNature"
mat=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
    "M_FoliageNature","/Game/EndlessWorld/Materials",unreal.Material,unreal.MaterialFactoryNew())
ml.delete_all_material_expressions(mat)
for key,value in {"used_with_nanite":True,"used_with_instanced_static_meshes":True,
                  "two_sided":True,"tangent_space_normal":True}.items():
    mat.set_editor_property(key,value)
mat.set_editor_property("blend_mode",unreal.BlendMode.BLEND_OPAQUE)
mat.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)

def node(cls,x,y):return ml.create_material_expression(mat,cls,x,y)
def scalar(value,x,y):
    result=node(unreal.MaterialExpressionConstant,x,y);result.set_editor_property("r",value);return result
def connect(a,out,b,pin):
    if not ml.connect_material_expressions(a,out,b,pin):raise RuntimeError("Nature material connection failed")
def output(a,out,prop):
    if not ml.connect_material_property(a,out,prop):raise RuntimeError("Nature material output failed")

def surface_texture(family,normal,size,x,y):
    """Project the existing original surface at a controlled physical scale."""
    texture=assets.load_asset("/Game/EndlessWorld/Textures/T_"+family+("_Normal" if normal else "_Base"))
    if not texture:raise RuntimeError("Missing nature surface texture: "+family)
    function=assets.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/"+
                               ("WorldAlignedNormal" if normal else "WorldAlignedTexture"))
    call=node(unreal.MaterialExpressionMaterialFunctionCall,x+220,y)
    if not function or not call.set_material_function(function):raise RuntimeError("Missing projected surface function")
    names=ml.get_material_expression_input_names(call)
    texture_pin=next(str(name) for name in names if str(name).startswith("TextureObject"))
    size_pin=next(str(name) for name in names if str(name).startswith("TextureSize"))
    source=node(unreal.MaterialExpressionTextureObject,x,y);source.set_editor_property("texture",texture)
    dimensions=node(unreal.MaterialExpressionConstant3Vector,x,y+140)
    dimensions.set_editor_property("constant",unreal.LinearColor(*size,1))
    connect(source,"",call,texture_pin);connect(dimensions,"",call,size_pin)
    return call

vc=node(unreal.MaterialExpressionVertexColor,-650,0)
palette=node(unreal.MaterialExpressionPower,-350,0)
connect(vc,"",palette,"Base");connect(scalar(1.7,-650,160),"",palette,"Exp")
output(palette,"",unreal.MaterialProperty.MP_BASE_COLOR)
output(scalar(.86,-300,230),"",unreal.MaterialProperty.MP_ROUGHNESS)
output(scalar(.1,-300,330),"",unreal.MaterialProperty.MP_SPECULAR)
transmission=node(unreal.MaterialExpressionMultiply,-100,490)
connect(palette,"",transmission,"A");connect(scalar(.09,-350,620),"",transmission,"B")
output(transmission,"",unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
# The authored leaf surface supplies its normal. A projected bark-scale normal
# texture should not turn the whole thin leaf toward the sun.
ml.recompile_material(mat)
if not assets.save_loaded_asset(mat,only_if_is_dirty=False):raise RuntimeError("Cannot save nature material")

catalog=json.loads((root/"SourceArt/kit-catalog.json").read_text(encoding="utf8"))
changed=[]
for item in catalog["assets"]:
    name=item["name"]
    if name not in ["GiantTree","FernPatch","GlowFlowers","TreeShrine","LuminousPond"] and not name.startswith(
            ("Tree_","RootIsland_","Wildflower_","CanopyWorld_","ForestVerge_")):continue
    mesh=assets.load_asset("/Game/EndlessWorld/Kit/SM_"+name)
    if not mesh:raise RuntimeError("Missing nature mesh: "+name)
    slots=list(mesh.get_editor_property("static_materials"));found=[]
    for index,slot in enumerate(slots):
        if str(slot.get_editor_property("imported_material_slot_name"))=="M_Foliage":
            slot.set_editor_property("material_interface",mat);found.append(index)
    if not found:raise RuntimeError("No foliage slot in nature mesh: "+name)
    mesh.set_editor_property("static_materials",slots)
    if any(mesh.get_material(index)!=mat for index in found):raise RuntimeError("Foliage binding lost: "+name)
    if not assets.save_loaded_asset(mesh,only_if_is_dirty=False):raise RuntimeError("Cannot save nature mesh: "+name)
    changed.append({"mesh":name,"slots":found})

# Keep the authored palette, with modest grain and normals from the original
# procedural bark/soil surfaces. Cliff faces do not use the city's masonry tile.
surface_materials={}
for surface,power,roughness,specular in [
        ("Bark",1.65,.92,.10),("Earth",1.50,.97,.08),
        ("Rock",1.60,.94,.12),("Pond",1.65,.30,.06)]:
    material_name="M_Nature"+surface
    material_path="/Game/EndlessWorld/Materials/"+material_name
    mat=assets.load_asset(material_path) if assets.does_asset_exist(material_path) else tools.create_asset(
        material_name,"/Game/EndlessWorld/Materials",unreal.Material,unreal.MaterialFactoryNew())
    ml.delete_all_material_expressions(mat)
    for key,value in {"used_with_nanite":True,"used_with_instanced_static_meshes":True,
                      "two_sided":False,"tangent_space_normal":True}.items():mat.set_editor_property(key,value)
    mat.set_editor_property("blend_mode",unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    colour=node(unreal.MaterialExpressionVertexColor,-650,0)
    graded=node(unreal.MaterialExpressionPower,-350,0)
    connect(colour,"",graded,"Base");connect(scalar(power,-650,160),"",graded,"Exp")
    if surface!="Pond":
        texture_family="Bark" if surface=="Bark" else "Soil"
        size=(130,130,540) if surface=="Bark" else (210,210,210) if surface=="Earth" else (600,600,300)
        grain=surface_texture(texture_family,False,size,-1450,-300)
        patterned=node(unreal.MaterialExpressionMultiply,-90,0)
        connect(graded,"",patterned,"A");connect(grain,"XYZ Texture",patterned,"B")
        density=node(unreal.MaterialExpressionMultiply,120,0)
        connect(patterned,"",density,"A")
        connect(scalar({"Bark":.28,"Earth":.36,"Rock":.55}[surface],-150,150),"",density,"B")
        output(density,"",unreal.MaterialProperty.MP_BASE_COLOR)
        normal=surface_texture(texture_family,True,size,-1450,590)
        subtle=node(unreal.MaterialExpressionLinearInterpolate,-250,610)
        connect(node(unreal.MaterialExpressionVertexNormalWS,-500,500),"",subtle,"A")
        connect(normal,"XYZ Texture",subtle,"B")
        connect(scalar(.25 if surface=="Bark" else .12,-500,850),"",subtle,"Alpha")
        norm=node(unreal.MaterialExpressionNormalize,0,590);connect(subtle,"",norm,"")
        output(norm,"",unreal.MaterialProperty.MP_NORMAL)
        mat.set_editor_property("tangent_space_normal",False)
    else:output(graded,"",unreal.MaterialProperty.MP_BASE_COLOR)
    output(scalar(roughness,-300,230),"",unreal.MaterialProperty.MP_ROUGHNESS)
    output(scalar(specular,-300,330),"",unreal.MaterialProperty.MP_SPECULAR)
    ml.recompile_material(mat)
    if not assets.save_loaded_asset(mat,only_if_is_dirty=False):raise RuntimeError("Cannot save "+material_name)
    surface_materials[surface]=mat

surface_bindings=[]
for name in [item["mesh"] for item in changed]+["GardenEarthDeck"]:
    mesh_path="/Game/EndlessWorld/Kit/SM_"+name
    if not assets.does_asset_exist(mesh_path):continue
    mesh=assets.load_asset(mesh_path);slots=list(mesh.get_editor_property("static_materials"));found=[]
    for index,slot in enumerate(slots):
        family=str(slot.get_editor_property("imported_material_slot_name"));surface=None
        if family=="M_Bark":surface="Bark"
        elif family=="M_Soil":surface="Earth"
        elif family=="M_Limestone" and (name.startswith(("RootIsland_","ForestVerge_")) or name=="LuminousPond"):surface="Rock"
        elif family=="M_Water" and name=="LuminousPond":surface="Pond"
        if surface:
            slot.set_editor_property("material_interface",surface_materials[surface]);found.append((index,surface))
    mesh.set_editor_property("static_materials",slots)
    for index,surface in found:
        if mesh.get_material(index)!=surface_materials[surface]:raise RuntimeError("Nature surface binding lost: "+name)
    if not assets.save_loaded_asset(mesh,only_if_is_dirty=False):raise RuntimeError("Cannot save "+name)
    surface_bindings.append({"mesh":name,"slots":found})
report={"success":True,"material":path,"palette_power":1.7,"roughness":.86,"specular":.1,
        "transmission_scale":.09,"normal_source":"authored curved leaf surface","bindings":changed,
        "surface_materials":{key:value.get_path_name() for key,value in surface_materials.items()},
        "surface_base_scales":{"Bark":.28,"Earth":.36,"Rock":.55},
        "surface_normal_strengths":{"Bark":.25,"Earth":.12,"Rock":.12},
        "surface_bindings":surface_bindings}
(root/"Saved/Verification/nature-material.json").write_text(json.dumps(report,indent=2),encoding="utf8")
unreal.log("EW_NATURE_MATERIAL_READY "+str(len(changed)))
