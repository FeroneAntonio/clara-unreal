# Re-imports the exported FBX into an empty default scene (metres) and checks scale, normals, UVs, materials.
# Usage: Blender -b --factory-startup --python verify_fbx.py -- <file.fbx>
import bpy, sys, json
from mathutils import Vector
path = sys.argv[sys.argv.index('--') + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=path)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
o = obs[0]; me = o.data
bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
mn = [min(v[i] for v in bb) for i in range(3)]; mx = [max(v[i] for v in bb) for i in range(3)]
me.calc_loop_triangles()
ngons = sum(1 for p in me.polygons if len(p.vertices) > 3)
print('VERIFY', json.dumps(dict(
    objects=len(obs), name=o.name,
    size_m=[round(mx[i] - mn[i], 4) for i in range(3)],
    min_m=[round(x, 4) for x in mn], max_m=[round(x, 4) for x in mx],
    triangles=len(me.loop_triangles), non_triangles=ngons,
    has_custom_normals=me.has_custom_normals, uv_layers=[u.name for u in me.uv_layers],
    materials=len(me.materials))))
