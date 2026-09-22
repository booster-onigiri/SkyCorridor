"""Seamless physically scaled pale limestone pavers, authored at 2048px."""
from pathlib import Path
import numpy as np
from PIL import Image
import json,hashlib
N=2048; y,x=np.mgrid[:N,:N].astype(np.float32)/N
rng=np.random.default_rng(95095)
row=np.floor(y*8).astype(int); u=np.mod(x*8+(row%2)*.5,1); v=np.mod(y*8,1)
col=np.floor(x*8+(row%2)*.5).astype(int)%8
variation=rng.uniform(-.026,.026,(8,8))[row,col]
edge=np.minimum(np.minimum(u,1-u),np.minimum(v,1-v))
bevel=np.clip(edge/.018,0,1)
grout=np.clip((edge-.003)/.008,0,1)
fine=rng.normal(0,1,(N,N)).astype(np.float32)
grain=(np.sin(x*2*np.pi*61+y*2*np.pi*19)+np.sin(x*2*np.pi*107-y*2*np.pi*41))*.003
stone=.80+variation+grain+fine*.003
base=(.53*(1-grout)+stone*grout)[...,None]*np.array([1.0,.989,.954])
h=bevel*.20+fine*.018
dx=(np.roll(h,-1,1)-np.roll(h,1,1))*.55;dy=(np.roll(h,-1,0)-np.roll(h,1,0))*.55
normal=np.stack([-dx,dy,np.ones_like(h)],axis=-1);normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
rough=.66+variation*.4+fine*.012+(.14*(1-grout))
orm=np.stack([.90+.10*bevel,np.clip(rough,.5,.88),np.zeros_like(h)],axis=-1)
out=Path(__file__).resolve().parents[1]/'Textures/Craft95';out.mkdir(parents=True,exist_ok=True)
records=[]
for kind,a in {'Base':base,'Normal':normal*.5+.5,'ORM':orm}.items():
    p=out/f'T_Craft95_Paving_{kind}.png';Image.fromarray(np.uint8(np.clip(a,0,1)*255)).save(p)
    records.append({'file':p.name,'kind':kind,'size':[N,N],'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(out.parents[1]/'craft95-paving.json').write_text(json.dumps({'period_cm':400,'paver_cm':50,'textures':records,'original_procedural_art':True},indent=2),encoding='utf8')
print('CRAFT95_PAVING_DONE',len(records))
