# Creates the Cyber Unload Station and Cyber Load Station Blueprints: children of the vanilla Truck Station with their cargo
# ports turned one way and more storage. Blueprint inherited components can only be changed in the editor, see README.
# Also creates the patches that repaint the turned ports, whose markings are part of the vanilla station's mesh.
# UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script=<path to this file> -unattended -nullrhi
import glob
import os
import tempfile
import unreal

PARENT = '/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation'
CONTENT = '/CyberWagonFirmware'
PATCH_CONTENT = f'{CONTENT}/PortPatch'
STORAGE_SLOTS = 240

# name: (linear color, glow)
PATCH_MATERIALS = {
    'MI_PortPatch_Panel': ((0.03, 0.035, 0.04), 0.0),
    'MI_PortPatch_Grey': ((0.07, 0.065, 0.055), 0.0),
    'MI_PortPatch_Light': ((0.55, 0.55, 0.52), 0.0),
    'MI_PortPatch_GreenTab': ((0.14, 0.26, 0.09), 0.0),
    'MI_PortPatch_YellowTab': ((0.45, 0.37, 0.12), 0.0),
    'MI_PortPatch_Teal': ((0.06, 0.85, 0.7), 3.0),
    'MI_PortPatch_Amber': ((1.0, 0.7, 0.3), 3.0),
}

# Material of each mesh slot, for each kind of port
OUTPUT_LOOK = {'Plate': 'MI_PortPatch_Light', 'Tab': 'MI_PortPatch_GreenTab', 'Side': 'MI_PortPatch_Panel',
               'Apron': 'MI_PortPatch_Light', 'Mark': 'MI_PortPatch_Teal'}
INPUT_LOOK = {'Plate': 'MI_PortPatch_Grey', 'Tab': 'MI_PortPatch_YellowTab', 'Side': 'MI_PortPatch_Panel',
              'Apron': 'MI_PortPatch_Light', 'Mark': 'MI_PortPatch_Amber'}

# Patch shape, in cm from the port. X points out of the station, Y across the port and Z up. The belt connection is well
# behind the frame and low in it. Measured from screenshots, as the starter project only has a placeholder for the mesh.
# A patch has to cover the other kind of port, and the plate on an output port's frame is further out than an input's.
OUTPUT_PLATE_X = (55.0, 120.0) # patches an input's frame
INPUT_PLATE_X = (60.0, 135.0) # patches an output's frame
PLATE_HALF_WIDTH = 42.0
OUTPUT_TAB_X = (88.0, 133.0)
INPUT_TAB_X = (94.0, 136.0)
TAB_WIDTH = 28.0
PLATE_Z = (176.0, 184.0)
INPUT_TAB_Z = (168.0, 184.0) # an output's frame has green tabs that would show under the patch
# The panel on the outer face of each side of the frame, below the hazard sign
SIDE_X = (60.0, 146.0)
OUTPUT_SIDE_Y = (104.0, 110.0)
INPUT_SIDE_Y = (96.0, 110.0) # an output's frame has a fin with a marked inner face, which this has to enclose
SIDE_Z = (41.0, 112.0)
# The apron is below the frame, so only the lower port of each pair has one
APRON_X = (110.0, 148.0)
APRON_HALF_WIDTH = 38.0
APRON_Z = (-26.0, -14.0)
MARK_OFFSET = 0.3 # keeps the marks above the surface they're on
MARK_WIDTH = 3.0

# Input0 is the fuel port (tagged "Fuel"), which stays an input on both stations
STATIONS = [
    ('Build_CyberWagonUnload', 'Cyber Unload Station',
     'Unloads resources from docked vehicles.\r\n\r\nHas 240 inventory slots and 4 output ports.\r\n\r\n'
     'Transfers up to 120 stacks per minute from docked vehicles.\r\n'
     'Always refuels vehicles if it has access to a matching fuel type.',
     ['Input1', 'Input2'], unreal.FactoryConnectionDirection.FCD_OUTPUT, 'Input1', 'SM_PortPatch_Output', OUTPUT_LOOK),
    ('Build_CyberWagonLoad', 'Cyber Load Station',
     'Loads resources into docked vehicles.\r\n\r\nHas 240 inventory slots and 4 input ports.\r\n\r\n'
     'Transfers up to 120 stacks per minute to docked vehicles.\r\n'
     'Always refuels vehicles if it has access to a matching fuel type.',
     ['Output0', 'Output1'], unreal.FactoryConnectionDirection.FCD_INPUT, 'Output0', 'SM_PortPatch_Input', INPUT_LOOK),
]


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError(f'{asset.get_name()}: could not save')


def load_or_create(name, asset_class, factory):
    asset = unreal.load_asset(f'{PATCH_CONTENT}/{name}')
    return asset or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATCH_CONTENT, asset_class, factory)


def create_patch_materials():
    library = unreal.MaterialEditingLibrary
    parent = load_or_create('M_PortPatch', unreal.Material, unreal.MaterialFactoryNew())
    parent.set_editor_property('two_sided', False)
    library.delete_all_material_expressions(parent)
    color = library.create_material_expression(parent, unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'Color')
    glow = library.create_material_expression(parent, unreal.MaterialExpressionScalarParameter)
    glow.set_editor_property('parameter_name', 'Glow')
    emissive = library.create_material_expression(parent, unreal.MaterialExpressionMultiply)
    library.connect_material_expressions(color, '', emissive, 'A')
    library.connect_material_expressions(glow, '', emissive, 'B')
    library.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(parent)
    save(parent)

    materials = {}
    for name, (rgb, glow_amount) in PATCH_MATERIALS.items():
        instance = load_or_create(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        instance.set_editor_property('parent', parent)
        library.set_material_instance_vector_parameter_value(instance, 'Color', unreal.LinearColor(*rgb, 1.0))
        library.set_material_instance_scalar_parameter_value(instance, 'Glow', glow_amount)
        save(instance)
        materials[name] = instance
    return materials


def facing(quad, normal):
    """Winds a quad so that it faces normal. Unreal's triangles are wound so that (b - a) x (c - a) points into the mesh."""
    a, b, c = quad[:3]
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    cross = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    return quad[::-1] if sum(cross[i] * normal[i] for i in range(3)) > 0 else quad


def box(xs, ys, zs):
    (x0, x1), (y0, y1), (z0, z1) = xs, ys, zs
    return [
        facing([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
        facing([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1)),
        facing([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)),
        facing([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0)),
        facing([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], (-1, 0, 0)),
        facing([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
    ]


def bar(start, end, to_point, normal=(0, 0, 1)):
    """A flat strip from start to end, in 2D. to_point places the 2D points on the surface it's on, which faces normal."""
    (a0, b0), (a1, b1) = start, end
    length = ((a1 - a0) ** 2 + (b1 - b0) ** 2) ** 0.5
    offset_a, offset_b = -(b1 - b0) / length * MARK_WIDTH / 2, (a1 - a0) / length * MARK_WIDTH / 2
    return [facing([to_point(a0 + offset_a, b0 + offset_b), to_point(a1 + offset_a, b1 + offset_b),
                    to_point(a1 - offset_a, b1 - offset_b), to_point(a0 - offset_a, b0 - offset_b)], normal)]


def chevron(base_x, tip_x, to_point):
    """An arrow pointing out of the station, with its open side towards the station"""
    return (bar((base_x, -15.0), (base_x, 15.0), to_point) + bar((base_x, -15.0), (tip_x, -26.0), to_point)
            + bar((base_x, 15.0), (tip_x, 26.0), to_point))


def frame_quads(is_output):
    """The plate on top of the frame with its two tabs and mark, and a marked panel on each side of the frame"""
    on_plate = lambda x, y: (x, y, PLATE_Z[1] + MARK_OFFSET)
    if is_output:
        plate_x, tab_x, tab_z, side_y = OUTPUT_PLATE_X, OUTPUT_TAB_X, PLATE_Z, OUTPUT_SIDE_Y
        plate_mark = chevron(110.0, 94.0, on_plate)
    else:
        plate_x, tab_x, tab_z, side_y = INPUT_PLATE_X, INPUT_TAB_X, INPUT_TAB_Z, INPUT_SIDE_Y
        plate_mark = [quad for y in (-19.0, 0.0, 19.0) for quad in bar((95.0, y), (113.0, y), on_plate)]
    quads = {
        'Plate': box(plate_x, (-PLATE_HALF_WIDTH, PLATE_HALF_WIDTH), PLATE_Z),
        'Tab': box(tab_x, (-PLATE_HALF_WIDTH - TAB_WIDTH, -PLATE_HALF_WIDTH), tab_z)
            + box(tab_x, (PLATE_HALF_WIDTH, PLATE_HALF_WIDTH + TAB_WIDTH), tab_z),
        'Side': [],
        'Mark': plate_mark,
    }
    for sign in (-1.0, 1.0):
        quads['Side'] += box(SIDE_X, sorted(sign * y for y in side_y), SIDE_Z)
        on_side = lambda x, z, y=sign * (side_y[1] + MARK_OFFSET): (x, y, z)
        outwards = (0, sign, 0)
        if is_output:
            # Bracket pointing out of the station
            corners = [(120.0, 100.0), (136.0, 84.0), (136.0, 66.0), (120.0, 50.0)]
            quads['Mark'] += [quad for start, end in zip(corners, corners[1:]) for quad in bar(start, end, on_side, outwards)]
        else:
            quads['Mark'] += [quad for z in (62.0, 78.0, 94.0) for quad in bar((86.0, z), (100.0, z), on_side, outwards)]
    return quads


def apron_quads(is_output):
    """The plate on the apron under a lower port"""
    on_apron = lambda x, y: (x, y, APRON_Z[1] + MARK_OFFSET)
    quads = {'Apron': box(APRON_X, (-APRON_HALF_WIDTH, APRON_HALF_WIDTH), APRON_Z)}
    if is_output:
        quads['Mark'] = chevron(138.0, 123.0, on_apron)
    return quads


def create_patch_mesh(name, look, slots, materials):
    # Imported from a file, since a mesh built from Python has no normals and so gets no sunlight
    lines = ['o patch']
    corner_count = 0
    for slot, quads in slots.items():
        lines.append(f'usemtl {slot}')
        for quad in quads:
            # The import mirrors Y. That also turns Unreal's winding into the one this file format expects.
            lines += [f'v {x} {-y} {z}' for x, y, z in quad]
            lines.append('f ' + ' '.join(str(corner_count + i) for i in (1, 2, 3, 4)))
            corner_count += 4
    file_path = os.path.join(tempfile.gettempdir(), f'{name}.obj')
    with open(file_path, 'w') as file:
        file.write('\n'.join(lines))

    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    options.set_editor_property('automated_import_should_detect_type', False)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', file_path)
    task.set_editor_property('destination_path', PATCH_CONTENT)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    task.set_editor_property('options', options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    os.remove(file_path)

    mesh = unreal.load_asset(f'{PATCH_CONTENT}/{name}')
    if not mesh:
        raise RuntimeError(f'{name}: could not import')
    mesh.set_editor_property('static_materials', [
        unreal.StaticMaterial(material_interface=materials[look[str(slot.material_slot_name)]], material_slot_name=slot.material_slot_name)
        for slot in mesh.get_editor_property('static_materials')])
    save(mesh)
    return mesh


def add_port_patches(blueprint, patches):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    templates = {}
    for handle in handles:
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        templates[str(library.get_variable_name(data))] = library.get_object_for_blueprint(data, blueprint)

    for patch_name, port, mesh in patches:
        patch = templates.get(patch_name)
        if not patch:
            handle, fail_reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
                parent_handle=handles[0], new_class=unreal.StaticMeshComponent, blueprint_context=blueprint))
            if not subsystem.rename_subobject(handle, unreal.Text(patch_name)):
                raise RuntimeError(f'{blueprint.get_name()}: could not add {patch_name}: {fail_reason}')
            patch = library.get_object_for_blueprint(subsystem.k2_find_subobject_data_from_handle(handle), blueprint)
        patch.set_editor_property('mobility', templates['FGColoredInstanceMeshProxy'].get_editor_property('mobility'))
        patch.set_editor_property('static_mesh', mesh)
        patch.set_editor_property('relative_location', templates[port].get_editor_property('relative_location'))
        patch.set_editor_property('relative_rotation', templates[port].get_editor_property('relative_rotation'))
        # Only paint, so belts and players must not hit it
        patch.set_collision_profile_name('NoCollision')
        patch.set_editor_property('generate_overlap_events', False)
        patch.set_editor_property('can_ever_affect_navigation', False)


def create_station(name, display_name, description, ports, direction, lower_port, patch_mesh_name, look, materials):
    path = f'{CONTENT}/{name}'
    blueprint = unreal.load_asset(path)
    if not blueprint:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.load_asset(PARENT).generated_class())
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, CONTENT, unreal.Blueprint, factory)

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    remaining = set(ports)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        port = str(library.get_variable_name(data))
        if port in remaining:
            # For an inherited component this creates the Blueprint's own override of the parent's template
            library.get_object_for_blueprint(data, blueprint).set_editor_property('mDirection', direction)
            remaining.remove(port)
    if remaining:
        raise RuntimeError(f'{name}: ports not found: {remaining}')
    is_output = look is OUTPUT_LOOK
    frame_mesh = create_patch_mesh(patch_mesh_name, look, frame_quads(is_output), materials)
    apron_mesh = create_patch_mesh(f'{patch_mesh_name}_Apron', look, apron_quads(is_output), materials)
    add_port_patches(blueprint, [(f'{port}_Patch', port, frame_mesh) for port in ports]
                     + [(f'{lower_port}_Apron', lower_port, apron_mesh)])
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    station = unreal.get_default_object(blueprint.generated_class())
    station.set_editor_property('mStorageInventorySize', STORAGE_SLOTS)
    station.set_editor_property('mDisplayName', unreal.Text(display_name))
    station.set_editor_property('mDescription', unreal.Text(description))
    save(blueprint)
    unreal.log(f'CWFU created {path}')


# Importing over an existing mesh would keep its old normals, and deleting a mesh from Python crashes the commandlet, so
# the files have to go before anything loads them
patch_folder = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Content', 'PortPatch')
for file_name in glob.glob(os.path.join(patch_folder, 'SM_PortPatch_*.uasset')):
    os.remove(file_name)

patch_materials = create_patch_materials()
for station in STATIONS:
    create_station(*station, patch_materials)
