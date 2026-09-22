"""Import the modular kit, author PBR materials and save the runtime bootstrap map."""
from pathlib import Path
import unreal, json, time, traceback, math, runpy, hashlib

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/"SourceArt"
REPORTS=ROOT/"Saved/Verification"
REPORTS.mkdir(parents=True,exist_ok=True)
catalog=json.loads((SRC/"kit-catalog.json").read_text(encoding="utf8"))
assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
ml=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mesh_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
started=time.time()
report={"meshes":[],"materials":[],"textures":[],"warnings":[],"failures":[]}
command=unreal.SystemLibrary.get_command_line()
REIMPORT="-EWReimport" in command
MATERIALS_ONLY="-EWMaterialsOnly" in command
SCENE_ONLY="-EWSceneOnly" in command
MESHES_ONLY="-EWMeshesOnly" in command
RAIN_WINDOW_ONLY="-EWRainWindowOnly" in command
WATER_CITY_ONLY="-EWWaterCityOnly" in command
WATER_MATERIALS=("WaterCitySurface","WaterCityFall","WaterCityFoam","WaterCityStone")
if RAIN_WINDOW_ONLY and WATER_CITY_ONLY:raise RuntimeError("Select only one bounded feature import")
RAIN_MATERIALS={"RainTimber":("Timber",.70,.04,.68),"RainPlaster":("Plaster",.72,.16,.78),
                "RainCeramic":("Ceramic",.40,.18,.23),"RainCloth":("Cloth",.65,.08,.86)}
if RAIN_WINDOW_ONLY:
    if MATERIALS_ONLY or SCENE_ONLY:raise RuntimeError("Residence-only import cannot rebuild materials or the scene")
    MESHES_ONLY=True
    wanted={"UrbanBlock_RainWindow","RainValveHandle","RainWaterCommon","RainWaterEaves","RainWaterWindow"}
    catalog["assets"]=[item for item in catalog["assets"] if item["name"] in wanted]
    if {item["name"] for item in catalog["assets"]}!=wanted:raise RuntimeError("Generate the complete rain-window kit first")
    if any(item.get("coordinate_contract")!="rain-window-unreal-cm-v2" for item in catalog["assets"]):
        raise RuntimeError("Regenerate the residence assets with measured Unreal handedness and anchor checks")
    finish=catalog.get("rain_window_art",{})
    if finish.get("finish_contract")!="rain-window-finish-v1" or set(finish.get("residence_materials",[]))!=set(RAIN_MATERIALS):
        raise RuntimeError("Regenerate the residence-only finish slots before importing")
    # The room predates the water-city materials; its inherited block uses
    # only the original shared families and the four residence finishes.
    catalog["materials"]=[family for family in catalog["materials"] if family not in WATER_MATERIALS]
if WATER_CITY_ONLY:
    if MATERIALS_ONLY or SCENE_ONLY:raise RuntimeError("Water-city-only import cannot rebuild the scene or shared materials")
    MESHES_ONLY=True
    wanted={"WaterCityCentralStone","WaterCityCentralWater","WaterCityCentralFoam",
            "WaterCityNorthStone","WaterCityNorthWater","WaterCityNorthFall","WaterCityNorthFoam",
            "WaterCitySouthStone","WaterCitySouthWater","WaterCitySouthFall","WaterCitySouthFoam",
            "WaterCityWalks","WaterCityDrop"}
    catalog["assets"]=[item for item in catalog["assets"] if item["name"] in wanted]
    if len(catalog["assets"])!=13 or {item["name"] for item in catalog["assets"]}!=wanted:
        raise RuntimeError("Generate exactly the complete 13-mesh water-city kit first")
    art=catalog.get("water_city_art",{})
    if art.get("material_contract")!="water-city-material-v1" or set(art.get("materials",[]))!=set(WATER_MATERIALS):
        raise RuntimeError("Missing bounded water-city material contract")
    rays=art.get("source_opaque_polygon_rays",[])
    if len(rays)!=5 or not all(check.get("clear") for check in rays):raise RuntimeError("Water-city source polygon visibility audit is incomplete")
    for item in catalog["assets"]:
        if item.get("coordinate_contract")!="water-city-unreal-cm-v1" or len(item.get("ue_anchor_vertices_cm",[]))<5:
            raise RuntimeError("Water-city mesh lacks signed coordinates and feature anchors")
        if hashlib.sha256((SRC/"Meshes"/item["file"]).read_bytes()).hexdigest()!=item["sha256"]:
            raise RuntimeError("Water-city source FBX changed after its audit: "+item["name"])
    catalog["materials"]=list(WATER_MATERIALS)
    report["scope"]="13 water-city meshes and four marked materials only; shared assets, scene and player data unchanged"
    report["water_city_art"]=art

def save(obj):
    if not assets.save_loaded_asset(obj,only_if_is_dirty=False):
        raise RuntimeError("Asset save failed: "+obj.get_path_name())

def source_vertex_bounds(mesh):
    """Measure imported vertices when the Nanite render envelope is expanded.

    GetBoundingBox includes built render bounds after a package is reloaded.
    Preserve Area can expand those bounds beyond the original leaf geometry.
    A fresh import still has cached source bounds, so that API alone is not a
    stable source-size check. FBX imports here use compact vertex identifiers.
    """
    desc=mesh.get_static_mesh_description(0)
    if not desc or desc.get_vertex_count()==0:
        raise RuntimeError("Missing source vertices: "+mesh.get_name())
    lo=[float("inf")]*3;hi=[float("-inf")]*3
    for index in range(desc.get_vertex_count()):
        vertex=unreal.VertexID(id_value=index)
        if not desc.is_vertex_valid(vertex):
            raise RuntimeError("Non-compact source vertices require an explicit audit: "+mesh.get_name())
        point=desc.get_vertex_position(vertex)
        for axis,value in enumerate((point.x,point.y,point.z)):
            if not math.isfinite(value):raise RuntimeError("Non-finite source vertex: "+mesh.get_name())
            lo[axis]=min(lo[axis],value);hi[axis]=max(hi[axis],value)
    return [lo,hi]

def feature_geometry(mesh,item):
    """Check signs and individual feature points; extents alone hid a reflection.

    The original failure retained every extent, but moved the room tangent and
    all dynamic water to opposite sides. Named source vertices in UE asset-local
    centimetres catch that exact failure before a new package is produced.
    """
    expected=item["ue_bounds_cm"];anchors=item["ue_anchor_vertices_cm"]
    if len(anchors)<5:raise RuntimeError("Missing residence coordinate anchors")
    desc=mesh.get_static_mesh_description(0)
    if not desc or not desc.get_vertex_count():raise RuntimeError("Missing residence source vertices")
    targets={};found=[False]*len(anchors);nearest=[float("inf")]*len(anchors)
    for index,anchor in enumerate(anchors):
        cell=[math.floor(v) for v in anchor["position"]]
        for dx in [-1,0,1]:
            for dy in [-1,0,1]:
                for dz in [-1,0,1]:targets.setdefault((cell[0]+dx,cell[1]+dy,cell[2]+dz),[]).append(index)
    lo=[float("inf")]*3;hi=[float("-inf")]*3
    for index in range(desc.get_vertex_count()):
        vertex=unreal.VertexID(id_value=index)
        if not desc.is_vertex_valid(vertex):raise RuntimeError("Residence source vertices are not compact")
        v=desc.get_vertex_position(vertex);p=[float(v.x),float(v.y),float(v.z)]
        if not all(math.isfinite(x) for x in p):raise RuntimeError("Non-finite residence coordinate")
        for axis in range(3):lo[axis]=min(lo[axis],p[axis]);hi[axis]=max(hi[axis],p[axis])
        for target in targets.get(tuple(math.floor(x) for x in p),[]):
            distance=math.sqrt(sum((p[axis]-anchors[target]["position"][axis])**2 for axis in range(3)))
            nearest[target]=min(nearest[target],distance)
            if distance<=.25:found[target]=True
    signed_bounds=[lo,hi]
    bounds_ok=all(abs(signed_bounds[end][axis]-expected[end][axis])<=.5 for end in range(2) for axis in range(3))
    details=[{"name":a["name"],"expected_cm":a["position"],"found":found[i],
              "distance_cm":nearest[i] if math.isfinite(nearest[i]) else None} for i,a in enumerate(anchors)]
    result={"asset":mesh.get_name(),"contract":item["coordinate_contract"],"signed_bounds_cm":signed_bounds,
            "expected_bounds_cm":expected,"bounds_pass":bounds_ok,"anchors":details,"pass":bounds_ok and all(found)}
    report.setdefault("water_city_geometry" if WATER_CITY_ONLY else "residence_geometry",[]).append(result)
    if not result["pass"]:report["failures"].append({"asset":mesh.get_name(),"reason":"feature Unreal coordinate/anchor mismatch"})
    return signed_bounds
def optional(obj,name,value):
    try: obj.set_editor_property(name,value);return True
    except Exception as error:report["warnings"].append(name+": "+str(error));return False
def node(mat,kind,x=0,y=0,**props):
    n=ml.create_material_expression(mat,getattr(unreal,kind),x,y)
    for name,value in props.items():n.set_editor_property(name,value)
    return n
def clear_material(mat):
    # UE 5.7 removes elements while iterating a view of the same expression
    # array. One DeleteAll call can leave nodes behind. Verify a strictly
    # shrinking count until empty, before authoring any new output nodes.
    count=ml.get_num_material_expressions(mat)
    initial=count;passes=0
    while count:
        ml.delete_all_material_expressions(mat);passes+=1
        remaining=ml.get_num_material_expressions(mat)
        if remaining>=count:raise RuntimeError("Material graph did not clear: "+mat.get_path_name())
        count=remaining
    report.setdefault("cleared_material_graphs",[]).append({"asset":mat.get_path_name(),
        "previous_nodes":initial,"passes":passes,"remaining_nodes":count})
def link(a,out,b,pin):
    if isinstance(a,unreal.MaterialExpressionVertexColor) and out=="RGB":out=""
    if not ml.connect_material_expressions(a,out,b,pin):
        raise RuntimeError("Cannot connect "+a.get_class().get_name()+"."+out+" -> "+pin)
def output(n,pin,prop):
    if isinstance(n,unreal.MaterialExpressionVertexColor) and pin=="RGB":pin=""
    if not ml.connect_material_property(n,pin,getattr(unreal.MaterialProperty,prop)):
        raise RuntimeError("Cannot connect material property "+prop)
def scalar(mat,value,x=0,y=0):
    return node(mat,"MaterialExpressionConstant",x,y,r=value)
def vector(mat,values,x=0,y=0):
    return node(mat,"MaterialExpressionConstant3Vector",x,y,constant=unreal.LinearColor(*values,1))
def aligned(mat,texture,normal=False,x=-1000,y=0):
    call=node(mat,"MaterialExpressionMaterialFunctionCall",x+250,y)
    path="/Engine/Functions/Engine_MaterialFunctions01/Texturing/"+("WorldAlignedNormal" if normal else "WorldAlignedTexture")
    function=unreal.load_object(None,path)
    if not function or not call.set_material_function(function):
        raise RuntimeError("World-aligned material function is unavailable: "+path)
    names=ml.get_material_expression_input_names(call)
    unreal.log("EW_FUNCTION_INPUTS "+path+" "+str(names))
    obj=node(mat,"MaterialExpressionTextureObject",x,y,texture=texture)
    # Input names contain explanatory type suffixes in some engine builds.
    tex_pin=next(str(n) for n in names if str(n).startswith("TextureObject"))
    size_pin=next(str(n) for n in names if str(n).startswith("TextureSize"))
    link(obj,"",call,tex_pin);link(vector(mat,(120,120,120),x,y+150),"",call,size_pin)
    return call

def residence_material(family):
    """Create only the four residence finishes; shared city assets are read-only.

    UE 5.7's WorldAlignedNormal defaults WorldSpace to false. These materials
    explicitly select its world output before blending with VertexNormalWS.
    """
    texture_family,clean_amount,normal_amount,roughness=RAIN_MATERIALS[family]
    name="M_"+family;path="/Game/EndlessWorld/Materials/"+name
    contract="rain-window-finish-v1"
    finish_revision="warm-daylight-v3-empty-graph"
    exists=assets.does_asset_exist(path)
    mat=assets.load_asset(path) if exists else tools.create_asset(name,"/Game/EndlessWorld/Materials",unreal.Material,unreal.MaterialFactoryNew())
    if not mat:raise RuntimeError("Residence material creation failed: "+path)
    if exists and assets.get_metadata_tag(mat,"EndlessWorld.ResidenceFinish")!=contract:
        raise RuntimeError("Refusing to replace a material without the residence finish marker: "+path)
    rebuild=not exists or assets.get_metadata_tag(mat,"EndlessWorld.ResidenceFinishRevision")!=finish_revision
    if rebuild:
        clear_material(mat)
        mat.set_editor_property("used_with_nanite",True)
        mat.set_editor_property("used_with_instanced_static_meshes",True)
        mat.set_editor_property("blend_mode",unreal.BlendMode.BLEND_OPAQUE)
        mat.set_editor_property("tangent_space_normal",False)
        def texture(suffix):
            result=assets.load_asset("/Game/EndlessWorld/Textures/T_"+texture_family+"_"+suffix)
            if not result:raise RuntimeError("Missing existing residence texture: "+texture_family+"_"+suffix)
            return result
        vc=node(mat,"MaterialExpressionVertexColor",-1350,-200)
        base=aligned(mat,texture("Base"),x=-1350,y=0)
        clean=node(mat,"MaterialExpressionLinearInterpolate",-850,0)
        link(base,"XYZ Texture",clean,"A");link(vector(mat,(1,1,1)),"",clean,"B")
        link(scalar(mat,clean_amount),"",clean,"Alpha")
        colour=node(mat,"MaterialExpressionMultiply",-550,0)
        link(clean,"",colour,"A");link(vc,"RGB",colour,"B")
        # The shared wood texture is deliberately neutral. Give only these
        # room finishes a warm pigment that survives the city's blue skylight,
        # and soften the oversized grain instead of raising global exposure.
        tint={"RainTimber":(1.,.67,.38),"RainCloth":(1.,.78,.55),
              "RainPlaster":(1.,.97,.88),"RainCeramic":(1.,1.,1.)}[family]
        pigment=node(mat,"MaterialExpressionMultiply",-300,0)
        link(colour,"",pigment,"A");link(vector(mat,tint),"",pigment,"B");output(pigment,"","MP_BASE_COLOR")
        normal=aligned(mat,texture("Normal"),normal=True,x=-1350,y=420)
        world_pin=next(str(p) for p in ml.get_material_expression_input_names(normal) if str(p).startswith("WorldSpace"))
        link(node(mat,"MaterialExpressionStaticBool",-1300,780,value=True),"",normal,world_pin)
        subtle=node(mat,"MaterialExpressionLinearInterpolate",-800,450)
        link(node(mat,"MaterialExpressionVertexNormalWS",-1100,650),"",subtle,"A")
        link(normal,"XYZ Texture",subtle,"B");link(scalar(mat,normal_amount),"",subtle,"Alpha")
        norm=node(mat,"MaterialExpressionNormalize",-520,450);link(subtle,"",norm,"");output(norm,"","MP_NORMAL")
        output(scalar(mat,roughness),"","MP_ROUGHNESS")
        output(scalar(mat,0),"","MP_METALLIC");output(scalar(mat,1),"","MP_AMBIENT_OCCLUSION")
        assets.set_metadata_tag(mat,"EndlessWorld.ResidenceFinish",contract)
        assets.set_metadata_tag(mat,"EndlessWorld.ResidenceFinishRevision",finish_revision)
        ml.recompile_material(mat);save(mat)
    report["materials"].append(name)
    report.setdefault("residence_materials",[]).append({"asset":path,"created":not exists,"rebuilt":rebuild,
        "contract":contract,"finish_revision":finish_revision,
        "world_aligned_normal_world_space":True,"normal_blend":normal_amount,"base_clean_blend":clean_amount,
        "roughness":roughness,"shared_textures_read_only":True})
    unreal.log("EW_RESIDENCE_MATERIAL_READY "+name)
    return mat

textures={}
for file in ([] if RAIN_WINDOW_ONLY or WATER_CITY_ONLY else sorted((SRC/"Textures").glob("*.png"))):
    path="/Game/EndlessWorld/Textures/"+file.stem
    if assets.does_asset_exist(path):
        tex=assets.load_asset(path)
    else:
        task=unreal.AssetImportTask();task.filename=str(file);task.destination_path="/Game/EndlessWorld/Textures"
        task.automated=True;task.replace_existing=False;task.save=True
        tools.import_asset_tasks([task])
        objs=task.get_objects()
        if not objs:raise RuntimeError("Texture not imported: "+file.name)
        tex=objs[0]
        if file.stem.endswith("_Normal"):
            tex.set_editor_property("srgb",False);tex.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif file.stem.endswith("_ORM"):
            tex.set_editor_property("srgb",False);tex.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_MASKS)
        save(tex)
    textures[file.stem]=tex;report["textures"].append(file.stem)

materials={}
water_builder=runpy.run_path(str(Path(__file__).with_name("water_city_materials.py")))["build_material"] if any(f in WATER_MATERIALS for f in catalog["materials"]) else None
for family in catalog["materials"]:
    if family in WATER_MATERIALS:
        materials[family]=water_builder(family,globals())
        continue
    if family in RAIN_MATERIALS:
        materials[family]=residence_material(family)
        continue
    name="M_"+family;path="/Game/EndlessWorld/Materials/"+name
    if RAIN_WINDOW_ONLY and not assets.does_asset_exist(path):raise RuntimeError("Residence import needs the existing material: "+path)
    mat=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,"/Game/EndlessWorld/Materials",unreal.Material,unreal.MaterialFactoryNew())
    materials[family]=mat
    if SCENE_ONLY or MESHES_ONLY:continue
    clear_material(mat)
    mat.set_editor_property("used_with_nanite",True)
    mat.set_editor_property("used_with_instanced_static_meshes",True)
    mat.set_editor_property("two_sided",family in ["Foliage","Petal"])
    mat.set_editor_property("blend_mode",unreal.BlendMode.BLEND_OPAQUE)
    vc=node(mat,"MaterialExpressionVertexColor",-1200,-200)
    if family not in ["Crystal","Water","Glow"]:
        texture_family={"Paint":"Plaster"}.get(family,family)
        b=aligned(mat,textures["T_"+texture_family+"_Base"],x=-1200,y=0)
        base_output="XYZ Texture"
        if family in ["Limestone","Paint","Foliage","Petal","Cloth"]:
            # Retain surface variation without baking a dirty gray cast into
            # every white wall and colored leaf. Vertex colors carry the palette.
            clean=node(mat,"MaterialExpressionLinearInterpolate",-720,-120)
            link(b,base_output,clean,"A")
            link(vector(mat,(.94,.96,.94)) if family=="Limestone" else vector(mat,(1,1,1)),"",clean,"B")
            link(scalar(mat,.58 if family=="Limestone" else .72 if family=="Paint" else .60),"",clean,"Alpha")
            b=clean;base_output=""
        mul=node(mat,"MaterialExpressionMultiply",-500,0)
        link(b,base_output,mul,"A");link(vc,"RGB",mul,"B");output(mul,"","MP_BASE_COLOR")
        n=aligned(mat,textures["T_"+texture_family+"_Normal"],normal=True,x=-1200,y=420)
        # The engine function returns world-space normals.
        mat.set_editor_property("tangent_space_normal",False)
        if family in ["Limestone","Paint"]:
            subtle=node(mat,"MaterialExpressionLinearInterpolate",-680,420)
            link(node(mat,"MaterialExpressionVertexNormalWS",-950,650),"",subtle,"A")
            link(n,"XYZ Texture",subtle,"B");link(scalar(mat,.40),"",subtle,"Alpha")
            norm=node(mat,"MaterialExpressionNormalize",-450,430);link(subtle,"",norm,"")
            output(norm,"","MP_NORMAL")
        else:output(n,"XYZ Texture","MP_NORMAL")
        orm=aligned(mat,textures["T_"+texture_family+"_ORM"],x=-1200,y=900)
        for mask,prop in [("r","MP_AMBIENT_OCCLUSION"),("g","MP_ROUGHNESS"),("b","MP_METALLIC")]:
            m=node(mat,"MaterialExpressionComponentMask",-470,950+["r","g","b"].index(mask)*100,
                   r=mask=="r",g=mask=="g",b=mask=="b",a=False)
            link(orm,"XYZ Texture",m,"");output(m,"",prop)
        if family in ["Foliage","Petal"]:
            mat.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
            sub=node(mat,"MaterialExpressionMultiply",-500,1400)
            link(vc,"RGB",sub,"A");link(scalar(mat,.25,-760,1400),"",sub,"B");output(sub,"","MP_SUBSURFACE_COLOR")
    else:
        output(vc,"RGB","MP_BASE_COLOR")
        output(scalar(mat,.2 if family=="Crystal" else .14 if family=="Water" else .6),"","MP_ROUGHNESS")
        output(scalar(mat,.12 if family=="Crystal" else 0),"","MP_METALLIC")
        glow=node(mat,"MaterialExpressionMultiply",-400,200)
        link(vc,"RGB",glow,"A");link(scalar(mat,3000. if family=="Glow" else .12 if family=="Crystal" else .08),"",glow,"B")
        output(glow,"","MP_EMISSIVE_COLOR")
        if family=="Water":
            n=node(mat,"MaterialExpressionTextureSample",-900,500,texture=textures["T_Ceramic_Normal"],
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            pan=node(mat,"MaterialExpressionPanner",-1100,500,speed_x=.007,speed_y=.01)
            link(pan,"",n,"");output(n,"RGB","MP_NORMAL")
    if family in ["Foliage","Petal"]:
        mat.set_editor_property("max_world_position_offset_displacement",2.0)
        tm=node(mat,"MaterialExpressionTime",-1000,1750)
        speed=node(mat,"MaterialExpressionMultiply",-800,1750)
        link(tm,"",speed,"A");link(scalar(mat,.2),"",speed,"B")
        wave=node(mat,"MaterialExpressionSine",-600,1750);link(speed,"",wave,"")
        wind=node(mat,"MaterialExpressionMultiply",-400,1750)
        link(wave,"",wind,"A");link(vector(mat,(1.6,.8,.2)),"",wind,"B")
        output(wind,"","MP_WORLD_POSITION_OFFSET")
    ml.recompile_material(mat);save(mat)
    report["materials"].append(name)
    unreal.log("EW_MATERIAL_READY "+name)

if not MATERIALS_ONLY and not SCENE_ONLY:
    for index,item in enumerate(catalog["assets"]):
        name="SM_"+item["name"];path="/Game/EndlessWorld/Kit/"+name
        current=assets.load_asset(path) if assets.does_asset_exist(path) else None
        if WATER_CITY_ONLY and current and assets.get_metadata_tag(current,"EndlessWorld.WaterCity")!="water-city-art-v1":
            raise RuntimeError("Refusing to replace an unmarked water-city mesh: "+path)
        if not current or REIMPORT or assets.get_metadata_tag(current,"EndlessWorld.SourceSHA256")!=item["sha256"]:
            task=unreal.AssetImportTask();task.filename=str(SRC/"Meshes"/item["file"])
            task.destination_path="/Game/EndlessWorld/Kit";task.destination_name=name;task.automated=True
            task.replace_existing=True;task.save=False
            opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
            opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
            data=opt.static_mesh_import_data
            data.convert_scene_unit=True;data.combine_meshes=True;data.auto_generate_collision=False
            data.generate_lightmap_u_vs=False;data.remove_degenerates=True;data.build_nanite=bool(item.get("nanite",True))
            data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
            data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
            task.options=opt;tools.import_asset_tasks([task]);objects=task.get_objects()
            if not objects:raise RuntimeError("Mesh not imported: "+name)
            current=objects[0]
            slots=list(current.get_editor_property("static_materials"))
            for slot,info in enumerate(slots):
                family=str(info.get_editor_property("imported_material_slot_name")).removeprefix("M_")
                if family not in materials:
                    family=str(info.get_editor_property("material_slot_name")).removeprefix("M_")
                if family not in materials:raise RuntimeError("Unmapped material slot "+family+" in "+name)
                info.set_editor_property("material_interface",materials[family])
            # Assign the complete, unchanged slot order in one edit; per-slot
            # edits rebuild the distance field repeatedly for the same mesh.
            current.set_editor_property("static_materials",slots)
            for slot,info in enumerate(slots):
                if current.get_material(slot)!=info.get_editor_property("material_interface"):
                    raise RuntimeError("Material assignment was not retained: "+name+" slot "+str(slot))
            ns=current.get_editor_property("nanite_settings");ns.enabled=bool(item.get("nanite",True));ns.fallback_relative_error=.05
            if item["name"] in ["GiantTree","FernPatch","GlowFlowers","Planter","LuminousPond"] or item["name"].startswith(("Tree_","RootIsland_","Wildflower_","CanopyWorld_","ForestVerge_")):
                ns.set_editor_property("shape_preservation",unreal.NaniteShapePreservation.PRESERVE_AREA)
            current.set_editor_property("nanite_settings",ns)
            assets.set_metadata_tag(current,"EndlessWorld.SourceSHA256",item["sha256"])
            if WATER_CITY_ONLY:assets.set_metadata_tag(current,"EndlessWorld.WaterCity","water-city-art-v1")
            save(current)
        residence_bounds=feature_geometry(current,item) if RAIN_WINDOW_ONLY or WATER_CITY_ONLY else None
        box=current.get_bounding_box()
        bounds=[[box.min.x,box.min.y,box.min.z],[box.max.x,box.max.y,box.max.z]]
        render_bounds=bounds
        bounds_source="static mesh bounding box"
        if residence_bounds:
            bounds=residence_bounds;bounds_source="LOD0 imported vertices with signed UE bounds and feature anchors"
        expected=item["bounds_m"]
        ext=[bounds[1][i]-bounds[0][i] for i in range(3)]
        want=[(expected[1][i]-expected[0][i])*100 for i in range(3)]
        if any(abs(ext[i]-want[i])>max(1,want[i]*.005) for i in range(3)):
            bounds=source_vertex_bounds(current)
            bounds_source="LOD0 imported vertices"
            ext=[bounds[1][i]-bounds[0][i] for i in range(3)]
        if any(abs(ext[i]-want[i])>max(1,want[i]*.005) for i in range(3)):
            report["failures"].append({"asset":name,"reason":"centimetre bounds mismatch","actual":ext,"expected":want})
        report["meshes"].append({"name":name,"nanite":current.get_editor_property("nanite_settings").enabled,
                                 "bounds_cm":bounds,"bounds_source":bounds_source,"render_bounds_cm":render_bounds,
                                 "source_triangles":item["triangles"]})
        unreal.log("EW_IMPORT_PROGRESS "+str(index+1)+"/"+str(len(catalog["assets"])))
        (REPORTS/("water-city-import-progress.json" if WATER_CITY_ONLY else "import-progress.json")).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf8")

if not MATERIALS_ONLY and not MESHES_ONLY:
    map_path="/Game/EndlessWorld/Maps/EndlessWorld"
    if assets.does_asset_exist(map_path):
        if not levels.load_level(map_path):raise RuntimeError("Map load failed")
        for actor in actors.get_all_level_actors():
            if actor.get_actor_label().startswith("EW_"):actors.destroy_actor(actor)
    else:
        if not levels.new_level(map_path):raise RuntimeError("Map creation failed")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    def spawn(cls,name,p=(0,0,0),rot=None):
        actor=actors.spawn_actor_from_class(cls,unreal.Vector(*p),rot or unreal.Rotator())
        actor.set_actor_label("EW_"+name);return actor
    sun=spawn(unreal.DirectionalLight,"Sun",(0,0,10000),unreal.Rotator(pitch=-38,yaw=-28,roll=0))
    lc=sun.light_component;lc.set_mobility(unreal.ComponentMobility.MOVABLE);lc.set_intensity(105000)
    lc.set_light_color(unreal.LinearColor(1,.992,.97,1));lc.set_editor_property("atmosphere_sun_light",True)
    lc.set_editor_property("light_source_angle",1.5);optional(lc,"cast_cloud_shadows",False)
    sky=spawn(unreal.SkyAtmosphere,"Atmosphere",(0,0,-100000))
    sc=sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    optional(sc,"transform_mode",unreal.SkyAtmosphereTransformMode.PLANET_TOP_AT_COMPONENT_TRANSFORM)
    optional(sc,"rayleigh_scattering_scale",.038065)
    fill=spawn(unreal.SkyLight,"SkyFill");fl=fill.light_component
    fl.set_mobility(unreal.ComponentMobility.MOVABLE);fl.set_real_time_capture(True);fl.set_intensity(1.05)
    fl.set_editor_property("lower_hemisphere_is_black",False)
    fl.set_lower_hemisphere_color(unreal.LinearColor(.12,.19,.22,1))
    cloud=spawn(unreal.VolumetricCloud,"CloudSea")
    cc=cloud.get_component_by_class(unreal.VolumetricCloudComponent)
    cloud_path="/Game/EndlessWorld/Materials/MI_CloudSea"
    cloud_mat=assets.load_asset(cloud_path) if assets.does_asset_exist(cloud_path) else assets.duplicate_asset("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst",cloud_path)
    for key,value in {"Layout_CloudGlobalScale":16.,"Cloud_GlobalCoverage":-.20,"Cloud_GlobalDensity":.008}.items():
        ml.set_material_instance_scalar_parameter_value(cloud_mat,key,value)
    save(cloud_mat)
    cc.set_editor_property("material",cloud_mat)
    cc.set_editor_property("layer_bottom_altitude",.05);cc.set_editor_property("layer_height",.65)
    fog=spawn(unreal.ExponentialHeightFog,"Mist",(0,0,-2500))
    fc=fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density",.014);fc.set_editor_property("fog_height_falloff",.16)
    fc.set_editor_property("fog_inscattering_luminance",unreal.LinearColor(.28,.48,.73,1));fc.set_volumetric_fog(True)
    pp=spawn(unreal.PostProcessVolume,"Look");pp.set_editor_property("unbound",True)
    settings=pp.get_editor_property("settings")
    for name,value in {"auto_exposure_min_brightness":13.5,"auto_exposure_max_brightness":13.5,"auto_exposure_bias":0.0,
                      "bloom_intensity":.18,"vignette_intensity":.08,"motion_blur_amount":0.,
                      "lumen_scene_lighting_quality":1.0,"lumen_final_gather_quality":1.0,
                      "lumen_reflection_quality":1.0,"ambient_occlusion_intensity":.35}.items():
        optional(settings,"override_"+name,True);optional(settings,name,value)
    pp.set_editor_property("settings",settings)
    spawn(unreal.PlayerStart,"PlayerStart",(6400,5150,3000),unreal.Rotator(pitch=0,yaw=22,roll=0))
    mode=unreal.load_class(None,"/Script/EndlessWorld.EWGameMode")
    world.get_world_settings().set_editor_property("default_game_mode",mode)
    world.get_world_settings().set_editor_property("kill_z",-15000)
    world.get_world_settings().set_editor_property("enable_world_bounds_checks",False)
    if not levels.save_current_level():raise RuntimeError("Map save failed")
    atmosphere=Path(__file__).with_name("finish_atmosphere.py")
    if atmosphere.exists():exec(compile(atmosphere.read_text(encoding="utf8"),str(atmosphere),"exec"),{"__name__":"endless_atmosphere_finish","__file__":str(atmosphere)})
    finish=Path(__file__).with_name("finish_art.py")
    if finish.exists():exec(compile(finish.read_text(encoding="utf8"),str(finish),"exec"),{"__name__":"endless_art_finish"})
    city_light=Path(__file__).with_name("finish_city_light.py")
    if city_light.exists():exec(compile(city_light.read_text(encoding="utf8"),str(city_light),"exec"),{"__name__":"endless_city_light","__file__":str(city_light)})
nature=Path(__file__).with_name("finish_nature.py")
if nature.exists() and not RAIN_WINDOW_ONLY and not WATER_CITY_ONLY:exec(compile(nature.read_text(encoding="utf8"),str(nature),"exec"),{"__name__":"endless_nature_finish","__file__":str(nature)})
report["seconds"]=round(time.time()-started,3)
report["success"]=not report["failures"]
(REPORTS/("water-city-import.json" if WATER_CITY_ONLY else "rain-window-import.json" if RAIN_WINDOW_ONLY else "kit-import.json")).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf8")
if report["failures"]:raise RuntimeError("Kit import bounds audit failed: "+str(report["failures"]))
unreal.log("EW_IMPORT_COMPLETE "+str(len(report["meshes"]))+" assets "+str(report["seconds"])+"s")
