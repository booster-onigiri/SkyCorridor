from pathlib import Path
import hashlib,json,numpy as np
def fingerprint(mesh):
 h=hashlib.sha256()
 def feed(collection,prop,n,dtype):
  a=np.empty(n,dtype=dtype);collection.foreach_get(prop,a);h.update(a.tobytes());return a
 feed(mesh.vertices,'co',len(mesh.vertices)*3,np.float32)
 feed(mesh.loops,'vertex_index',len(mesh.loops),np.int32)
 mi=feed(mesh.polygons,'material_index',len(mesh.polygons),np.int32)
 h.update(json.dumps([(int(i),mesh.materials[i].name) for i in sorted(set(mi))]).encode())
 col=mesh.color_attributes.get('Color')
 if col:feed(col.data,'color',len(col.data)*4,np.float32)
 for uv in mesh.uv_layers:feed(uv.data,'uv',len(uv.data)*2,np.float32)
 return h.hexdigest()
