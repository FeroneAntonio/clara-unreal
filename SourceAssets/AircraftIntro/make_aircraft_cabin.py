"""Original TNES aircraft cabin. Offline Blender; cm coordinates, no downloaded meshes."""
import bpy, math, os, json, sys
from mathutils import Vector
ROOT=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT=os.path.join(ROOT,'SourceAssets','AircraftIntro');os.makedirs(OUT,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01

def material(name,color,rough=.6,metal=0):
 m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal;return m
fabric=material('TNES_SeatFabric',(0.075,.13,.17),.88)
polymer=material('TNES_IvoryPolymer',(.68,.68,.63),.48)
shell=material('TNES_SeatShell',(.055,.065,.073),.46)
metal=material('TNES_BrushedMetal',(.31,.34,.35),.3,.85)
seam=material('TNES_Stitching',(.17,.23,.25),.9)
tray=material('TNES_Tray',(.43,.45,.44),.6)
carpet=material('TNES_Carpet',(.04,.055,.07),.95)
paper=material('TNES_CVPaper',(.94,.94,.90),.91)
ink=material('TNES_CVInk',(.005,.005,.005),1);ink.node_tree.nodes.get('Principled BSDF').inputs['Specular IOR Level'].default_value=0
parts=[]
def finish(obj,mat):
 obj.data.materials.append(mat);parts.append(obj)
 if obj.type=='MESH':
  for p in obj.data.polygons:p.use_smooth=True
 return obj

def soft(name,loc,size,mat,bevel=2,rot=(0,0,0)):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=size;o.rotation_euler=rot;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 mod=o.modifiers.new('Manufactured rounded edges','BEVEL');mod.width=bevel;mod.segments=6
 bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
 n=o.modifiers.new('Weighted surface normals','WEIGHTED_NORMAL');n.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=n.name)
 return finish(o,mat)
def tube(name,points,radius,mat):
 c=bpy.data.curves.new(name,'CURVE');c.dimensions='3D';c.resolution_u=18;c.bevel_depth=radius;c.bevel_resolution=3;s=c.splines.new('POLY');s.points.add(len(points)-1)
 for p,v in zip(s.points,points):p.co=(*v,1)
 o=bpy.data.objects.new(name,c);bpy.context.collection.objects.link(o);bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.convert(target='MESH');o.select_set(False);return finish(o,mat)
def oval_ring(name,profiles,mat):
 # A rounded, recessed injection-moulded window bezel, not an overlay frame.
 verts=[];faces=[];N=128
 for y,rx,rz in profiles:
  for i in range(N):
   t=2*math.pi*i/N;verts.append((rx*math.cos(t),y,136+rz*math.sin(t)))
 for j in range(len(profiles)-1):
  for i in range(N):a=j*N+i;b=j*N+(i+1)%N;faces.append((a,b,b+N,a+N))
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o);return finish(o,mat)
# Window wall is a genuine opening; its surrounding panel is a continuous curved surface.
verts=[];faces=[];N=128
for ring in range(2):
 for i in range(N):
  t=2*math.pi*i/N;dx=math.cos(t);dz=math.sin(t)
  if ring==0:x=30*dx;z=136+46*dz
  else:
   fac=min(235/max(abs(dx),1e-6),(250-136 if dz>0 else 136-12)/max(abs(dz),1e-6));x=dx*fac;z=136+dz*fac
  y=130-.0011*(z-112)**2;verts.append((x,y,z))
for i in range(N):a=i;b=(i+1)%N;faces.append((a,b,b+N,a+N))
me=bpy.data.meshes.new('Curved aircraft sidewall');me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new('Curved aircraft sidewall with oval aperture',me);bpy.context.collection.objects.link(o);finish(o,polymer)
oval_ring('Recessed oval window moulding',[(124.8,31,47),(122.6,30.4,46.4),(121.8,28.5,44.4),(123.5,25.2,40.2),(128.3,23.6,38.2),(135,23.4,38)],polymer)
oval_ring('Window dark gasket',[(134.7,23.45,38.05),(135.2,23.15,37.75)],shell)
# Inner window shade rail, handle and curved sill.
soft('Shade handle',(0,121,181),(15,3,2),polymer,.8)
soft('Window sill',(0,120,86),(48,13,5),polymer,2)
soft('Cabin carpet',(20,0,-2),(440,270,4),carpet,1)
soft('Opposite cabin sidewall',(20,-136,120),(440,8,240),polymer,3)
soft('Overhead ceiling',(20,0,248),(440,270,8),polymer,4)
for x in [-170,-60,50,160]:
 tube('Ceiling panel seam',[(x,-135,237),(x,-65,247),(x,65,247),(x,123,226)],.2,seam)
 soft('Reading light fascia',(x,82,231),(28,20,3),polymer,1)
 soft('Air vent inset',(x,84,228.8),(9,7,1.4),shell,.6)
# Three passenger rows. Rounded shells, upholstered cushions, bolsters, actual tray and belts.
for row in [-110,0,110,220]:
 for col in [-52,10,72]:
  soft('Moulded seat back',(row-25,col,82),(13,54,80),shell,5,rot=(0,-.11,0))
  soft('Upholstered seat back',(row-18,col,84),(12,47,62),fabric,5.3,rot=(0,-.11,0))
  soft('Cushioned headrest',(row-25,col,119),(13,42,21),fabric,5,rot=(0,-.11,0))
  for side in [-1,1]:
   soft('Headrest side wing',(row-21,col+side*21,118),(12,8,23),fabric,3.7,rot=(0,0,-side*.18))
   soft('Soft armrest',(row+1,col+side*28,65),(48,7,8),shell,3)
   soft('Armrest aluminium support',(row-12,col+side*28,53),(5,4,22),metal,1)
   tube('Upholstery piping',[(row-10,col+side*20,58),(row-14,col+side*22,77),(row-19,col+side*21,105)],.10,seam)
  soft('Shaped seat cushion',(row+3,col,47),(52,49,13),fabric,5.2)
  soft('Seat base moulding',(row,col,36),(46,47,13),shell,3)
  for side in [-1,1]:soft('Seat leg',(row+side*15,col,19),(5,36,31),metal,1.8,rot=(0,side*.15,0))
  tube('Seat belt',[(row+6,col-22,54),(row+14,col-4,54.5),(row+9,col+20,54)],1.2,shell)
  soft('Seat belt buckle',(row+12,col-2,55),(4,5,1.5),metal,.4)
# Real deployed tray on the seat in front of the POV.
soft('Deployed tray table',(53,72,77),(59,44,2.8),tray,3)
for y in [55,89]:tube('Tray folding arm',[(83,y,88),(75,y,77),(34,y,76)],.9,metal)
# Printed A4 CV, honest junior application; no invented qualifications.
soft('Printed A4 curriculum',(49,72,79),(29.7,21,.13),paper,.07,rot=(0,0,.035))
def print_line(text,x,y,size):
 c=bpy.data.curves.new('CV print','FONT');c.body=text;c.size=size;c.extrude=0;c.align_x='LEFT';o=bpy.data.objects.new('Printed CV '+text,c);bpy.context.collection.objects.link(o);o.location=(94-x,y,79.12);o.rotation_euler=(0,0,-math.pi/2);bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.convert(target='MESH');o.select_set(False);finish(o,ink)
print_line('CURRICULUM VITAE',36.5,81,1.7)
print_line('TNES / NAPOLI',41,81,1.3)
print_line('Candidatura junior',45.5,81,1.2)
print_line('Nome: candidato TNES',50,81,1.2)
# A camera looking along +X has its right side on -Y in Blender. Keep the print readable.
for obj in parts:
 if obj.name.startswith('Printed CV '):obj.location.y=-63
 else:
  obj.location.y*=-1;obj.scale.y*=-1
# Join the authored cabin into one mesh: source curves remain fully baked/portable.
bpy.ops.object.select_all(action='DESELECT')
for p in parts:p.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();cabin=bpy.context.object;cabin.name='SM_TNES_AircraftCabin'
bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
# Explicit marker sockets survive FBX axis conversion with the mesh.
for name,loc in [('Eye',(0,-72,122)),('CV',(49,-72,79)),('RightWindow',(0,-130,136)),('Ahead',(100,-72,122))]:
 e=bpy.data.objects.new('SOCKET_'+name,None);bpy.context.collection.objects.link(e);e.location=loc;e.parent=cabin;e.empty_display_size=1;e.select_set(True)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,'TNES_AircraftCabin.fbx'),use_selection=True,object_types={'MESH','EMPTY'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_space_transform=False,add_leaf_bones=False,path_mode='AUTO')
# Outside view is a separate UV plane: never covers the cabin interior itself.
bpy.ops.object.select_all(action='DESELECT');verts=[(100,-180,36),(-100,-180,36),(-100,-180,236),(100,-180,236)];me=bpy.data.meshes.new('Aerial photograph UV surface');me.from_pydata(verts,[],[(0,1,2,3)]);me.update();p=bpy.data.objects.new('SM_TNES_AircraftView',me);bpy.context.collection.objects.link(p)
uv=me.uv_layers.new(name='UVMap')
for loop,coord in zip(uv.data,[(-.43,-.5),(1.57,-.5),(1.57,1.5),(-.43,1.5)]):loop.uv=coord
p.select_set(True);bpy.context.view_layer.objects.active=p
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,'TNES_AircraftView.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,add_leaf_bones=False)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'TNES_AircraftCabin.blend'))
# Small offline preview, labelled separately from Unreal QA.
for obj in list(bpy.data.objects):
 if obj.type=='EMPTY':obj.hide_render=True
photo=os.path.join(ROOT,'Content','Data','Intro','Vesuvius_Aerial.jpg')
if os.path.exists(photo):
 image=bpy.data.images.load(photo);m=bpy.data.materials.new('Aerial source photograph preview');m.use_nodes=True;n=m.node_tree.nodes;n.clear();o=n.new('ShaderNodeOutputMaterial');em=n.new('ShaderNodeEmission');tex=n.new('ShaderNodeTexImage');tex.image=image;tex.extension='EXTEND';m.node_tree.links.new(tex.outputs['Color'],em.inputs['Color']);m.node_tree.links.new(em.outputs[0],o.inputs[0]);p.data.materials.append(m);p.scale.x=3*image.size[0]/image.size[1];p.scale.z=3;p.location.z=-359
scene.render.engine='CYCLES';scene.cycles.samples=4;scene.cycles.max_bounces=2;scene.cycles.diffuse_bounces=1;scene.cycles.glossy_bounces=1;scene.cycles.use_denoising=True
scene.world.color=(.35,.35,.35)
for pos,power,size in [((30,-85,215),1800,170),((0,-50,150),600,100),((0,-170,150),1600,80)]:
 bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power*100;l.data.shape='DISK';l.data.size=size;l.rotation_euler=(Vector((30,-60,80))-l.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(0,-72,122));camera=bpy.context.object;scene.camera=camera;camera.data.lens=28;camera.data.clip_start=.1;camera.data.clip_end=10000
scene.render.resolution_x=960;scene.render.resolution_y=600;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast'
for name,target in [('preview-cv',Vector((49,-72,79))),('preview-window',Vector((0,-130,136)))]:
 if '--no-preview' in sys.argv:continue
 if '--preview-window-only' in sys.argv and name=='preview-cv':continue
 camera.data.lens=55.4 if name=='preview-cv' else 20.7;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=os.path.join(OUT,name+'.png');bpy.ops.render.render(write_still=True)
print('TNES_AIRCRAFT_ASSETS_READY',OUT)
