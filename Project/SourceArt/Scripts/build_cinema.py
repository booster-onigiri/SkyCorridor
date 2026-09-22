"""Waterlight cinema: one authored plan for meshes, collision, seats and audio bounds.

Coordinates are Unreal metres (+Y faces the screen); FBX reflection is compensated.
Only three new meshes are exported. Existing city geometry and seeds stay intact.
"""
from pathlib import Path
import ast, json, math, random, runpy
import bpy
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
Builder,export,column=g['Builder'],g['export'],g['column']
OUT,CATALOG=g['OUT'],g['CATALOG'];ONLY=None
FINISH={'Timber':'InteriorOak','Paint':'InteriorPlaster','Cloth':'InteriorFabric',
        'Ceramic':'InteriorCeramic','Glow':'CinemaGlow','Bronze':'InteriorBrass',
        'Foliage':'InteriorLeaf','Bark':'InteriorStem','Paper':'InteriorPaper'}
for original,family in FINISH.items():
    g['FAMILIES'].append(family);g['COLOURS'][family]=g['COLOURS'][original]
    mat=bpy.data.materials.new('M_'+family);mat.diffuse_color=g['COLOURS'][family];g['MATERIALS'].append(mat)
OAK=(.24,.105,.042,1);LIGHT_OAK=(.42,.235,.105,1);WALNUT=(.07,.03,.015,1)
IVORY=(.77,.72,.61,1);TEAL=(.012,.085,.078,1);LINEN=(.34,.22,.11,1)
GOLD=(.48,.29,.10,1);CARPET=(.019,.026,.036,1);VELVET=(.025,.105,.11,1)
defs=[n for n in ast.parse(Path(__file__).with_name('build_interiors.py').read_text(encoding='utf8')).body
      if isinstance(n,ast.FunctionDef) and n.name in {'merge','box','beam','exported','plant','vase','art'}]
exec(compile(ast.Module(body=defs,type_ignores=[]),'cinema_helpers','exec'),globals())
COLLISIONS=[];SEATS=[];shell=Builder();furniture=Builder();detail=Builder()
font=bpy.data.fonts.load(str(OUT.parent/'Content/Fonts/DroidSansFallback.ttf'))

def solid(b,p,d,family='Cloth',colour=CARPET,bevel=.015,floor=False):
    box(b,p,d,family,colour,bevel)
    COLLISIONS.append({'centre':[v*100 for v in p],'extent':[v*50 for v in d],'floor':floor})

def label(b,text,p,size=.22,colour=IVORY,yaw=0):
    curve=bpy.data.curves.new('Cinema lettering','FONT');curve.body=text;curve.font=font
    curve.size=size;curve.align_x='CENTER';curve.align_y='CENTER';curve.extrude=.0015;curve.resolution_u=3
    obj=bpy.data.objects.new('Cinema lettering',curve);bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.object.convert(target='MESH');mesh=obj.data
    part=Builder();r=Matrix.Rotation(math.pi/2,3,'X')
    # Unreal's +Y-facing camera has screen-right along -X. Reflect the
    # lettering horizontally and preserve its front-face winding.
    rotated=[r@v.co for v in mesh.vertices]
    part.v=[(-v.x,v.y,v.z) for v in rotated];part.f=[tuple(reversed(poly.vertices)) for poly in mesh.polygons]
    part.m=[g['FAMILIES'].index('Paint')]*len(part.f);part.col=[colour]*len(part.v);part.smooth=[False]*len(part.f)
    merge(b,part,p,yaw);bpy.data.objects.remove(obj,do_unlink=True)

# An opaque room inside the retained library shell; the original columns stay
# in the perimeter gallery, beyond these acoustic walls.
solid(shell,(-12.2,1.5,5.65),(.4,25.8,11.3),'Paint',(.018,.030,.037,1))
solid(shell,(12.2,1.5,5.65),(.4,25.8,11.3),'Paint',(.018,.030,.037,1))
solid(shell,(0,14.2,5.65),(24.4,.4,11.3),'Paint',(.018,.030,.037,1))
solid(shell,(-1.65,-11.2,5.65),(20.7,.4,11.3),'Paint',TEAL)
solid(shell,(11.75,-11.2,5.65),(.9,.4,11.3),'Paint',TEAL)
solid(shell,(10,-11.2,7.75),(2.6,.4,7.1),'Paint',TEAL)
solid(shell,(0,1.5,11.45),(24.8,25.8,.3),'Paint',(.010,.016,.021,1))
solid(shell,(0,1.4,.005),(24.,25.2,.05),'Cloth',CARPET,floor=True)
# Six 19cm risers, with wide 1m treads between the paired row transitions.
for top_y,height in [(1.25,.19),(.25,.38),(-2.25,.57),(-3.25,.76),(-5.75,.95),(-6.75,1.14)]:
    solid(shell,(0,(-11+top_y)/2,height-.095),(24.,11+top_y,.19),'Cloth',CARPET,floor=True)
    for x,w in [(0,2.7),(-10.45,2.3),(10.45,2.3)]:
        box(detail,(x,top_y+.006,height-.07),(w,.022,.025),'Glow',(1,.47,.15,1),.003)
# A separate low lobby and a right-hand dogleg, with another six shallow steps.
solid(shell,(0,-14.1,.005),(26.8,5.4,.05),'Timber',LIGHT_OAK,floor=True)
solid(shell,(-4.2,-14.1,3.85),(18.2,5.4,.3),'Paint',TEAL)
solid(shell,(10,-13.5,4.25),(2.6,4.4,.22),'Paint',TEAL)
solid(shell,(8.55,-13.2,2.1),(.3,4.0,4.2),'Paint',TEAL)
solid(shell,(11.45,-12.95,2.1),(.3,3.5,4.2),'Paint',TEAL)
for i in range(6):
    start=-14.9+i*.6;height=(i+1)*.19
    solid(shell,(10,(start-11.0)/2,height-.095),(2.6,-11.0-start,.19),'Cloth',CARPET,floor=True)
    box(detail,(10,start-.008,height-.07),(2.2,.025,.03),'Glow',(1,.5,.16,1),.003)
# Stage and a physical screen surround; the video face sits 2cm in front.
solid(shell,(0,11.7,.29),(19.4,4.6,.58),'Timber',WALNUT,floor=True)
box(shell,(0,13.20,6.20),(14.55,.30,8.34),'Paint',(.006,.009,.012,1),.04)
for x in [-7.33,7.33]:box(detail,(x,13.01,6.20),(.055,.05,8.36),'Bronze',GOLD,.009)
for z in [2.015,10.385]:box(detail,(0,13.01,z),(14.68,.05,.055),'Bronze',GOLD,.009)
# Pleated side curtains, sound-absorbing panels and restrained brass battens.
for s in [-1,1]:
    for i in range(14):
        x=s*(7.5+i*.27);y=12.83+.10*math.sin(i*math.pi*.65)
        box(detail,(x,y,5.4),(.28,.22,9.8),'Cloth',VELVET,.09)
    for y in [-8.8,-5.3,-1.8,1.7,5.2,8.7]:
        box(detail,(s*11.93,y,5.7),(.13,2.5,5.5),'Cloth',(.038,.065,.081,1),.06)
        for d in [-.78,0,.78]:box(detail,(s*11.83,y+d,5.7),(.055,.065,5.25),'Timber',LIGHT_OAK,.008)
        box(detail,(s*11.8,y,2.6),(.04,2.35,.035),'Glow',(1,.48,.16,1),.004)
    box(detail,(s*11.9,1.4,9.3),(.12,24.2,.065),'Glow',(1,.48,.18,1),.012)
    for y in [-9.8,-6.3,-2.8,.7,4.2,7.7,11.2]:
        box(detail,(s*11.77,y,1.8),(.12,.55,.18),'Bronze',GOLD)
        box(detail,(s*11.69,y,1.76),(.035,.42,.09),'Glow',(1,.45,.14,1),.006)
for y in [-8.4,-4.9,-1.4,2.1,5.6,9.1]:
    box(detail,(0,y,10.97),(21.2,.58,.18),'Cloth',(.024,.037,.045,1),.06)

# 32 upholstered armchairs, four rows with three generous aisles.
for row,(y,z) in enumerate([(3.,0),(-.5,.38),(-4.,.76),(-7.5,1.14)]):
    for col,x in enumerate([-7.95,-6.1,-4.25,-2.4,2.4,4.25,6.1,7.95]):
        name=f'{chr(65+row)}{col+1:02}'
        solid(furniture,(x,y,z+.37),(1.16,1.32,.74),'Timber',WALNUT,.05)
        box(furniture,(x,y+.12,z+.61),(1.02,1.15,.23),'Cloth',VELVET,.10)
        solid(furniture,(x,y-.52,z+.93),(1.13,.26,.76),'Cloth',VELVET,.09)
        box(furniture,(x,y-.40,z+1.22),(.88,.15,.22),'Cloth',(.041,.13,.13,1),.08)
        for dx in [-.64,.64]:
            box(furniture,(x+dx,y,z+.70),(.21,1.30,.24),'Timber',OAK,.05)
            box(furniture,(x+dx,y,z+.83),(.23,1.26,.10),'Cloth',VELVET,.045)
            furniture.lathe([(.065,0),(.085,0),(.085,.025),(.065,.025)],(x+dx,y+.40,z+.891),'Bronze',24,GOLD)
        label(detail,name,(x,y-.667,z+.92),.11,IVORY)
        SEATS.append({'name':name,'position':[x*100,(y+.10)*100,(z+.95)*100],
                      'stand':[x*100,(y+1.35)*100,(z+.89)*100]})
    label(detail,chr(65+row),(11.65,y,z+1.6),.24,GOLD,-math.pi/2)

# Lobby: the title is visible through the original south arch from the plaza.
label(detail,'水鏡の映写室',(1,-11.43,2.7),.68,IVORY)
label(detail,'W A T E R L I G H T   C I N E M A',(1,-11.44,1.85),.20,GOLD)
label(detail,'32 SEATS     /     SCREEN 01',(1,-11.44,1.32),.16,IVORY)
label(detail,'シアター',(10,-15.48,3.30),.36,IVORY)
label(detail,'CINEMA',(11.5,-18.30,4.50),.68,IVORY)
box(detail,(11.5,-18.19,4.50),(7.2,.13,1.15),'Paint',TEAL,.03)
for y in [-15.9,-12.2]:box(detail,(-4.5,y,3.65),(17.8,.04,.05),'Glow',(1,.63,.28,1),.005)
for x in [-10.,-6.5,-3.]:
    box(detail,(x,-11.46,2.12),(2.3,.10,2.35),'Timber',WALNUT)
    art(detail,x,-11.53,2.12,1.92,1.72)
solid(furniture,(-4.5,-15.55,.36),(5.9,1.2,.72),'Cloth',VELVET,.12)
solid(furniture,(-4.5,-16.07,.79),(5.9,.20,1.00),'Timber',OAK,.07)
for x in [-8.4,6.5]:plant(detail,x,-15.2,1.6,int(x*10+100))
solid(furniture,(5,-13.4,.6),(1.7,.8,1.2),'Timber',OAK)
box(detail,(5,-13.4,1.23),(1.85,.92,.08),'Ceramic',IVORY)
label(detail,'上映を選ぶ   E',(5,-13.87,1.0),.17,GOLD)
label(detail,'EXIT',(10,-10.965,3.55),.28,(.32,.65,.43,1),math.pi)
box(detail,(10,-11.02,3.55),(1.12,.08,.44),'Paint',TEAL,.02)

# Check all seated eyes against the full auditorium mesh, including other seats.
all_b=Builder()
for b in [shell,furniture,detail]:merge(all_b,b)
tree=BVHTree.FromPolygons(all_b.v,all_b.f)
rays=[]
for seat in SEATS:
    eye=Vector([v/100 for v in seat['position']])+Vector((0,0,.47))
    for target in [(-6.6,13.00,2.5),(6.6,13.00,2.5),(0,13.00,6.2),(0,13.00,9.9)]:
        delta=Vector(target)-eye;hit=tree.ray_cast(eye,delta.normalized(),delta.length-.03)[0]
        assert hit is None,(seat['name'],target,list(hit) if hit else None)
        rays.append({'seat':seat['name'],'target':target,'clear':True})
exported('CinemaShell',shell,'Opaque auditorium and vestibule with six safe 19cm risers and a raised stage')
exported('CinemaFurniture',furniture,'32 velvet armchairs with cup holders, four sightline-checked tiers, lobby seating')
exported('CinemaDetails',detail,'Acoustic panels, curtains, warm aisle lights, signage and original lobby art')
# Distinct standing/seated visitors keep the same eye height as the local
# character. Rounded coats and ceramic faces fit the city's quiet palette.
for seated in [False,True]:
    b=Builder();hip=.63 if seated else .91;shoulder=1.24 if seated else 1.53
    b.lathe([(0,0),(.32,0),(.30,.28),(.23,shoulder-hip),(.14,shoulder-hip+.08),(0,shoulder-hip+.08)],(0,0,hip),'Cloth',32,VELVET)
    head_z=1.46 if seated else 1.76
    profile=[(.20*math.sin(t*math.pi/16),.23*math.cos(t*math.pi/16)) for t in range(16,-1,-1)]
    b.lathe(profile,(0,0,head_z),'Ceramic',32,IVORY)
    for side in [-1,1]:
        x=side*.17
        if seated:
            beam(b,(x,-.05,.68),(x,.48,.61),.095,'Cloth',CARPET)
            beam(b,(x,.48,.61),(x,.52,.15),.083,'Cloth',CARPET)
            box(b,(x,.63,.10),(.22,.44,.17),'Timber',WALNUT,.07)
            beam(b,(side*.27,0,shoulder-.04),(side*.35,.02,.89),.075,'Cloth',VELVET)
            beam(b,(side*.35,.02,.89),(side*.39,.41,.82),.065,'Cloth',VELVET)
            box(b,(side*.39,.45,.82),(.13,.23,.12),'Ceramic',IVORY,.05)
        else:
            beam(b,(x,0,.97),(x,0,.17),.095,'Cloth',CARPET)
            box(b,(x,.09,.10),(.23,.45,.18),'Timber',WALNUT,.07)
            beam(b,(side*.26,0,shoulder-.03),(side*.38,.02,.92),.075,'Cloth',VELVET)
            box(b,(side*.38,.035,.85),(.13,.16,.20),'Ceramic',IVORY,.05)
    # A short woven scarf points toward +Y (the face's forward direction).
    box(b,(0,.16,shoulder+.01),(.30,.14,.12),'Cloth',(.54,.28,.10,1),.045)
    box(b,(.085,.235,shoulder-.18),(.12,.08,.33),'Cloth',(.54,.28,.10,1),.025)
    exported('CinemaGuestSeated' if seated else 'CinemaGuestStanding',b,'Original rounded visitor in a teal coat and warm scarf; '+('seated' if seated else 'standing'))
plan={'revision':73,'assets':[i['name'] for i in CATALOG if not i['name'].startswith('CinemaGuest')],
      'guest_assets':[i['name'] for i in CATALOG if i['name'].startswith('CinemaGuest')],'seats':SEATS,'collisions':COLLISIONS,
      'screen_cm':[0,1300,620],'screen_width_cm':1400,'auditorium_min_cm':[-1200,-1100,0],
      'auditorium_max_cm':[1200,1400,1130],'entry_cm':[1150,-2100,89],
      'route_cm':[[1150,-2100,0],[1150,-1540,0],[1000,-1540,0],[1000,-1070,114],[1000,-600,114],[1000,400,0],[0,400,0]],
      'sightlines':rays,'source':'Cinema73/Cinema73.blend'}
(OUT/'cinema-layout.json').write_text(json.dumps(plan,indent=2,ensure_ascii=False),encoding='utf8')
existing=json.loads((OUT/'kit-catalog.json').read_text(encoding='utf8'));names=set(plan['assets']+plan['guest_assets'])
existing['assets']=[a for a in existing['assets'] if a['name'] not in names]+CATALOG
existing['materials']=list(dict.fromkeys(existing['materials']+list(FINISH.values())))
existing['triangles']=sum(a['triangles'] for a in existing['assets']);existing['cinema_art']=plan
(OUT/'kit-catalog.json').write_text(json.dumps(existing,indent=2,ensure_ascii=False),encoding='utf8')
def vec(v):return 'FVector('+','.join(f'{n:.5f}' for n in v)+')'
header=['#pragma once','#include "CoreMinimal.h"','// Generated by SourceArt/Scripts/build_cinema.py. Unreal centimetres.',
        'namespace EWCinemaPlan {','struct Box { FVector Centre,Extent; bool Floor; };',
        'struct Seat { const TCHAR* Name; FVector Position,Stand; };','inline TArray<Box> Boxes(){return {']
header += ['{'+vec(c['centre'])+','+vec(c['extent'])+','+str(c['floor']).lower()+'},' for c in COLLISIONS]
header += ['};}','inline TArray<Seat> Seats(){return {']
header += ['{TEXT("'+s['name']+'"),'+vec(s['position'])+','+vec(s['stand'])+'},' for s in SEATS]
header += ['};}','inline FVector Screen(){return '+vec(plan['screen_cm'])+';}',
           'inline FVector Entry(){return '+vec(plan['entry_cm'])+';}',
           'inline TArray<FVector> Route(){return {'+','.join(vec(p) for p in plan['route_cm'])+'};}',
           'inline float AudioGain(const FVector& P){',
           'if(P.Z<0 || P.Z>=1130 || P.X<=-1200 || P.X>=1200 || P.Y<=-1100 || P.Y>=1400)return 0;',
           'return float(FMath::Clamp(FMath::Min(1200-FMath::Abs(P.X),FMath::Min(P.Y+1100,1400-P.Y))/70.,0.,1.));','}','}']
(OUT/'quality93-cinema-plan.generated.h').write_text('\n'.join(header)+'\n',encoding='utf8')
archive=OUT/'CinemaQuality93';archive.mkdir(exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(archive/'Cinema73.blend'))
(archive/'source-checks.json').write_text(json.dumps({'success':True,'seats':len(SEATS),'sightlines':len(rays),'meshes':CATALOG},indent=2))
print('EW_CINEMA_ART_READY',len(SEATS),len(rays),len(COLLISIONS),flush=True)
