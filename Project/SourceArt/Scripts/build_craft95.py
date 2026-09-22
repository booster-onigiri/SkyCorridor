"""Original Craft95 foreground assets. Run in Blender 4.5 with --background.

All source geometry is authored here in metres. UE coordinates are +Z up,
with a Y reflection at FBX export matching the Sky92 asset convention.
No existing meshes, materials, scripts, collision or levels are modified.
"""
from pathlib import Path
import bpy, math, random, json, runpy, hashlib, tempfile, sys
from mathutils import Vector, Matrix

G = runpy.run_path(str(Path(__file__).with_name('mesh_primitives.py')))
B = G['Builder']; OUT = G['OUT']; RECORDS = []
sys.path.insert(0, str(Path(__file__).parent))
import craft_quality93 as craft

# These are existing, tested runtime materials, not new material assets.
FAMILIES = {
    'Sky92Stone': (.69, .67, .58, 1),
    'Sky92Edge': (.80, .78, .67, 1),
    'Sky92Copper': (.45, .33, .19, 1),
    'Sky92Wood': (.43, .30, .18, 1),
    'Sky92Leaf': (.34, .46, .22, 1),
    'InteriorStem': (.22, .17, .10, 1),
}
for name, colour in FAMILIES.items():
    G['FAMILIES'].append(name); G['COLOURS'][name] = colour
    m = bpy.data.materials.new('M_' + name); m.diffuse_color = colour
    m.use_nodes = True
    shader = m.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = colour
    shader.inputs['Roughness'].default_value = .38 if name == 'Sky92Copper' else .62
    if name == 'Sky92Copper': shader.inputs['Metallic'].default_value = .88
    G['MATERIALS'].append(m)

STONE = 'Sky92Stone'; EDGE = 'Sky92Edge'; METAL = 'Sky92Copper'
WOOD = 'Sky92Wood'; LEAF = 'Sky92Leaf'; BARK = 'InteriorStem'

def tint(c, gain):
    return tuple(min(.95, max(.015, v * gain)) for v in c[:3]) + (1,)

def tube(b, points, radii, family=BARK, sides=12, colour=None, ribs=0):
    b.tube(points, radii, family, sides, colour, ribs)

def ring(b, radius, z, thickness, family=METAL, sides=80, colour=None):
    points = [(radius * math.cos(i * math.tau / sides), radius * math.sin(i * math.tau / sides), z) for i in range(sides + 1)]
    tube(b, points, [thickness] * len(points), family, 8, colour)

def bezier(a, b, c, d, steps=12):
    a, b, c, d = map(Vector, (a, b, c, d))
    return [a * (1-t)**3 + b * 3*t*(1-t)**2 + c * 3*t*t*(1-t) + d*t**3 for t in (j/steps for j in range(steps+1))]

def leaf(b, origin, direction, length, width, roll, colour):
    """A curved, cupped blade with a raised midrib; sixteen visible triangles."""
    axis = Vector(direction).normalized()
    ref = Vector((0, 0, 1)) if abs(axis.z) < .88 else Vector((0, 1, 0))
    across = axis.cross(ref).normalized()
    up = across.cross(axis).normalized()
    across, up = across * math.cos(roll) + up * math.sin(roll), up * math.cos(roll) - across * math.sin(roll)
    origin = Vector(origin); vertices = []; faces = []
    for j in range(5):
        t = j/4
        half = max(.002, math.sin(t*math.pi)**.8 * width/2)
        for s in [-1, 0, 1]:
            ridge = length * (.095 * math.sin(t*math.pi) - .09*t*t)
            ridge += abs(s) * length * .065 * math.sin(math.pi*t)
            vertices.append(tuple(origin + axis*t*length + across*s*half + up*ridge))
    for j in range(4):
        for k in range(2):
            a = j*3+k; faces.append((a, a+1, a+4, a+3))
    b.mesh(vertices, faces, LEAF, colour)

def foliage_spray(b, start, direction, length, rng, density=6):
    start = Vector(start); axis = Vector(direction).normalized()
    side = axis.cross(Vector((0,0,1))).normalized()
    end = start + axis*length + Vector((0,0,.12))
    pts = bezier(start, start+axis*length*.32+Vector((0,0,.06)), end-axis*length*.25, end, 5)
    tube(b, pts, [.009*(1-j/6)+.001 for j in range(6)], BARK, 6)
    for j in range(1, density+1):
        t = j/(density+1)
        base = start.lerp(end, t)
        for sign in [-1, 1]:
            direction = axis*.30 + side*sign*.88 + Vector((0,0,rng.uniform(-.30,.55)))
            ll = rng.uniform(.17,.29) * (.8 + .2*math.sin(t*math.pi))
            shade = rng.uniform(.82,1.23)
            c = tint((.33, .46, .20, 1), shade)
            leaf(b, base, direction, ll, ll*rng.uniform(.35,.53), rng.uniform(-.70,.70), c)
    leaf(b, end, axis+Vector((0,0,.10)), .22, .085, rng.uniform(-.5,.5), (.39,.51,.25,1))

def tree(seed, height=5.1, spread=2.2, lean=.16):
    rng = random.Random(seed); b = B()
    # Buttress roots terminate at the ground. Main stem is visibly sinuous,
    # with two unequal leaders instead of a cone beneath stacked leaf discs.
    trunk = []
    for j in range(17):
        t = j/16
        trunk.append((lean*math.sin(t*3.6)+.12*t, .07*math.sin(t*5.4), t*height*.84))
    radii = [.175*(1-j/18)**1.24+.012 for j in range(17)]
    tube(b, trunk, radii, BARK, 22, (.24,.18,.11,1), .065)
    for k in range(7):
        a = k*math.tau/7 + rng.uniform(-.15,.15)
        r = rng.uniform(.34,.52)
        pts = [(r*math.cos(a),r*math.sin(a),.009),(.20*math.cos(a),.20*math.sin(a),.07),(0,0,.42)]
        tube(b, pts, [.008,.05,.09], BARK, 10, (.22,.17,.10,1), .06)
    # Strong asymmetry, negative spaces, and staggered forks make the crown
    # readable at a walking-camera distance, not only in a close-up render.
    for branch in range(11):
        az = branch*2.399963 + rng.uniform(-.24,.24)
        base_z = height*(.30 + branch*.037)
        radial = Vector((math.cos(az), math.sin(az), 0))
        root = Vector((lean*math.sin(base_z/height*3.6)+.12*base_z/height, .03, base_z))
        reach = spread * rng.uniform(.62,1.04) * (1-.025*branch)
        tip = root + radial*reach + Vector((0,0, height*rng.uniform(.23,.40)))
        pts = bezier(root, root+radial*.25+Vector((0,0,.46)), tip-radial*.46-Vector((0,0,.15)), tip, 13)
        thick = .071*(1-branch*.045)
        tube(b, pts, [thick*(1-j/14)**1.25+.006 for j in range(14)], BARK, 14, (.27,.20,.12,1), .045)
        for twig in range(8):
            t = .30 + twig*.089
            anchor = pts[min(12,int(t*13))]
            sign = -1 if twig%2 else 1
            angle = az + sign*rng.uniform(.60,1.30)
            direction = Vector((math.cos(angle),math.sin(angle),rng.uniform(.2,.65))).normalized()
            twig_len = rng.uniform(.54,.88) * (1-.24*t)
            tip2 = anchor+direction*twig_len
            secondary = bezier(anchor,anchor+direction*twig_len*.25+Vector((0,0,.12)),tip2-Vector((0,0,.05)),tip2,6)
            tube(b, secondary, [.023*(1-j/7)**1.2+.002 for j in range(7)], BARK, 8, (.26,.19,.11,1))
            for shoot in range(4):
                aa = angle + (-1 if shoot%2 else 1)*rng.uniform(.42,1.04)
                dd = Vector((math.cos(aa),math.sin(aa),rng.uniform(-.18,.72)))
                foliage_spray(b,secondary[2+shoot],dd,rng.uniform(.27,.46),rng,5)
            foliage_spray(b,tip2,direction,rng.uniform(.32,.46),rng,5)
        foliage_spray(b,tip,radial+Vector((0,0,.5)),.40,rng,6)
    # A light vertical leader prevents a flattened umbrella silhouette.
    for k in range(9):
        a = k*2.3999; p = Vector(trunk[-4 + k%4])
        d = Vector((math.cos(a)*.55,math.sin(a)*.55,.78))
        for j in range(3):foliage_spray(b,p+d*(j*.14),d,.40,rng,6)
    return b

def fluted_planter():
    b = B()
    profile=[(0,0),(.65,0),(.69,.035),(.70,.075),(.65,.115),(.64,.15),(.71,.20),(.79,.43),(.87,.65),(.88,.70)]
    b.lathe(profile, family=STONE, segments=96)
    # Carved lobes change the silhouette; each flute remains a broad curve.
    vv=[];ff=[];levels=[(.64,.16),(.72,.27),(.79,.43),(.84,.58),(.87,.67)]
    n=192
    for radius,z in levels:
        for k in range(n):
            a=k*math.tau/n; rr=radius+.025*math.cos(a*24)
            vv.append((rr*math.cos(a),rr*math.sin(a),z))
    for j in range(len(levels)-1):
        for k in range(n):ff.append((j*n+k,j*n+(k+1)%n,(j+1)*n+(k+1)%n,(j+1)*n+k))
    b.mesh(vv,ff,EDGE)
    b.lathe([(.87,.65),(.94,.70),(.96,.75),(.94,.80),(.87,.81),(.80,.79),(.79,.75),(.79,.70)],family=EDGE,segments=128)
    ring(b,.904,.694,.017);ring(b,.948,.764,.009)
    b.lathe([(0,.681),(.80,.681),(.80,.695),(0,.695)],family='Soil',segments=80,colour=(.13,.105,.075,1))
    ring(b,.681,.097,.012)
    # Bronze inlay garlands, quiet and large enough to read from the path.
    for medallion in range(12):
        a=medallion*math.tau/12
        pts=[]
        for k in range(17):
            aa=a-.19+k*.38/16;z=.59-.065*math.sin(k*math.pi/16)
            pts.append((.859*math.cos(aa),.859*math.sin(aa),z))
        tube(b,pts,[.0065]*17,METAL,7)
        p=(.853*math.cos(a),.853*math.sin(a),.57)
        # Domed studs at the garland junctions, integral to the bronze casting.
        for dz in [0,.025]:
            tube(b,[(p[0]*.99,p[1]*.99,p[2]+dz),(p[0]*1.025,p[1]*1.025,p[2]+dz)],[.017,.010],METAL,10)
    return b

def bentwood_chair():
    b=B();wood=(.43,.30,.18,1)
    # Four flared legs with foot ferrules and mortise rings.
    for sx in [-1,1]:
        for sy in [-1,1]:
            p=(sx*.22,sy*.19,.015);q=(sx*.18,sy*.16,.445)
            tube(b,[p,(sx*.205,sy*.18,.20),q],[.017,.019,.025],WOOD,12,wood)
            tube(b,[p,(sx*.218,sy*.189,.065)],[.019,.019],METAL,12)
    b.lathe([(0,.420),(.250,.420),(.263,.435),(.264,.466),(.252,.478),(.224,.478),(.219,.460),(0,.460)],family=WOOD,segments=72,colour=wood)
    # Actual woven cane seat, with alternating crossings and an open centre.
    for axis in [0,1]:
        for i in range(-13,14):
            offset=i*.0156;half=math.sqrt(max(0,.222**2-offset**2))
            if half<.02:continue
            pts=[]
            for j in range(17):
                q=-half+2*half*j/16
                z=.468+(.002 if axis else -.002)*math.cos(q/.0156*math.pi)
                pts.append((offset,q,z) if axis else (q,offset,z))
            tube(b,pts,[.004]*17,'Cloth',4,(.63,.52,.33,1))
    # Continuous steam-bent back loop, curved in both plan and elevation.
    pts=bezier((-.192,.17,.41),(-.30,.26,.82),(-.22,.29,.985),(0,.29,.987),18)
    pts+=bezier((0,.29,.987),(.22,.29,.985),(.30,.26,.82),(.192,.17,.41),18)[1:]
    tube(b,pts,[.024]*len(pts),WOOD,14,wood)
    for i in range(-4,5):
        x=i*.043;top=.933-.30*x*x
        pts=bezier((x,.22,.545),(x*.92,.29,.68),(x*.92,.30,.85),(x,.279,top),12)
        tube(b,pts,[.010]*len(pts),WOOD,9,tint(wood,1.12))
    for z in [.573,.884]:
        pts=[(-.195+j*.39/20,.258+.019*math.sin(j*math.pi/20),z+.015*math.sin(j*math.pi/20)) for j in range(21)]
        tube(b,pts,[.012]*21,WOOD,10,wood)
    pts=[(.195*math.cos(k*math.tau/48),.167*math.sin(k*math.tau/48),.215) for k in range(49)]
    tube(b,pts,[.012]*49,WOOD,10,wood)
    return b

def cafe_set():
    b=B()
    # Recessed stone insert, raised bronze bead, turned pedestal and cast feet.
    b.lathe([(0,.728),(.55,.728),(.581,.742),(.588,.772),(.579,.79),(0,.79)],family=EDGE,segments=112,colour=(.82,.79,.68,1))
    ring(b,.582,.778,.009);ring(b,.563,.731,.012)
    b.lathe([(0,.08),(.11,.08),(.13,.13),(.08,.20),(.055,.27),(.053,.58),(.086,.66),(.14,.69),(.17,.72),(0,.72)],family=METAL,segments=48)
    for k in range(3):
        a=k*math.tau/3
        def p(r,z):return (r*math.cos(a),r*math.sin(a),z)
        pts=bezier(p(.065,.29),p(.24,.19),p(.21,.025),p(.43,.033),18)
        tube(b,pts,[.034-j*.0009 for j in range(19)],METAL,12)
        foot=B();foot.box((.42,0,.019),(.10,.07,.037),METAL,.012);craft.merge(b,foot,yaw=a)
    for x,yaw in [(-1.01,-math.pi/2),(1.02,math.pi/2)]:craft.merge(b,bentwood_chair(),(x,0,0),yaw)
    craft.cup(b,-.20,-.15,.808,(.76,.77,.69,1),'Ceramic',METAL)
    craft.cup(b,.24,.17,.808,(.45,.64,.59,1),'Ceramic',METAL)
    # Linen napkin with seams; a slender vase and one quiet foliage sprig.
    craft.drape(b,(.05,-.04,.801),.23,.32,(.63,.58,.45,1),'Cloth',.015)
    b.lathe([(0,0),(.055,0),(.064,.015),(.054,.11),(.028,.20),(.030,.23),(.024,.23),(.022,.20)],(.01,.31,.795),'Ceramic',36,(.55,.68,.62,1))
    rng=random.Random(951)
    foliage_spray(b,(.01,.31,.965),(.15,.1,1),.24,rng,4)
    return b

def urn():
    b=B()
    b.lathe([(0,0),(.30,0),(.33,.025),(.33,.07),(.28,.10),(.25,.17),(.31,.22),(.40,.36),(.44,.55),(.41,.68),(.35,.73),(.39,.76),(.39,.81),(.33,.82),(.30,.76),(.29,.70)],family=EDGE,segments=96)
    ring(b,.332,.071,.010);ring(b,.384,.797,.011)
    for side in [-1,1]:
        pts=bezier((side*.37,0,.64),(side*.65,0,.86),(side*.68,0,.39),(side*.40,0,.39),25)
        tube(b,pts,[.026]*26,METAL,12)
    b.lathe([(0,.719),(.30,.719),(.30,.725),(0,.725)],family='Soil',segments=48)
    rng=random.Random(958)
    for k in range(11):
        a=k*2.399;d=Vector((math.cos(a),math.sin(a),rng.uniform(.8,1.7))).normalized()
        root=Vector((0,0,.74));height=rng.uniform(.55,.95)
        pts=bezier(root,root+Vector((0,0,.30)),root+d*height,root+d*height+Vector((0,0,-.07)),10)
        tube(b,pts,[.012*(1-j/12)+.002 for j in range(11)],BARK,8)
        for j in range(3,10):
            aa=a+(-1 if j%2 else 1)*.92
            direction=(math.cos(aa),math.sin(aa),.15)
            ll=rng.uniform(.19,.30)
            leaf(b,pts[j],direction,ll,ll*.39,rng.uniform(-.4,.4),tint((.30,.43,.22,1),rng.uniform(.8,1.2)))
    return b

def emit(name, b, description, collision, extra=None):
    bounds=[[min(v[i] for v in b.v) for i in range(3)],[max(v[i] for v in b.v) for i in range(3)]]
    used=[G['FAMILIES'][i] for i in sorted(set(b.m))]
    # Preserve exactly the established Sky92 → Unreal axis convention.
    b.v=[(x,-y,z) for x,y,z in b.v];b.f=[tuple(reversed(f)) for f in b.f]
    G['export'](name,b,description)
    record=dict(G['CATALOG'][-1]);record.update({
        'used_materials':used,'ue_bounds_cm':[[v*100 for v in side] for side in bounds],
        'dimensions_m':[bounds[1][i]-bounds[0][i] for i in range(3)],
        'origin':'ground contact at local (0,0,0); +Z up; metre authoring, UE centimetres',
        'collision_guidance':collision,
        'material_paths':{family:('/Game/EndlessWorld/Materials/Quality93Final/M_' if family in FAMILIES or family in ['Timber','Bronze','Cloth','Ceramic','Paper','Paint'] else '/Game/EndlessWorld/Materials/M_')+family for family in used},
    })
    if extra:record.update(extra)
    RECORDS.append(record)
    return bpy.data.objects['SM_'+name]

objs=[]
objs.append(emit('Craft95CourtyardTreeA',tree(9517,5.2,2.18,.21),
    'Asymmetric courtyard tree; tapering buttress roots, eleven curved scaffold branches, forked shoots and individually cupped leaves. Original deterministic geometry.',
    'Keep canopy collision disabled. Existing trunk/planter blockers may be retained; minimum walking clearance below outer foliage approximately 1.8 m.'))
objs.append(emit('Craft95CourtyardTreeB',tree(9541,4.8,1.86,-.17),
    'Second independent courtyard tree silhouette with a narrower crown and differently placed forks; no stacked flat leaf clusters.',
    'Keep canopy collision disabled. Existing trunk/planter blockers may be retained.'))
objs.append(emit('Craft95CourtyardPlanter',fluted_planter(),
    'Fluted carved limestone bowl with rolled coping, bronze beading and twelve cast garland inlays. Recessed open soil bed.',
    'Optional simple cylinder blocker radius 0.96 m, height 0.81 m. Do not use triangle collision for the foliage tree.',
    {'tree_root_offset_m':.697,'soil_surface_z_m':.695}))
objs.append(emit('Craft95CafeTableSet',cafe_set(),
    'Two bentwood cafe chairs with individually woven cane seats, rounded stone and bronze pedestal table, cast feet, coffee cups, saucers, napkin and a vase sprig.',
    'Decorative placement only outside the walking corridor. Optional blocker for table radius .59 m and chairs footprint .62 x .65 m; no automatic large combined convex collision.'))
objs.append(emit('Craft95CourtyardUrn',urn(),
    'Classical stone urn with curved bronze handles, open soil and individually curved leafy stems; small counterpart to the courtyard planters.',
    'Optional cylinder blocker radius .44 m, height .82 m; handles and foliage need no collision.'))

# A tidy source-library presentation, separate from each FBX local origin.
for ob,location in zip(objs,[(0,0,0),(6.8,0,0),(0,-5.0,0),(4.0,-5.0,0),(7.4,-5.0,0)]):ob.location=location
# Preview the actual authored vertex colours. The runtime Sky92 finishes
# decode display-space vertex colours, while older kit families do not.
for material in G['MATERIALS']:
    material.use_nodes=True;nodes=material.node_tree.nodes;links=material.node_tree.links
    shader=nodes.get('Principled BSDF');vc=nodes.new('ShaderNodeVertexColor');vc.layer_name='Color'
    if material.name.startswith('M_Sky92'):
        gamma=nodes.new('ShaderNodeGamma');gamma.inputs['Gamma'].default_value=2.2
        links.new(vc.outputs['Color'],gamma.inputs['Color']);links.new(gamma.outputs['Color'],shader.inputs['Base Color'])
    else:links.new(vc.outputs['Color'],shader.inputs['Base Color'])
scene=bpy.context.scene
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24
scene.render.resolution_x=1500;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
scene.world.color=(.25,.25,.25)
scene.view_settings.view_transform='AgX'
for name,loc,energy,size in [('Craft95Key',(1,-5,10),1600,7),('Craft95Fill',(8,5,8),1300,6)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=energy;data.shape='DISK';data.size=size
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=loc
    ob.rotation_euler=(Vector((3,-1,1.5))-ob.location).to_track_quat('-Z','Y').to_euler()
camdata=bpy.data.cameras.new('Craft95Review');cam=bpy.data.objects.new('Craft95Review',camdata);scene.collection.objects.link(cam)
cam.location=(12,-17,10);cam.rotation_euler=(Vector((3.1,-1.4,2.0))-cam.location).to_track_quat('-Z','Y').to_euler()
camdata.type='ORTHO';camdata.ortho_scale=17.0;scene.camera=cam
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(Path(tempfile.gettempdir())/'craft95-review.png')
(OUT/'Blender').mkdir(exist_ok=True)
bpy.context.preferences.filepaths.save_version=0
blend_path=OUT/'Blender/Craft95.blend';bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
manifest={
    'revision':95,'purpose':'Original high-detail foreground craft assets; standalone additions only.',
    'generator':'SourceArt/Scripts/build_craft95.py','blender_version':bpy.app.version_string,
    'coordinate_contract':'Metres in Blender, +Z up. Geometry reflected in Y before FBX; import convert_scene_unit=True, scale 1 produces centimetres matching ue_bounds_cm.',
    'assets':RECORDS,'total_triangles':sum(x['triangles'] for x in RECORDS),
    'source_blend':{'file':'Blender/Craft95.blend','sha256':hashlib.sha256(blend_path.read_bytes()).hexdigest()},
    'validation':{'finite_vertices':all(all(math.isfinite(c) for v in ob.data.vertices for c in v.co) for ob in objs),
        'nonempty_assets':all(x['triangles']>0 for x in RECORDS),
        'runtime_import':'NOT_RUN','runtime_visual_acceptance':'NOT_RUN'},
}
(OUT/'craft95-assets.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
print('CRAFT95_MANIFEST_READY',manifest['total_triangles'],flush=True)
if '--render-review' in sys.argv:bpy.ops.render.render(write_still=True)
