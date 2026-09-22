"""Craft95 cafe/library visual rebuild. Blender --factory-startup -b -t 4.

Reuse the original room construction and collision plan in memory; export four
new FBX candidates only. Never write EWExplore85Data.h or existing asset files.
"""
from pathlib import Path
import hashlib
import json
import math
import random
import shutil
import bpy

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
ROOT = PROJECT.parent
source_path = HERE / 'build_exploration85.py'
source_text = source_path.read_text(encoding='utf-8')
protected = [source_path, PROJECT / 'Source/EndlessWorld/EWExplore85Data.h',
             PROJECT / 'SourceArt/exploration85-catalog.json']
protected += [PROJECT / f'SourceArt/Meshes/SM_Explore85{group}{index}.fbx'
              for group in ('Shell', 'Furniture') for index in (0, 2)]
protected_before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
baseline = json.loads((PROJECT / 'SourceArt/exploration85-catalog.json').read_text(encoding='utf-8'))
assert not (PROJECT/'SourceArt/craft95-interiors.json').exists(),'Preserve the prior candidate manifest before regenerating'

# This prefix defines original helpers and material families without building
# or exporting the six rooms. The suffix that writes data/header is never run.
prefix, room_body = source_text.split('for k in range(6):', 1)
exec(compile(prefix, str(source_path), 'exec'), globals())
old_bench, old_books, old_emit = bench, books, emit
new_records = []


def tint(colour, factor):
    return tuple(min(.92, max(.015, x * factor)) for x in colour[:3]) + (1,)


def curve(a, b, c, d, count=16):
    a, b, c, d = map(Vector, (a, b, c, d))
    return [tuple(a * (1-t)**3 + b * 3*t*(1-t)**2 + c * 3*t*t*(1-t) + d * t**3)
            for t in (i / count for i in range(count + 1))]


def moulding(b, centre, length, profile, colour=WOOD, family='Timber'):
    """Continuous carved cross-section, extruded along X with real shadow lips."""
    x, y, z = centre
    vertices = [(x + side * length / 2, y + py, z + pz)
                for side in (-1, 1) for py, pz in profile]
    n = len(profile)
    faces = [tuple(reversed(range(n))), tuple(range(n, 2*n))]
    faces += [(i, (i+1) % n, (i+1) % n + n, i+n) for i in range(n)]
    b.mesh(vertices, faces, family, colour, smooth=False)


def stone_floor(b):
    """75cm cut stone; a 5mm grout reveal and a 4mm chamfer, no raised grid."""
    pitch, gap, edge = .75, .005, .004
    half = (pitch-gap)/2
    outer = [(-half+edge,-half), (half-edge,-half), (half,-half+edge), (half,half-edge),
             (half-edge,half), (-half+edge,half), (-half,half-edge), (-half,-half+edge)]
    inner = [(x*(half-edge)/half, y*(half-edge)/half) for x,y in outer]
    vertices = [(x,y,-.011) for x,y in outer] + [(x,y,.003) for x,y in outer] + [(x,y,.007) for x,y in inner]
    faces = [tuple(reversed(range(8))), tuple(range(16,24))]
    for start in (0,8):
        faces += [(start+i,start+(i+1)%8,start+(i+1)%8+8,start+i+8) for i in range(8)]
    for iy in range(48):
        for ix in range(48):
            # Quiet variation, not a checkerboard. Every surface is still the
            # existing InteriorCeramic material after the original FINISH map.
            gain = .955 + .012 * ((ix*17 + iy*29 + ix*iy*3) % 7)
            b.mesh(vertices, faces, 'Ceramic', tint(STONE, gain),
                   position=(-17.625+ix*pitch,-17.625+iy*pitch,0), smooth=False)


def table(b, rows, x, y, w, d, h, colour):
    # Same old collision envelope and cup locations; furniture fits the same
    # authored seating bays. Turned legs, tenons and apron replace the cone.
    solid(rows,(x,y,h/2),(w,d,h+.09))
    box(b,(x,y,h),(w,d,.09),'Ceramic',colour,.018)
    box(b,(x,y,h-.059),(w-.05,d-.05,.031),'Bronze',GOLD,.008)
    for sx in (-1,1):
        for sy in (-1,1):
            lx=x+sx*(w/2-.16); ly=y+sy*(d/2-.16)
            profile=[(0,.006),(.043,.006),(.047,.025),(.044,.08),(.029,.12),
                     (.029,h*.40),(.047,h*.46),(.045,h*.50),(.031,h*.57),
                     (.031,h-.14),(.059,h-.12),(.059,h-.065),(0,h-.065)]
            b.lathe(profile,(lx,ly,0),'Timber',24,tint(WOOD,1.05))
            b.lathe([(.044,.017),(.050,.020),(.050,.058),(.045,.062)],(lx,ly,0),'Bronze',24,GOLD)
    for sy in (-1,1):
        box(b,(x,y+sy*(d/2-.17),h-.145),(w-.25,.055,.15),'Timber',WOOD,.01)
        for dx in (-w/2+.23,w/2-.23):
            # Small pegged mortise caps remain below the top and within its rim.
            box(b,(x+dx,y+sy*(d/2-.202),h-.14),(.038,.009,.038),'Bronze',GOLD,.004)
    for sx in (-1,1):box(b,(x+sx*(w/2-.17),y,h-.145),(.055,d-.25,.15),'Timber',WOOD,.01)
    cup(b,x-.25,y,h+.08);cup(b,x+.25,y+.12,h+.08)


def chair(b, rows, x, y, yaw=0, c=TEAL):
    t=B();rr=[]
    # Preserve the exact two original solid envelopes and orientation.
    solid(rr,(0,0,.24),(.75,.72,.48));solid(rr,(0,.30,.85),(.79,.17,.66))
    for sx in (-1,1):
        for sy in (-1,1):
            points=[(sx*.29,sy*.27,.025),(sx*.265,sy*.25,.23),(sx*.25,sy*.24,.50)]
            t.tube(points,[.025,.029,.036],'Timber',14,tint(WOOD,1.1))
            t.tube([points[0],(sx*.287,sy*.268,.09)],[.028,.028],'Bronze',14,GOLD)
        points=curve((sx*.25,.24,.44),(sx*.32,.35,.76),(sx*.29,.37,1.115),(sx*.18,.32,1.13))
        t.tube(points,[.030]*len(points),'Timber',14,tint(WOOD,1.1))
        t.tube([(sx*.268,-.24,.29),(sx*.268,.24,.29)],[.014,.014],'Timber',10,WOOD)
    box(t,(0,0,.474),(.69,.665,.063),'Timber',WOOD,.022)
    soft(t,(0,-.018,.556),(.65,.632,.135),tint(c,1.09))
    soft(t,(0,.296,.92),(.638,.132,.405),c)
    top=curve((-.29,.335,1.09),(-.20,.38,1.175),(.20,.38,1.175),(.29,.335,1.09),24)
    t.tube(top,[.025]*len(top),'Timber',14,tint(WOOD,1.15))
    # A bow-shaped lower back rail casts a fine shadow beneath the upholstery.
    low=curve((-.29,.31,.71),(-.12,.365,.66),(.12,.365,.66),(.29,.31,.71),20)
    t.tube(low,[.020]*len(low),'Timber',12,WOOD)
    merge(b,t,(x,y,0),yaw)
    for row in rr:
        q=Matrix.Rotation(yaw,3,'Z');p=q@Vector(row['p']);e=q@Vector(row['e'])
        rows.append({'p':[p.x+x*100,p.y+y*100,p.z],'e':[abs(e.x),abs(e.y),abs(e.z)],'floor':False})


def bench(b,rows,x,y,w=4):
    old_bench(b,rows,x,y,w)
    # Routed stiles, toe reveal and restrained joinery on the original base.
    for z in (.09,.395):box(b,(x,y-.407,z),(w-.09,.024,.026),'Timber',tint(WOOD,1.25),.006)
    for i in range(max(2,int(w/.62))+1):
        xx=x-(w-.18)/2+i*(w-.18)/max(2,int(w/.62))
        box(b,(xx,y-.41,.24),(.033,.024,.29),'Timber',tint(WOOD,1.15),.005)


def books(b,rows,x,y,w,wood,accent):
    old_books(b,rows,x,y,w,wood,accent)
    # Curved cornice and bottom bead, contained in the same bookshelf bays.
    profile=[(-.015,-.025),(-.015,.015),(-.045,.020),(-.049,.002),(-.065,-.008),(-.065,-.025)]
    moulding(b,(x,y-.24,2.97),w-.025,profile,tint(wood,1.2))
    moulding(b,(x,y-.235,.135),w-.07,[(0,-.019),(0,.019),(-.038,.019),(-.044,.01),(-.044,-.01),(-.038,-.019)],wood)


def gallery_rail(b,rows,a,c):
    # Keep the original post spacing, top height, and exact collider envelope.
    for z in (4.36,5.34):beam(b,(*a,z),(*c,z),.045)
    distance=math.dist(a,c);steps=math.ceil(distance/.8)
    for i in range(steps+1):
        p=[a[n]+(c[n]-a[n])*i/steps for n in range(2)]
        beam(b,(*p,4.2),(*p,5.34),.023)
        for z in (4.22,4.36,5.27):
            b.lathe([(.029,z-.026),(.050,z-.020),(.053,z+.006),(.046,z+.026),(.029,z+.030)],(*p,0),'Bronze',20,GOLD)
        # Slim centre flutes break the uninterrupted pole highlight.
        for side in (-1,1):
            beam(b,(p[0]+side*.015,p[1],4.43),(p[0]+side*.015,p[1],5.21),.004,'Bronze',tint(GOLD,.65))
    solid(rows,((a[0]+c[0])/2,(a[1]+c[1])/2,4.78),(max(.12,abs(a[0]-c[0])),max(.12,abs(a[1]-c[1])),1.16))


def detail_counter(b):
    # All relief remains in front of the existing nine-metre counter solid.
    for i in range(6):
        x=-11.675+i*1.47
        box(b,(x,8.075,.57),(1.34,.033,.72),'Timber',tint(WOOD,.73),.024)
        for dx in (-.647,.647):box(b,(x+dx,8.048,.57),(.032,.034,.72),'Timber',tint(WOOD,1.2),.009)
        for z in (.23,.91):box(b,(x,8.048,z),(1.315,.034,.038),'Timber',tint(WOOD,1.17),.010)
    moulding(b,(-8,8.08,.13),8.91,[(0,-.075),(0,.045),(-.075,.045),(-.08,.025),(-.085,-.075)],tint(WOOD,.62))
    moulding(b,(-8,8.09,1.047),8.98,[(0,0),(0,.045),(-.075,.045),(-.08,.017),(-.045,0)],tint(WOOD,1.1))


def emit(name,b,notes,rays=()):
    if name.startswith('Explore85Glass'):
        return  # Existing glazing is deliberately not rebuilt or exported.
    target=name
    new_name=name.replace('Explore85','Craft95Interior')
    previous=PROJECT/f'SourceArt/Meshes/SM_{new_name}.fbx'
    if previous.exists():
        digest=hashlib.sha256(previous.read_bytes()).hexdigest()
        backup=ROOT/'BeforeCraft95/GeneratedInteriors'/digest/previous.name
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists():shutil.copy2(previous,backup)
        assert hashlib.sha256(backup.read_bytes()).hexdigest()==digest
    old_emit(new_name,b,notes+'; Craft95 stone joints, turned joinery and jointed rails',rays)
    item=dict(records[-1]);item['target_name']=target
    item['used_materials']=sorted({g['FAMILIES'][i] for i in b.m})
    reference=next(row for row in baseline['assets'] if row['name']==target)
    item['reference_catalog_ue_bounds_cm']=reference['ue_bounds_cm']
    item['bounds_delta_cm']=[[round(item['ue_bounds_cm'][i][j]-reference['ue_bounds_cm'][i][j],5) for j in range(3)] for i in range(2)]
    require_bounds=max(abs(value) for side in item['bounds_delta_cm'] for value in side)
    # Material/leaf helper improvements from 93 may move decorative extremities;
    # structural shells must retain their frame exactly.
    if 'Shell' in target:assert require_bounds < .2,(target,item['bounds_delta_cm'])
    # The existing library furniture already has 3.65m triangles of individual
    # books. Bound growth relative to that baseline rather than lowering its
    # established craft or duplicating it into new actors.
    limit=max(900000,int(reference['triangles']*1.12)) if 'Furniture' in target else 260000
    assert item['triangles'] < limit, (target,item['triangles'],limit)
    item['triangle_budget']=limit
    new_records.append(item)


body_source='for k in (0,2):'+room_body.split("OUT.joinpath('exploration85-catalog.json')",1)[0]
old_floor="""    for j in range(-8,9):
        box(s,(j*2,0,.006),(.012,35.95,.009),'Bronze',(.21,.23,.19,1),0)
        box(s,(0,j*2,.006),(35.95,.012,.009),'Bronze',(.21,.23,.19,1),0)"""
assert old_floor in body_source
body_source=body_source.replace(old_floor,'    stone_floor(s)')
body_source=body_source.replace("body(s,rows,(0,0,-.13),(36,36,.26),'Ceramic',STONE,.012)",
                                "body(s,rows,(0,0,-.13),(36,36,.26),'Ceramic',(.24,.25,.225,1),.012)")
body_source=body_source.replace("        letters(f,'SLOW COFFEE  /  SKY ROAST'", "        detail_counter(f)\n        letters(f,'SLOW COFFEE  /  SKY ROAST'")
exec(compile(body_source,str(source_path)+' [Craft95 bounded visuals]','exec'),globals())

# Compare generated plans in memory against the unchanged original catalogue.
# Rounded floats tolerate benign matrix arithmetic only, not changed obstacles.
def rounded(value):
    if isinstance(value,float):return round(value,3)
    if isinstance(value,list):return [rounded(x) for x in value]
    if isinstance(value,dict):return {k:rounded(v) for k,v in value.items()}
    return value

plan_checks=[]
for plan in plans:
    old=next(row for row in baseline['rooms'] if row['kind']==plan['kind'])
    for field in ('bodies','lights','route','seat','stand'):
        equal=rounded(plan[field])==rounded(old[field])
        plan_checks.append({'kind':plan['kind'],'field':field,'unchanged':equal})
        assert equal,(plan['kind'],field,'original plan changed')
protected_after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
assert protected_before==protected_after,'Existing source/data changed during visual generation'
result={'revision':95,'scope':'Cafe and library visual refinement only; current physical plan preserved',
        'assets':new_records,'plan_checks':plan_checks,'protected_sha256':protected_after,
        'floor':{'pitch_cm':75,'grout_mm':5,'bevel_mm':4,'tile_top_cm':.7,'collision_floor_cm':0},
        'runtime_visual_acceptance':'NOT_RUN','generated_in_blender':bpy.app.version_string,
        'material_policy':'Existing FINISH families and Quality93Final materials; no material assets modified'}
manifest=PROJECT/'SourceArt/craft95-interiors.json'
assert not manifest.exists(),'Preserve prior candidate manifest before regenerating'
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(PROJECT/'SourceArt/Blender/Craft95Interiors.blend'))
print('CRAFT95_INTERIORS_COMPLETE',len(new_records),sum(item['triangles'] for item in new_records),flush=True)
