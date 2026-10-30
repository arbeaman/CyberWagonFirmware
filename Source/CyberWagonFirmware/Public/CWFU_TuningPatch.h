#pragma once

#include "CoreMinimal.h"

/**
 * Runtime side of the handling and cargo upgrade (the class defaults are changed by UCWFU_WorldSubsystem):
 * grows the storage of patched wagons from older saves to the new cargo size.
 */
class CYBERWAGONFIRMWARE_API FCWFU_TuningPatch
{
public:
	static void Install();
};
