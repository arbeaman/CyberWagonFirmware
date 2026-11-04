#include "CWFU_StationPatch.h"
#include "CyberWagonFirmwareModule.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Patching/NativeHookManager.h"

TWeakObjectPtr<UClass> FCWFU_StationPatch::UnloadStation;
TWeakObjectPtr<UClass> FCWFU_StationPatch::LoadStation;

void FCWFU_StationPatch::SetStationClasses(UClass* UnloadStationClass, UClass* LoadStationClass)
{
	UnloadStation = UnloadStationClass;
	LoadStation = LoadStationClass;
}

void FCWFU_StationPatch::Install()
{
	// Stations start in load mode, so the unload station has to be switched after it is built
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableDockingStation::BeginPlay, GetMutableDefault<AFGBuildableDockingStation>(), [](AFGBuildableDockingStation* Self)
	{
		if (UnloadStation.IsValid() && Self->IsA(UnloadStation.Get()))
		{
			Self->SetIsInLoadMode(false);
		}
	});

	// Ignore the UI's load/unload toggle. An unload station has no input to fill it with cargo and a load station has
	// no output to empty it, so the other mode would leave the station stuck.
	SUBSCRIBE_METHOD_VIRTUAL(AFGBuildableDockingStation::SetIsInLoadMode, GetMutableDefault<AFGBuildableDockingStation>(),
		[](auto& Scope, AFGBuildableDockingStation* Self, bool bIsInLoadMode)
	{
		const bool bIsUnloadStation = UnloadStation.IsValid() && Self->IsA(UnloadStation.Get());
		const bool bIsLoadStation = LoadStation.IsValid() && Self->IsA(LoadStation.Get());
		if ((bIsUnloadStation && bIsInLoadMode) || (bIsLoadStation && !bIsInLoadMode))
		{
			Scope.Cancel();
		}
	});

	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Station patch installed"));
}
