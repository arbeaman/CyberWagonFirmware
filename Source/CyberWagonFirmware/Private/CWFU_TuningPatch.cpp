#include "CWFU_TuningPatch.h"
#include "CWFU_SteeringPatch.h"
#include "CyberWagonFirmwareModule.h"
#include "FGInventoryComponent.h"
#include "Patching/NativeHookManager.h"
#include "WheeledVehicles/FGWheeledVehicle.h"

void FCWFU_TuningPatch::Install()
{
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGWheeledVehicle::BeginPlay, GetMutableDefault<AFGWheeledVehicle>(), [](AFGWheeledVehicle* Self)
	{
		if (!FCWFU_SteeringPatch::IsPatched(Self) || !Self->HasAuthority())
		{
			return;
		}
		// Wagons saved before the cargo upgrade load with their old, smaller storage. Only ever grow it.
		UFGInventoryComponent* Storage = Self->GetStorageInventory();
		if (Storage && Storage->GetSizeLinear() < Self->mInventorySize)
		{
			UE_LOG(LogCyberWagonFirmware, Log, TEXT("Growing %s storage from %d to %d slots"), *Self->GetName(), Storage->GetSizeLinear(), Self->mInventorySize);
			Storage->Resize(Self->mInventorySize);
		}
	});

	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Tuning patch installed"));
}
