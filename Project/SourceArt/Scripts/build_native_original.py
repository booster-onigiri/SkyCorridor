"""Build an editable, physically detailed native scene from the authored world.
No downloaded artwork is embedded. All surface maps and new props are procedural.
"""
import bpy, bmesh, numpy as np, math, json, time, random
from pathlib import Path
from collections import defaultdict
from mathutils import Matrix
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceArt'
FBX=OUT/'Meshes'; TEX=OUT/'Textures'
for p in [OUT,FBX,TEX]:p.mkdir(parents=True,exist_ok=True)
rng=np.random.default_rng(41017)
random.seed(41017)
start=time.time()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
bpy.context.preferences.filepaths.save_version=0
world=json.loads((ROOT/'SourceArt/world.json').read_text(encoding='utf8'))
materials=['Limestone','Plaster','Bronze','Timber','Foliage','Cloth','Glass','Water','Glow','Petal','Paper','Bark','Ceramic','Leather','Fruit','Pollen','Soil']
matidx={n:i for i,n in enumerate(materials)}
palette={}
for n in materials:
 m=bpy.data.materials.get(n) or bpy.data.materials.new(n);m.diffuse_color=(.7,.72,.67,1);m.use_nodes=True;palette[n]=m

def write_image(name, rgb, color=True):
 h,w=rgb.shape[:2]
 im=bpy.data.images.new(name,width=w,height=h,alpha=False,float_buffer=False)
 if not color:im.colorspace_settings.name='Non-Color'
 a=np.ones((h,w,4),np.float32);a[:,:,:3]=np.clip(rgb,0,1)
 im.pixels.foreach_set(a.ravel());im.filepath_raw=str(TEX/(name+'.png'));im.file_format='PNG';im.save()
 bpy.data.images.remove(im)

def soft_noise(n,period):
 a=rng.random((period,period)).astype(np.float32)
 pos=np.arange(n)*period/n;i=pos.astype(int);t=pos-i;t=t*t*(3-2*t)
 row=a[:,i]*(1-t)+a[:,(i+1)%period]*t
 return row[i,:]*(1-t[:,None])+row[(i+1)%period,:]*t[:,None]

N=1024;yy,xx=np.mgrid[:N,:N]/N
grain=rng.random((N,N)).astype(np.float32)
noise=sum(soft_noise(N,p)*w for p,w in [(4,.43),(16,.27),(64,.18),(256,.12)])
for name in materials:
 if name in ['Glass','Water','Glow']:continue
 base=.80+noise*.19
 height=noise*.08+grain*.006
 rough=np.full((N,N),.64,np.float32)+noise*.17
 if name=='Limestone':
  rows=yy*4;cols=xx*3+(np.floor(rows)%2)*.5
  edge=np.minimum(np.minimum(rows%1,1-rows%1),np.minimum(cols%1,1-cols%1))
  joint=np.clip(edge/.022,0,1)
  pores=np.clip((grain-.983)*45,0,.5)
  base=(.79+noise*.20)*(joint*.18+.82)-pores*.05
  height=joint*.055+noise*.045-pores*.035
 elif name=='Plaster':
  base=.88+noise*.10;height=noise*.04+grain*.011;rough=.7+noise*.17
 elif name=='Bronze':
  scratch=np.sin(xx*N*2.1+noise*8)*.014
  base=.71+noise*.26+scratch;height=noise*.018+scratch*.07;rough=.28+noise*.27
 elif name=='Timber':
  grainline=np.sin((xx*42+np.sin(yy*7)*.5+noise*2.7)*math.tau)
  base=.62+noise*.25+grainline*.08;height=noise*.016+grainline*.011;rough=.48+noise*.2
 elif name=='Cloth':
  weave=(np.cos(xx*256*math.tau)+np.cos(yy*256*math.tau))*.5
  base=.88+weave*.035+noise*.05;height=weave*.008;rough=.78+noise*.14
 elif name=='Foliage':
  midrib=np.exp(-np.abs(xx-.5)*100)
  ribs=np.exp(-np.abs(np.sin((yy*13-np.abs(xx-.5)*2.7)*math.tau))*28)
  base=.83+noise*.14+ribs*.016; height=midrib*.022+ribs*.005+noise*.004;rough=.39+noise*.22
 elif name=='Petal':
  veins=np.sin((xx-.5)*45+yy*5)**2
  base=.9+noise*.08;height=veins*.0015+noise*.002;rough=.62+noise*.11
 elif name=='Paper':
  lines=np.sin(yy*420*math.tau)*.022
  base=.86+noise*.10+lines;height=lines*.1;rough=.8+noise*.12
 elif name=='Bark':
  ribs=np.abs(np.sin((xx*19+np.sin(yy*9)*.13+noise*.6)*math.tau))**.45
  cracks=np.clip((.25-ribs)*3,0,1)
  base=.61+noise*.27+ribs*.11-cracks*.08;height=ribs*.035+noise*.01;rough=.8+noise*.12
 elif name=='Ceramic':
  base=.94+noise*.055;height=noise*.003;rough=.13+noise*.12
 elif name=='Leather':
  pores=(np.sin(xx*280*math.tau)*np.cos(yy*263*math.tau))*.5+.5
  base=.78+noise*.15+pores*.025;height=pores*.0025+noise*.006;rough=.42+noise*.15
 elif name=='Fruit':
  specks=np.clip((grain-.995)*180,0,1)
  base=.85+noise*.13-specks*.02;height=noise*.002;rough=.29+noise*.18
 elif name=='Pollen':
  base=.92+noise*.07;height=noise*.001;rough=.66+noise*.14
 elif name=='Soil':
  base=.48+noise*.37;height=noise*.065+grain*.005;rough=.87+noise*.1
 gx=(np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))*22
 gy=(np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))*22
 normals=np.stack([-gx,gy,np.ones_like(gx)],axis=2)
 normals/=np.linalg.norm(normals,axis=2,keepdims=True)
 write_image('T_'+name+'_Base',np.repeat(base[:,:,None],3,axis=2))
 write_image('T_'+name+'_Normal',normals*.5+.5,False)
 # Occlusion, roughness, metalness; UE texture is imported without sRGB.
 metal=np.full_like(rough,.86 if name=='Bronze' else 0)
 write_image('T_'+name+'_ORM',np.stack([np.ones_like(rough),rough,metal],axis=2),False)
print('SURFACE_TEXTURES_READY',flush=True)

cache={}
def mesh_arrays(bm):
 bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.verts.ensure_lookup_table()
 v=np.array([tuple(v.co) for v in bm.verts],dtype=np.float32)
 f=np.array([[a.index for a in f.verts] for f in bm.faces],dtype=np.int32)
 bm.free();return v,f

def beveled_box(dims):
 dims=np.maximum(np.abs(dims),.0004)
 # Quantization shares topology without changing the final outside dimensions.
 q=tuple(np.round(dims,2));q=tuple(max(.005,d) for d in q)
 if q not in cache:
  bm=bmesh.new();bmesh.ops.create_cube(bm,size=1)
  for v in bm.verts:v.co.x*=q[0];v.co.y*=q[1];v.co.z*=q[2]
  width=min(.045,max(.001,min(q)*.16))
  bmesh.ops.bevel(bm,geom=list(bm.edges),offset=width,segments=3,affect='EDGES',profile=.5)
  v,f=mesh_arrays(bm);cache[q]=(v/np.array(q),f)
 v,f=cache[q];return v*dims,f

def lathe(profile,segments=40):
 vs=[];fs=[]
 for r,y in profile:
  for j in range(segments):
   a=j*math.tau/segments;vs.append((r*math.cos(a),y,r*math.sin(a)))
 for k in range(len(profile)-1):
  for j in range(segments):
   a=k*segments+j;b=k*segments+(j+1)%segments;c=b+segments;d=a+segments
   fs.extend([(a,b,c),(a,c,d)])
 return np.array(vs,np.float32),np.array(fs,np.int32)[:,[0,2,1]]

rounded_cyl=lathe([(0,-.5),(.96,-.5),(1,-.48),(1,.48),(.96,.5),(0,.5)],48)
rounded_cone=lathe([(0,-.5),(.16,-.5),(.18,-.48),(.98,.48),(1,.5),(0,.5)],40)
bm=bmesh.new();bmesh.ops.create_uvsphere(bm,u_segments=32,v_segments=20,radius=1);sphere=mesh_arrays(bm)
source_geo={k:(np.array(g['vertices'],np.float32).reshape(-1,3),np.array(g['indices'] if g['indices'] else range(len(g['vertices'])//3),np.int32).reshape(-1,3)) for k,g in world['geometries'].items()}
groups={};counts=defaultdict(int);group_parts=defaultdict(int)
P=np.array([[1,0,0],[0,0,-1],[0,1,0]],dtype=np.float32)

def group_for(pos,family,face_count):
 tile=(math.floor(pos[0]/64),math.floor(pos[2]/64),math.floor(pos[1]/80))
 trans=family if family in ['Glass','Water'] else 'Opaque'
 base=(*tile,trans);part=group_parts[base]
 key=(*tile,trans+('_detail'+str(part).zfill(3) if part else ''))
 if key in groups and groups[key]['face_count']+face_count>800000:
  part+=1;group_parts[base]=part;key=(*tile,trans+'_detail'+str(part).zfill(3))
 if key not in groups:
  groups[key]={'v':[],'f':[],'col':[],'mat':[],'n':0,'face_count':0,'uv_overrides':[],'wind_overrides':[],'family':trans,'anchor':np.array([tile[0]*64+32,tile[2]*80+40,tile[1]*64+32],np.float32)}
 return groups[key]

def push(v,f,pos,family,color,uv=None,wind=None):
 if len(f)==0:return
 if wind is None and family in ['Foliage','Cloth','Petal']:wind=np.zeros((len(f),3,2),np.float32)
 g=group_for(pos,family,len(f))
 if uv is not None:g['uv_overrides'].append((g['face_count'],np.asarray(uv,np.float32)))
 if wind is not None:g['wind_overrides'].append((g['face_count'],np.asarray(wind,np.float32)))
 g['face_count']+=len(f)
 rgb=np.array([(color>>16&255),(color>>8&255),(color&255)],np.float32)/255
 lin=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
 g['v'].append((v-g['anchor'])@P.T)
 g['f'].append(f+g['n']);g['n']+=len(v)
 g['col'].append(np.tile(np.r_[lin,1].astype(np.float32),(len(f)*3,1)))
 g['mat'].append(np.full(len(f),matidx[family],np.int32));counts[family]+=1

def box(pos,dims,color,family='Limestone',rot=0):
 v,f=beveled_box(dims);co,si=math.cos(rot),math.sin(rot)
 R=np.array([[co,0,si],[0,1,0],[-si,0,co]],np.float32)
 v=v@R.T+np.array(pos);push(v,f,pos,family,color)

def shape(pos,scale,geo,color,family,rotation=None):
 v,f=geo;v=v*np.array(scale)
 if rotation is not None:v=v@rotation.T
 push(v+np.array(pos),f,pos,family,color)

bookcolors={0x596d78,0xb19d73,0x8a6d62,0x9aaa9a,0xb8b3a0,0x738b84}
barkcolors={0x7d8170,0x747967,0x817d70,0x8d8170,0x8b806e,0x8f9277,0xb2b197}
def family_for(o,d):
 k=o['kind'];c=o['color'];r,g,b=(c>>16&255,c>>8&255,c&255)
 if o.get('water'):return 'Water'
 if o.get('transparent'):return 'Glass'
 if k=='glow':return 'Glow'
 if k=='cloth':return 'Cloth'
 if k in ['leaf','blossom']:return 'Petal' if k=='blossom' else 'Foliage'
 if c in barkcolors and k in ['box','cyl','fine']:return 'Bark'
 if c==0x626559:return 'Soil'
 if c in [0xc79461,0x809665,0xb98369] and k=='sphere':return 'Fruit'
 if k=='cone' and c in [0xc6b192,0xb99271]:return 'Ceramic'
 if k=='sphere' and g>r*1.06 and g>b*1.06:return 'Foliage'
 if c in bookcolors and max(d)<1.3:return 'Leather'
 if min(r,g,b)>174:return 'Limestone' if min(d)<.8 else 'Plaster'
 if c in [0x426980,0x398698] and min(d)<.2:return 'Glass'
 if r>g*1.12 and g>b*1.10:return 'Timber'
 if c in [0xc6b680,0xbdb077,0x3d576c,0x8197ae,0x8b9ba5,0x728d86] or min(d)<.11:return 'Bronze'
 return 'Limestone'

leaf_v=np.array([[0,0,0],[-.23,.38,.055],[0,.52,.15],[.23,.38,.055],[0,1,.04]],np.float32)
leaf_f=np.array([[0,1,2],[0,2,3],[1,4,2],[2,4,3]],np.int32)
exec(Path(__file__).with_name('detail_primitives.py').read_text(encoding='utf8'))
exec(Path(__file__).with_name('mesh_fingerprint.py').read_text(encoding='utf8'))
for i,o in enumerate(world['instances']):
 k=o['kind'];M=np.array(o['matrix'],np.float32).reshape(4,4).T
 d=np.linalg.norm(M[:3,:3],axis=0);R=M[:3,:3]/np.maximum(d,.000001);pos=M[:3,3];family=family_for(o,d)
 # The native cups, tins and woven baskets replace the original proxy goods.
 if 5<abs(pos[0])<10 and 50<pos[2]<150 and ((k=='box' and o['color'] in [0xa4ba97,0xcfb586,0x9caaa3] and abs(pos[1]-3.33)<.01) or (k=='sphere' and o['color'] in [0xc79461,0x809665,0xb98369] and abs(pos[1]-3.56)<.01)):continue
 if k=='leaf':frond(d,R,pos,o['color'],i);continue
 if k=='blossom':blossom_cluster(d,R,pos,o['color'],i);continue
 if k=='cloth':garment(d,R,pos,o['color'],i);continue
 if family=='Bark':
  dd=d.copy()
  if k=='box':dd[[0,2]]*=.5
  bark_tube(dd,R,pos,o['color'],i);continue
 if k=='box' and family=='Leather' and .3<d[1]<1.1 and .15<d[2]<.8:
  bound_book(d,R,pos,o['color'],i);continue
 if k=='sphere' and family=='Foliage' and max(d)>.36:
  leaf_canopy(d,R,pos,o['color'],i);continue
 if k in ['box','glow']:v,f=beveled_box(d);v=v@R.T+pos
 elif k in ['cyl','fine','cone']:
  v,f=rounded_cone if k=='cone' else rounded_cyl;v=(v*d)@R.T+pos
 elif k=='sphere' and family=='Foliage' and max(d)>.36:
  # Replace the old low-poly ellipsoid with hundreds of real, curved leaves.
  n=min(270,max(48,int(np.prod(d)**.5*160)))
  vv=[];ff=[]
  for j in range(n):
   direction=rng.normal(size=3);direction/=np.linalg.norm(direction)
   center=direction*(rng.random()**.23)*d*.95
   a=rng.uniform(0,math.tau);ca,sa=np.cos(a),np.sin(a)
   til=rng.uniform(-.75,.75)
   rot=np.array([[ca,-sa*math.cos(til),sa*math.sin(til)],[sa,ca*math.cos(til),-ca*math.sin(til)],[0,math.sin(til),math.cos(til)]])
   leaf=(leaf_v-.4)*(.18+min(max(d),3)*.15)
   vv.append(leaf@rot.T+center);ff.append(leaf_f+j*len(leaf_v))
  v=np.concatenate(vv)@R.T+pos;f=np.concatenate(ff)
 elif k=='sphere':v,f=sphere;v=(v*d)@R.T+pos
 else:
  v,f=source_geo[k];v=v.copy()
  if k=='cloth':v[:,2]+=np.sin(v[:,0]*6+pos[0])*.11*(.5-v[:,1])
  v=(v*d)@R.T+pos
 push(v,f,pos,family,o['color'])
 if i%10000==0:print('ARCHITECTURE',i,'/',len(world['instances']),'cache',len(cache),flush=True)

# High-detail goods at the street counters, readable at human distance.
cup=lathe([(0,0),(.07,0),(.089,.015),(.12,.19),(.114,.203),(.10,.199),(.08,.03),(0,.03)],48)
saucer=lathe([(0,0),(.11,0),(.19,.016),(.18,.027),(.10,.022),(0,.022)],48)
tin=lathe([(0,0),(.13,0),(.14,.02),(.14,.31),(.148,.32),(.148,.34),(0,.34)],48)
def torus_geo(major,minor,segments=48,sides=10):
 vs=[];fs=[]
 for a in range(segments):
  t=a*math.tau/segments
  for b in range(sides):
   p=b*math.tau/sides;r=major+minor*math.cos(p);vs.append((r*math.cos(t),r*math.sin(t),minor*math.sin(p)))
 for a in range(segments):
  for b in range(sides):
   aa=a*sides+b;bb=((a+1)%segments)*sides+b;cc=((a+1)%segments)*sides+(b+1)%sides;dd=a*sides+(b+1)%sides
   fs.extend([(aa,bb,cc),(aa,cc,dd)])
 return np.array(vs,np.float32),np.array(fs,np.int32)
handle=torus_geo(.075,.014,36,8)
for side in [-1,1]:
 for z in [60,77,110,127,143]:
  for j in range(9):
   p=np.array([side*7.42,3.16,z-4+j*.93])
   if j%3==0:
    shape(p,(1,1,1),saucer,0xe5e7d9,'Ceramic')
    shape(p+[0,.028,0],(1,1,1),cup,0xe1e4d5,'Ceramic')
    shape(p+[.138,.13,0],(1,1,1),handle,0xbca975,'Bronze')
   elif j%3==1:
    for n in range(3):shape(p+[0,0,n*.27-.27],(1,1,1),tin,[0x718f89,0xbca477,0x819aab][n],'Bronze')
   else:
    # Woven basket, rolled rim, and clustered bread loaves.
    for row in range(6):
     rr=.24+row*.006
     shape(p+[0,.022+row*.029,0],(1,1,1),torus_geo(rr,.012,48,7),0xa68e63,'Timber',np.array([[1,0,0],[0,0,1],[0,-1,0]]))
    for n in range(18):
     a=n*math.tau/18
     shape(p+[math.cos(a)*.25,.095,math.sin(a)*.25],(.012,.18,.012),rounded_cyl,0xb49b71,'Timber')
    for n in range(4):shape(p+[math.cos(n*1.5)*.1,.17,math.sin(n*1.5)*.1],(.12,.09,.075),sphere,0xcaa675,'Fruit')

# Rivets, hinges, framed inspection plates and drain gratings at street height.
for side in [-1,1]:
 for z in [60,77,110,127,143]:
  for zz in [z-5.4,z+5.4]:
   for h in [2.45,3.3,4.15]:
    box([side*7.79,h,zz],[.09,.22,.34],0x537373,'Bronze')
    for off in [-.09,.09]:shape([side*7.72,h,zz+off],[.036,.018,.036],rounded_cyl,0xb5a67c,'Bronze',np.array([[0,1,0],[-1,0,0],[0,0,1]]))
 for z in np.arange(56,148,7):
  for i in range(9):box([side*4.52,2.064,z+(i-4)*.032],[.21,.025,.018],0x3e605f,'Bronze')

# Japanese signs are editable font objects before conversion and real geometry in UE.
fontpath=Path(__import__('os').environ.get('EWSKY_FONT', str(ROOT/'Content/Fonts/DroidSansFallback.ttf')))
font=bpy.data.fonts.load(str(fontpath)) if fontpath.exists() else None
for idx,s in enumerate(world['signs']):
 cu=bpy.data.curves.new('SignText_'+str(idx),'FONT');cu.body=s['text'];cu.size=.7;cu.align_x='CENTER';cu.align_y='CENTER';cu.extrude=.002;cu.bevel_depth=.0009;cu.bevel_resolution=2
 if font:cu.font=font
 ob=bpy.data.objects.new('SignText_'+str(idx),cu);bpy.context.collection.objects.link(ob)
 bpy.context.view_layer.objects.active=ob;ob.select_set(True)
 # Text local XY plane corresponds to the original sign plane in Three.js.
 bpy.context.view_layer.update()
 factor=min(s['w']*.86/max(ob.dimensions.x,.1),s['h']*.78/max(ob.dimensions.y,.1))
 ob.scale=(factor,)*3
 bpy.ops.object.convert(target='MESH');ob=bpy.context.object
 mesh=ob.data;v=np.array([tuple(a.co) for a in mesh.vertices],np.float32)*factor
 mesh.calc_loop_triangles();f=np.array([tuple(t.vertices) for t in mesh.loop_triangles],np.int32)
 M=np.array(s['matrix']).reshape(4,4).T;v[:,2]+=.012
 v=v@M[:3,:3].T+M[:3,3]
 col=int(s.get('options',{}).get('color','#365650').lstrip('#'),16)
 push(v,f,M[:3,3],'Bronze',col)
 bpy.data.objects.remove(ob,do_unlink=True)
print('DETAILING_READY groups',len(groups),flush=True)

manifest={'version':2,'units':'metres','mapping':'Three(x,y,z) -> Unreal(100*x,100*z,100*y)','chunks':[],'places':world['places'],'materials':materials,'sourceObjects':len(world['instances']),'familyCounts':dict(counts),'detailCounts':dict(detail_counts),'totalTriangles':0}
for num,(key,g) in enumerate(sorted(groups.items())):
 v=np.concatenate(g['v']).astype(np.float32);f=np.concatenate(g['f']).astype(np.int32);c=np.concatenate(g['col']);mi=np.concatenate(g['mat'])
 name='SM_City_'+('_'.join(str(i).replace('-','n') for i in key))
 me=bpy.data.meshes.new(name);me.vertices.add(len(v));me.vertices.foreach_set('co',v.ravel())
 me.loops.add(f.size);me.loops.foreach_set('vertex_index',f.ravel());me.polygons.add(len(f));me.polygons.foreach_set('loop_start',np.arange(len(f),dtype=np.int32)*3);me.polygons.foreach_set('loop_total',np.full(len(f),3,np.int32))
 for n in materials:me.materials.append(palette[n])
 me.polygons.foreach_set('material_index',mi);me.update()
 attr=me.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER');attr.data.foreach_set('color',c.ravel())
 # Metric projection per triangle avoids stretching on the giant walls and decks.
 uv=me.uv_layers.new(name='MetricUV');normal=np.cross(v[f[:,1]]-v[f[:,0]],v[f[:,2]]-v[f[:,0]]);axes=np.argmax(np.abs(normal),axis=1)
 coords=v[f].copy()+g['anchor']@P.T
 tex=np.empty((len(f),3,2),np.float32)
 for axis,which in [(0,[1,2]),(1,[0,2]),(2,[0,1])]:
  mask=axes==axis;tex[mask]=coords[mask][:,:,which]*.5
 for at,values in g['uv_overrides']:tex[at:at+len(values)]=values
 uv.data.foreach_set('uv',tex.ravel())
 if g['wind_overrides']:
  wind=me.uv_layers.new(name='WindWeights');weights=np.zeros((len(f),3,2),np.float32)
  for at,values in g['wind_overrides']:weights[at:at+len(values)]=values
  wind.data.foreach_set('uv',weights.ravel())
 ob=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(ob)
 ob.location=g['anchor']@P.T
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 # Export local mesh with a manifest anchor so UE doesn't lose precision far away.
 saved=ob.location.copy();ob.location=(0,0,0)
 bpy.ops.export_scene.fbx(filepath=str(FBX/(name+'.fbx')),use_selection=True,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_space_transform=False,mesh_smooth_type='FACE',use_mesh_modifiers=True,add_leaf_bones=False,path_mode='STRIP',use_custom_props=False)
 ob.location=saved
 manifest['chunks'].append({'name':name,'family':g['family'],'anchor':g['anchor'].tolist(),'triangles':len(f),'vertices':len(v),'materialSlots':materials,'contentHash':fingerprint(me)})
 manifest['totalTriangles']+=len(f)
 g.clear()
 print('CHUNK',num+1,'/',len(groups),name,len(f),flush=True)

# Authoring camera and sun make the Blender source immediately useful.
bpy.ops.object.camera_add(location=(2,-49,7));camera=bpy.context.object;camera.name='Street_Study';target=np.array([0,-82,14]);direction=Matrix.Identity(4)
from mathutils import Vector
camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.lens=27;bpy.context.scene.camera=camera
bpy.ops.object.light_add(type='SUN',location=(40,80,120));sun=bpy.context.object;sun.name='Afternoon_Sun';sun.rotation_euler=(.42,-.32,-.7);sun.data.energy=3
bpy.context.scene.world.color=(.14,.21,.34)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AerialCity.blend'),compress=True)
manifest['seconds']=round(time.time()-start,2)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False),encoding='utf8')
print('NATIVE_BLENDER_BUILD_SUCCEEDED',manifest['totalTriangles'],'triangles',manifest['seconds'],'seconds',flush=True)
