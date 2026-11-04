# Creates the Cyber Unload Station and Cyber Load Station Blueprints: children of the vanilla Truck Station with their cargo
# ports turned one way and more storage. Blueprint inherited components can only be changed in the editor, see README.
# UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script=<path to this file> -unattended -nullrhi
import unreal

PARENT = '/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation'
CONTENT = '/CyberWagonFirmware'
STORAGE_SLOTS = 240

# Input0 is the fuel port (tagged "Fuel"), which stays an input on both stations
STATIONS = [
    ('Build_CyberWagonUnload', 'Cyber Unload Station',
     'Unloads resources from docked vehicles.\r\n\r\nHas 240 inventory slots and 4 output ports.\r\n\r\n'
     'Transfers up to 120 stacks per minute from docked vehicles.\r\n'
     'Always refuels vehicles if it has access to a matching fuel type.',
     ['Input1', 'Input2'], unreal.FactoryConnectionDirection.FCD_OUTPUT),
    ('Build_CyberWagonLoad', 'Cyber Load Station',
     'Loads resources into docked vehicles.\r\n\r\nHas 240 inventory slots and 4 input ports.\r\n\r\n'
     'Transfers up to 120 stacks per minute to docked vehicles.\r\n'
     'Always refuels vehicles if it has access to a matching fuel type.',
     ['Output0', 'Output1'], unreal.FactoryConnectionDirection.FCD_INPUT),
]


def create_station(name, display_name, description, ports, direction):
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
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    station = unreal.get_default_object(blueprint.generated_class())
    station.set_editor_property('mStorageInventorySize', STORAGE_SLOTS)
    station.set_editor_property('mDisplayName', unreal.Text(display_name))
    station.set_editor_property('mDescription', unreal.Text(description))
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
        raise RuntimeError(f'{name}: could not save')
    unreal.log(f'CWFU created {path}')


for station in STATIONS:
    create_station(*station)
