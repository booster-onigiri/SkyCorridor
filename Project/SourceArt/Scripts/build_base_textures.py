"""Generate the 42 original base PBR texture maps with the original seeded formula.
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

