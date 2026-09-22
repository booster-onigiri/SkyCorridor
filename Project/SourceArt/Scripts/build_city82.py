"""Original roof cinema, streamline cab and skyship. Metres, reproducible FBX."""
from pathlib import Path
import bpy,runpy,math,json
from mathutils import Vector
g=runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
B,export,OUT=g['Builder'],g['export'],g['OUT'];records=[]
for name,col in [('RailGlass',(.08,.23,.25,1))]:
 g['FAMILIES'].append(name);g['COLOURS'][name]=col
 m=bpy.data.materials.new('M_'+name);m.diffuse_color=col;g['MATERIALS'].append(m)
TEAL=(.025,.22,.23,1);DARK=(.013,.021,.03,1);IVORY=(.78,.79,.72,1)
GOLD=(.40,.29,.13,1);OAK=(.27,.14,.055,1)
def out(name,b):
 lo=[min(v[i] for v in b.v)*100 for i in range(3)];hi=[max(v[i] for v in b.v)*100 for i in range(3)]
 b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(f)) for f in b.f]
 export(name,b);row=g['CATALOG'][-1];row['ue_bounds_cm']=[lo,hi];records.append(row)
def beam(b,a,c,r=.04,col=GOLD):b.tube([a,c],[r,r],'Bronze',12,col)
def label(b,text,p,size=.24,col=IVORY):
 curve=bpy.data.curves.new('lettering','FONT');curve.body=text;curve.align_x='CENTER';curve.size=size;curve.extrude=.002
 obj=bpy.data.objects.new('lettering',curve);bpy.context.collection.objects.link(obj)
 obj.rotation_euler=(math.pi/2,0,0);obj.location=p;bpy.context.view_layer.update()
 mesh=bpy.data.meshes.new_from_object(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()))
 b.mesh([tuple(obj.matrix_world@v.co) for v in mesh.vertices],[tuple(f.vertices) for f in mesh.polygons],'Ceramic',col,smooth=False)
 bpy.data.objects.remove(obj,do_unlink=True);bpy.data.meshes.remove(mesh)

# Tapered panoramic cabs; the original doors and passenger space remain intact.
b=B();glass=B()
levels=[(0,1.42,.02),(.22,1.47,.36),(.24,1.48,.94),(.0,1.35,2.88),(-.22,1.28,3.24)]
verts=[]
for forward,width,z in levels:
 for i in range(17):
  a=-math.pi/2+i*math.pi/16
  verts.append((forward+1.05*math.cos(a),width*math.sin(a),z))
for level in range(4):
 target=glass if level==2 else b;family='RailGlass' if level==2 else 'Paint'
 for i in range(16):
  ix=level*17+i;target.mesh([verts[ix],verts[ix+1],verts[ix+18],verts[ix+17]],[(0,1,2,3)],family,TEAL if level<2 else IVORY)
for level in [1,2,3,4]:
 pts=verts[level*17:(level+1)*17];b.tube(pts,[.028]*17,'Bronze',10,GOLD if level==2 else DARK)
for i in [0,4,8,12,16]:beam(b,verts[2*17+i],verts[3*17+i],.035,DARK)
for side in [-1,1]:
 b.box((1.17,side*.72,.7),(.075,.40,.045),'Glow',.015,colour=(.7,.88,1,1))
 b.box((1.05,side*.9,.42),(.08,.19,.04),'Glow',.01,colour=(1,.12,.02,1))
for z in [.22,.31,.40]:b.box((1.14,0,z),(.08,1.06,.025),'Bronze',.008,colour=DARK)
out('Skyrail82Cab',b);out('Skyrail82CabGlass',glass)
b=B()
for y in [-1.48,1.48]:
 for x in [-2.94,2.94]:
  b.box((x,y,.75),(4.35,.022,.045),'Glow',.01,colour=(.4,.8,1,1))
  b.box((x,y,.33),(4.4,.026,.055),'Bronze',.01,colour=GOLD)
for x in [-3.8,3.8]:
 for y in [-.64,.64]:
  b.box((x,y,-.45),(1.8,.19,.28),'Paint',.09,colour=TEAL)
  for xx in [-.6,0,.6]:b.box((x+xx,y*1.07,-.44),(.035,.04,.19),'Bronze',.01,colour=DARK)
for x in [-2.8,2.8]:
 b.box((x,0,3.33),(2.6,1.68,.13),'Paint',.065,colour=IVORY)
 for xx in range(-8,9,2):b.box((x+xx*.1,0,3.4),(.045,1.1,.02),'Bronze',.006,colour=DARK)
out('Skyrail82Trim',b)

# Rooftop amphitheatre: open skyline, warm timber, three broad aisles.
b=B();seats=B()
b.box((0,0,-.16),(47,47,.32),'Limestone',.08,colour=IVORY)
b.box((0,0,-.34),(47.3,47.3,.10),'Paint',.04,colour=TEAL)
for i in range(6):
 edge=-2.9-i*2.25;z=(i+1)*.19
 b.box((0,(-18.3+edge)/2,z-.095),(42,18.3+edge,.19),'Timber',.01,colour=OAK)
 for x in [0,-20.4,20.4]:b.box((x,edge-.01,z-.025),(2.6,.03,.025),'Glow',.007,colour=(1,.6,.24,1))
for row in range(4):
 y=-1-row*4.5;z=row*.38
 for x in [-15.5,-11.8,-8.1,-4.4,4.4,8.1,11.8,15.5]:
  seats.box((x,y-.1,z+.38),(2.5,1.3,.76),'Timber',.07,colour=OAK)
  seats.box((x,y+.08,z+.69),(2.38,1.12,.16),'Cloth',.065,colour=TEAL)
  seats.box((x,y-.62,z+.96),(2.5,.24,.84),'Timber',.07,colour=OAK)
  seats.box((x,y-.47,z+1.03),(2.32,.13,.65),'Cloth',.055,colour=TEAL)
  for side in [-1,1]:seats.box((x+side*1.34,y,z+.89),(.17,1.2,.11),'Bronze',.045,colour=GOLD)
  seats.box((x+1.7,y,z+.60),(.36,.44,.055),'Paint',.045,colour=IVORY)
  beam(seats,(x+1.7,y,z),(x+1.7,y,z+.57),.035,TEAL)
# A slim cantilevered surround leaves the horizon visible on either side.
b.box((0,19.65,14.5),(40.8,.7,23.3),'Paint',.09,colour=DARK)
for x in [-20.3,20.3]:b.box((x,19.17,14.5),(.15,.11,23.05),'Bronze',.035,colour=GOLD)
for z in [2.96,26.04]:b.box((0,19.17,z),(40.75,.11,.15),'Bronze',.035,colour=GOLD)
for x in [-16,16]:
 b.box((x,19.9,1.55),(.44,.55,3.1),'Paint',.08,colour=TEAL)
 beam(b,(x,21.7,0),(x,20,9.5),.12,TEAL)
label(b,'S K Y   G A R D E N   C I N E M A',(0,19.13,2.25),.70,GOLD)
for x in [-21.6,21.6]:
 for y in [-16,-5,7]:
  b.box((x,y,.38),(1.12,2.2,.76),'Paint',.08,colour=IVORY)
  for k in range(8):
   a=k*math.tau/8;p=(x+.35*math.sin(a),y+.75*math.cos(a),.75)
   beam(b,p,(p[0]+.2*math.sin(a),p[1],1.5+.2*math.sin(k)),.018,TEAL)
   b.mesh([(p[0]-.35,p[1],1.0),(p[0],p[1]-.13,1.8),(p[0]+.35,p[1],1.0),(p[0],p[1]+.13,1.3)],[(0,1,2,3)],'Foliage',(.09,.28,.16,1))
out('SkyTheatre82Deck',b);out('SkyTheatre82Seats',seats)

# Cream and petrol-blue airship, with a glazed gondola and four propulsors.
b=B();glass=B();rings=32;segments=48;verts=[]
for j in range(rings+1):
 t=-math.pi/2+j*math.pi/rings;x=22*math.sin(t);radius=max(.035,math.cos(t))
 for i in range(segments):
  a=i*math.tau/segments;verts.append((x,4.2*radius*math.cos(a),4.5*radius*math.sin(a)))
for j in range(rings):
 for i in range(segments):
  a=j*segments+i;c=j*segments+(i+1)%segments
  z=(verts[a][2]+verts[c][2])*.5
  b.mesh([verts[a],verts[c],verts[c+segments],verts[a+segments]],[(0,1,2,3)],'Paint',TEAL if -1.15<z<-.45 else IVORY)
for j in [4,8,12,16,20,24,28]:
 t=-math.pi/2+j*math.pi/rings;pts=[(22*math.sin(t),4.22*math.cos(t)*math.cos(i*math.tau/48),4.52*math.cos(t)*math.sin(i*math.tau/48)) for i in range(49)]
 b.tube(pts,[.022]*49,'Bronze',8,GOLD)
for angle in [0,math.pi/2,math.pi,3*math.pi/2]:
 def v(x,r):return (x,r*math.cos(angle),r*math.sin(angle))
 b.mesh([v(-13,2.8),v(-19,6.8),v(-21,6.2),v(-20,1.8)],[(0,1,2,3),(3,2,1,0)],'Paint',TEAL,smooth=False)
b.box((2,0,-5.38),(10.2,2.6,.50),'Paint',.15,colour=TEAL)
b.box((2,0,-4.15),(10.7,2.85,.16),'Paint',.075,colour=IVORY)
glass.box((2,0,-4.74),(10.1,2.58,1.10),'RailGlass',.04)
for x in [-2.85,-1,1,3,5,6.85]:
 for side in [-1,1]:beam(b,(x,side*1.35,-5.25),(x,side*1.35,-4.15),.035,DARK)
for x in [-3,7]:
 for side in [-1,1]:beam(b,(x,side*1.25,-4.2),(x-1,side*2.1,-2.6),.045,GOLD)
for x in [-10,9]:
 for side in [-1,1]:
  beam(b,(x,side*3.1,-1.8),(x,side*5.5,-2.3),.10,TEAL)
  b.box((x,side*5.5,-2.3),(2.0,.55,.55),'Paint',.2,colour=TEAL)
  b.box((x+.95,side*5.5,-2.3),(.05,.32,.32),'Glow',.04,colour=(1,.6,.24,1))
label(b,'A U R O R A   /   0 1',(3,-4.17,.15),.68,GOLD)
out('Airship82Hull',b);out('Airship82Glass',glass)
b=B()
for angle in [0,math.pi/2,math.pi,3*math.pi/2]:
 y,z=math.cos(angle),math.sin(angle)
 b.mesh([(0,y*.15,z*.15),(.05,y*1.15-z*.15,z*1.15+y*.15),(0,y*1.4+z*.08,z*1.4-y*.08),(-.02,y*.25+z*.1,z*.25-y*.1)],[(0,1,2,3),(3,2,1,0)],'Bronze',DARK)
out('Airship82Prop',b)
(OUT/'city82-catalog.json').write_text(json.dumps({'revision':82,'assets':records},indent=2),encoding='utf8')
(OUT/'Blender').mkdir(exist_ok=True);bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Blender/City82.blend'))
print('CITY82_ART',len(records),sum(r['triangles'] for r in records))
