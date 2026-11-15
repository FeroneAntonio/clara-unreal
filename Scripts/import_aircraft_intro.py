"""Run once in Unreal Editor/commandlet AFTER other UE jobs; never inside gameplay."""
import unreal,os
PROJECT=unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE=os.path.join(PROJECT,'SourceAssets','AircraftIntro')
DEST='/Game/AircraftIntro'
PHOTO=os.path.join(PROJECT,'Content','Data','Intro','Vesuvius_Aerial.jpg')
tools=unreal.AssetToolsHelpers.get_asset_tools()
def import_file(filename,name,fbx=False):
 assert os.path.isfile(filename),filename
 task=unreal.AssetImportTask();task.set_editor_property('filename',filename);task.set_editor_property('destination_path',DEST);task.set_editor_property('destination_name',name);task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
 if fbx:
  task.set_editor_property('factory',unreal.FbxFactory())
  ui=unreal.FbxImportUI();ui.set_editor_property('import_mesh',True);ui.set_editor_property('import_as_skeletal',False);ui.set_editor_property('import_materials',True);ui.set_editor_property('import_textures',False);ui.set_editor_property('automated_import_should_detect_type',False);ui.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
  data=ui.static_mesh_import_data;data.set_editor_property('combine_meshes',True);data.set_editor_property('generate_lightmap_u_vs',True);data.set_editor_property('auto_generate_collision',False);data.set_editor_property('convert_scene',True);data.set_editor_property('convert_scene_unit',True);data.set_editor_property('force_front_x_axis',False);data.set_editor_property('import_uniform_scale',1.0);task.set_editor_property('options',ui)
 tools.import_asset_tasks([task]);asset=unreal.load_asset(DEST+'/'+name)
 assert asset is not None,(name,task.get_editor_property('imported_object_paths'))
 return asset
cabin=import_file(os.path.join(SOURCE,'TNES_AircraftCabin.fbx'),'SM_TNES_AircraftCabin',True)
view=import_file(os.path.join(SOURCE,'TNES_AircraftView.fbx'),'SM_TNES_AircraftView',True)
photo=import_file(PHOTO,'T_Vesuvius_Aerial')
photo.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);photo.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
weave=None
if os.path.isfile(os.path.join(SOURCE,'FabricWeave.png')):weave=import_file(os.path.join(SOURCE,'FabricWeave.png'),'T_SeatWeave')
def scalar(mat,value,property):
 n=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);unreal.MaterialEditingLibrary.connect_material_property(n,'',property)
def create_material(name,color,rough,metal=0,texture=None,unlit=False):
 mat=unreal.load_asset(DEST+'/'+name)
 if not mat:mat=tools.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
 unreal.MaterialEditingLibrary.delete_all_material_expressions(mat);mat.set_editor_property('two_sided',True)
 if unlit:mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 if texture:
  node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D);node.set_editor_property('texture',texture);node.set_editor_property('parameter_name','AerialPhoto' if unlit else 'Fabric')
  if not unlit:
   uv=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate);uv.set_editor_property('u_tiling',6);uv.set_editor_property('v_tiling',6);unreal.MaterialEditingLibrary.connect_material_expressions(uv,'',node,'UVs')
 else:
  node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);node.set_editor_property('constant',unreal.LinearColor(*color,1))
 unreal.MaterialEditingLibrary.connect_material_property(node,'RGB' if texture else '',unreal.MaterialProperty.MP_EMISSIVE_COLOR if unlit else unreal.MaterialProperty.MP_BASE_COLOR)
 if not unlit:
  scalar(mat,rough,unreal.MaterialProperty.MP_ROUGHNESS);scalar(mat,metal,unreal.MaterialProperty.MP_METALLIC)
  if name=='M_CVInk':scalar(mat,0,unreal.MaterialProperty.MP_SPECULAR)
 unreal.MaterialEditingLibrary.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);return mat
palette={
 'TNES_SeatFabric':create_material('M_SeatFabric',(.075,.13,.17),.86,texture=weave),
 'TNES_IvoryPolymer':create_material('M_IvoryPolymer',(.68,.68,.63),.48),
 'TNES_SeatShell':create_material('M_SeatShell',(.055,.065,.073),.46),
 'TNES_BrushedMetal':create_material('M_BrushedMetal',(.31,.34,.35),.3,.85),
 'TNES_Stitching':create_material('M_Stitching',(.17,.23,.25),.9),
 'TNES_Tray':create_material('M_Tray',(.43,.45,.44),.6),
 'TNES_Carpet':create_material('M_Carpet',(.04,.055,.07),.95),
 'TNES_CVPaper':create_material('M_CVPaper',(.94,.94,.90),.91),
 'TNES_CVInk':create_material('M_CVInk',(.025,.04,.05),.9),
}
slots=cabin.get_editor_property('static_materials')
for index,slot in enumerate(slots):
 for key,mat in palette.items():
  if key in str(slot.material_slot_name):cabin.set_material(index,mat);break
view.set_material(0,create_material('M_TNES_Aerial',(1,1,1),1,texture=photo,unlit=True))
sockets={key:cabin.find_socket(key) for key in ['Eye','CV','RightWindow','Ahead']}
sockets={key:value for key,value in sockets.items() if value is not None}
for key in ['Eye','CV','RightWindow','Ahead']:assert key in sockets,('Missing FBX socket',key,list(sockets))
for key,value in sockets.items():unreal.log('TNES_AIRCRAFT_SOCKET '+key+' '+str(value.relative_location))
e=sockets['Eye'].relative_location;a=sockets['Ahead'].relative_location;r=sockets['RightWindow'].relative_location
cross=(a.x-e.x)*(r.y-e.y)-(a.y-e.y)*(r.x-e.x)
assert cross>0,('Aircraft window must be camera-right, no runtime mirror allowed',cross)

bounds=cabin.get_bounding_box();size=bounds.max-bounds.min
assert 180<size.z<300,('Cabin FBX scale must be centimetres; expected height ~250cm',size)
unreal.EditorAssetLibrary.save_directory(DEST,only_if_is_dirty=False,recursive=True)
unreal.log('TNES_AIRCRAFT_IMPORT_PASS: authored seats, actual aperture, aerial material, four camera sockets; height='+str(size.z))
