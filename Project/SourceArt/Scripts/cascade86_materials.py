"""Author only the four marked water-city materials during a bounded import.

No shared material or texture is saved. A script SHA permits rebuilding these
four owned graphs after visual review without silently reusing an old shader.
"""
from pathlib import Path
import hashlib
import unreal

FAMILIES=("Cascade86Surface","Cascade86Fall","Cascade86Foam","Cascade86Stone")
CONTRACT="cascade86-water-v1"


def noise_field(name,position):
    """Continuous two-dimensional value noise without texture or sine stripes.

    Four corner hashes interpolate over a cell. Sampling an advected position
    gives each ribbon a different width and length while retaining downward
    motion; the same field compiles in the pixel and vertex shader stages.
    """
    return f"""
float2 {name}_p={position};
float2 {name}_cell=floor({name}_p);
float2 {name}_f=frac({name}_p);
{name}_f={name}_f*{name}_f*(3.0-2.0*{name}_f);
float4 {name}_x=frac(({name}_cell.x+float4(0,1,0,1))*0.1031);
float4 {name}_y=frac(({name}_cell.y+float4(0,0,1,1))*0.1031);
float4 {name}_z={name}_x;
float4 {name}_d={name}_x*({name}_y+33.33)+{name}_y*({name}_z+33.33)+{name}_z*({name}_x+33.33);
{name}_x+={name}_d;{name}_y+={name}_d;{name}_z+={name}_d;
float4 {name}_h=frac(({name}_x+{name}_y)*{name}_z);
float {name}=lerp(lerp({name}_h.x,{name}_h.y,{name}_f.x),lerp({name}_h.z,{name}_h.w,{name}_f.x),{name}_f.y);
"""


def build_material(family,context):
    if family not in FAMILIES:raise ValueError("Not an owned water-city material")
    assets=context["assets"];tools=context["tools"];ml=context["ml"]
    node=context["node"];link=context["link"];output=context["output"]
    scalar=context["scalar"];vector=context["vector"];save=context["save"]
    name="M_"+family;path="/Game/EndlessWorld/Materials/"+name
    source_sha=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    exists=assets.does_asset_exist(path)
    mat=assets.load_asset(path) if exists else tools.create_asset(name,"/Game/EndlessWorld/Materials",unreal.Material,unreal.MaterialFactoryNew())
    if not mat:raise RuntimeError("Water-city material creation failed: "+path)
    if exists and assets.get_metadata_tag(mat,"EndlessWorld.Cascade86")!=CONTRACT:
        raise RuntimeError("Refusing to replace an unmarked material: "+path)
    rebuild=not exists or assets.get_metadata_tag(mat,"EndlessWorld.Cascade86ShaderSHA256")!=source_sha
    if rebuild:
        context["clear_material"](mat)
        mat.set_editor_property("used_with_instanced_static_meshes",True)
        mat.set_editor_property("used_with_nanite",family=="Cascade86Stone")
        mat.set_editor_property("tangent_space_normal",False)
        mat.set_editor_property("two_sided",family in ("Cascade86Fall","Cascade86Foam"))
        mat.set_editor_property("blend_mode",unreal.BlendMode.BLEND_OPAQUE if family in ("Cascade86Surface","Cascade86Stone") else unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER if family=="Cascade86Surface" else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        vc=node(mat,"MaterialExpressionVertexColor",-1600,-200)
        time=node(mat,"MaterialExpressionTime",-1600,100)
        uv=node(mat,"MaterialExpressionTextureCoordinate",-1600,300,coordinate_index=0)

        def custom(label,code,kind,inputs,x=-750,y=0):
            custom_inputs=[]
            for pin in inputs:
                value=unreal.CustomInput();value.set_editor_property("input_name",pin);custom_inputs.append(value)
            result=node(mat,"MaterialExpressionCustom",x,y,description=label,code=code,
                        output_type=getattr(unreal.CustomMaterialOutputType,"CMOT_FLOAT"+str(kind)),
                        inputs=custom_inputs)
            for pin,(source,socket) in inputs.items():link(source,socket,result,pin)
            return result

        if family=="Cascade86Surface":
            # UV is mesh-local metres with measured FBX handedness. Unlike
            # world-position phase it does not jump when the origin rebases.
            common="""
float2 q=float2(UV.x,-UV.y);
float2 flow=normalize(Flow.rg*2.0-1.0+float2(0.0001,0.0));
float2 p=q-flow*T*0.22;
float a=dot(p,(6.28318530718/128.0)*float2(13.0,7.0))+T*0.47;
float b=dot(p,(6.28318530718/128.0)*float2(-9.0,19.0))-T*0.72;
float c=dot(p,(6.28318530718/128.0)*float2(51.0,23.0))+T*1.08;
"""
            inputs={"UV":(uv,""),"Flow":(vc,"RGB"),"T":(time,"")}
            normal=custom("Rebase-stable flowing world normal",common+"""
float2 slope=0.006*cos(a)*(6.28318530718/128.0)*float2(13.0,7.0)+0.0025*cos(b)*(6.28318530718/128.0)*float2(-9.0,19.0)
            +0.0007*cos(c)*(6.28318530718/128.0)*float2(51.0,23.0);
return normalize(float3(-slope.x,-slope.y,1.0));
""",3,inputs,y=250)
            output(normal,"","MP_NORMAL")
            wave=custom("Centimetre surface displacement",common+"return float3(0,0,0.85*sin(a)+0.4*sin(b));",3,inputs,y=600)
            output(wave,"","MP_WORLD_POSITION_OFFSET")
            rough=custom("Gentle ripples with broad reflected highlights",common+"return 0.16+0.025*(0.5+0.5*sin(c));",1,inputs,y=-100)
            output(rough,"","MP_ROUGHNESS")
            output(vector(mat,(.012,.035,.075)),"","MP_BASE_COLOR")
            output(scalar(mat,0),"","MP_METALLIC")
            output(scalar(mat,.255),"","MP_SPECULAR")
            output(scalar(mat,.055),"","MP_OPACITY")
            water=node(mat,"MaterialExpressionSingleLayerWaterMaterialOutput",0,550)
            input_names=[str(pin) for pin in ml.get_material_expression_input_names(water)]
            def water_input(label,expression):
                wanted=label.replace("_","").replace(" ","").lower()
                pin=next((p for p in input_names if p.replace("_","").replace(" ","").lower().startswith(wanted)),None)
                if pin is None:raise RuntimeError("Missing single-layer water input "+label+": "+str(input_names))
                link(expression,"",water,pin)
            # Native UE units are reciprocal centimetres. At the real 2.6 m
            # depth the stone bed stays visible through blue-green absorption.
            water_input("ScatteringCoefficients",vector(mat,(.00012,.00028,.00050)))
            water_input("AbsorptionCoefficients",vector(mat,(.0024,.00065,.00023)))
            water_input("PhaseG",scalar(mat,.22))
            caustic=custom("Subtle moving transmitted caustics",common+"return 0.985+0.025*pow(saturate(0.5+0.5*sin(a+b)),7.0);",1,inputs,y=900)
            water_input("ColorScaleBehindWater",caustic)

        elif family=="Cascade86Fall":
            mat.set_editor_property("translucency_lighting_mode",unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
            mat.set_editor_property("screen_space_reflections",False)
            mat.set_editor_property("compute_fog_per_pixel",True)
            mat.set_editor_property("enable_responsive_aa",True)
            # Patch.g runs from zero at the lip to one at the impact. Both
            # fields advect toward increasing v. Their unequal cell dimensions
            # form tapered, interrupted streams instead of full-height folds.
            common="""
float u=Patch.r;float v=Patch.g;float seed=Patch.b*31.73;
// Patch.b stores the measured fall height / 100 m. The same six metre per
// second current then reads consistently on the 18 m and 36 m drops.
float trail=v*Patch.b*100.0-T*6.0;
"""+noise_field("coarse","float2(u*55.0+seed,trail*0.45-seed*0.17)")+noise_field(
                "fine","float2(u*160.0+coarse*0.6+seed*2.13,trail*3.0+seed*0.41)")+"""
float stream=smoothstep(0.25,0.72,coarse);
float froth=smoothstep(0.42,0.82,fine+0.08*coarse)*smoothstep(0.20,0.75,coarse);
float foam=saturate(froth*(0.20+0.72*smoothstep(0.25,0.98,v)));
float edge=smoothstep(0.0,0.025,u)*smoothstep(0.0,0.025,1.0-u);
"""
            inputs={"Patch":(vc,"RGB"),"T":(time,"")}
            colour=custom("Transparent streams with irregular white froth",common+"return lerp(float3(0.028,0.11,0.23),float3(0.82,0.91,0.96),saturate(0.32*stream+0.9*foam));",3,inputs,y=-300)
            alpha=custom("Separated falling ribbons and short foam trails",common+"return edge*saturate(0.07+0.36*stream+0.60*foam);",1,inputs,y=-30)
            normal=node(mat,"MaterialExpressionVertexNormalWS",-300,240)
            offset=custom("Fall flutter limited to 1.1 cm",common+"return float3(0,edge*sin(3.14159265*v)*1.1*(coarse*2.0-1.0),0);",3,inputs,y=480)
            output(colour,"","MP_BASE_COLOR");output(alpha,"","MP_OPACITY")
            output(normal,"","MP_NORMAL");output(offset,"","MP_WORLD_POSITION_OFFSET")
            output(scalar(mat,.22),"","MP_ROUGHNESS");output(scalar(mat,.28),"","MP_SPECULAR")
            output(scalar(mat,0),"","MP_METALLIC")

        elif family=="Cascade86Foam":
            mat.set_editor_property("translucency_lighting_mode",unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
            mat.set_editor_property("compute_fog_per_pixel",True)
            mat.set_editor_property("enable_responsive_aa",True)
            # The old concentric/sine mask left recognizable oval stamps at
            # each impact. Break the existing soft patch into drifting foam.
            foam_code="""
float2 p=Patch.rg*2.0-1.0;float r=length(p);float seed=Patch.b*17.0;
float rim=1.0-smoothstep(0.42,1.0,r);
"""+noise_field("coarse","float2(p.x*8.0+seed,p.y*8.0-T*0.75)")+noise_field(
                "fine","float2(p.x*19.0+coarse*0.7-seed,p.y*19.0-T*1.3+seed)")+"""
float islands=smoothstep(0.43,0.77,coarse)*smoothstep(0.30,0.72,fine);
return saturate(rim*(0.018+0.66*islands));
"""
            alpha=custom("Irregular soft impact foam and droplets",foam_code,1,{"Patch":(vc,"RGB"),"T":(time,"")},y=0)
            output(alpha,"","MP_OPACITY")
            output(vector(mat,(.82,.92,.98)),"","MP_BASE_COLOR")
            output(scalar(mat,.30),"","MP_ROUGHNESS");output(scalar(mat,.25),"","MP_SPECULAR")
            output(scalar(mat,0),"","MP_METALLIC")
            output(node(mat,"MaterialExpressionVertexNormalWS",-300,250),"","MP_NORMAL")

        else:
            aligned=context["aligned"]
            def texture(suffix):
                obj=assets.load_asset("/Game/EndlessWorld/Textures/T_Limestone_"+suffix)
                if not obj:raise RuntimeError("Missing existing limestone texture: "+suffix)
                return obj
            base=aligned(mat,texture("Base"),x=-1550,y=0)
            clean=node(mat,"MaterialExpressionLinearInterpolate",-950,0)
            link(base,"XYZ Texture",clean,"A");link(vector(mat,(1,1,1)),"",clean,"B")
            link(scalar(mat,.71),"",clean,"Alpha")
            colour=node(mat,"MaterialExpressionMultiply",-600,0)
            link(clean,"",colour,"A");link(vc,"RGB",colour,"B");output(colour,"","MP_BASE_COLOR")
            normal=aligned(mat,texture("Normal"),normal=True,x=-1550,y=450)
            pin=next(str(p) for p in ml.get_material_expression_input_names(normal) if str(p).startswith("WorldSpace"))
            link(node(mat,"MaterialExpressionStaticBool",-1600,850,value=True),"",normal,pin)
            blend=node(mat,"MaterialExpressionLinearInterpolate",-900,450)
            link(node(mat,"MaterialExpressionVertexNormalWS",-1250,700),"",blend,"A")
            link(normal,"XYZ Texture",blend,"B");link(scalar(mat,.18),"",blend,"Alpha")
            norm=node(mat,"MaterialExpressionNormalize",-500,450);link(blend,"",norm,"");output(norm,"","MP_NORMAL")
            output(scalar(mat,.69),"","MP_ROUGHNESS");output(scalar(mat,0),"","MP_METALLIC")
            output(scalar(mat,1),"","MP_AMBIENT_OCCLUSION")

        assets.set_metadata_tag(mat,"EndlessWorld.Cascade86",CONTRACT)
        assets.set_metadata_tag(mat,"EndlessWorld.Cascade86ShaderSHA256",source_sha)
        ml.recompile_material(mat);save(mat)
    context["report"]["materials"].append(name)
    context["report"].setdefault("water_city_materials",[]).append({"asset":path,"created":not exists,
        "rebuilt":rebuild,"contract":CONTRACT,"shader_sha256":source_sha,
        "shading_model":str(mat.get_editor_property("shading_model")),
        "blend_mode":str(mat.get_editor_property("blend_mode")),"tangent_space_normal":False,
        "shared_textures_read_only":True})
    unreal.log("EW_WATER_CITY_MATERIAL_READY "+name+" "+source_sha)
    return mat
