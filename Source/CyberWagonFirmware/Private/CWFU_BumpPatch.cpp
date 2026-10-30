#include "CWFU_BumpPatch.h"
#include "CWFU_SteeringPatch.h"
#include "CyberWagonFirmwareModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Patching/NativeHookManager.h"
#include "WheeledVehicles/FGWheeledVehicle.h"

// The Blueprint aims exactly at the marker's location; allow a little slack for floating point
static constexpr double BumpLocTolerance = 5.0; // cm

static bool IsAtBumpLoc(const AActor* Vehicle, const FVector& Location)
{
	TInlineComponentArray<USceneComponent*> Components(Vehicle);
	for (const USceneComponent* Component : Components)
	{
		if (Component->GetName().StartsWith(TEXT("BumpLoc"))
			&& FVector::Dist(Component->GetComponentLocation(), Location) <= BumpLocTolerance)
		{
			return true;
		}
	}
	return false;
}

void FCWFU_BumpPatch::Install()
{
	// AddImpulseAtLocation isn't overridden below UPrimitiveComponent, so any skeletal mesh works as the sample
	SUBSCRIBE_METHOD_VIRTUAL(UPrimitiveComponent::AddImpulseAtLocation, GetMutableDefault<USkeletalMeshComponent>(),
		[](auto& Scope, UPrimitiveComponent* Self, FVector Impulse, FVector Location, FName BoneName)
	{
		const AFGWheeledVehicle* Vehicle = Cast<AFGWheeledVehicle>(Self->GetOwner());
		if (FCWFU_SteeringPatch::IsPatched(Vehicle) && IsAtBumpLoc(Vehicle, Location))
		{
			Scope.Cancel();
		}
	});

	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Bump patch installed"));
}
