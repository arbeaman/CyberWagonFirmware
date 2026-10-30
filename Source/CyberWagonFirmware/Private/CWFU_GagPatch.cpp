#include "CWFU_GagPatch.h"
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

void FCWFU_GagPatch::Install()
{
	// Shake. AddImpulseAtLocation isn't overridden below UPrimitiveComponent, so any skeletal mesh works as the sample
	SUBSCRIBE_METHOD_VIRTUAL(UPrimitiveComponent::AddImpulseAtLocation, GetMutableDefault<USkeletalMeshComponent>(),
		[](auto& Scope, UPrimitiveComponent* Self, FVector Impulse, FVector Location, FName BoneName)
	{
		const AFGWheeledVehicle* Vehicle = Cast<AFGWheeledVehicle>(Self->GetOwner());
		if (FCWFU_SteeringPatch::IsPatched(Vehicle) && IsAtBumpLoc(Vehicle, Location))
		{
			Scope.Cancel();
		}
	});

	// Sticky throttle. Both events reach the wagon through ProcessEvent (timers), so blocking them there stops them
	SUBSCRIBE_METHOD_VIRTUAL(AActor::ProcessEvent, GetMutableDefault<AFGWheeledVehicle>(),
		[](auto& Scope, AActor* Self, UFunction* Function, void* Parameters)
	{
		static const FName StickyThrottleFailure(TEXT("StickyThrottleFailure"));
		static const FName StickThrottleToFloor(TEXT("StickThrottleToFloor"));
		if (Function && (Function->GetFName() == StickyThrottleFailure || Function->GetFName() == StickThrottleToFloor)
			&& FCWFU_SteeringPatch::IsPatched(Cast<AFGWheeledVehicle>(Self)))
		{
			Scope.Cancel();
		}
	});

	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Gag patch installed (shake, sticky throttle)"));
}
