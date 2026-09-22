"""Craft95 rooftop refinement, visual assets only. Blender -b --factory-startup -t 4.

The captured dome cluster is city_volume.py's UrbanCrown, not the garden canopy.
New source files target three UrbanGarden and four UrbanCrown visual meshes.
Existing FBX, blend, uassets, C++ collision plans and global lighting are untouched.
"""
from pathlib import Path
import bpy, math, random, json, hashlib, ast, tempfile, sys
from mathutils import Vector, Matrix

HERE=Path(__file__).resolve().parent;P=HERE.parents[1];ART=P/'SourceArt'
code=(HERE/'build_craft95_surroundings.py').read_text(encoding='utf-8')
env={'__file__':str(HERE/'build_craft95_surroundings.py'),'__name__':'roof_refine_primitives'}
exec(compile(code[:code.index('for variant in range(3):')],str(HERE/'build_craft95_surroundings.py'),'exec'),env)
B=env['B'];box=env['box'];merge=env['merge'];column=env['column'];roof=env['roof'];shrub=env['balcony_shrub']
G=env['scope']['g'];CATALOG=[]

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

protected=[HERE/'build_craft95_surroundings.py',HERE/'city_districts.py',HERE/'city_volume.py',
           P/'Source/EndlessWorld/EWCityAccess.cpp',P/'Source/EndlessWorld/EWPoolPlan.cpp',
           P/'Source/EndlessWorld/EWDayCycle.cpp']
for family,count in [('UrbanGarden',3),('UrbanCrown',4)]:
    protected += [P/'Content/EndlessWorld/Kit'/f'SM_{family}_{i}.uasset' for i in range(count)]
protected += list((ART/'Meshes').glob('SM_Craft95RoofGarden_*.fbx'))
before={str(f.relative_to(P)):sha(f) for f in protected}
base_catalog={a['name']:a for a in json.loads((ART/'kit-catalog.json').read_text())['assets']}
previous_garden={a['name']:a for a in json.loads((ART/'craft95-surroundings.json').read_text())['assets']}

custom={
    'Craft95RoofStone':((.70,.72,.66,1),'/Game/EndlessWorld/Materials/Quality93Final/M_Sky92Stone'),
    'Craft95RoofEdge':((.79,.80,.72,1),'/Game/EndlessWorld/Materials/Quality93Final/M_Sky92Edge'),
    'Craft95RoofCopper':((.49,.68,.65,1),'/Game/EndlessWorld/Materials/Quality93Final/M_Sky92Roof'),
    'Craft95RoofBronze':((.61,.46,.27,1),'/Game/EndlessWorld/Materials/Quality93Final/M_Sky92Copper'),
}
for name,(colour,path) in custom.items():
    G['FAMILIES'].append(name);G['COLOURS'][name]=colour
    mat=bpy.data.materials.new('M_'+name);mat.diffuse_color=colour;mat.use_nodes=True
    shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Base Color'].default_value=colour
    shader.inputs['Roughness'].default_value=.43 if 'Copper' in name or 'Bronze' in name else .68
    if 'Copper' in name or 'Bronze' in name:shader.inputs['Metallic'].default_value=.72
    G['MATERIALS'].append(mat)
STONE='Craft95RoofStone';EDGE='Craft95RoofEdge';COPPER='Craft95RoofCopper';METAL='Craft95RoofBronze'

def tint(c,s):return tuple(min(.95,x*s) for x in c[:3])+(1,)

def pbox(b,p,d,mat=STONE,colour=None,bevel=.012):
    b.box(p,d,mat,bevel,colour=colour or G['COLOURS'][mat])

def tile(b,cx,cy,w,d,z,family,colour,thickness=.035,bevel=.008):
    """28 triangles: a broad stone face, small true chamfer and closed sides."""
    corners=[(-w/2+bevel,-d/2), (w/2-bevel,-d/2),(w/2,-d/2+bevel),(w/2,d/2-bevel),
             (w/2-bevel,d/2),(-w/2+bevel,d/2),(-w/2,d/2-bevel),(-w/2,-d/2+bevel)]
    vertices=[(cx+x,cy+y,z-thickness) for x,y in corners]
    vertices += [(cx+x*.997,cy+y*.997,z) for x,y in corners]
    faces=[tuple(reversed(range(8))),tuple(range(8,16))]
    faces += [(i,(i+1)%8,(i+1)%8+8,i+8) for i in range(8)]
    b.mesh(vertices,faces,family,colour,smooth=False)

def floor(b,width,z,seed,garden):
    # Datum is exactly unchanged. The grout is 1.5cm below the walkable tops.
    bottom=-1 if garden else -.45
    box(b,(0,0,(bottom+z-.035)/2),(width,width,z-.035-bottom),colour=(.55,.60,.56,1))
    rng=random.Random(seed);n=50 if garden else 46;pitch=width/n
    for iy in range(n):
        for ix in range(n):
            x=-width/2+(ix+.5)*pitch;y=-width/2+(iy+.5)*pitch
            if garden and any(abs(x-a)<2.8 and abs(y-c)<2.8 for a,c in [(11,11),(-12,-10)]):continue
            if not garden and any(abs(x-a)<5.6 and abs(y-c)<5.1 for a in [-9,9] for c in [-9,9]):continue
            colour=tint((.69,.71,.65,1),rng.uniform(.96,1.035))
            # A quiet alternating perimeter band, no raised walking obstruction.
            if ix in (1,n-2) or iy in (1,n-2):colour=(.59,.64,.61,1)
            tile(b,x,y,pitch-.013,pitch-.013,z,STONE,colour)

def planter_shell(b,p,length,width,height,square=False):
    x,y,z=p;body_l=length if square else length-.30;body_w=width if square else width-.25
    # All stone and metal remain inside the old bounding solid.
    pbox(b,(x,y,z+.105),(body_l,body_w,.21),EDGE)
    inset=.055
    pbox(b,(x,y,z+height*.47),(body_l-.10,body_w-.10,height*.73),STONE)
    for sign in [-1,1]:
        # Recessed long panel fields, stepped end pilasters and bead mouldings.
        yy=y+sign*(body_w/2-.023)
        for zz in [.26,height-.27]:pbox(b,(x,yy,z+zz),(body_l-.28,.044,.048),EDGE,bevel=.009)
        for xx in [-body_l/2+.17,body_l/2-.17]:
            pbox(b,(x+xx,yy,z+height*.49),(.11,.047,height-.47),EDGE)
        panel_count=max(2,round(body_l/1.05))
        for k in range(panel_count):
            xx=x-body_l/2+.35+(body_l-.70)*(k+.5)/panel_count
            pbox(b,(xx,yy+sign*.006,z+height*.48),(.012,.016,height-.57),METAL,bevel=.002)
        xx=x+sign*(body_l/2-.023)
        for zz in [.26,height-.27]:pbox(b,(xx,y,z+zz),(.044,body_w-.28,.048),EDGE,bevel=.009)
        for yy2 in [-body_w/2+.16,body_w/2-.16]:
            pbox(b,(xx,y+yy2,z+height*.49),(.047,.10,height-.47),EDGE)
    # Coping is built as three inset steps, kept inside the existing maximum.
    for zz,h,over in [(height-.18,.065,.07),(height-.10,.095,.00),(height-.0275,.055,.035)]:
        outer_l=length-over;outer_w=width-over;thick=.20 if square else .17
        for s in [-1,1]:
            pbox(b,(x,y+s*(outer_w-thick)/2,z+zz),(outer_l,thick,h),EDGE,bevel=.010)
            pbox(b,(x+s*(outer_l-thick)/2,y,z+zz),(thick,outer_w-2*thick,h),EDGE,bevel=.010)
    # Soil height matches the retained tree insertion plane and border planting.
    soil=1.065 if square else height-.045
    pbox(b,(x,y,z+soil),(length-.43,width-.43,.025),'Soil',(.12,.105,.075,1),.001)

def garden(variant):
    b=B();floor(b,41.5,0,9560+variant,True)
    for side in range(4):
        for x in [-14,-5,5,14]:
            a=B();planter_shell(a,(x,-18.6,0),5.30,2.05,1.225)
            seed=variant*79+side*13+x
            # Exactly the current Craft95 foliage positions/seeds and silhouette.
            for j in range(3):
                q=(x-1.8+j*1.3,-18.6,1.3);shrub(a,q,1.15,seed+j)
                if j%2==0:shrub(a,(q[0],-19.3,.9),2.15,seed+50+j,True)
            merge(b,a,yaw=side*math.pi*.5)
    for x in [-12,0]:
        for y in [5,17]:column(b,(x,y,.1),5.2,.22)
    a=B();roof(a,14,15,5.65)
    # Underside trim and seams stay below the existing 7.475m crown.
    for y in [-7.40,7.40]:
        pts=[(-7+14*i/64,y,5.65+1.6*(1-(-1+2*i/64)**2)-.08) for i in range(65)]
        a.tube(pts,[.037]*65,METAL,8)
    for x in [-6.5,-5.2,-3.9,-2.6,-1.3,0,1.3,2.6,3.9,5.2,6.5]:
        z=5.65+1.6*(1-(x/7)**2)+.07
        a.tube([(x,-7.4,z),(x,7.4,z)],[.018,.018],METAL,6)
    merge(b,a,(-6,11,0))
    for p in [(11,11,0),(-12,-10,0)]:planter_shell(b,p,5.4,5.4,1.1,True)
    return b

def circular_ring(b,r,z,thickness=.022,family=METAL,n=128):
    pts=[(r*math.cos(i*math.tau/n),r*math.sin(i*math.tau/n),z) for i in range(n+1)]
    b.tube(pts,[thickness]*len(pts),family,8)

def window(b,x,y,z,w=2.0,h=3.65):
    # Closed decorative glazing: no new opening, portal, light or collision.
    r=w/2;spring=z+h-r
    vertices=[(x-r,y+.13,z),(x+r,y+.13,z)]
    vertices += [(x+r*math.cos(i*math.pi/32),y+.13,spring+r*math.sin(i*math.pi/32)) for i in range(33)]
    b.mesh(vertices,[tuple(range(len(vertices)))],'Paint',(.08,.20,.23,1),smooth=False)
    for side in [-1,1]:
        pbox(b,(x+side*(r+.095),y+.085,z+(h-r)/2),(.19,.17,h-r),EDGE)
        pbox(b,(x+side*(r+.20),y+.145,z+(h-r)/2),(.07,.07,h-r+.05),STONE)
    for i in range(20):
        lo=i*math.pi/20+.009;hi=(i+1)*math.pi/20-.009
        b.arc((x,y+.083,spring),r+.09,.18,lo,hi,EDGE,.165,3)
    b.arc((x,y+.13,spring),r+.245,.065,0,math.pi,STONE,.09,40)
    pbox(b,(x,y+.10,z-.09),(w+.48,.20,.18),EDGE)
    for level in [.92,2.15]:pbox(b,(x,y+.055,z+level),(w-.10,.045,.048),METAL,bevel=.008)
    pbox(b,(x,y+.05,z+(h-.1)/2),(.055,.055,h-.1),METAL,bevel=.008)
    for a in [math.pi*.25,math.pi*.5,math.pi*.75]:
        b.tube([(x,y+.05,spring),(x+math.cos(a)*(r-.04),y+.05,spring+math.sin(a)*(r-.04))],[.023,.023],METAL,8)
    pbox(b,(x,y+.079,spring+r+.02),(.23,.155,.32),EDGE,bevel=.014)

def crown_room(b,x,y,variant):
    a=B()
    # Original building solid: 11 x 10 m, z=.25..6.15. Insets create relief
    # within that solid; cornices and jambs return to the exact old footprint.
    pbox(a,(0,0,3.2),(10.64,9.64,5.9),STONE,(.80,.83,.79,1),.020)
    for z,h in [(.36,.22),(.62,.16),(5.62,.12),(5.94,.18),(6.105,.09)]:
        pbox(a,(0,0,z),(11,10,h),EDGE,bevel=.02)
    for sx in [-1,1]:
        for sy in [-1,1]:pbox(a,(sx*5.365,sy*4.865,3.08),(.27,.27,4.75),EDGE)
    # Side elevation and rear now have genuine recessed arched window relief.
    for side in range(4):
        face=B();width=11 if side%2==0 else 10;depth=5 if side%2==0 else 5.5
        for xx in [-3.35,0,3.35] if width==11 else [-2.65,2.65]:window(face,xx,-depth,.98,1.83,3.64)
        for z in [1.70,2.52,3.34,4.16,4.98]:
            # Thin horizontal recessed masonry joints between pilasters.
            pbox(face,(0,-depth+.177,z),(width-.5,.009,.018),'Paint',(.26,.29,.25,1),0)
        merge(a,face,yaw=side*math.pi/2)
    # Smooth elliptical copper cupola within the original r=6.2,z=6.4..12 cap.
    a.lathe([(0,6.4),(6.2,6.4),(6.2,6.52),(6.11,6.62),(6.11,6.69)],family=COPPER,segments=128)
    profile=[]
    for j in range(41):
        t=j*math.pi/2/40
        profile.append((6.10*math.cos(t),6.60+5.40*math.sin(t)))
    a.lathe(profile,family=COPPER,segments=128)
    for k in range(24):
        ang=k*math.tau/24;pts=[]
        for j in range(33):
            t=.018+j*(math.pi/2-.082)/32;r=6.10*math.cos(t)+.009
            pts.append((r*math.cos(ang),r*math.sin(ang),6.60+5.40*math.sin(t)+.006))
        a.tube(pts,[.017*(1-j/32)+.001 for j in range(33)],COPPER,6,(.44,.61,.58,1))
    for z in [6.63,7.55,8.62,9.71,10.68]:
        r=6.10*math.sqrt(max(0,1-((z-6.60)/5.4)**2))
        circular_ring(a,r+.006,z,.012,COPPER,96)
    circular_ring(a,6.16,6.48,.018,METAL)
    a.lathe([(0,11.80),(.10,11.80),(.115,11.91),(.085,11.98),(0,12.0)],family=METAL,segments=32)
    merge(b,a,(x,y,0))

# Preserve the same original hanging garlands and seed contracts on the crown.
city_text=(HERE/'city_volume.py').read_text(encoding='utf-8')
parsed=ast.parse(city_text);hanging_def=next(x for x in parsed.body if isinstance(x,ast.FunctionDef) and x.name=='hanging')
exec(compile(ast.Module(body=[hanging_def],type_ignores=[]),str(HERE/'city_volume.py'),'exec'),globals())

def crown(variant):
    b=B();floor(b,38,.45,9580+variant,False)
    for x in [-17,17]:
        for y in [-17,17]:
            column(b,(x,y,.3),6,.40)
            a=B();profile=[(0,0),(1.8,0),(1.8,.25)]
            profile += [(1.79*math.cos(j*math.pi/2/32),.25+3.15*math.sin(j*math.pi/2/32)) for j in range(33)]
            a.lathe(profile,family=COPPER,segments=72)
            circular_ring(a,1.78,.20,.016,METAL,72)
            merge(b,a,(x,y,6.3))
    for x in [-9,9]:
        for y in [-9,9]:
            crown_room(b,x,y,variant)
            hanging(b,(x,y-5.3,.8),variant+int(x+y))
    return b

def emit(name,target,b,old):
    old_bounds=old['bounds_m']
    lo=[min(v[i] for v in b.v) for i in range(3)];hi=[max(v[i] for v in b.v) for i in range(3)]
    # Ground, XY travel envelope and highest roof points remain unchanged.
    # Hanging foliage may define the visual minimum below the structural slab.
    assert all(abs([lo,hi][a][k]-old_bounds[a][k])<.002 for a,k in [(0,0),(0,1),(1,0),(1,1),(1,2)]),(name,lo,hi,old_bounds)
    used=[G['FAMILIES'][i] for i in sorted(set(b.m))]
    G['export'](name,b,'Craft95 original roof refinement; unchanged runtime collision and placement')
    item=dict(G['CATALOG'][-1]);assert 80000<=item['triangles']<=500000,(name,item['triangles'])
    item.update(target_name=target,used_materials=used,previous_bounds_m=old_bounds,
        ue_bounds_cm=[[lo[0]*100,-hi[1]*100,lo[2]*100],[hi[0]*100,-lo[1]*100,hi[2]*100]],
        material_overrides={family:custom[family][1] for family in used if family in custom},
        geometry_contract='metres, original city authoring +Y; FBX uses -Y forward, UE receives reflected Y; replace visual asset only',
        collision='Existing EWCityAccess.cpp recipes, floor datums, planter volumes and canopy-strip colliders retained; no auto collision',
        maximum_structural_bounds_delta_m=.002)
    CATALOG.append(item)
    return bpy.data.objects['SM_'+name]

objects=[]
for v in range(3):objects.append(emit('Craft95RoofRefineGarden_'+str(v),'UrbanGarden_'+str(v),garden(v),previous_garden['Craft95RoofGarden_'+str(v)]))
for v in range(4):objects.append(emit('Craft95RoofRefineCrown_'+str(v),'UrbanCrown_'+str(v),crown(v),base_catalog['UrbanCrown_'+str(v)]))
after={str(f.relative_to(P)):sha(f) for f in protected};assert before==after,'Protected source/uasset changed during generation; inspect concurrent work'

for i,obj in enumerate(objects):obj.location=(i*50,0,0)
for mat in G['MATERIALS']:
    mat.use_nodes=True;nodes=mat.node_tree.nodes;links=mat.node_tree.links;shader=nodes.get('Principled BSDF')
    attr=nodes.new('ShaderNodeVertexColor');attr.layer_name='Color'
    if mat.name.removeprefix('M_') in custom:
        gamma=nodes.new('ShaderNodeGamma');gamma.inputs['Gamma'].default_value=2.2
        links.new(attr.outputs['Color'],gamma.inputs['Color']);links.new(gamma.outputs['Color'],shader.inputs['Base Color'])
    else:links.new(attr.outputs['Color'],shader.inputs['Base Color'])
bpy.context.preferences.filepaths.save_version=0
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24
scene.render.threads_mode='FIXED';scene.render.threads=4;scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.world.color=(.28,.28,.28);scene.view_settings.view_transform='AgX'
for name,location,power,size in [('RoofKey',(10,-15,38),19000,22),('RoofFill',(-15,20,25),12500,20)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=location
    obj.rotation_euler=(Vector((0,0,2))-obj.location).to_track_quat('-Z','Y').to_euler()
camera=bpy.data.objects.new('RoofReview',bpy.data.cameras.new('RoofReview'));scene.collection.objects.link(camera);scene.camera=camera
camera.location=(33,-45,36);camera.rotation_euler=(Vector((0,0,1.2))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=54
blend=ART/'Blender/Craft95RoofRefine.blend';bpy.ops.wm.save_as_mainfile(filepath=str(blend))
manifest={'revision':95,'refinement':'rooftop-craft-r2','assets':CATALOG,'total_triangles':sum(a['triangles'] for a in CATALOG),
    'source_blend':{'file':'Blender/Craft95RoofRefine.blend','sha256':sha(blend)},
    'identified_domes':{'runtime_meshes':'UrbanCrown_0..3','author':'city_volume.py crown loop','captured_reference':'Evidence/craft95-capture/shipping-pilot01/05-rooftop-garden/Frames/still-native.png'},
    'preservation':{'before':before,'after':after,'unchanged':before==after},
    'collider_contract':{'garden_floor_z_cm':0,'crown_floor_z_cm':45,'garden_planters':'existing 5.4x5.4x1.1m solids; border 5.30x2.05x1.225m solids',
        'garden_canopy':'existing 25 strips at z=5.65+1.6*(1-u*u)m, same four support columns',
        'crown_buildings':'four unchanged 11x10m solid footprints; visual cap remains within r6.2m and z12m',
        'gameplay_source_written':False,'uassets_written':False,'runtime_import':'NOT_RUN','runtime_acceptance':'NOT_RUN'},
    'materials':'No new material assets required. Explicit RoofStone/Edge/Copper/Bronze slots reuse Quality93Final finishes without editing their graphs.'}
(ART/'craft95-roof-refine.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('CRAFT95_ROOF_REFINE_READY',manifest['total_triangles'],flush=True)
if '--render-review' in sys.argv:
    scene.render.image_settings.file_format='PNG'
    for kind,index in [('garden',0),('crown',3)]:
        for j,obj in enumerate(objects):obj.hide_render=j!=index
        objects[index].location=(0,0,0)
        scene.render.filepath=str(Path(tempfile.gettempdir())/('craft95-roof-'+kind+'.png'))
        bpy.ops.render.render(write_still=True)
