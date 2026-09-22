"""Build a fail-closed art ledger from reviewed source associations (no UE needed)."""
from pathlib import Path
import argparse, hashlib, json, re

REPO = Path(__file__).resolve().parents[2]
ART = REPO / 'Project/SourceArt'
SCRIPTS = ART / 'Scripts'
ASSET_LICENSE = 'CC-BY-NC-4.0; see ASSET-LICENSE and NOTICE'
CODE_LICENSE = 'PolyForm-Noncommercial-1.0.0; see LICENSE and NOTICE'

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def script(name):
    p = SCRIPTS / name
    if not p.is_file(): raise ValueError('Missing authoring recipe: ' + name)
    return p.relative_to(REPO).as_posix()

def mesh_recipes(stem):
    name = stem.removeprefix('SM_')
    families = [
        (('Aero87Hull','Aero87Fins'), ['build_aero87.py']),
        (('Aero87','SkyTheatre87'), ['build_aero87_support.py','build_aero87.py']),
        (('Airship82','Skyrail82','SkyTheatre82'), ['build_city82.py']),
        (('Cascade86',), ['build_cascade86.py']),
        (('Cascade88',), ['build_cascade88.py']),
        (('Cinema',), ['build_cinema.py']),
        (('Craft95Interior',), ['build_craft95_interiors.py']),
        (('Craft95RoofRefine',), ['build_craft95_roof_refine.py']),
        (('Craft95Courtyard','Craft95RoofGarden','Craft95RoofCrown'), ['build_craft95_surroundings.py']),
        (('Craft95Cafe',), ['build_craft95.py']),
        (('Explore85',), ['build_exploration85.py']),
        (('Hotel83',), ['build_hotel83.py']),
        (('Interior',), ['build_interiors.py','interior_details.py']),
        (('Pool90',), ['build_pool90.py']),
        (('Sky92',), ['build_sky92.py']),
        (('Skyrail',), ['build_skyrail.py']),
        (('UrbanBlock_Rain','Rain',), ['rain_window_kit.py']),
        (('UrbanWalkRingFloor',), ['walk_floor_geometry.py']),
        (('UrbanLift','UrbanWalkRing'), ['city_access_kit.py']),
        (('UrbanBlock','UrbanBridge','UrbanCrown'), ['city_volume.py']),
        (('UrbanLibrary','UrbanTerrace','UrbanGarden','UrbanPromenade'), ['city_districts.py']),
        (('WaterCityWalks',), ['walk_floor_geometry.py','water_city_kit.py']),
        (('WaterCity',), ['water_city_kit.py','water_city_gardens.py']),
        (('Memory89','Terminal89'), ['build_memory89_assets.py']),
        (('Tree_','RootIsland','ForestVerge','GardenEarthDeck','LuminousPond'), ['understory_world.py','expand_art.py']),
        (('CanopyWorld',), ['forest_world.py','vertical_world.py']),
        (('SkyWard','CrystalWorld','Window'), ['vertical_world.py']),
        (('CrystalGarden','WeatheredRock','Wildflower','SkyCitadel','CascadeTemple','Balcony_','Arcade_','Fountain_'), ['expand_art.py']),
    ]
    for prefixes, recipes in families:
        if name.startswith(prefixes): return [script(s) for s in recipes]
    # Only a literal name in the original kit export is accepted as a fallback.
    text = (SCRIPTS/'build_kit.py').read_text(encoding='utf-8-sig')
    if '"'+name+'"' in text or "'"+name+"'" in text: return [script('build_kit.py')]
    return []

def texture_recipes(p):
    if 'Quality93' in p.parts: return [script('build_quality93_textures.py')]
    if 'Craft95' in p.parts: return [script('build_craft95_paving.py')]
    return [script('build_base_textures.py')]

def material_recipes(p):
    n=p.stem
    if '/Memory89/' in p.as_posix(): return [script('import_memory89.py')]
    if '/Quality93' in p.as_posix(): return [script('import_quality93.py')]
    if n=='M_Craft95Paving': return [script('import_craft95_scene.py')]
    for prefix, name in [
        ('M_Aero87','import_aero87.py'),('M_Cascade86','cascade86_materials.py'),
        ('M_Cascade88','import_cascade88.py'),('M_Cinema','import_cinema.py'),
        ('M_Explore85','fix_exploration85_steel.py'),('M_FoliageNature','finish_nature.py'),
        ('M_Nature','finish_nature.py'),('M_Hotel83','import_hotel83.py'),
        ('M_IllustratedLight','finish_art.py'),('M_Interior','import_interiors.py'),
        ('M_Life','create_fishing_materials.py'),('M_Night','import_night_lighting.py'),
        ('M_Pool90','import_pool90.py'),('M_RailGlass','import_skyrail.py'),
        ('M_Rain','import_kit.py'),('M_Sky92','import_sky92.py'),
        ('M_WaterCity','water_city_materials.py'),('MPC_DayCycle','import_cinema.py')]:
        if n.startswith(prefix): return [script(name)]
    base={'Bark','Bronze','Ceramic','Cloth','Crystal','Foliage','Fruit','Glow','Leather',
          'Limestone','Paint','Paper','Petal','Pollen','Soil','Timber','Water'}
    if n.removeprefix('M_') in base: return [script('import_kit.py')]
    return []

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-map',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=Path(__file__).with_name('provenance.json'))
    args=parser.parse_args()
    source_map=json.loads(args.source_map.read_text(encoding='utf-8'))
    imports={'Project/Content/'+r['content']:r for r in source_map['assets']}
    content=[]; source=[]; unknown=[]
    exclusions=['Project/SourceArt/Generated/**','Project/SourceArt/**/__pycache__/**',
                'Project/SourceArt/**/*.blend','Project/SourceArt/**/*.blend1','Project/SourceArt/References/**']
    clouds={'M_CloudLayers','MI_CloudSea'}
    for p in sorted((REPO/'Project/Content').rglob('*')):
        if not p.is_file(): continue
        file=p.relative_to(REPO).as_posix()
        row={'file':file,'classification':'unknown','generated_by':[], 'origin':'unresolved','license':'UNKNOWN'}
        if '/Audio89/' in file:
            row.update(classification='delegated-audio',origin='Public-release replacement audio; separate audio provenance ledger required',license='See separate audio ledger')
        elif '/Fonts/' in file:
            row.update(classification='technical-font',origin='Droid font technical dependency; separate font notice required',license='Apache-2.0; see THIRD-PARTY-NOTICES.md')
        elif p.stem in clouds:
            row.update(classification='engine-generated-not-source-archive',origin='Duplicated from recipient installed UE EngineSky VolumetricClouds; do not include raw uasset in source archive',license='Unreal Engine license; not covered by project asset license',generated_by=['Tools/Art/ensure_cloud_materials.py'])
            exclusions.append(file)
        elif file in imports:
            entry=imports[file]; src=Path(entry['source']); actual=REPO/src
            if not actual.is_file() or sha(actual)!=entry['source_sha256']: raise ValueError('Frozen source changed: '+str(src))
            recipes=mesh_recipes(src.stem) if src.suffix=='.fbx' else texture_recipes(src)
            row.update(classification='own-generated' if recipes else 'unknown',origin='Original project procedural art; matched imported source or documented public-only reimport',license=ASSET_LICENSE,generated_by=recipes,source_file=src.as_posix(),source_sha256=entry['source_sha256'],source_association='import-md5-and-reviewed-metadata-only-change' if not entry['intentional_reimport'] else 'public-only-reimport')
        elif p.suffix=='.umap':
            row.update(classification='own-generated',origin='Original project scene assembly using project art and references to installed engine assets',license=ASSET_LICENSE,generated_by=[script('import_kit.py'),script('finish_atmosphere.py'),script('finish_city_light.py')])
        else:
            recipes=material_recipes(p)
            if recipes: row.update(classification='own-generated',origin='Original procedural material graph or project parameter collection; engine functions are references, not copied source',license=ASSET_LICENSE,generated_by=recipes)
        if row['classification']=='unknown': unknown.append(file)
        content.append(row)
    for p in sorted(ART.rglob('*')):
        if not p.is_file() or any(x in p.parts for x in ['Generated','__pycache__','References','Audio89']):continue
        if p.suffix.lower() in ['.blend','.blend1','.pyc']:continue
        file=p.relative_to(REPO).as_posix(); recipes=[]
        if p.suffix=='.fbx': recipes=mesh_recipes(p.stem)
        elif p.suffix=='.png': recipes=texture_recipes(p)
        elif p.suffix in ['.py','.hlsl']: recipes=[file]
        elif p.suffix in ['.json','.h']:
            data_families=[
                ('aero87',['build_aero87.py','build_aero87_support.py']),
                ('cascade86',['build_cascade86.py']),('cascade88',['build_cascade88.py']),
                ('cinema',['build_cinema.py']),('city82',['build_city82.py']),
                ('craft95-interiors',['build_craft95_interiors.py']),
                ('craft95-paving',['build_craft95_paving.py']),
                ('craft95-roof-refine',['build_craft95_roof_refine.py']),
                ('craft95-surroundings',['build_craft95_surroundings.py']),
                ('craft95-assets',['build_craft95.py']),('exploration85',['build_exploration85.py']),
                ('hotel83',['build_hotel83.py']),('interior-layout',['build_interiors.py']),
                ('kit-catalog',['build_kit.py','expand_art.py','city_volume.py','city_districts.py']),
                ('pool90',['build_pool90.py']),('quality93-cinema',['build_cinema.py']),
                ('quality93-interior',['build_interiors.py']),('quality93-textures',['build_quality93_textures.py']),
                ('sky92',['build_sky92.py']),('skyrail',['build_skyrail.py']),
                ('water-city-gardens',['water_city_gardens.py']),
                ('water-city-layout',['water_city_layout.py']),('water-city-source-audit',['water_city_kit.py']),
                ('source-checks',['build_cinema.py'])]
            recipes=next(([script(s) for s in names] for prefix,names in data_families if p.name.startswith(prefix)),[])
            if p.name=='catalog.json' and p.parent.name=='Memory89':recipes=[script('build_memory89_assets.py')]
            if not recipes:recipes=[file] # Explicitly authored settings/manifests, not imported art.
        row={'file':file,'sha256':sha(p),'bytes':p.stat().st_size,'classification':'own-generated' if recipes else 'unknown',
             'generated_by':recipes,'origin':'Original project procedural authoring file; imported baselines retain exact pre-review geometry',
             'license':CODE_LICENSE if p.suffix in ['.py','.h','.hlsl'] else ASSET_LICENSE}
        if p.suffix in ['.json','.h']:row['origin']='Project-authored generation parameters, geometry catalog, or generated runtime layout data; not an imported creative resource'
        if row['classification']=='unknown':unknown.append(file)
        source.append(row)
    result={'schema_version':1,'success':not unknown,'content_assets':content,'source_art':source,'unknown':unknown,
            'source_archive_exclusions':exclusions,
            'limits':['Source association and procedural authoring audit; not a legal originality guarantee.',
                      'No bit-identical clean regeneration claim: historical imported baselines and procedural revisions coexist.',
                      'Content final hashes must be set by release packager after UE import metadata sanitization.',
                      'Delegated audio and technical fonts require their separate release ledgers.',
                      'Generated reports and raw engine-derived cloud uassets are not public source files.']}
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'success':not unknown,'content':len(content),'source_art':len(source),'unknown':unknown}))
    if unknown: raise SystemExit(2)

if __name__=='__main__':main()
