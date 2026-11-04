#pragma once

#include "CoreMinimal.h"

/**
 * Keeps the CyberWagon Unload and Load stations in the one mode their ports allow. The mode is a saved setting of the
 * station instance with a toggle in its UI, so it can't be fixed in the Blueprints.
 */
class CYBERWAGONFIRMWARE_API FCWFU_StationPatch
{
public:
	static void Install();

	/** Called by UCWFU_WorldSubsystem once the station Blueprints are loaded */
	static void SetStationClasses(UClass* UnloadStationClass, UClass* LoadStationClass);

private:
	static TWeakObjectPtr<UClass> UnloadStation;
	static TWeakObjectPtr<UClass> LoadStation;
};
