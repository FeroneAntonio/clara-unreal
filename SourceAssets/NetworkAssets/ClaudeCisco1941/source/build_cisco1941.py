# Cisco 1941 ISR - procedural build for CLARA (Blender 5.2).
# Usage:
#   Blender -b --factory-startup --python build_cisco1941.py -- <out_dir> <real|compat> [render]
# Units: 1 Blender unit = 1 cm (scene scale_length 0.01). Origin: centre X/Y, bottom of feet Z=0.
# Blender axes: front bezel +Y, rear panel -Y. After the standard FBX import into Unreal
# (Y is negated) the front faces UE -Y and the rear UE +Y, like network-devices.json.
#   real   : physically correct layout (seen from the rear, ports are on the right).
#   compat : layout mirrored so the sockets match the CURRENT network-devices.json X values;
#            every part and every text is still built un-mirrored (readable).
import bpy, bmesh, math, sys, os, json
from collections import defaultdict
from mathutils import Vector
from mathutils.geometry import tessellate_polygon
import numpy as np

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = os.path.abspath(argv[0]) if argv else os.getcwd()
VARIANT = argv[1] if len(argv) > 1 else 'real'
DO_RENDER = 'render' in argv
S = 1.0 if VARIANT == 'real' else -1.0
TEX = os.path.join(OUT, 'textures')
os.makedirs(TEX, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.unit_settings.system = 'METRIC'
sc.unit_settings.scale_length = 0.01
sc.unit_settings.length_unit = 'CENTIMETERS'
COLL = bpy.data.collections.new('Cisco1941_Parts')
sc.collection.children.link(COLL)

# ---------------------------------------------------------------- dimensions (cm)
W2 = 17.15          # half width  (343 mm)
YR, YF = -14.9, 14.9  # rear lip / front brow (292 mm chassis + bezel relief = 298 mm, as JSON)
ZT = 8.9            # 89 mm incl. feet
ZB = 0.35           # chassis bottom (feet 3.5 mm)
YP = -14.6          # rear panel plane (recessed 3 mm inside the cover lip)
LIP = 0.55          # cover lip around the rear panel

def rx(u):   # rear: "viewer right" coordinate -> world X
    return S * u
def fx(u):   # front: "viewer right" coordinate -> world X
    return -S * u

# ---------------------------------------------------------------- textures (tileable, numpy)
rng = np.random.default_rng(1941)
def blur(a, sx, sy):
    F = np.fft.fft2(a)
    ky = np.fft.fftfreq(a.shape[0])[:, None]
    kx = np.fft.fftfreq(a.shape[1])[None, :]
    return np.real(np.fft.ifft2(F * np.exp(-2 * np.pi ** 2 * ((kx * sx) ** 2 + (ky * sy) ** 2))))
def norm01(a):
    return (a - a.min()) / (a.max() - a.min() + 1e-9)
def save_img(name, rgb, noncolor=True):
    h, w = rgb.shape[:2]
    img = bpy.data.images.new(name, w, h, alpha=False)
    rgba = np.ones((h, w, 4), np.float32); rgba[..., :3] = rgb
    img.pixels.foreach_set(rgba.ravel())
    path = os.path.join(TEX, name + '.png')
    img.filepath_raw = path; img.file_format = 'PNG'; img.save()
    img.filepath = path
    if noncolor: img.colorspace_settings.name = 'Non-Color'
    return img
def height_to_normal(h, k, flip_g=False):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    n = np.dstack([-dx * k, -dy * k, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    if flip_g: n[..., 1] *= -1
    return n * 0.5 + 0.5

N = 1024
pc = norm01(blur(rng.standard_normal((N, N)), 1.6, 1.6)) * 0.8 + norm01(blur(rng.standard_normal((N, N)), 5, 5)) * 0.2
T_PC_N = save_img('T_C1941_PowderCoat_N', height_to_normal(pc, 2.0))
save_img('T_C1941_PowderCoat_N_DirectX', height_to_normal(pc, 2.0, flip_g=True))
pc_r = 0.60 + (norm01(blur(rng.standard_normal((N, N)), 30, 30)) - 0.5) * 0.10
T_PC_R = save_img('T_C1941_PowderCoat_R', np.dstack([pc_r] * 3))
br = norm01(blur(rng.standard_normal((N, N)), 60, 0.6)) * 0.8 + norm01(blur(rng.standard_normal((N, N)), 4, 0.5)) * 0.2
br_r = 0.27 + br * 0.16
T_BR_R = save_img('T_C1941_BrushedZinc_R', np.dstack([br_r] * 3))
T_BR_N = save_img('T_C1941_BrushedZinc_N', height_to_normal(br, 1.2))
save_img('T_C1941_BrushedZinc_N_DirectX', height_to_normal(br, 1.2, flip_g=True))

# ---------------------------------------------------------------- materials
MATS = {}
def mk(name, col, metal=0.0, rough=0.5, rtex=None, ntex=None, nstr=0.3, emit=None):
    m = bpy.data.materials.new('M_C1941_' + name)
    try: m.use_nodes = True
    except Exception: pass
    nt = m.node_tree; b = nt.nodes.get('Principled BSDF')
    b.inputs['Base Color'].default_value = (*col, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    m.diffuse_color = (*col, 1); m.metallic = metal; m.roughness = rough
    if rtex:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = rtex; t.location = (-500, 0)
        nt.links.new(t.outputs['Color'], b.inputs['Roughness'])
    if ntex:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = ntex; t.location = (-700, -300)
        nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = nstr; nm.location = (-300, -300)
        nt.links.new(t.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
    if emit:
        b.inputs['Emission Color'].default_value = (*emit, 1)
        b.inputs['Emission Strength'].default_value = 0.0
    MATS[name] = m
    return m

COVER = mk('CoverPowderCoat', (0.020, 0.021, 0.023), 0.0, 0.6, rtex=T_PC_R, ntex=T_PC_N, nstr=0.12)
BEZEL = mk('BezelGloss', (0.010, 0.010, 0.011), 0.0, 0.16)
SATIN = mk('BezelSatin', (0.012, 0.012, 0.013), 0.0, 0.38)
GRILLE = mk('GrilleMetal', (0.030, 0.052, 0.060), 0.65, 0.42)
GRBACK = mk('GrilleBacking', (0.004, 0.010, 0.012), 0.0, 0.80)
ZINC = mk('RearPanelZinc', (0.60, 0.61, 0.62), 1.0, 0.33, rtex=T_BR_R, ntex=T_BR_N, nstr=0.07)
SHIELD = mk('JackShieldNickel', (0.66, 0.66, 0.64), 1.0, 0.30)
CAVITY = mk('CavityBlack', (0.013, 0.013, 0.014), 0.0, 0.62)
PLASTIC = mk('PlasticBlack', (0.018, 0.018, 0.019), 0.0, 0.48)
ROCKER = mk('RockerBlack', (0.014, 0.014, 0.015), 0.0, 0.30)
GOLD = mk('ContactGold', (1.00, 0.76, 0.33), 1.0, 0.22)
STEEL = mk('ScrewSteel', (0.62, 0.62, 0.60), 1.0, 0.36)
SCRBLK = mk('ScrewBlackOxide', (0.035, 0.035, 0.037), 0.8, 0.42)
WHITE = mk('PrintWhite', (0.80, 0.80, 0.78), 0.0, 0.45)
INK = mk('PrintInk', (0.012, 0.013, 0.014), 0.0, 0.55)
YEL = mk('LabelYellow', (0.86, 0.62, 0.05), 0.0, 0.45)
BLUE = mk('LabelBlue', (0.20, 0.47, 0.76), 0.0, 0.45)
BEIGE = mk('LabelBeige', (0.80, 0.72, 0.52), 0.0, 0.55)
LED = mk('LedGreen', (0.05, 0.20, 0.08), 0.0, 0.20, emit=(0.1, 1.0, 0.25))
LEDA = mk('LedAmber', (0.22, 0.12, 0.02), 0.0, 0.20, emit=(1.0, 0.55, 0.05))
RUBBER = mk('Rubber', (0.014, 0.014, 0.015), 0.0, 0.88)

# ---------------------------------------------------------------- geometry helpers
class Fr:
    """Local frame: u = right and v = up as seen from outside, w = depth INTO the chassis."""
    def __init__(s, O, U, V, W):
        s.O, s.U, s.V, s.W = Vector(O), Vector(U), Vector(V), Vector(W)
    def p(s, u, v, w=0.0):
        return s.O + s.U * u + s.V * v + s.W * w
    def out(s):
        return -s.W
    def sub(s, u, v, w=0.0):
        return Fr(s.p(u, v, w), s.U, s.V, s.W)

def F_rear(x, z, y=YP): return Fr((x, y, z), (1, 0, 0), (0, 0, 1), (0, 1, 0))
def F_front(x, z, y):    return Fr((x, y, z), (-1, 0, 0), (0, 0, 1), (0, -1, 0))
def F_top(x, y, z):      return Fr((x, y, z), (-1, 0, 0), (0, -1, 0), (0, 0, -1))
def F_side(sign, y, z):  return Fr((sign * W2 - sign * 0.01, y, z), (0, sign, 0), (0, 0, 1), (-sign, 0, 0))
def F_bottom(x, y):      return Fr((x, y, ZB), (1, 0, 0), (0, -1, 0), (0, 0, 1))

def newell(pts):
    n = Vector((0, 0, 0))
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n.x += (a.y - b.y) * (a.z + b.z); n.y += (a.z - b.z) * (a.x + b.x); n.z += (a.x - b.x) * (a.y + b.y)
    return n

def area2(loop):
    return sum(loop[i][0] * loop[(i + 1) % len(loop)][1] - loop[(i + 1) % len(loop)][0] * loop[i][1] for i in range(len(loop)))
def ccw(loop):
    return list(loop) if area2(loop) > 0 else list(reversed(loop))

def rr(w, h, r, seg=6, cx=0.0, cy=0.0):
    r = max(0.0, min(r, w / 2, h / 2))
    if r < 1e-6:
        return [(cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2), (cx - w / 2, cy + h / 2), (cx - w / 2, cy - h / 2)]
    pts = []
    for (x, y, a0) in ((w / 2 - r, -h / 2 + r, -90), (w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180)):
        for i in range(seg + 1):
            a = math.radians(a0 + 90 * i / seg)
            pts.append((cx + x + r * math.cos(a), cy + y + r * math.sin(a)))
    return pts

def circle(r, n=24, cx=0.0, cy=0.0):
    return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]

def shift(loop, du, dv):
    return [(u + du, v + dv) for (u, v) in loop]

class Part:
    def __init__(s, name):
        s.name = name; s.bm = bmesh.new(); s.mats = []
        s.curv = s.bm.faces.layers.int.new('curv')
    def mi(s, m):
        if m not in s.mats: s.mats.append(m)
        return s.mats.index(m)
    def face(s, pts, m, want=None, curved=False):
        pts = [Vector(p) for p in pts]
        if want is not None and newell(pts).dot(want) < 0:
            pts.reverse()
        try:
            f = s.bm.faces.new([s.bm.verts.new(p) for p in pts])
        except ValueError:
            return None
        f.material_index = s.mi(m); f[s.curv] = 1 if curved else 0
        return f

def fill(P, F, loops, m, w=0.0):
    """Planar face(s) bounded by loops[0] with holes loops[1:], facing out of frame F."""
    loops = [ccw(l) for l in loops]
    flat = [pt for l in loops for pt in l]
    tris = tessellate_polygon([[(u, v, 0.0) for (u, v) in l] for l in loops])
    want = F.out()
    for t in tris:
        P.face([F.p(*flat[i], w) for i in t], m, want)

def walls(P, F, loop, w0, w1, m, inward=False, curved=False):
    loop = ccw(loop); n = len(loop)
    for i in range(n):
        (u0, v0), (u1, v1) = loop[i], loop[(i + 1) % n]
        du, dv = u1 - u0, v1 - v0
        if abs(du) + abs(dv) < 1e-9: continue
        want = F.U * dv - F.V * du
        if inward: want = -want
        seg_curved = curved and math.hypot(du, dv) < 0.35
        P.face([F.p(u0, v0, w0), F.p(u1, v1, w0), F.p(u1, v1, w1), F.p(u0, v0, w1)], m, want, seg_curved)

def prism(P, F, w, h, r, d, m_face, m_side=None, c=0.0, holes=(), seg=6, cu=0.0, cv=0.0, back=False):
    """Rounded-rect slab: front face at w=0 (optional rounded chamfer c), sides to depth d.
    holes: list of dict(pts, depth or None, wall, bottom or None)."""
    m_side = m_side or m_face
    if c > 0:
        r = max(r, c + 0.01)
        K = 3; rings = []
        for k in range(K + 1):
            th = (math.pi / 2) * k / K
            ins = c - c * math.sin(th); ww = c - c * math.cos(th)
            rings.append((rr(w - 2 * ins, h - 2 * ins, r - ins, seg, cu, cv), ww))
        front = rings[0][0]
        for k in range(K):
            la, wa = rings[k]; lb, wb = rings[k + 1]
            la, lb = ccw(la), ccw(lb)
            for i in range(len(la)):
                j = (i + 1) % len(la)
                du, dv = lb[j][0] - lb[i][0], lb[j][1] - lb[i][1]
                want = (F.U * dv - F.V * du).normalized() + F.out()
                P.face([F.p(*la[i], wa), F.p(*la[j], wa), F.p(*lb[j], wb), F.p(*lb[i], wb)], m_side, want, True)
        outer, w_side = rings[-1][0], c
    else:
        front = rr(w, h, r, seg, cu, cv); outer, w_side = front, 0.0
    fill(P, F, [front] + [hh['pts'] for hh in holes], m_face)
    walls(P, F, outer, w_side, d, m_side, curved=r > 0)
    if back:
        fill(P, Fr(F.p(0, 0, d), F.U * -1, F.V, F.W * -1), [[(-u, v) for (u, v) in outer]], m_side)
    for hh in holes:
        dep = hh.get('depth') or d
        walls(P, F, hh['pts'], 0.0, dep, hh['wall'], inward=True)
        if hh.get('bottom') is not None:
            fill(P, F, [hh['pts']], hh['bottom'], dep)

def cyl(P, F, u, v, r, w0, w1, m, seg=24, cap_m=None, knurl=0, curved=True):
    """Cylinder along the frame depth axis; cap at the outermost end (min w)."""
    loop = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        rad = r * (0.92 if (knurl and i % 2) else 1.0)
        loop.append((u + rad * math.cos(a), v + rad * math.sin(a)))
    walls(P, F, loop, min(w0, w1), max(w0, w1), m, curved=curved and not knurl)
    fill(P, F, [loop], cap_m or m, min(w0, w1))

def flat(P, F, loop, m, lift=0.012, holes=()):
    fill(P, F, [loop] + list(holes), m, -lift)

FONT = None
def text(P, F, s, size, m, u=0.0, v=0.0, align='CENTER', rot=0, lift=0.022, bold=0.0):
    cu = bpy.data.curves.new('tmp_text', 'FONT')
    cu.body = s; cu.size = size; cu.align_x = align; cu.align_y = 'CENTER'; cu.offset = bold
    ob = bpy.data.objects.new('tmp_text', cu); sc.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    xs = [vv.co.x for vv in me.vertices] or [0]
    want = F.out()
    for poly in me.polygons:
        pts = []
        for vi in poly.vertices:
            x, y = me.vertices[vi].co.x, me.vertices[vi].co.y
            if rot == 90: x, y = -y, x
            pts.append(F.p(u + x, v + y, -lift))
        P.face(pts, m, want)
    width = max(xs) - min(xs)
    bpy.data.objects.remove(ob); bpy.data.curves.remove(cu); bpy.data.meshes.remove(me)
    return width

def cisco_logo(P, F, u, v, H, m, lift=0.02):
    """Bars + word, total height H, centred on (u, v)."""
    word_size = H * 0.56
    wv = v - H * 0.27
    ww = text(P, F, 'cisco', word_size, m, u, wv, lift=lift, bold=word_size * 0.012)
    pitch = ww / 8.6; bw = pitch * 0.48; hmax = H * 0.50; bv = v + H * 0.24
    for i, k in enumerate((0.40, 0.70, 1.0, 0.70, 0.40, 0.70, 1.0, 0.70, 0.40)):
        hh = hmax * k
        flat(P, F, rr(bw, hh, bw / 2, 4, u + (i - 4) * pitch, bv), m, lift)

def finish(P, smooth_angle=50.0):
    bmesh.ops.remove_doubles(P.bm, verts=P.bm.verts, dist=1e-5)
    me = bpy.data.meshes.new(P.name); P.bm.to_mesh(me); P.bm.free()
    for m in P.mats: me.materials.append(m)
    ob = bpy.data.objects.new(P.name, me); COLL.objects.link(ob)
    post_process(ob, smooth_angle)
    return ob

def post_process(ob, smooth_angle=50.0, curv_from_axis=False):
    """Custom split normals: planar faces keep their own normal (no shading bleed onto
    flat panels or text), curved faces are averaged only with curved neighbours.
    Plus box-projected UVs in world centimetres (25 cm per UV tile)."""
    me = ob.data
    attr = me.attributes.get('curv')
    curved = [bool(attr.data[i].value) for i in range(len(me.polygons))] if (attr and not curv_from_axis) else \
             [max(abs(c) for c in p.normal) < 0.9995 for p in me.polygons]
    if attr and not curv_from_axis:
        # bevel / boolean meshes: also treat non axis-aligned faces as curved
        pass
    cosl = math.cos(math.radians(smooth_angle))
    vf = defaultdict(list)
    for p in me.polygons:
        for vi in p.vertices: vf[vi].append(p.index)
    ln = []
    for p in me.polygons:
        n = p.normal.copy()
        for li in p.loop_indices:
            if not curved[p.index]:
                ln.append(n); continue
            acc = Vector()
            for fi in vf[me.loops[li].vertex_index]:
                q = me.polygons[fi]
                if curved[fi] and q.normal.dot(n) >= cosl:
                    acc += q.normal * max(q.area, 1e-8)
            ln.append(acc.normalized() if acc.length > 1e-9 else n)
    for p in me.polygons: p.use_smooth = True
    me.normals_split_custom_set(ln)
    uv = me.uv_layers.new(name='UVMap') if not me.uv_layers else me.uv_layers[0]
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            a, b = ((co.y, co.z), (co.x, co.z), (co.x, co.y))[ax]
            uv.data[li].uv = (a / 25.0, b / 25.0)
    if attr: me.attributes.remove(me.attributes['curv'])

def bevel_box(name, x0, x1, y0, y1, z0, z1, wfn, width, segs, mat, rear_recess=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = x0 if v.co.x < 0 else x1; v.co.y = y0 if v.co.y < 0 else y1; v.co.z = z0 if v.co.z < 0 else z1
    bw = bm.edges.layers.float.new('bevel_weight_edge')
    for e in bm.edges:
        a, b = e.verts[0].co, e.verts[1].co; d = b - a; mid = (a + b) / 2
        axis = 'XYZ'[max(range(3), key=lambda i: abs(d[i]))]
        e[bw] = wfn(axis, mid)
    if rear_recess:
        inset, depth = rear_recess
        bm.normal_update(); bm.faces.ensure_lookup_table()
        f = [f for f in bm.faces if f.normal.y < -0.9][0]
        bmesh.ops.inset_individual(bm, faces=[f], thickness=inset - 0.03, depth=0.0)
        bmesh.ops.inset_individual(bm, faces=[f], thickness=0.03, depth=0.0)
        nv = list(f.verts)
        for v in nv: v.co.y += depth
        for e in bm.edges:
            if all(v in nv for v in e.verts) or any(v in nv for v in e.verts): e[bw] = 0.0
    me = bpy.data.meshes.new(name + '_src'); bm.to_mesh(me); bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me); COLL.objects.link(ob)
    md = ob.modifiers.new('Bevel', 'BEVEL')
    md.limit_method = 'WEIGHT'; md.width = width; md.segments = segs; md.use_clamp_overlap = True
    md.miter_outer = 'MITER_ARC'
    return ob

def apply_mods(ob):
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    old = ob.data; ob.modifiers.clear(); ob.data = me; me.name = ob.name
    bpy.data.meshes.remove(old)

# ================================================================= BUILD
PORTS = {}   # id -> (world centre of socket face, outward normal)

# ---- cover (matte powder-coated steel) -------------------------------------
def w_cover(axis, mid):
    if axis == 'Y': return 1.0 if mid.z > 4 else 0.3
    if axis == 'Z': return 0.3 if mid.y < 0 else 0.0
    if axis == 'X':
        if mid.y > 0: return 0.0
        return 0.35 if mid.z > 4 else 0.25
    return 0.0
cover = bevel_box('Cover', -W2 + 0.01, W2 - 0.01, YR, 12.6, ZB, ZT - 0.01, w_cover, 1.0, 6, COVER, rear_recess=(LIP, -YR + YP))
apply_mods(cover)
# side ventilation slots (cut into the flat side walls only)
cut = Part('VentCutter')
for sign in (-1, 1):
    for i in range(13):
        y = 3.6 + i * 0.6
        F = Fr((sign * (W2 + 0.3), y, 3.9), (0, sign, 0), (0, 0, 1), (-sign, 0, 0))
        prism(cut, F, 0.26, 3.6, 0.13, 0.95, CAVITY, back=True, seg=5)
cutter = finish(cut)
md = cover.modifiers.new('Vents', 'BOOLEAN'); md.operation = 'DIFFERENCE'; md.object = cutter
md.solver = 'EXACT'; md.material_mode = 'TRANSFER'
apply_mods(cover)
bpy.data.objects.remove(cutter)
# open the rear recess (the zinc panel sits there)
bm = bmesh.new(); bm.from_mesh(cover.data)
kill = [f for f in bm.faces if f.normal.y < -0.99 and abs(f.calc_center_median().y - YP) < 0.02]
bmesh.ops.delete(bm, geom=kill, context='FACES'); bm.to_mesh(cover.data); bm.free()
post_process(cover, 50, curv_from_axis=True)

# ---- front brow + bezel frame (gloss black plastic) -------------------------
def w_brow(axis, mid):
    if axis == 'X':
        if mid.y > 13 and mid.z > 7: return 1.0
        if mid.y > 13: return 0.12
        return 0.0
    if axis == 'Y': return (0.95 / 2.0) if mid.z > 7 else 0.0
    if axis == 'Z': return (0.25 / 2.0) if mid.y > 13 else 0.0
    return 0.0
brow = bevel_box('FrontBrow', -W2, W2, 12.4, YF, 6.3, ZT, w_brow, 2.0, 8, BEZEL)
apply_mods(brow); post_process(brow, 50, curv_from_axis=True)

YFF = 14.65   # front face of the lower bezel
bez = Part('FrontBezel')
FF = F_front(0, (ZB + 6.29) / 2, YFF)
hz = (6.29 - ZB)
win = rr(32.7, 3.4, 0.15, 4, 0.0, (0.85 + 4.25) / 2 - FF.O.z)
prism(bez, FF, 2 * W2, hz, 0.3, YFF - 12.6, BEZEL, c=0.2,
      holes=[dict(pts=win, depth=0.55, wall=BEZEL, bottom=GRBACK)])
# perforated grille (hex), recessed 1.5 mm in the window
gr = Part('FrontGrille')
FG = F_front(0, (0.85 + 4.25) / 2, YFF - 0.15)
hexes = []
hw, hh = 0.44, 0.30
row = 0
zc = -1.6 + 0.24
while zc < 1.6 - 0.22:
    off = 0.26 if row % 2 else 0.0
    xc = -15.9 + off
    while xc < 15.9:
        hexes.append(dict(pts=[(xc + hw / 2, zc), (xc + hw / 4, zc + hh / 2), (xc - hw / 4, zc + hh / 2),
                               (xc - hw / 2, zc), (xc - hw / 4, zc - hh / 2), (xc + hw / 4, zc - hh / 2)],
                          depth=None, wall=GRILLE))
        xc += 0.52
    zc += 0.38; row += 1
prism(gr, FG, 32.7, 3.4, 0.15, 0.06, GRILLE, holes=hexes, seg=4)
# logo window, LEDs, model text
lb = F_front(fx(-13.55), 5.3, YFF)
prism(bez, lb.sub(0, 0, -0.06), 5.2, 1.7, 0.12, 0.07, SATIN, c=0.03)
cisco_logo(bez, lb.sub(0, 0, -0.06), 0.0, 0.0, 1.25, WHITE, 0.012)
for i, lab in enumerate(('SYS', 'ACT', 'POE')):
    fu = -10.1 + i * 0.75
    text(bez, F_front(fx(fu), 5.95, YFF), lab, 0.20, WHITE, bold=0.002)
    prism(bez, F_front(fx(fu), 5.55, YFF - 0.0).sub(0, 0, -0.03), 0.22, 0.11, 0.04, 0.05, LED, c=0.015)
text(bez, F_front(fx(12.35), 5.3, YFF), 'Cisco  1900  Series', 0.50, WHITE, bold=0.004)
bez_ob = finish(bez); gr_ob = finish(gr)

# top glossy logo (embossed-look print on the cover)
top = Part('TopLogo')
cisco_logo(top, F_top(0, -0.5, ZT - 0.01), 0, 0, 3.6, BEZEL, 0.008)
finish(top)

# side screws (flush, Phillips)
scr = Part('CoverScrews')
for sign in (-1, 1):
    for (y, z) in ((-13.4, 1.2), (11.2, 1.2)):
        Fs = F_side(sign, y, z)
        flat(scr, Fs, circle(0.28, 20), SCRBLK, 0.004)
        flat(scr, Fs, rr(0.32, 0.06, 0, 1), CAVITY, 0.006)
        flat(scr, Fs, rr(0.06, 0.32, 0, 1), CAVITY, 0.006)
finish(scr)

# feet
ft = Part('Feet')
for x in (-14.6, 14.6):
    for y in (-11.8, 11.8):
        cyl(ft, F_bottom(x, y), 0, 0, 0.95, 0.0, -ZB, RUBBER, 28)
finish(ft)

# ---- rear panel --------------------------------------------------------------
RP = Part('RearPanel')
PZ0, PZ1 = ZB + LIP, ZT - 0.01 - LIP
PW = 2 * (W2 - 0.01 - LIP)
FR0 = F_rear(0, (PZ0 + PZ1) / 2)
def ph(x, z, w, h, r=0.04):
    """panel hole at world x/z"""
    return dict(pts=rr(w, h, r, 3, x - FR0.O.x, z - FR0.O.z), depth=None, wall=ZINC)
JACK_W, JACK_H = 1.62, 1.42
holes = [
    ph(rx(14.9), 4.15, JACK_W + 0.04, JACK_H + 0.04),        # Gi0/0
    ph(rx(12.0), 1.95, JACK_W + 0.04, JACK_H + 0.04),        # Gi0/1
    ph(rx(9.9), 2.75, JACK_W + 0.04, 3.04),                   # AUX/CONSOLE
    ph(rx(14.9), 2.175, 1.52, 1.64),                          # USB stack
    ph(rx(7.8), 1.96, 0.96, 0.54, 0.08),                      # mini USB
    ph(rx(-4.7), 6.6, 8.2, 1.7, 0.1),                         # power module
    ph(rx(-15.75), 3.4, 0.72, 0.32, 0.12),                    # Kensington slot
]
EH = [(-14.9, -7.5, 'EHWIC 1'), (-7.2, 0.2, None), (0.6, 8.4, 'EHWIC 0')]
for (a, b, _) in EH:
    holes.append(ph(rx((a + b) / 2), 3.75, (b - a) - 0.9, 1.6, 0.05))
prism(RP, FR0, PW, PZ1 - PZ0, 0.0, 0.12, ZINC, holes=holes, seg=1)
# dark interior seen through openings
fill(RP, F_rear(0, (PZ0 + PZ1) / 2, -12.6), [rr(PW, PZ1 - PZ0, 0, 1)], CAVITY)

# rear printing
cisco_logo(RP, FR0, rx(-13.9) - FR0.O.x, 7.15 - FR0.O.z, 1.15, INK, 0.012)
text(RP, F_rear(rx(12.3), 7.65), 'Cisco 1941 Series', 0.30, INK, bold=0.003)
for (a, b, lab) in EH:
    if lab: text(RP, F_rear(rx(a + 0.9), 5.18), lab, 0.20, INK, align='LEFT', bold=0.002)

def rj45_loop(notch):   # notch -1: latch slot at bottom, +1: at top
    W = 0.585; T = 0.36; nw = 0.32; nd = 0.14
    base = [(-W, -T), (-nw, -T), (-nw, -T - nd), (nw, -T - nd), (nw, -T), (W, -T), (W, T), (-W, T)]
    pts = [(u, v + nd / 2) for (u, v) in base]
    if notch > 0: pts = [(u, -v) for (u, v) in pts]
    return ccw(pts)

JF = YP - 0.06    # jack shield face (6 mm proud... 0.6 mm)
def rj45_block(pid_list, x, z, h):
    P = RP; F = F_rear(x, z, JF)
    hs = []
    for (pid, dv, notch) in pid_list:
        hs.append(dict(pts=shift(rj45_loop(notch), 0, dv), depth=1.5, wall=CAVITY, bottom=CAVITY))
    prism(P, F, JACK_W, h, 0.06, 1.9, SHIELD, c=0.03, holes=hs)
    for (pid, dv, notch) in pid_list:
        vc = dv + (-notch) * 0.36 + notch * 0.0 + (0.07 if notch < 0 else -0.07)
        for i in range(8):
            u = (i - 3.5) * 0.102
            a = F.p(u - 0.022, vc + notch * 0.02, 0.30); b = F.p(u + 0.022, vc + notch * 0.02, 0.30)
            c_ = F.p(u + 0.022, vc + notch * 0.22, 1.05); d = F.p(u - 0.022, vc + notch * 0.22, 1.05)
            P.face([a, b, c_, d], GOLD, F.out())
        # small spring tabs on the shield face
        for su in (-0.66, 0.66):
            flat(P, F, rr(0.10, 0.22, 0.02, 2, su, dv), CAVITY, 0.004)
        PORTS[pid] = (F.p(0, dv, 0), F.out())

rj45_block([('Gi0/0', 0.0, -1)], rx(14.9), 4.15, JACK_H)
rj45_block([('Gi0/1', 0.0, -1)], rx(12.0), 1.95, JACK_H)
rj45_block([('Aux', 0.8, 1), ('Console', -0.8, -1)], rx(9.9), 2.75, 3.0)

# USB-A stack
Fu = F_rear(rx(14.9), 2.175, JF)
uh = []
for dv in (0.375, -0.375):
    uh.append(dict(pts=rr(1.22, 0.50, 0.04, 2, 0, dv), depth=1.3, wall=SHIELD, bottom=CAVITY))
prism(RP, Fu, 1.48, 1.60, 0.06, 1.6, SHIELD, c=0.03, holes=uh)
for pid, dv in (('USB1', 0.375), ('USB0', -0.375)):
    prism(RP, Fu.sub(0, dv + 0.105, 0.16), 1.10, 0.19, 0.02, 1.1, PLASTIC)
    for cu_ in (-0.35, -0.12, 0.12, 0.35):
        vb = dv + 0.105 - 0.096
        ln = 0.55 if abs(cu_) > 0.2 else 0.45
        RP.face([Fu.p(cu_ - 0.05, vb, 0.25), Fu.p(cu_ + 0.05, vb, 0.25), Fu.p(cu_ + 0.05, vb, 0.25 + ln), Fu.p(cu_ - 0.05, vb, 0.25 + ln)], GOLD, -Fu.V)
    PORTS[pid] = (Fu.p(0, dv, 0), Fu.out())

# mini-USB (USB console)
Fm = F_rear(rx(7.8), 1.96, JF)
mini = ccw([(-0.345, 0.155), (-0.345, 0.02), (-0.25, -0.155), (0.25, -0.155), (0.345, 0.02), (0.345, 0.155)])
prism(RP, Fm, 0.92, 0.50, 0.08, 0.9, SHIELD, c=0.02, holes=[dict(pts=mini, depth=0.7, wall=SHIELD, bottom=CAVITY)])
prism(RP, Fm.sub(0, 0.03, 0.12), 0.48, 0.09, 0.01, 0.55, PLASTIC)
PORTS['USBConsole'] = (Fm.p(0, 0, 0), Fm.out())

# port labels / LEDs
def label(x, z, w, h, mat, txt, tmat, size, rot=0):
    F = F_rear(x, z)
    flat(RP, F, rr(w, h, 0.03, 2), mat, 0.010)
    text(RP, F, txt, size, tmat, rot=rot, lift=0.020, bold=size * 0.01)
def led(x, z, mat=LED):
    prism(RP, F_rear(x, z).sub(0, 0, -0.03), 0.20, 0.11, 0.035, 0.06, mat, c=0.015)
label(rx(9.9), 4.55, 1.20, 0.34, INK, 'AUX', WHITE, 0.22)
label(rx(8.85), 1.08, 3.9, 0.30, BLUE, 'CONSOLE', INK, 0.21)
label(rx(12.0), 2.92, 1.25, 0.32, YEL, 'GE 0/1', INK, 0.20)
label(rx(13.72), 4.15, 0.34, 1.38, YEL, 'GE 0/0', INK, 0.20, rot=90)
for (x, z, t) in ((rx(14.45), 5.12, 'S'), (rx(15.35), 5.12, 'L')):
    led(x, z, LED if t == 'L' else LEDA)
    text(RP, F_rear(x, 5.42), t, 0.18, INK, bold=0.002)
for (x, t) in ((rx(11.45), 'S'), (rx(12.55), 'L')):
    led(x, 3.32, LED if t == 'L' else LEDA)
    text(RP, F_rear(x + 0.25, 3.32), t, 0.16, INK, bold=0.002)
text(RP, F_rear(rx(14.9), 1.12), 'USB', 0.17, INK, bold=0.002)
text(RP, F_rear(rx(13.95), 2.55), '1', 0.16, INK)
text(RP, F_rear(rx(13.95), 1.80), '0', 0.16, INK)
led(rx(6.95), 1.85)
text(RP, F_rear(rx(6.95), 2.17), 'EN', 0.15, INK, bold=0.002)

# power module: black plate, rocker switch, IEC C14 inlet
FP = F_rear(rx(-4.7), 6.6, YP - 0.24)
ux = lambda wx: wx - FP.O.x
iec = ccw([(-1.15, -0.825), (1.15, -0.825), (1.15, 0.425), (0.75, 0.825), (-0.75, 0.825), (-1.15, 0.425)])
iu, iv = ux(rx(-3.1)), 6.75 - 6.6
su, sv = ux(rx(-7.9)), 0.0
prism(RP, FP, 8.6, 2.1, 0.15, 0.36, PLASTIC, c=0.05, holes=[
    dict(pts=shift(iec, iu, iv), depth=1.0, wall=PLASTIC, bottom=CAVITY),
    dict(pts=rr(0.96, 1.52, 0.06, 2, su, sv), depth=0.65, wall=PLASTIC, bottom=CAVITY)])
for (pu, pv) in ((-0.7, -0.22), (0.7, -0.22), (0.0, 0.28)):
    prism(RP, FP.sub(iu + pu, iv + pv, 0.32), 0.18, 0.42, 0.03, 0.7, STEEL, c=0.02)
flat(RP, FP, rr(2.62, 1.95, 0.12, 3, iu, 0.025), ROCKER, 0.008, holes=[shift(iec, iu, iv)])
PORTS['Power'] = (FP.p(iu, iv, 0), FP.out())
# rocker (I up / O down), pivoting about a horizontal axis
Fk = FP.sub(su, sv, 0)
up = lambda v: 0.10 - 0.16 * (v / 0.68)       # front surface depth, upper half (proud)
lo = lambda v: 0.10 + 0.18 * (-v / 0.68)      # lower half (pressed in)
X0, X1 = -0.42, 0.42
RP.face([Fk.p(X0, 0.68, up(0.68)), Fk.p(X1, 0.68, up(0.68)), Fk.p(X1, 0, 0.10), Fk.p(X0, 0, 0.10)], ROCKER, Fk.out())
RP.face([Fk.p(X0, 0, 0.10), Fk.p(X1, 0, 0.10), Fk.p(X1, -0.68, lo(-0.68)), Fk.p(X0, -0.68, lo(-0.68))], ROCKER, Fk.out())
RP.face([Fk.p(X0, 0.68, up(0.68)), Fk.p(X1, 0.68, up(0.68)), Fk.p(X1, 0.68, 0.6), Fk.p(X0, 0.68, 0.6)], ROCKER, Fk.V)
RP.face([Fk.p(X0, -0.68, lo(-0.68)), Fk.p(X1, -0.68, lo(-0.68)), Fk.p(X1, -0.68, 0.6), Fk.p(X0, -0.68, 0.6)], ROCKER, -Fk.V)
for X, sgn in ((X0, -1), (X1, 1)):
    RP.face([Fk.p(X, 0.68, up(0.68)), Fk.p(X, 0, 0.10), Fk.p(X, -0.68, lo(-0.68)), Fk.p(X, -0.68, 0.6), Fk.p(X, 0.68, 0.6)], ROCKER, Fk.U * sgn)
RP.face([Fk.p(-0.035, 0.52, up(0.52) - 0.006), Fk.p(0.035, 0.52, up(0.52) - 0.006), Fk.p(0.035, 0.24, up(0.24) - 0.006), Fk.p(-0.035, 0.24, up(0.24) - 0.006)], WHITE, Fk.out())
ring_o, ring_i = circle(0.13, 20, 0, -0.36), circle(0.085, 20, 0, -0.36)
lo_o = [ccw(ring_o), ccw(ring_i)]
flat_pts = [pt for l in lo_o for pt in l]
for t in tessellate_polygon([[(u, v, 0) for (u, v) in l] for l in lo_o]):
    RP.face([Fk.p(flat_pts[i][0], flat_pts[i][1], lo(flat_pts[i][1]) - 0.006) for i in t], WHITE, Fk.out())
vt = ux(rx(-5.75))
for k, line in enumerate(('100-240V~', '2-1A', '50/60 Hz')):
    text(RP, FP, line, 0.19, WHITE, vt, 0.32 - k * 0.32, bold=0.002)

# EHWIC blank plates with vents and captive thumbscrews; CF slot covers
for (a, b, lab) in EH:
    w = b - a
    Fe = F_rear(rx((a + b) / 2), 3.75, YP - 0.06)
    ncol = int((w - 2.4) / 0.72)
    vents = []
    for row_v in (0.62, 0.22):
        for i in range(ncol):
            vents.append(dict(pts=rr(0.50, 0.20, 0.07, 3, (i - (ncol - 1) / 2) * 0.72, row_v), depth=None, wall=ZINC))
    prism(RP, Fe, w, 2.4, 0.08, 0.1, ZINC, c=0.03, holes=vents)
    for su_ in (-(w / 2 - 0.55), (w / 2 - 0.55)):
        cyl(RP, Fe, su_, 0.0, 0.40, 0.0, -0.03, STEEL, 28)
        cyl(RP, Fe, su_, 0.0, 0.30, -0.03, -0.24, STEEL, 32, knurl=1)
        flat(RP, Fe.sub(0, 0, -0.237), rr(0.36, 0.06, 0.0, 1, su_, 0.0), CAVITY, 0.003)
    if lab == 'EHWIC 1':
        text(RP, Fe, 'SEE MANUAL BEFORE REMOVING', 0.13, INK, 0.0, -0.55, bold=0.0015)
    for sx in (-(w / 2 - 0.18), w / 2 - 0.18):
        for sz in (-0.95, 0.95):
            flat(RP, Fe, circle(0.09, 12, sx, sz), STEEL, 0.006)
for (a, b, lab) in ((-13.6, -9.4, 'CF 1'), (-6.3, -2.1, 'CF 0')):
    Fc = F_rear(rx((a + b) / 2), 1.62, YP - 0.04)
    prism(RP, Fc, b - a, 0.80, 0.05, 0.08, ZINC, c=0.02)
    flat(RP, Fc, rr(b - a - 0.5, 0.56, 0.03, 2), BEIGE, 0.008)
    text(RP, Fc, 'DO NOT REMOVE DURING', 0.13, INK, 0, 0.11, lift=0.016, bold=0.0015)
    text(RP, Fc, 'NETWORK OPERATION', 0.13, INK, 0, -0.12, lift=0.016, bold=0.0015)
    text(RP, F_rear(rx(a - 0.5), 1.62), lab, 0.16, INK, bold=0.002)
text(RP, F_rear(rx(1.4), 1.62), 'ISM/WLAN', 0.15, INK, bold=0.0015)

# ground lug and earth symbol, Kensington lock icon
Fg = F_rear(rx(15.75), 6.4)
cyl(RP, Fg, 0, 0, 0.46, 0.0, -0.04, STEEL, 28)
cyl(RP, Fg, 0, 0, 0.30, -0.04, -0.20, STEEL, 6, curved=False)
for k, wdt in enumerate((0.42, 0.28, 0.14)):
    flat(RP, Fg, rr(wdt, 0.045, 0, 1, 0, 0.78 - k * 0.09), INK, 0.010)
flat(RP, Fg, rr(0.045, 0.20, 0, 1, 0, 0.88), INK, 0.010)
Fl = F_rear(rx(-15.75), 4.05)
flat(RP, Fl, rr(0.26, 0.20, 0.03, 2, 0, -0.06), INK, 0.010)
flat(RP, Fl, circle(0.10, 16, 0, 0.06), INK, 0.010, holes=[circle(0.06, 16, 0, 0.06)])

rp_ob = finish(RP)

# ================================================================= EXPORT
for o in COLL.objects:
    o.data.name = o.name
blend_path = os.path.join(OUT, 'Cisco1941_Claude.blend' if VARIANT == 'real' else 'Cisco1941_Claude_JsonCompat.blend')
fbx_name = 'SM_Cisco1941_Claude.fbx' if VARIANT == 'real' else 'SM_Cisco1941_Claude_JsonCompat.fbx'

# joined export copy
bpy.ops.object.select_all(action='DESELECT')
copies = []
for o in COLL.objects:
    c = o.copy(); c.data = o.data.copy(); sc.collection.objects.link(c); copies.append(c)
bpy.context.view_layer.objects.active = copies[0]
for c in copies: c.select_set(True)
bpy.ops.object.join()
joined = bpy.context.view_layer.objects.active
joined.name = 'SM_Cisco1941'; joined.data.name = 'SM_Cisco1941'
bpy.ops.object.select_all(action='DESELECT'); joined.select_set(True)
tri = joined.modifiers.new('Triangulate', 'TRIANGULATE'); tri.keep_custom_normals = True; tri.quad_method = 'BEAUTY'; tri.min_vertices = 4
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, fbx_name), use_selection=True, apply_unit_scale=True,
                         apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Z', axis_up='Y',
                         mesh_smooth_type='FACE', use_mesh_modifiers=True, use_tspace=True,
                         add_leaf_bones=False, bake_anim=False, path_mode='STRIP', embed_textures=False,
                         object_types={'MESH'})
me = joined.data
bb = [joined.matrix_world @ Vector(c) for c in joined.bound_box]
mn = [min(v[i] for v in bb) for i in range(3)]; mx = [max(v[i] for v in bb) for i in range(3)]
me.calc_loop_triangles()
report = dict(variant=VARIANT, fbx=fbx_name, units='centimeters (1 Blender unit = 1 cm, scale_length 0.01)',
              blender_bounds_min=[round(x, 3) for x in mn], blender_bounds_max=[round(x, 3) for x in mx],
              size_cm=[round(mx[i] - mn[i], 3) for i in range(3)],
              triangles=len(me.loop_triangles), vertices=len(me.vertices),
              materials=[m.name for m in me.materials])
# ports in Unreal coordinates (standard FBX import negates Blender Y)
ports = []
labels = {'Power': ('AC POWER IEC C14', 'power'), 'Aux': ('AUX', 'aux'), 'Console': ('CONSOLE RJ-45', 'console'),
          'USBConsole': ('USB mini-B console', 'usb'), 'Gi0/0': ('GigabitEthernet0/0', 'ethernet'),
          'Gi0/1': ('GigabitEthernet0/1', 'ethernet'), 'USB0': ('USB Type-A 0', 'usb'), 'USB1': ('USB Type-A 1', 'usb')}
for pid in ('Power', 'Aux', 'Console', 'USBConsole', 'Gi0/0', 'Gi0/1', 'USB0', 'USB1'):
    p, n = PORTS[pid]
    ports.append(dict(id=pid, label=labels[pid][0], kind=labels[pid][1],
                      position=[round(p.x, 3), round(-p.y, 3), round(p.z, 3)],
                      normal=[round(n.x, 3), round(-n.y, 3), round(n.z, 3)]))
report['ports_unreal_cm'] = ports
bpy.data.objects.remove(joined)
with open(os.path.join(OUT, 'build-report-%s.json' % VARIANT), 'w') as fh:
    json.dump(report, fh, indent=2)
print('REPORT', json.dumps({k: v for k, v in report.items() if k != 'ports_unreal_cm'}))

# ================================================================= RENDER
def look(cam, target):
    d = Vector(target) - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()

def setup_render():
    sc.render.engine = 'CYCLES'
    try:
        pr = bpy.context.preferences.addons['cycles'].preferences
        pr.compute_device_type = 'METAL'; pr.get_devices()
        for d in pr.devices: d.use = True
        sc.cycles.device = 'GPU'
    except Exception as e:
        print('GPU setup failed', e)
    print('CYCLES DEVICE', sc.cycles.device, [(d.name, d.type, d.use) for d in bpy.context.preferences.addons['cycles'].preferences.devices])
    sc.cycles.samples = 128; sc.cycles.use_denoising = True; sc.cycles.use_adaptive_sampling = True
    sc.render.resolution_x, sc.render.resolution_y = 1600, 1000
    sc.render.film_transparent = False
    sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Medium High Contrast'
    w = bpy.data.worlds.new('Studio'); sc.world = w
    try: w.use_nodes = True
    except Exception: pass
    nt = w.node_tree; bg = nt.nodes.get('Background')
    env = nt.nodes.new('ShaderNodeTexEnvironment')
    env.image = bpy.data.images.load(os.path.join(bpy.utils.system_resource('DATAFILES'), 'studiolights', 'world', 'studio.exr'))
    nt.links.new(env.outputs['Color'], bg.inputs['Color']); bg.inputs['Strength'].default_value = 0.45
    lp = nt.nodes.new('ShaderNodeLightPath'); bg2 = nt.nodes.new('ShaderNodeBackground')
    bg2.inputs['Color'].default_value = (0.36, 0.37, 0.38, 1); bg2.inputs['Strength'].default_value = 1.0
    mix = nt.nodes.new('ShaderNodeMixShader'); out = nt.nodes.get('World Output')
    nt.links.new(lp.outputs['Is Camera Ray'], mix.inputs[0]); nt.links.new(bg.outputs[0], mix.inputs[1])
    nt.links.new(bg2.outputs[0], mix.inputs[2]); nt.links.new(mix.outputs[0], out.inputs['Surface'])
    fl = bpy.data.meshes.new('Floor')
    fl.from_pydata([(-400, -400, 0), (400, -400, 0), (400, 400, 0), (-400, 400, 0)], [], [(0, 1, 2, 3)])
    fo = bpy.data.objects.new('Floor', fl); sc.collection.objects.link(fo)
    fm = bpy.data.materials.new('Floor'); fm.diffuse_color = (0.5, 0.5, 0.5, 1)
    try: fm.use_nodes = True
    except Exception: pass
    pb = fm.node_tree.nodes.get('Principled BSDF')
    pb.inputs['Base Color'].default_value = (0.42, 0.43, 0.44, 1); pb.inputs['Roughness'].default_value = 0.7
    fl.materials.append(fm)
    for name, en, ang in (('Key', 3.0, 16), ('Fill', 0.9, 35), ('Rim', 1.6, 8)):
        ld = bpy.data.lights.new(name, 'SUN'); ld.energy = en; ld.angle = math.radians(ang)
        lo_ = bpy.data.objects.new(name, ld); sc.collection.objects.link(lo_)
    cd = bpy.data.cameras.new('Cam'); cd.clip_start = 1; cd.clip_end = 5000; cd.sensor_width = 36
    cam = bpy.data.objects.new('Cam', cd); sc.collection.objects.link(cam); sc.camera = cam
    return cam

def shot(cam, name, loc, target, lens=50, ortho=None, res=(1600, 1000), samples=128):
    cam.location = Vector(loc); look(cam, target)
    v = Vector(loc) - Vector(target); v.z = 0; v.normalize()
    for lname, yaw, elev in (('Key', 38, 48), ('Fill', -70, 25), ('Rim', 160, 35)):
        a_ = math.radians(yaw); d = Vector((v.x * math.cos(a_) - v.y * math.sin(a_), v.x * math.sin(a_) + v.y * math.cos(a_), 0))
        d.z = math.tan(math.radians(elev)); d.normalize()   # direction towards the light
        bpy.data.objects[lname].rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    if ortho:
        cam.data.type = 'ORTHO'; cam.data.ortho_scale = ortho
    else:
        cam.data.type = 'PERSP'; cam.data.lens = lens
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.cycles.samples = samples
    sc.render.filepath = os.path.join(OUT, 'renders', name + '.png')
    bpy.ops.render.render(write_still=True)

bpy.ops.wm.save_as_mainfile(filepath=blend_path, relative_remap=True)

if DO_RENDER:
    cam = setup_render()
    if VARIANT == 'real':
        shot(cam, '01_fronte_3-4', (-34, 66, 30), (0, 2, 3.4), 50)
        shot(cam, '02_retro_3-4', (36, -64, 28), (1.5, -8, 3.6), 50)
        shot(cam, '03_retro_ortogonale', (0, -120, 4.45), (0, 0, 4.45), ortho=36.5, res=(1800, 640))
        shot(cam, '04_dettaglio_porte', (17.5, -35, 9.5), (11.6, -14.6, 2.9), 100)
        shot(cam, '05_dettaglio_alimentazione', (-12, -36, 14), (-5.5, -14.6, 5.2), 85)
        shot(cam, '06_fronte_ortogonale', (0, 120, 4.45), (0, 0, 4.45), ortho=36.5, res=(1800, 640))
        shot(cam, '07_controllo_riflessi_radente', (46, 22, 13), (0, -2, 4), 55)
    else:
        shot(cam, 'compat_retro_ortogonale', (0, -120, 4.45), (0, 0, 4.45), ortho=36.5, res=(1800, 640), samples=96)
print('DONE', VARIANT)
