"""Original two-car skyline tram. No reference pixels or external models used.

The retained Blender source, FBX hashes and Unreal bounds form one import receipt.
"""
from pathlib import Path
import bpy, runpy, math, json
from mathutils import Vector
g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
B,export,OUT=g['Builder'],g['export'],g['OUT']
for name,col in [('RailGlass',(.08,.23,.25,1))]:
    g['FAMILIES'].append(name);g['COLOURS'][name]=col
    m=bpy.data.materials.new('M_'+name);m.diffuse_color=col;g['MATERIALS'].append(m)
TEAL=(.024,.23,.235,1);DARK=(.018,.028,.032,1);IVORY=(.69,.73,.69,1)
GOLD=(.43,.29,.11,1);SEAT=(.035,.13,.145,1);GLOW=(1,.74,.36,1)
records=[]
def out(name,b):
    lo=[min(v[i] for v in b.v)*100 for i in range(3)];hi=[max(v[i] for v in b.v)*100 for i in range(3)]
    # Blender FBX's Y reflection is compensated before export, including winding.
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(f)) for f in b.f]
    export(name,b);row=g['CATALOG'][-1];row['ue_bounds_cm']=[lo,hi];records.append(row)
def beam(b,a,c,r=.025,family='Bronze',colour=DARK):b.tube([a,c],[r,r],family,12,colour)
def label(b,text,p,size=.16,colour=IVORY):
    curve=bpy.data.curves.new('lettering','FONT');curve.body=text;curve.align_x='CENTER';curve.size=size;curve.extrude=.002
    obj=bpy.data.objects.new('lettering',curve);bpy.context.collection.objects.link(obj)
    obj.rotation_euler=(math.pi/2,0,0);obj.location=p;bpy.context.view_layer.update()
    mesh=bpy.data.meshes.new_from_object(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    b.mesh([tuple(obj.matrix_world@v.co) for v in mesh.vertices],[tuple(f.vertices) for f in mesh.polygons],'Ceramic',colour,smooth=False)
    bpy.data.objects.remove(obj,do_unlink=True);bpy.data.meshes.remove(mesh)

b=B();glass=B()
# Long bevels, wraparound cab glazing, two continuous waist lines.
b.box((0,0,-.09),(10.5,2.82,.18),'Bronze',.045,colour=DARK)
b.box((0,0,.015),(10.25,2.63,.07),'Timber',.02,colour=(.25,.16,.08,1))
b.box((0,0,3.05),(10.75,2.98,.24),'Paint',.12,colour=IVORY)
b.box((0,0,3.21),(9.4,2.16,.14),'Paint',.07,colour=TEAL)
for y in [-1.43,1.43]:
    for x in [-2.93,2.93]:
        b.box((x,y,.44),(4.56,.11,.85),'Paint',.045,colour=TEAL)
        b.box((x,y*.965,.47),(4.54,.035,.79),'Paint',.015,colour=IVORY)
        for z in [.15,.86,2.89]:b.box((x,y*1.026,z),(4.57,.045,.048),'Bronze',.012,colour=GOLD if z==.86 else DARK)
        glass.box((x,y,1.83),(4.54,.018,1.83),'RailGlass',.001)
        for dx in [-1.06,1.06]:b.box((x+dx,y,1.87),(.055,.08,2.08),'Bronze',.015,colour=DARK)
    for x in [-5.18,-.66,.66,5.18]:b.box((x,y,1.85),(.07,.1,2.24),'Bronze',.018,colour=DARK)
    b.box((0,y,2.90),(1.36,.13,.25),'Paint',.035,colour=TEAL)
    # The southern doors stay shut; the northern leaves are separate moving meshes.
    if y<0:
        glass.box((0,y,1.5),(1.26,.025,2.8),'RailGlass',.001)
        b.box((0,y,1.42),(.035,.045,2.77),'Bronze',.007,colour=DARK)
    for x in [-.76,.76]:beam(b,(x,y*.83,.12),(x,y*.83,2.91),.026,'Bronze',GOLD)
for x in [-5.23,5.23]:
    b.box((x,0,.43),(.12,2.75,.80),'Paint',.05,colour=TEAL)
    glass.box((x,0,1.84),(.018,2.73,1.98),'RailGlass',.001)
    for y in [-1.37,0,1.37]:b.box((x,y,1.91),(.10,.048,2.08),'Bronze',.012,colour=DARK)
    b.box((x,0,-.15),(.22,2.50,.21),'Bronze',.035,colour=DARK)
    for y in [-1.01,1.01]:
        b.box((x*1.018,y,.49),(.05,.26,.13),'Glow',.035,colour=GLOW)
        b.box((x*1.02,y,.32),(.025,.11,.045),'Glow',.009,colour=(1,.04,.015,1))
for x in [-3.8,3.8]:
    b.box((x,0,-.46),(1.6,.95,.64),'Bronze',.12,colour=DARK)
    for y in [-.60,.60]:b.box((x,y,-.30),(1.9,.13,.23),'Paint',.05,colour=TEAL)
for y in [-1.06,1.06]:
    for x in [-3.65,-2.6,-1.55,1.55,2.6,3.65]:
        b.box((x,y,.43),(.85,.53,.20),'Cloth',.08,colour=SEAT)
        b.box((x,y*1.19,.80),(.85,.14,.67),'Cloth',.065,colour=SEAT)
        for yy in [-.18,.18]:b.box((x,y+yy,.19),(.06,.055,.32),'Bronze',.014,colour=GOLD)
        for xx in [-.43,.43]:beam(b,(x+xx,y-.25,.67),(x+xx,y+.22,.67),.018,'Bronze',GOLD)
for y in [-.75,.75]:
    b.box((0,y,2.9),(9.75,.045,.03),'Glow',.01,colour=GLOW)
    beam(b,(-4.9,y,2.59),(4.9,y,2.59),.027,'Bronze',GOLD)
    for x in [-4,-2,2,4]:beam(b,(x,y,2.58),(x,y,2.35),.012,'Bronze',GOLD)
for y in [-1.505,1.505]:label(b,'01  SKYLINE',(2.7,y,.48),.21)
out('SkyrailCar',b);out('SkyrailGlass',glass)
b=B();b.box((0,1.46,.48),(.62,.07,.94),'Paint',.025,colour=TEAL)
b.box((0,1.46,1.87),(.62,.018,1.80),'RailGlass',.001)
for x in [-.305,.305]:b.box((x,1.46,1.42),(.027,.065,2.81),'Bronze',.007,colour=DARK)
for z in [0,1,2.82]:b.box((0,1.46,z),(.62,.06,.035),'Bronze',.01,colour=GOLD)
out('SkyrailDoor',b)
b=B();b.box((0,0,-1.07),(4,.78,.75),'Paint',.09,colour=IVORY)
for y in [-.50,.50]:b.box((0,y,-.61),(4,.11,.16),'Bronze',.03,colour=DARK)
for x in [-1.5,-.5,.5,1.5]:b.box((x,0,-.65),(.1,1.16,.1),'Bronze',.018,colour=DARK)
for y in [-.42,.42]:b.box((0,y,-1.12),(4,.035,.09),'Paint',.01,colour=TEAL)
out('SkyrailTrack',b)
b=B();b.box((0,0,-.3),(.18,1.2,1.2),'Bronze',.045,colour=DARK)
for y in [-.42,.42]:b.box((0,y,.27),(.24,.24,.16),'Glow',.03,colour=(1,.1,.025,1))
out('SkyrailBuffer',b)
b=B();b.box((1,4.2,-.14),(28,5,.28),'Limestone',.04,colour=IVORY)
# Front platform edge, bronze nosing and tactile warning strip.
b.box((1,1.80,.01),(28,.17,.04),'Paint',.005,colour=TEAL)
for x in range(-124,147,3):
    for y in [2.03,2.17,2.31]:b.box((x*.1,y,.028),(.045,.045,.025),'Bronze',.005,colour=GOLD)
for x in [-12.7,-8,-3,2,7,12.7]:
    b.box((x,6.32,1.57),(.09,.11,3.14),'Bronze',.025,colour=TEAL)
    beam(b,(x,6.32,2.7),(x,2.4,3.15),.04,'Bronze',TEAL)
    b.box((x,6.32,.59),(.08,.07,1.18),'Bronze',.01,colour=DARK)
for z in [.23,1.19]:b.box((1,6.6,z),(28,.075,.07),'Bronze',.014,colour=GOLD)
for x in range(-124,149,6):b.box((x*.1,6.6,.67),(.033,.043,1.03),'Bronze',.008,colour=TEAL)
for z in [.23,1.19]:b.box((-12.95,4.2,z),(.07,5,.07),'Bronze',.014,colour=GOLD)
for x,wide in [(-9.7,6.6),(0,9.6),(10.7,8.6)]:
    for z in [.28,1.1]:b.box((x,1.78,z),(wide,.055,.055),'Bronze',.012,colour=GOLD)
    for i in range(int(wide/.6)+1):b.box((x-wide/2+i*.6,1.78,.67),(.04,.045,.86),'Bronze',.008,colour=TEAL)
b.box((1,4.42,3.28),(27.8,4.7,.12),'Paint',.055,colour=TEAL)
b.box((1,2.11,3.28),(28,.09,.22),'Bronze',.02,colour=GOLD)
for x in [-8,-2,5,11]:
    b.box((x,4.6,3.18),(2.3,.16,.035),'Glow',.014,colour=GLOW)
for x in [-9,7.7]:
    b.box((x,5.5,.48),(2.8,.66,.15),'Timber',.04,colour=(.28,.14,.055,1))
    for dx in [-1.15,1.15]:b.box((x+dx,5.5,.22),(.10,.48,.44),'Bronze',.025,colour=TEAL)
    b.box((x,5.77,.83),(2.8,.065,.61),'Timber',.035,colour=(.28,.14,.055,1))
out('SkyrailStation',b)
(OUT/'skyrail-catalog.json').write_text(json.dumps({'revision':78,'assets':records},indent=2),encoding='utf8')
archive=OUT/'Blender';archive.mkdir(exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(archive/'Skyrail78.blend'))
print('EW_SKYRAIL_AUTHORED',len(records),sum(r['triangles'] for r in records))
