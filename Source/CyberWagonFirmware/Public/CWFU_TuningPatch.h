#pragma once

#include "CoreMinimal.h"

/**
 * Runtime side of the handling and cargo upgrade (the class defaults are changed by UCWFU_WorldSubsystem).
 *
 * - Grows the storage of patched wagons from older saves to the new cargo size.
 * - Logs each 0-100 km/h run, so handling changes can be measured.
 * - Logs when anything changes the wagon's grip, engine or drag settings at runtime (the vanilla Blueprint might).
 */
class CYBERWAGONFIRMWARE_API FCWFU_TuningPatch
{
public:
	static void Install();
};
