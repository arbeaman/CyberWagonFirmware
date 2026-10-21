#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

class AFGWheeledVehicle;

/**
 * The "firmware update" itself.
 *
 * The vanilla Cyber Wagon turns a held left (negative raw) steering input into a right turn at twice the
 * normal angle, while a negative smoothed steering value with no raw input behind it steers left normally.
 * For patched wagons, left input is kept out of the raw input entirely and fed into the smoothed value
 * directly after the movement component's UpdateState. Right steering uses the game's normal path.
 *
 * Only vehicles of the patched class (the Firmware Update wagon) are touched; vanilla Cyber Wagons keep
 * their authentic right-only driving experience.
 */
class CYBERWAGONFIRMWARE_API FCWFU_SteeringPatch
{
public:
	static void Install();

	/** Vehicle class that receives the patch. Set by UCWFU_WorldSubsystem once the class is loaded */
	static void SetPatchedVehicleClass(UClass* VehicleClass);

private:
	static bool IsPatched(const AFGWheeledVehicle* Vehicle);
	/** True while the driver of a patched wagon is holding left this frame; OutSteering is the (negative) input */
	static bool GetPlayerLeftSteering(AFGWheeledVehicle* Vehicle, float& OutSteering);

	struct FCapturedSteering
	{
		float Value = 0.0f;
		uint64 Frame = 0;
	};

	static TWeakObjectPtr<UClass> PatchedVehicleClass;
	/** Raw steering input most recently fed into MoveRight, per vehicle */
	static TMap<FObjectKey, FCapturedSteering> RawSteeringInput;
	/** Our smoothed left steering value per vehicle, carried over between frames while left is held */
	static TMap<FObjectKey, FCapturedSteering> SmoothedLeftSteering;
};
