"""Original, walkable public halls. Geometry and physical routes share one plan."""
from pathlib import Path
import math,json,bpy
# Reuse the suite's furniture craft helpers without running its eight-room build.
src=Path(__file__).with_name('build_hotel83.py').read_text(encoding='utf8')
exec(compile(src.split('for k,(name,jp,wood,wall,accent) in enumerate(THEMES):')[0],str(Path(__file__)),'exec'))
g['FAMILIES'].append('Explore85Steel');g['COLOURS']['Explore85Steel']=(.50,.57,.60,1)
m=bpy.data.materials.new('M_Explore85Steel');m.diffuse_color=(.50,.57,.60,1);g['MATERIALS'].append(m)
NAMES=['水庭のカフェ','雲待ちのラウンジ','空の大図書館','旅の地図室','光の美術館','硝子の植物園']
ENGLISH=['WATER GARDEN CAFE','CLOUD LOUNGE','GRAND SKY LIBRARY','ATLAS ROOM','MUSEUM OF LIGHT','GLASS CONSERVATORY']
WOOD=(.27,.13,.055,1);STONE=(.61,.59,.53,1);TEAL=(.045,.24,.23,1);STEEL=(.50,.57,.60,1)
records=[];plans=[]
_letters=letters
def letters(b,text,p,size=.16,c=GOLD):
    # Preserve readable type through the FBX handedness conversion.
    v0=len(b.v);f0=len(b.f);_letters(b,text,p,size,c)
    b.v[v0:]=[(2*p[0]-x,y,z) for x,y,z in b.v[v0:]]
    b.f[f0:]=[tuple(reversed(face)) for face in b.f[f0:]]

def chair(b,rows,x,y,yaw=0,c=TEAL):
    t=B();rr=[]
    g['craft'].chair(t,0,0,0,c,WOOD)
    t.v=[(v[0]*1.17,v[1]*1.04,v[2]*1.20) for v in t.v]
    solid(rr,(0,0,.24),(.75,.72,.48))
    solid(rr,(0,.30,.85),(.79,.17,.66))
    merge(b,t,(x,y,0),yaw)
    for r in rr:
        q=Matrix.Rotation(yaw,3,'Z');v=q@Vector(r['p']);e=q@Vector(r['e'])
        rows.append({'p':[v.x+x*100,v.y+y*100,v.z],'e':[abs(e.x),abs(e.y),abs(e.z)],'floor':False})

def bench(b,rows,x,y,w=4):
    body(b,rows,(x,y,.24),(w,.8,.48),'Timber',WOOD,.05)
    soft(b,(x,y,.52),(w-.1,.78,.13),(.37,.45,.36,1))
    body(b,rows,(x,y+.32,.83),(w,.16,.65),'Timber',WOOD,.035)

def pond(b,rows,x,y,w,d):
    body(b,rows,(x,y,.18),(w,d,.36),'Ceramic',(.22,.27,.26,1),.09)
    box(b,(x,y,.367),(w-.35,d-.35,.012),'Water',(.075,.23,.22,1),.03)
    for j in range(4):
        xx=x-w*.3+j*w*.18
        b.lathe([(0,0),(.23,0),(.23,.01),(0,.01)],(xx,y+.1,.386),'Foliage',24,(.16,.28,.12,1))

def pendant(b,x,y,z,r=.8):
    beam(b,(x,y,z),(x,y,15.8),.018)
    b.lathe([(r*.8,0),(r,.12),(r*.6,.4),(.08,.42)],(x,y,z),'Bronze',32,GOLD)
    b.lathe([(0,0),(r*.78,0),(r*.78,.012),(0,.012)],(x,y,z-.01),'Glow',32,(1,.73,.43,1))

def gallery_rail(b,rows,a,c):
    for z in [4.36,5.34]:beam(b,(*a,z),(*c,z),.045)
    distance=math.dist(a,c);steps=math.ceil(distance/.8)
    for i in range(steps+1):
        p=[a[n]+(c[n]-a[n])*i/steps for n in range(2)]
        beam(b,(*p,4.2),(*p,5.34),.023)
    solid(rows,((a[0]+c[0])/2,(a[1]+c[1])/2,4.78),(max(.12,abs(a[0]-c[0])),max(.12,abs(a[1]-c[1])),1.16))

for k in range(6):
    s=B();f=B();glass=B();rows=[];steel=k==5;wood=WOOD;accent=TEAL
    facade='Explore85Steel' if steel else 'Paint';col=STEEL if steel else STONE
    # Four generous doors open onto the existing 6 m ring galleries.
    # No hidden original wall remains behind the new facade.
    for side in range(4):
        t=B();gr=B();rr=[]
        for x in [-10,10]:
            body(t,rr,(x,-18,1),(16,.24,2),facade,col,.035)
            body(t,rr,(x,-18,16.75),(16,.25,2.5),facade,col,.035)
            solid(rr,(x,-18,8.75),(15.85,.06,13.5))
            box(gr,(x,-18,8.75),(15.82,.025,13.48),'Hotel83Glass',(.67,.83,.84,1),0)
        body(t,rr,(0,-18,4.65),(4,.3,.5),facade,col,.03)
        body(t,rr,(0,-18,16.75),(4,.25,2.5),facade,col,.03)
        solid(rr,(0,-18,10.2),(4,.06,10.6))
        box(gr,(0,-18,10.2),(3.97,.025,10.58),'Hotel83Glass',(.67,.83,.84,1),0)
        # A visibly open 4 m by 4.4 m entrance, scaled with the original tower.
        for x in [-17.8,-13.8,-9.8,-5.8,-2.12,2.12,5.8,9.8,13.8,17.8]:
            body(t,rr,(x,-18.06,8.8),(.15,.34,17.6),'Explore85Steel' if steel else 'Timber',STEEL if steel else wood,.025)
        for z in [4.5,9.5,14.5]:box(t,(0,-18.07,z),(35.8,.36,.16),'Explore85Steel' if steel else 'Bronze',STEEL if steel else GOLD,.025)
        box(t,(0,-18.27,3.97),(3.72,.17,.61),'Paint',INK,.045)
        letters(t,ENGLISH[k],(0,-18.37,3.94),.20 if k!=5 else .18,(.65,.73,.68,1))
        for x in [-2.35,2.35]:box(t,(x,-18.31,2.6),(.12,.08,1.45),'Glow',(1,.69,.38,1),.02)
        q=Matrix.Rotation(side*math.pi/2,3,'Z');merge(s,t,yaw=side*math.pi/2);merge(glass,gr,yaw=side*math.pi/2)
        for r in rr:
            v=q@Vector(r['p']);e=q@Vector(r['e']);rows.append({'p':list(v),'e':[abs(a) for a in e],'floor':r['floor']})
    # A single floor skin avoids the previous coplanar floor flicker.
    body(s,rows,(0,0,-.13),(36,36,.26),'Ceramic',STONE,.012);rows[-1]['floor']=True
    body(s,rows,(0,0,17.87),(36,36,.26),facade,col,.02)
    for j in range(-8,9):
        box(s,(j*2,0,.006),(.012,35.95,.009),'Bronze',(.21,.23,.19,1),0)
        box(s,(0,j*2,.006),(35.95,.012,.009),'Bronze',(.21,.23,.19,1),0)
    if k in (0,1,3):
        for x in [-8,8]:
            box(f,(x,-5,.028),(9.8,16,.035),'Cloth',(.20,.28,.25,1),.005)
            for edge in [-4.68,4.68]:box(f,(x+edge,-5,.05),(.018,15.6,.01),'Cloth',(.64,.55,.36,1),0)
    for x in [-17,17]:
        for y in [-17,17]:plant(f,x,y,3.0,k*100+int(x+y)+50)
    # Colonnade, hanging timber fins and a lower, human-scale band of light.
    for x in [-11,11]:
        for y in [-12,-4,4,12]:
            if steel:beam(s,(x,y,0),(x,y,17.7),.13,'Explore85Steel',STEEL)
            else:g['column'](s,(x,y,0),16.6,.24)
            solid(rows,(x,y,8.3),(.58,.58,16.6))
    lights=[[-700,-800,510],[700,-800,510],[-700,700,510],[700,700,510],[0,1300,550]]
    if k==2:lights[-1][2]=900
    for x,y,z in lights:pendant(f,x/100,y/100,z/100)
    # Reading/coffee seat shared by every venue, with a clear stand position.
    bench(f,rows,6,-13,3.8)
    route=[[0,-2100,0],[0,-1450,0],[0,-850,0],[0,-250,0],[0,500,0],[0,1300,0],[0,1950,0],[0,1300,0],[0,500,0],[0,-1450,0],[600,-1450,0],[0,-1450,0],[0,-2100,0]]
    if k==0:
        # Water cafe: an indoor garden, detailed counter and varied table clusters.
        pond(f,rows,7,7,7,5);plant(f,8,8,3,82,z=.39)
        body(f,rows,(-8,9,.55),(9,1.8,1.1),'Timber',wood,.07)
        box(f,(-8,9,1.13),(9.2,1.96,.12),'Ceramic',(.34,.40,.36,1),.055)
        body(f,rows,(-10,9,1.52),(1.4,.72,.68),'Explore85Steel',STEEL,.055)
        for x in [-10.4,-10,-9.6]:beam(f,(x,8.6,1.35),(x,8.6,1.65),.025,'Explore85Steel',STEEL)
        for x in [-7,-6.5,-6,-5.5]:cup(f,x,8.7,1.23)
        books(f,rows,-8,16,9,wood,accent)
        letters(f,'SLOW COFFEE  /  SKY ROAST',(-8,15.63,3.5),.32)
        for x,y in [(-8,-9),(-8,-3),(7,-7),(7,-1)]:
            table(f,rows,x,y,2,1.1,.74,STONE);chair(f,rows,x,y+1.15);chair(f,rows,x,y-1.15,math.pi)
        for y in range(-13,15,2):box(f,(-9,y,4.2),(10,.11,.18),'Timber',wood,.02)
    elif k==1:
        # Rest room has small islands of seating and a meditative reflecting pool.
        pond(f,rows,7,6,8,7)
        for x,y in [(-8,-8),(-8,4),(7,-5),(-7,12)]:
            bench(f,rows,x,y,5);table(f,rows,x,y-1.8,2.3,1.1,.45,STONE)
            plant(f,x-3,y,2.1,k*100+int(y)+30)
        for x in [-9,9]:
            for z in [4.7,5,5.3]:box(f,(x,0,z),(9,24,.05),'Cloth',(.62,.62,.48,1),.01)
        letters(f,'TAKE YOUR TIME',(0,17.78,5.4),.65)
    elif k==2:
        # Floor-to-ceiling stacks surround a two-storey reading gallery.
        for y in [7.0,14.8]:
            for x in [-7,7]:books(f,rows,x,y,7,wood,(.18,.19,.15,1))
        for x in [-8,8]:
            table(f,rows,x,-7,4,1.3,.77,STONE)
            for dx in [-1.4,0,1.4]:chair(f,rows,x+dx,-8.25,math.pi);chair(f,rows,x+dx,-5.75)
        # 28 x 15 cm risers; 2.6 m wide, measured collision for every tread.
        for n in range(28):
            h=(n+1)*.15;body(s,rows,(-14.1,-6+n*.36,h/2),(2.6,.36,h),'Timber',wood,.006);rows[-1]['floor']=True
        body(s,rows,(-14.1,10.72,4.08),(3.8,13.28,.24),'Timber',wood,.02);rows[-1]['floor']=True
        body(s,rows,(2.1,15.5,4.08),(28.6,3.5,.24),'Timber',wood,.02);rows[-1]['floor']=True
        gallery_rail(s,rows,(-12.17,4.15),(-12.17,13.45))
        gallery_rail(s,rows,(-16.03,4.1),(-16.03,17.3))
        gallery_rail(s,rows,(-12.2,13.7),(16.4,13.7))
        for x in [-15.48,-12.72]:
            beam(s,(x,-6,1.0),(x,4.1,5.2),.045);solid(rows,(x,-1.0,2.7),(.1,10.2,1.0))
        top=B();br=[]
        for x in [-8,8]:books(top,br,x,16.8,10,wood,(.22,.27,.23,1))
        merge(f,top,(0,0,4.2))
        for b in br:b['p'][2]+=420;rows.append(b)
        for z in [7.2,10.2]:
            tall=B();rr=[]
            for x in [-8,8]:books(tall,rr,x,16.8,10,wood,(.18,.23,.20,1))
            merge(f,tall,(0,0,z))
            for b in rr:b['p'][2]+=z*100;rows.append(b)
        for x in [-8,0,8]:lights.append([x*100,1480,720]);pendant(f,x,14.8,7.2,.5)
        letters(f,'THE GRAND SKY LIBRARY',(0,17.75,10),.68)
        route=[[0,-2100,0],[0,-1450,0],[-1410,-1450,0],[-1410,-680,0],[-1410,440,420],[-1410,1500,420],[-1050,1500,420],[0,1500,420],[1050,1500,420],[0,1500,420],[-1410,1500,420],[-1410,440,420],[-1410,-680,0],[-1410,-1450,0],[0,-1450,0],[0,-250,0],[0,500,0],[0,-1450,0],[600,-1450,0],[0,-1450,0],[0,-2100,0]]
    elif k==3:
        # A freestanding atlas with a model archipelago, no copied map imagery.
        table(f,rows,7,4,9,9,.7,(.09,.23,.26,1))
        rng=random.Random(85)
        for n in range(46):
            x=7+rng.uniform(-3.9,3.9);y=4+rng.uniform(-3.9,3.9);h=rng.uniform(.1,1.7)
            box(f,(x,y,.83+h/2),(.22,.22,h),'Ceramic',(.48,.55,.52,1),.02)
        for x in [-8,7]:books(f,rows,x,15.6,8,wood,(.08,.29,.30,1))
        for x,y in [(-8,-5),(-8,5)]:
            table(f,rows,x,y,4,2,.76,STONE);chair(f,rows,x,y-1.4,math.pi)
        art(f,-8,16,6,9,5,(.04,.22,.27,1))
        letters(f,'ATLAS OF UNWRITTEN PLACES',(0,17.75,11),.55)
    elif k==4:
        # Sculptural installations and separate picture bays invite a circuit.
        for x,y in [(-8,-8),(8,-5),(-8,7),(8,9)]:
            body(f,rows,(x,y,.4),(3.2,3.2,.8),'Ceramic',STONE,.04)
            for j in range(3):
                pts=[(x+math.cos(a)*1.3,y+math.sin(a)*.7,.9+j*.8+math.sin(a*2)*.25) for a in [i*math.tau/64 for i in range(65)]]
                f.tube(pts,[.12]*len(pts),'Explore85Steel',12,STEEL)
            for yy in [y+3]:
                body(f,rows,(x,yy,2.4),(7,.25,4.8),'Paint',STONE,.05)
                art(f,x,yy-.18,2.65,5,2.9,[TEAL,(.45,.15,.07,1)][x>0])
        letters(f,'FORMS OF AIR  /  PERMANENT COLLECTION',(0,17.75,9),.53)
    else:
        # Exposed stainless diagrid, glass walls and rich planted islands.
        for y in [-14,-7,0,7,14]:
            beam(s,(-17,y,3),(-5,y,16.8),.10,'Explore85Steel',STEEL)
            beam(s,(17,y,3),(5,y,16.8),.10,'Explore85Steel',STEEL)
            beam(s,(-5,y,16.8),(5,y,16.8),.10,'Explore85Steel',STEEL)
        for x,y in [(-8,-7),(8,-6),(-8,7),(8,7)]:
            pond(f,rows,x,y,6,7)
            for j in range(6):plant(f,x-1.5+(j%3)*1.5,y-1.3+(j//3)*2.6,2.6+(j%3)*.6,850+j+int(x),z=.4)
        for x in [-8,8]:bench(f,rows,x,13,5)
        lights=[[-700,-700,450],[700,-700,450],[-700,700,450],[700,700,450],[0,1300,500]]
        letters(f,'THE GLASS CONSERVATORY',(0,17.75,8),.58,STEEL)
    # Two landmark plaques: physical discovery marker lives inside, not at menu spawn.
    box(f,(-2.5,5,.88),(.65,.50,1.76),'Timber',wood,.035)
    box(f,(-2.5,5,1.79),(.8,.62,.06),'Bronze',GOLD,.02)
    solid(rows,(-2.5,5,.88),(.8,.62,1.76))
    # Public floor foundation handled above; entrance and cross paths must be unobstructed.
    emit(f'Explore85Shell{k}',s,NAMES[k]+' public shell')
    emit(f'Explore85Furniture{k}',f,NAMES[k]+' original interior')
    emit(f'Explore85Glass{k}',glass,NAMES[k]+' translucent glazing')
    # Place the actual emitters below the opaque lamp shades and cloth canopies.
    lights=[[x,y,z-65] for x,y,z in lights]
    plans.append({'kind':19+k,'name':NAMES[k],'bodies':rows,'lights':lights,'route':route,
                  'seat':[600,-1300,90],'stand':[600,-1450,90],'eye':[-120,-1420,165],
                  'aim':[0,550,430 if k==2 else 270]})
OUT.joinpath('exploration85-catalog.json').write_text(json.dumps({'assets':records,'rooms':plans},ensure_ascii=False,indent=2),encoding='utf8')
header=['#pragma once','#include "CoreMinimal.h"','namespace EWExplore85Data {','struct Body { FVector P,E; bool Floor; };','inline TArray<Body> Bodies(int32 Kind) { switch(Kind) {']
def vec(v):return 'FVector('+','.join(f'{x:.3f}' for x in v)+')'
for p in plans:header.append('case '+str(p['kind'])+':return {'+','.join('{'+vec(b['p'])+','+vec(b['e'])+','+str(b['floor']).lower()+'}' for b in p['bodies'])+'};')
header+=['default:return {};}}']
for key,typ in [('lights','Lights'),('route','Route')]:
    header+=['inline TArray<FVector> '+typ+'(int32 Kind) {switch(Kind) {']
    for p in plans:header.append('case '+str(p['kind'])+':return {'+','.join(vec(v) for v in p[key])+'};')
    header+=['default:return {};}}']
header+=['}'];(OUT/'Generated/EWExplore85Data.h').write_text('\n'.join(header)+'\n',encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Blender/Exploration85.blend'))
print('EXPLORATION85_COMPLETE',len(records),sum(x['triangles'] for x in records),flush=True)
