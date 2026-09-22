from pathlib import Path
"""Human-scale replacements. Executed by build_blender.py after its mesh helpers."""
from mathutils import Vector
from mathutils.geometry import tessellate_polygon
detail_counts=defaultdict(int)
detail_cache={}

def tint(hex_color,r=1.,g=1.,b=1.):
 return sum(min(255,max(0,round(((hex_color>>shift)&255)*mul)))<<shift for shift,mul in [(16,r),(8,g),(0,b)])

def curved_leaf(length=1.,width=.26,steps=9):
 v=[];uv=[];f=[]
 for i in range(steps+1):
  t=i/steps;w=max(.004,math.sin(math.pi*t)**.74)*width
  for j in range(5):
   s=j/2-1
   v.append((s*w*.5,t*length,.045*length*math.sin(math.pi*t)+.035*length*s*s*math.sin(math.pi*t)))
   uv.append((j/4,t))
 for i in range(steps):
  for j in range(4):
   a=i*5+j;b=a+5;f.extend([(a,a+1,b),(a+1,b+1,b)])
 return np.array(v,np.float32),np.array(f,np.int32),np.array(uv,np.float32)

def emit_local(v,f,R,pos,family,color,uv=None,wind=None):
 push(v@R.T+pos,f,pos,family,color,uv=uv[f] if uv is not None else None,wind=wind[f] if wind is not None else None)

def frond(d,R,pos,color,index):
 # Pinnate palm/fern frond: rachis, paired tapering leaflets, and a curved tip.
 local_rng=np.random.default_rng(7349+index)
 length=float(d[1]);palm=length>2.;pairs=34 if palm else 17
 vv=[];ff=[];uu=[];ww=[];offset=0
 base_v,base_f,base_uv=curved_leaf(1.,.21 if palm else .3,9 if palm else 7)
 for j in range(pairs):
  t=.045+j/(pairs-1)*.915
  along=(t-.5)*length
  lateral=float(d[0])*(.72 if palm else .8)*(math.sin(math.pi*t)**.55)*local_rng.uniform(.92,1.08)
  for side in [-1,1]:
   spread=np.array([side*.88,.40,.19*math.sin(t*7)],np.float32);spread/=np.linalg.norm(spread)
   across=np.array([spread[1],-spread[0],0],np.float32);across/=np.linalg.norm(across)
   normal=np.cross(across,spread)
   orient=np.column_stack([across,spread,normal])
   leaf=base_v*lateral
   center=np.array([0,along,(math.sin(math.pi*t)*.08+t*t*.11)*length])
   leaf=leaf@orient.T+center
   vv.append(leaf);ff.append(base_f+offset);uu.append(base_uv)
   ww.append(np.column_stack([base_uv[:,1]*(1.3 if palm else .75),np.full(len(leaf),index%37*.11)]));offset+=len(leaf)
 v=np.concatenate(vv);f=np.concatenate(ff);uv=np.concatenate(uu);wind=np.concatenate(ww)
 emit_local(v,f,R,pos,'Foliage',tint(color,.61,.77,.61),uv,wind)
 # Organic shaft follows the same curvature. Each short cylinder retains its round silhouette.
 for j in range(12):
  a=j/12;b=(j+1)/12
  p0=np.array([0,(a-.5)*length,(math.sin(math.pi*a)*.08+a*a*.11)*length])
  p1=np.array([0,(b-.5)*length,(math.sin(math.pi*b)*.08+b*b*.11)*length])
  delta=p1-p0;axis=delta/np.linalg.norm(delta);x=np.array([1.,0,0]);z=np.cross(x,axis)
  rr=max(.0012,(.013 if palm else .005)*(1-a*.75));v,f=rounded_cyl
  rot=np.column_stack([x,axis,z]);v=(v*np.array([rr,np.linalg.norm(delta),rr]))@rot.T+(p0+p1)*.5
  emit_local(v,f,R,pos,'Bark',0x506b3b)
 detail_counts['pinnate_fronds']+=1;detail_counts['leaflets']+=pairs*2

def bark_tube(d,R,pos,color,index):
 length=float(d[1]);segments=40;levels=max(3,min(22,int(length*4)))
 v=[];uv=[];f=[]
 for j in range(levels+1):
  t=j/levels;bow=math.sin(math.pi*t)*min(.045,length*.012)
  for i in range(segments):
   a=i/segments*math.tau
   ripple=1+.055*math.sin(a*11+t*9+index)+.018*math.sin(a*27-t*18)
   taper=1-.12*t if length>.35 else 1.
   v.append((math.cos(a)*d[0]*ripple*taper+bow,(t-.5)*length,math.sin(a)*d[2]*ripple*taper))
   uv.append((i/segments*max(d[0],d[2])*math.tau*2,t*length*.75))
 for j in range(levels):
  for i in range(segments):
   a=j*segments+i;b=j*segments+(i+1)%segments;c=b+segments;e=a+segments
   f.extend([(a,c,b),(a,e,c)])
 # Caps preserve collision and prevent visible holes at branch joins.
 for row,reverse in [(0,False),(levels,True)]:
  ci=len(v);v.append((0,(-.5 if row==0 else .5)*length,0));uv.append((.5,.5))
  for i in range(segments):
   a=row*segments+i;b=row*segments+(i+1)%segments
   f.append((ci,b,a) if reverse else (ci,a,b))
 emit_local(np.array(v,np.float32),np.array(f,np.int32),R,pos,'Bark',tint(color,.63,.57,.48),np.array(uv,np.float32))
 detail_counts['rounded_bark_parts']+=1

def leaf_canopy(d,R,pos,color,index):
 local_rng=np.random.default_rng(17891+index)
 n=min(350,max(55,int(np.prod(d)**.5*180)))
 base,faces,tex=curved_leaf(1.,.47,6)
 vv=[];ff=[];uu=[];ww=[];offset=0
 for j in range(n):
  direction=local_rng.normal(size=3);direction/=np.linalg.norm(direction)
  center=direction*(local_rng.random()**.24)*d*.93
  a=local_rng.uniform(0,math.tau);til=local_rng.uniform(-.9,.9)
  co,si=math.cos(a),math.sin(a);ct,st=math.cos(til),math.sin(til)
  rot=np.array([[co,-si*ct,si*st],[si,co*ct,-co*st],[0,st,ct]],np.float32)
  size=min(.33,.105+max(d)*.14)*local_rng.uniform(.75,1.2)
  leaf=(base-np.array([0,.4,.015]))*size
  vv.append(leaf@rot.T+center);ff.append(faces+offset);uu.append(tex)
  ww.append(np.column_stack([tex[:,1]*.35,np.full(len(leaf),(index+j)%53*.17)]));offset+=len(leaf)
 emit_local(np.concatenate(vv),np.concatenate(ff),R,pos,'Foliage',tint(color,.59,.76,.57),np.concatenate(uu),np.concatenate(ww))
 detail_counts['broadleaf_canopies']+=1;detail_counts['broad_leaves']+=n

def blossom_cluster(d,R,pos,color,index):
 # Each previous broad petal becomes three small flowers with five cupped petals.
 local_rng=np.random.default_rng(7201+index)
 if 'petal' not in detail_cache:detail_cache['petal']=curved_leaf(1.,.91,5)
 pv,pf,pu=detail_cache['petal'];vv=[];ff=[];uu=[];ww=[];offset=0
 for cluster in range(3):
  radius=float(min(d[0],d[1]))*local_rng.uniform(.092,.126)
  center=np.array([(cluster-1)*d[0]*.24,local_rng.uniform(-.2,.2)*d[1],local_rng.uniform(-.02,.02)])
  for j in range(5):
   a=j*math.tau/5+local_rng.uniform(-.06,.06);co,si=math.cos(a),math.sin(a)
   rot=np.array([[co,-si,0],[si,co,0],[0,0,1]])
   v=(pv*radius)@rot.T+center
   vv.append(v);ff.append(pf+offset);uu.append(pu);ww.append(np.column_stack([np.full(len(v),.3),np.full(len(v),index%41*.16)]));offset+=len(v)
  # A small golden anther cluster reads as the centre without a heavy sphere.
  for j in range(5):
   a=j*math.tau/5;r=radius*.17
   v=np.array([(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)],np.float32)
   f=np.array([(0,2,4),(2,1,4),(1,3,4),(3,0,4),(2,0,5),(1,2,5),(3,1,5),(0,3,5)],np.int32)
   scale=np.array([radius*.06,radius*.25,radius*.06]);v=v*scale
   rot=np.array([[1,0,0],[0,0,-1],[0,1,0]])
   v=v@rot.T+center+np.array([math.cos(a)*r,math.sin(a)*r,radius*.16])
   emit_local(v,f,R,pos,'Pollen',0xd5a74b)
 emit_local(np.concatenate(vv),np.concatenate(ff),R,pos,'Petal',tint(color,.98,.86,.94),np.concatenate(uu),np.concatenate(ww))
 detail_counts['cherry_flowers']+=3

def lettering(label):
 key='type_'+label
 if key in detail_cache:return detail_cache[key]
 cu=bpy.data.curves.new('BookTitle','FONT');cu.body=label;cu.align_x='CENTER';cu.align_y='CENTER';cu.size=1.;cu.extrude=0.;cu.resolution_u=3
 font=Path(__import__('os').environ.get('EWSKY_FONT', str(ROOT/'Content/Fonts/DroidSansFallback.ttf')))
 if font.exists():
  if 'type_font' not in detail_cache:detail_cache['type_font']=bpy.data.fonts.load(str(font))
  cu.font=detail_cache['type_font']
 ob=bpy.data.objects.new('BookTitle',cu);bpy.context.collection.objects.link(ob);bpy.context.view_layer.objects.active=ob;ob.select_set(True)
 bpy.ops.object.convert(target='MESH');me=ob.data;me.calc_loop_triangles()
 v=np.array([tuple(a.co) for a in me.vertices],np.float32);f=np.array([tuple(t.vertices) for t in me.loop_triangles],np.int32)
 lo=v.min(axis=0);hi=v.max(axis=0);v[:,:2]-=(lo[:2]+hi[:2])*.5
 v/=max(hi[0]-lo[0],.01);detail_cache[key]=(v,f)
 bpy.data.objects.remove(ob,do_unlink=True);return v,f

def bound_book(d,R,pos,color,index):
 dx,dy,dz=map(float,d)
 def part(center,dims,family,c):
  v,f=beveled_box(dims);emit_local(v+np.array(center),f,R,pos,family,c)
 # Pages and two thin boards replace the single solid cuboid.
 part((0,0,-.01),(max(.012,dx-.013),dy*.961,dz*.937),'Paper',0xe7dcc3)
 for side in [-1,1]:part((side*(dx*.5-.003),0,0),(.006,dy,dz),'Leather',color)
 if 'book_spine' not in detail_cache:detail_cache['book_spine']=lathe([(0,-.5),(1,-.5),(1,.5),(0,.5)],24)
 v,f=detail_cache['book_spine'];v=v*np.array([dx*.5,dy,dz*.06])+np.array([0,0,dz*.442])
 emit_local(v,f,R,pos,'Leather',color)
 for t in [-.39,-.25,.25,.39]:
  v,f=detail_cache['book_spine'];v=v*np.array([dx*.506,.009,dz*.065])+np.array([0,dy*t,dz*.442])
  emit_local(v,f,R,pos,'Bronze',0xad925a)
 part((0,dy*.015,dz*.503),(dx*.72,dy*.3,.0025),'Leather',tint(color,.51,.51,.51))
 label=['天文','植物','航路','記録','工藝'][index%5]
 v,f=lettering(label);v=v*min(dx*.63,dy*.13)+np.array([0,dy*.015,dz*.508])
 emit_local(v,f,R,pos,'Pollen',0xd8c891)
 detail_counts['bound_volumes']+=1

def garment(d,R,pos,color,index):
 choice=index%4
 if choice==0:
  outline=[(-.12,.5),(-.3,.45),(-.5,.24),(-.36,.09),(-.25,.24),(-.25,-.5),(.25,-.5),(.25,.24),(.36,.09),(.5,.24),(.3,.45),(.12,.5),(.08,.37),(-.08,.37)]
 elif choice==1:
  outline=[(-.29,.5),(-.34,-.5),(-.06,-.5),(0,.02),(.06,-.5),(.34,-.5),(.29,.5)]
 elif choice==2: outline=[(-.4,.5),(-.4,-.5),(.4,-.5),(.4,.5)]
 else: outline=[(-.22,.5),(-.22,.22),(-.38,-.5),(.38,-.5),(.22,.22),(.22,.5)]
 pts=[Vector((x,y,0)) for x,y in outline];tri=tessellate_polygon([pts])
 bm=bmesh.new();vs=[bm.verts.new(p) for p in pts]
 for t in tri:
  ids=[int(p) if isinstance(p,(int,np.integer)) else min(range(len(pts)),key=lambda j:(pts[j]-p).length) for p in t];bm.faces.new([vs[j] for j in ids])
 bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=4,use_grid_fill=True)
 v,f=mesh_arrays(bm)
 x,y=v[:,0].copy(),v[:,1].copy();fall=.5-y
 v[:,2]=(np.sin(x*21+index)*.018+np.sin(x*8+y*3+index)*.046)*fall
 v*=d
 uv=np.column_stack([x+.5,y+.5]);wind=np.column_stack([fall*8.,np.full(len(v),index*.17)])
 emit_local(v,f,R,pos,'Cloth',color,uv,wind)
 # Clothespins secure the upper edge to the cable.
 for side in [-1,1]:
  v,f=beveled_box((.022,.085,.025));v+=np.array([side*d[0]*.2,d[1]*.485,.012])
  emit_local(v,f,R,pos,'Timber',0xbca16e)
 detail_counts['sewn_garments']+=1
