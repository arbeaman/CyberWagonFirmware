# Cyber Wagon (Firmware Update)

A Satisfactory mod that fixes several problems with the vanilla Cyber Wagon through 'firmware updates'.

The fixes include:
   1. The ability to turn left
   2. The elimination of periodic 'jumps' (called the 'SpaceShipFeature')
   3. Fixes for a sticky throttle that periodically caused the car to accelerate to full speed (called 'StickyThrottleFailure' and 'StickThrottleToFloor').
   4. Better acceleration and tire grip: more grip, traction control on all wheels and ABS on all wheels. 0-100 km/h takes about 5 seconds, close to a real Cybertruck.
   5. More cargo space: 120 inventory slots (the vanilla Cyber Wagon has 1).
   6. Self-driving: follows Truck and Universal paths and loads and unloads at Truck Stations, with the same acceleration, top speed, braking and cornering grip as when you drive it yourself.
   7. Two new stations, CyberWagon Unload and CyberWagon Load, with 240 inventory slots. See [Stations](#stations).

## How to get the Cyber Wagon

- **AWESOME Shop → Vehicles → "Cyber Wagon (Firmware Update)"**: 25 FICSIT Coupons. It only appears after you've bought the vanilla Cyber Wagon.
- **Build recipe**: the vanilla Cyber Wagon ingredients (10 Reinforced Iron Plate) plus 2 Circuit Boards, built with the build gun.
- Vanilla Cyber Wagons are not changed.

## Stations

Buying the AWESOME Shop entry also unlocks two stations in the build menu, next to the Truck Station. Any vehicle that can use a Truck Station can use them.

| Station | Cargo ports | Mode | Storage |
|---|---|---|---|
| **CyberWagon Unload** | 4 outputs | Always unloads vehicles | 240 slots |
| **CyberWagon Load** | 4 inputs | Always loads vehicles | 240 slots |

- Both keep the Truck Station's separate fuel input, so they can refuel vehicles.
- The load/unload toggle in the station's UI has no effect.
- Each costs 30 Modular Frame, 40 Rotor, 100 Cable, 10 Reinforced Iron Plate and 2 Circuit Board. That's twice a Truck Station plus a Cyber Wagon (Firmware Update), and it's a fixed value in the code.

## Release Install

The simplest way to install is select the latest **Release** on the right and follow the directions on that page.

The release files require specific versions of Satisfactory Mod Loader (SML) and the game version.  The version required is listed with each release.

## How it works

| File | Role |
|---|---|
| `CWFU_SteeringPatch.cpp` | The left-turn fix. While a left (negative) raw steering input is applied, the vanilla wagon steers its wheels right at twice the normal angle. A negative *smoothed* steering value with no raw input behind it steers left normally. So for the mod's wagon, left input is kept out of the raw input: `AFGWheeledVehicle::MoveRight` is skipped for negative values. Right after `UFGWheeledVehicleMovementComponent::UpdateState`, the mod drives the smoothed steering value toward the player's input using the vehicle's own rise rate. Right steering uses the game's normal path. Only the mod's wagon class is affected. |
| `CWFU_GagPatch.cpp` | Removes two deliberate malfunctions of the vanilla wagon. **Shake:** every 7.5 seconds the Blueprint's `SpaceShipFeature` event kicks the wagon with a large upward impulse at one of its `BumpLoc` corner markers, picked at random; `UPrimitiveComponent::AddImpulseAtLocation` calls aimed at a `BumpLoc` marker are cancelled, other impulses go through. **Sticky throttle:** every ~6 seconds `StickyThrottleFailure` may start `StickThrottleToFloor`, which accelerates the wagon to top speed for 10-20 seconds; both events are blocked in `AActor::ProcessEvent`. |
| `CWFU_TuningPatch.cpp` | Wagons built before the current cargo size load from the save with their old, smaller storage; on `BeginPlay` it is grown to the new size. Storage is never shrunk. |
| `CWFU_StationPatch.cpp` | Keeps the stations in the one mode their ports allow: `AFGBuildableDockingStation::BeginPlay` switches the unload station to unload mode and `SetIsInLoadMode` ignores requests for the other mode. |
| `Scripts/CreateStationBlueprints.py` | Creates the two station Blueprints: children of `Build_TruckStation` with the cargo ports turned one way, 240 slots (`mStorageInventorySize`) and their own name and description. The station sorts its ports by direction in `BeginPlay`, and any input tagged `Fuel` is for fuel only, so the Blueprints only need these values. |
| `CWFU_Content.*` | C++ descriptor, recipe and schematic classes that hold the names and text, and the mod's own front/rear wheel classes. |
| `CWFU_WorldSubsystem.*` | At world init it copies the vanilla Cyber Wagon item's settings (icons, build-menu category, ...), the recipe ingredients, the shop category, the coupon item and the unlock/dependency object types. It registers the schematic through `UModContentRegistry`. It also gives the mod's wagon class single-angle steering and Unreal's standard speed-vs-steering curve; the vanilla wagon's curve is deliberately jagged. **Grip:** it swaps the wheel classes for copies of the vanilla wheels with `FrictionForceMultiplier` 3.6 instead of 2.0 and traction control and ABS on all four wheels. The vanilla wagon's engine delivers far more force than its tyres can hold, so it spun its wheels at about 0.4 g; it now accelerates and corners at about 0.7 g. Mass and top speed (165 km/h) are unchanged. **Cargo:** it sets `mInventorySize` to 120 (`FCWFU_INVENTORY_SLOTS`). **Autopilot:** the vanilla wagon has an autopilot component, but no path preset or movement curves. It uses the Truck's path preset, and builds its own curves from the wagon's mass: 0-100 km/h in about 5 seconds, 165 km/h and about 0.7 g braking and cornering. Autopilot doesn't use the wheel physics, so these have to be set separately. It then rebuilds the Blueprint's list of properties to copy to new instances, because the preset wasn't on that list when the class was loaded. |
| `Config/AccessTransformers.ini` | Gives the code access to the protected FactoryGame members it needs. |

## Building

This mod can be installed from the zip files under the **Releases** on the right, or it can be built from source.  The initial build takes several hours.

This project is based on the starter project from Satisfactory [SML starter project](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/project_setup.html).

1. Set up the starter project and the matching Unreal Engine (CSS 5.6) by following the docs linked above.
2. Copy this `CyberWagonFirmware` folder into the starter project's `Mods/` directory, then regenerate the Visual Studio project files.
3. Build **Development Editor** and open the editor.
4. **Create the vehicle Blueprint.** This is the one step code can't do:
   - In the Content Browser, find **`Testa_BP_WB`** (the vanilla Cyber Wagon actor, under `FactoryGame/Buildable/Vehicle/Cyberwagon`).
   - Right-click it → **Create Child Blueprint Class**, name it **`BP_CyberWagonFirmware`**, and save it at the root of the **CyberWagonFirmware** plugin content (`/CyberWagonFirmware/BP_CyberWagonFirmware`).
   - You don't need to change anything inside it. A distinct paint or a "v2.0" decal is optional.
   - The station Blueprints `Build_CyberWagonUnload` and `Build_CyberWagonLoad` are included in `Content`. `Scripts/CreateStationBlueprints.py` recreates them: `UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script=<path to the script> -unattended -nullrhi`.
5. Package with **Alpakit** (target: Windows, and Windows Server if you play dedicated). Then install it from Alpakit, or copy the output into `Satisfactory/FactoryGame/Mods/`.

## Verifying

- `FactoryGame.log` should contain `Steering firmware patch installed`, `Gag patch installed`, `Tuning patch installed`, `Station patch installed`, `Autopilot ready` and `Cyber Wagon firmware content ready`. A wagon from an older save also logs `Growing ... storage from 1 to 120 slots` once (from 48 if it was last saved with v1.1.0).
- If you see `Missing class: ... vehicle=0`, the Blueprint from step 4 isn't at `/CyberWagonFirmware/BP_CyberWagonFirmware`. `unloadstation=0` or `loadstation=0` means the same for the station Blueprints.
- Left and right steering should both work, the wagon should not shake or speed up on its own, and it should grip and accelerate without spinning its wheels.
- The trunk should have 120 slots.
- A wagon loaded from a save keeps whatever speed it was saved with.
- Each wagon should log `uses path preset VehiclePathPreset_Truck`. `None` means the preset didn't reach it.
- On a Truck or Universal path, the wagon should accept a route with Truck Stations, drive it, and load and unload at each stop.
- A CyberWagon Unload should move a docked vehicle's cargo into its 240 slots and out of its 4 output ports. A CyberWagon Load should fill from its 4 input ports and load the vehicle. Toggling the mode in the UI should do nothing.
