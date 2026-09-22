"""Original seamless PBR micro-surfaces; deterministic, no downloaded imagery."""
from pathlib import Path
import numpy as np
from PIL import Image
import hashlib,json
S=Path(__file__).resolve().parents[1];out=S/'Textures/Quality93';out.mkdir(parents=True,exist_ok=True)
N=1024;y,x=np.mgrid[0:N,0:N]/N;rng=np.random.default_rng(93093)
def noise(bands):
    result=np.zeros((N,N))
    for freq,amp,count in bands:
        for i in range(count):
            a=int(rng.integers(1,freq+1));b=int(rng.integers(-freq,freq+1))
            result+=amp*np.sin(2*np.pi*(a*x+b*y)+rng.uniform(0,6.283))/np.sqrt(count)
    return result/(np.std(result)*3)
macro=noise([(3,.8,12),(13,.24,16),(47,.10,16)])
fine=noise([(110,.35,18),(290,.08,18)])
records=[]
for kind in ['Oak','Linen','Plaster','Glaze','Metal','Stone','Paper','Leaf']:
    if kind=='Oak':
        phase=x*72+.55*np.sin(2*np.pi*y*2)+.3*np.sin(2*np.pi*(x*3+y))+.28*macro
        grain=(np.sin(phase*2*np.pi)+.28*np.sin(phase*6*np.pi))/1.28
        pores=np.maximum(0,np.sin(2*np.pi*(x*241+.22*np.sin(y*31.416))))**18
        h=grain*.06+fine*.028-pores*.016;base=.94+grain*.026+macro*.015-pores*.006;rough=.44+grain*.022+fine*.013;strength=.28;tile=.80
    elif kind=='Linen':
        warp=np.cos(2*np.pi*x*128);weft=np.cos(2*np.pi*y*128)
        weave=(warp+weft)*.16+.08*warp*weft
        h=weave+fine*.05;base=.93+weave*.065+macro*.018;rough=.80+weave*.07+fine*.025;strength=.95;tile=.34
    elif kind=='Plaster':
        h=macro*.001+fine*.04;base=.985+macro*.006+fine*.003;rough=.78+fine*.018;strength=.55;tile=.62
    elif kind=='Glaze':
        h=macro*.015+fine*.014;base=.98+macro*.012;rough=.22+macro*.025+fine*.01;strength=.48;tile=.42
    elif kind=='Metal':
        brush=noise([(3,.10,5)])+.2*np.sin(2*np.pi*x*237)+.07*np.sin(2*np.pi*(x*361+y*2))
        h=brush*.014;base=.98+brush*.004;rough=.32+brush*.009+macro*.005;strength=.12;tile=.38
    elif kind=='Stone':
        vein=np.exp(-((np.sin(2*np.pi*(x*2+y+.10*macro)))/.065)**2)
        h=macro*.04+fine*.05;base=.94+macro*.024-vein*.045;rough=.60+macro*.045+fine*.025;strength=.85;tile=1.4
    elif kind=='Paper':
        h=fine*.08;base=.95+macro*.015+fine*.025;rough=.80+fine*.035;strength=.65;tile=.28
    else:
        h=macro*.04+fine*.05;base=.93+macro*.035;rough=.44+macro*.04+fine*.02;strength=.55;tile=.48
    dx=(np.roll(h,-1,1)-np.roll(h,1,1))*strength
    dy=(np.roll(h,-1,0)-np.roll(h,1,0))*strength
    normal=np.stack([-dx,dy,np.ones_like(h)],axis=-1);normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
    images={'Base':np.repeat(np.clip(base,.55,1)[...,None],3,axis=2),
            'Normal':normal*.5+.5,'ORM':np.stack([np.full_like(h,1),np.clip(rough,.15,.92),np.zeros_like(h)],axis=-1)}
    for suffix,array in images.items():
        f=out/f'T_Q93_{kind}_{suffix}.png'
        Image.fromarray(np.round(np.clip(array,0,1)*255).astype('uint8')).save(f)
        records.append({'file':f.name,'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'surface':kind,'map':suffix,'tile_metres':tile,'size':[N,N]})
manifest={'original_procedural_art':True,'revision':93,'textures':records,'seed':93093,'note':'Original mathematical surfaces, no external source imagery. Mipmaps are generated at Unreal import.'}
(S/'quality93-textures.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
print('QUALITY93_TEXTURES_COMPLETE',len(records))
