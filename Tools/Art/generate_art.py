"""Run selected original Blender recipes in this checkout; packaged art needs no regeneration."""
from pathlib import Path
import argparse, json, os, shutil, subprocess

REPO=Path(__file__).resolve().parents[2]
SCRIPTS=REPO/'Project/SourceArt/Scripts'
RECIPES={
    'aero-public':'build_aero87.py',
    'aero-support':'build_aero87_support.py',
    'base-textures':'build_base_textures.py',
    'quality-textures':'build_quality93_textures.py',
    'paving':'build_craft95_paving.py',
    'cafe':'build_craft95.py',
    'interior-detail':'build_craft95_interiors.py',
    'roof-gardens':'build_craft95_surroundings.py',
    'roof-detail':'build_craft95_roof_refine.py',
    'hotels':'build_hotel83.py',
    'exploration':'build_exploration85.py',
    'water-city':'build_cascade86.py',
    'water-palaces':'build_cascade88.py',
    'sky-landmarks':'build_sky92.py',
    'pools':'build_pool90.py',
    'skyrail':'build_skyrail.py',
    'airship-theatre':'build_city82.py',
    'memory-terminal':'build_memory89_assets.py',
    'cinema':'build_cinema.py',
    'interiors':'build_interiors.py',
}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--list',action='store_true')
    p.add_argument('--recipe',choices=sorted(RECIPES))
    p.add_argument('--blender',help='Blender 4.5 executable; alternatively BLENDER_EXE or PATH')
    a=p.parse_args()
    if a.list or not a.recipe:
        print(json.dumps(RECIPES,indent=2));return
    blender=a.blender or os.environ.get('BLENDER_EXE') or shutil.which('blender')
    if not blender or not Path(blender).is_file():p.error('Provide --blender with your installed Blender 4.5 executable')
    target=(SCRIPTS/RECIPES[a.recipe]).resolve()
    if not target.is_relative_to(SCRIPTS.resolve()) or not target.is_file():p.error('Recipe missing from this checkout')
    generated=REPO/'Project/SourceArt/Generated';generated.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ);env['PYTHONDONTWRITEBYTECODE']='1'
    # Blender recipes create authoring outputs in this checkout. They never launch UE.
    command=[blender,'--background','--factory-startup','--python',str(target)]
    print('Generating',a.recipe,'in',REPO,flush=True)
    with (generated/(a.recipe+'-blender.log')).open('w',encoding='utf-8') as log:
        completed=subprocess.run(command,cwd=REPO,env=env,stdout=log,stderr=subprocess.STDOUT)
    if completed.returncode:raise SystemExit(completed.returncode)
    print('Generated. Review output before running the corresponding UE import recipe.')

if __name__=='__main__':main()
