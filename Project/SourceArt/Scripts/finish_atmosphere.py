"""A cloud sea below the paths and a second bank of clouds above them."""
from pathlib import Path
import json
import unreal

root=Path(__file__).resolve().parents[2]
assets=unreal.EditorAssetLibrary
ml=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not levels.load_level("/Game/EndlessWorld/Maps/EndlessWorld"):
    raise RuntimeError("Cannot load atmosphere map")

base_path="/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud"
base=unreal.load_object(None,base_path)
if not base: raise RuntimeError("Engine cloud material is missing")
mat_path="/Game/EndlessWorld/Materials/M_CloudLayers"
mat=assets.load_asset(mat_path) if assets.does_asset_exist(mat_path) else tools.duplicate_asset("M_CloudLayers","/Game/EndlessWorld/Materials",base)
extinction=ml.get_material_property_input_node(mat,unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
if not extinction: raise RuntimeError("Expected the cloud extinction input")
if extinction.get_editor_property("desc") == "EW_CloudLayerMask":
    mask=extinction
else:
    old_output=ml.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mask=ml.create_material_expression(mat,unreal.MaterialExpressionCustom,100,0)
    mask.set_editor_property("desc","EW_CloudLayerMask")
    inputs=[]
    for name in ("Base","Position"):
        item=unreal.CustomInput();item.set_editor_property("input_name",name);inputs.append(item)
    mask.set_editor_property("inputs",inputs)
    mask.set_editor_property("output_type",unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    position=ml.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-200,500)
    if not ml.connect_material_expressions(extinction,old_output,mask,"Base"):
        raise RuntimeError("Cannot preserve cloud density")
    if not ml.connect_material_expressions(position,"",mask,"Position"):
        raise RuntimeError("Cannot connect cloud altitude")
mask.set_editor_property("code",r"""
float z=Position.z*0.01;
// The engine cloud function fades its bottom using normalized layer height.
// A separate density term lets the lower bank survive that fade.
struct SeaNoise {
    float hash(float3 p) {
        p=frac(p*0.1031); p+=dot(p,p.yzx+33.33);
        return frac((p.x+p.y)*p.z);
    }
    float value(float3 p) {
        float3 a=floor(p),b=frac(p); b=b*b*(3.0-2.0*b);
        return lerp(lerp(lerp(hash(a),hash(a+float3(1,0,0)),b.x),
                         lerp(hash(a+float3(0,1,0)),hash(a+float3(1,1,0)),b.x),b.y),
                    lerp(lerp(hash(a+float3(0,0,1)),hash(a+float3(1,0,1)),b.x),
                         lerp(hash(a+float3(0,1,1)),hash(a+float3(1,1,1)),b.x),b.y),b.z);
    }
};
SeaNoise noise;
float3 p=Position*0.000035;
float billows=noise.value(p)*0.6+noise.value(p*2.13+17.0)*0.28+noise.value(p*4.17+41.0)*0.12;
// Vary the ceiling and base across the world; the sea is a volume of billows,
// not a single horizontal height mask with a visibly flat upper edge.
float swell=noise.value(float3(p.xy*.31,7.4));
float seaCentre=-620.0+240.0*swell;
float sea=smoothstep(seaCentre-260.0,seaCentre-100.0,z)*
          (1.0-smoothstep(seaCentre+95.0,seaCentre+250.0,z));
float density=smoothstep(0.32,0.68,billows);
// Separate cumulus banks leave actual empty sky between them. A continuous
// positive density, however slight, becomes an overcast ceiling along a ray.
float3 metres=Position*.01;
if (z<420. || z>2080.) return 0.014*sea*density;
float2 cell=floor(metres.xy/2200.0);
float3 detailP=metres/140.0;
float erosion=noise.value(detailP)*.65+noise.value(detailP*2.13+19.0)*.35;
float edgeNoise=(erosion-.5)*.65;
float skyDensity=0.0;
[unroll] for (int ix=-1;ix<=1;ix++) {
    [unroll] for (int iy=-1;iy<=1;iy++) {
        float2 c=cell+float2(ix,iy);
        float seed=noise.hash(float3(c,3.1));
        if (seed<.59) {
            float3 centre=float3((c+float2(.25+.5*noise.hash(float3(c,7.3)),
                                                  .25+.5*noise.hash(float3(c,12.7))))*2200.,
                                  910.+350.*noise.hash(float3(c,21.4)));
            float width=280.+510.*noise.hash(float3(c,16.8));
            float height=230.+240.*noise.hash(float3(c,27.6));
            float angle=6.283185*noise.hash(float3(c,31.8));
            float3 delta=metres-centre;
            delta.xy=float2(delta.x*cos(angle)-delta.y*sin(angle),
                            delta.x*sin(angle)+delta.y*cos(angle));
            float field=1.-length(delta/float3(width*.74,width*.66,height*.80));
            // Raised heads and irregular side billows vary by bank. They occupy
            // real depth; the fine field erodes their surfaces instead of tiling
            // a soft oval across the sky.
            field=max(field,1.-length((delta-float3(width*.30,-width*.12,height*.53)) /
                                      float3(width*.44,width*.47,height*.84)));
            field=max(field,1.-length((delta-float3(-width*.50,width*.08,-height*.15)) /
                                      float3(width*.52,width*.48,height*.58)));
            field=max(field,1.-length((delta-float3(width*.57,width*.26,-height*.19)) /
                                      float3(width*.47,width*.41,height*.52)));
            field=max(field,1.-length((delta-float3(-width*.19,-width*.42,height*.15)) /
                                      float3(width*.44,width*.45,height*.71)));
            float volume=smoothstep(-.02,.12,field+edgeNoise)*(.55+.45*erosion);
            skyDensity=max(skyDensity,volume);
        }
    }
}
return .007*skyDensity+0.014*sea*density;
""")
if not ml.connect_material_property(mask,"",unreal.MaterialProperty.MP_SUBSURFACE_COLOR):
    raise RuntimeError("Cannot connect layered extinction")
ml.recompile_material(mat)
if not assets.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError("Cloud material save failed")

inst_path="/Game/EndlessWorld/Materials/MI_CloudSea"
inst=assets.load_asset(inst_path)
if not inst: raise RuntimeError("Expected project cloud instance")
ml.set_material_instance_parent(inst,mat)
parameters={"Layout_CloudGlobalScale":14.,"Cloud_GlobalCoverage":-.18,"Cloud_GlobalDensity":.014}
for name,value in parameters.items(): ml.set_material_instance_scalar_parameter_value(inst,name,value)
ml.update_material_instance(inst)
assets.save_loaded_asset(inst,only_if_is_dirty=False)

for actor in actors.get_all_level_actors():
    if actor.get_actor_label()=="EW_CloudSea":
        comp=actor.get_component_by_class(unreal.VolumetricCloudComponent)
        comp.set_editor_property("layer_bottom_altitude",.03)
        comp.set_editor_property("layer_height",3.4)
        comp.set_editor_property("material",inst)
        comp.set_view_sample_count_scale(2.)
        comp.set_sky_light_cloud_bottom_occlusion(.18)
    elif actor.get_actor_label()=="EW_Atmosphere":
        comp=actor.get_component_by_class(unreal.SkyAtmosphereComponent)
        # The component stores a normalized RGB color. Its Earth-scale multiplier
        # is 0.0331, not 1.0 (see USkyAtmosphereComponent's constructor).
        comp.set_editor_property("rayleigh_scattering_scale",.038065)
    elif actor.get_actor_label()=="EW_Mist":
        comp=actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
        comp.set_editor_property("fog_density",.0025)
        comp.set_editor_property("fog_height_falloff",.12)
        # A sparse layer persists at roof height, with a pale daylight fill and
        # a smaller contribution from the actual atmospheric lighting.
        second=comp.get_editor_property("second_fog_data")
        second.set_editor_property("fog_density",.0025)
        second.set_editor_property("fog_height_falloff",.032)
        second.set_editor_property("fog_height_offset",0.)
        comp.set_editor_property("second_fog_data",second)
        # Daylit suspended haze needs daylight-scale radiance, as do the windows.
        # A weak atmospheric blue contribution remains over a pale cool fill.
        comp.set_editor_property("fog_inscattering_luminance",unreal.LinearColor(7200,8800,9600,1))
        comp.set_editor_property("directional_inscattering_luminance",unreal.LinearColor(0,0,0,1))
        comp.set_editor_property("sky_atmosphere_ambient_contribution_color_scale",unreal.LinearColor(.25,.25,.25,1))
        comp.set_editor_property("start_distance",6000.)
if not levels.save_current_level(): raise RuntimeError("Atmosphere map save failed")
(root/"Saved/Verification/sky-layers.json").write_text(json.dumps({"success":True,"material":mat_path,
    "cloud_layer_altitude_from_planet_km":[.03,3.43],"sea_band_world_m":[-880,-130],
    "sea_ceiling_varies_with_xy":True,"sky_band_world_m":[420,2080],"parameters":parameters,
    "upper_shape":"separate world-space cumulus banks, irregular height, yaw and five eroded lobes",
    "view_sample_scale":2,"cloud_bottom_occlusion":.18,
    "roof_haze":{"density":.0025,"height_falloff":.032,"luminance":[7200,8800,9600],
                 "atmosphere_contribution_scale":.25}},indent=2),encoding="utf8")
unreal.log("EW_LAYERED_ATMOSPHERE_READY")
