#include "CWFU_SteeringPatch.h"
#include "CyberWagonFirmwareModule.h"
#include "Patching/NativeHookManager.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleMovementComponent.h"

// Engine classes can't use AccessTransformers, so reach the protected steering state through a derived type
struct FCWFU_ChaosMovementAccess : UChaosVehicleMovementComponent
{
	static float& SteeringInputOf(UChaosVehicleMovementComponent* Movement)
	{
		return Movement->*(&FCWFU_ChaosMovementAccess::SteeringInput);
	}
	static const FVehicleInputRateConfig& SteeringRateOf(UChaosVehicleMovementComponent* Movement)
	{
		return Movement->SteeringInputRate;
	}
};

TWeakObjectPtr<UClass> FCWFU_SteeringPatch::PatchedVehicleClass;
TMap<FObjectKey, FCWFU_SteeringPatch::FCapturedSteering> FCWFU_SteeringPatch::RawSteeringInput;
TMap<FObjectKey, FCWFU_SteeringPatch::FCapturedSteering> FCWFU_SteeringPatch::SmoothedLeftSteering;

void FCWFU_SteeringPatch::Install()
{
	// The vanilla wagon reacts to a negative raw steering input by steering the wheels right at twice the
	// normal angle, but a negative *smoothed* steering value with no raw input behind it steers left correctly.
	// So left input never reaches the vehicle as raw input; we drive the smoothed value ourselves instead.
	SUBSCRIBE_METHOD(AFGWheeledVehicle::MoveRight, [](auto& Scope, AFGWheeledVehicle* Self, float AxisValue)
	{
		if (!IsPatched(Self))
		{
			return;
		}
		RawSteeringInput.Add(FObjectKey(Self), { AxisValue, GFrameCounter });
		if (AxisValue < 0.0f)
		{
			Scope.Cancel();
			// Keep what the original would have stored: how hard steering is held, used for path following
			Self->mCurrentSteeringInput = FMath::Abs(AxisValue);
		}
	});

	// Keep the raw input at zero while steering left, in case anything else set it this frame
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGWheeledVehicle::Tick, GetMutableDefault<AFGWheeledVehicle>(), [](AFGWheeledVehicle* Self, float DeltaTime)
	{
		float LeftSteering;
		if (GetPlayerLeftSteering(Self, LeftSteering))
		{
			Self->GetVehicleMovementComponent()->SetSteeringInput(0.0f);
		}
	});

	// UpdateState turns the raw input into the smoothed SteeringInput that Update() hands to physics.
	// Straight after it, move the smoothed value towards the player's left input with the vehicle's own rates.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGWheeledVehicleMovementComponent::UpdateState, GetMutableDefault<UFGWheeledVehicleMovementComponent>(),
		[](UFGWheeledVehicleMovementComponent* Self, float DeltaTime)
	{
		AFGWheeledVehicle* Vehicle = Cast<AFGWheeledVehicle>(Self->GetOwner());
		float LeftSteering;
		if (GetPlayerLeftSteering(Vehicle, LeftSteering))
		{
			// The game's UpdateState has just eased SteeringInput back towards the (zero) raw input, so continue
			// from our own value of last frame rather than from that; otherwise the two cancel out near zero.
			float& Smoothed = FCWFU_ChaosMovementAccess::SteeringInputOf(Self);
			const FObjectKey Key(Vehicle);
			const FCapturedSteering* Previous = SmoothedLeftSteering.Find(Key);
			const float Start = Previous && GFrameCounter - Previous->Frame <= 1 ? Previous->Value : Smoothed;
			Smoothed = FCWFU_ChaosMovementAccess::SteeringRateOf(Self).InterpInputValue(DeltaTime, Start, LeftSteering);
			SmoothedLeftSteering.Add(Key, { Smoothed, GFrameCounter });
		}
	});

	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Steering firmware patch installed"));
}

void FCWFU_SteeringPatch::SetPatchedVehicleClass(UClass* VehicleClass)
{
	PatchedVehicleClass = VehicleClass;
}

bool FCWFU_SteeringPatch::IsPatched(const AFGWheeledVehicle* Vehicle)
{
	const UClass* VehicleClass = PatchedVehicleClass.Get();
	return Vehicle && VehicleClass && Vehicle->IsA(VehicleClass);
}

bool FCWFU_SteeringPatch::GetPlayerLeftSteering(AFGWheeledVehicle* Vehicle, float& OutSteering)
{
	if (!IsPatched(Vehicle) || !Vehicle->GetDriver() || !Vehicle->GetVehicleMovementComponent() || !Vehicle->CanApplyVehicleInput())
	{
		return false;
	}
	// Steering input fires every frame while held, so an older capture means the key was released
	const FCapturedSteering* Captured = RawSteeringInput.Find(FObjectKey(Vehicle));
	if (!Captured || GFrameCounter - Captured->Frame > 1 || Captured->Value >= 0.0f)
	{
		return false;
	}
	OutSteering = Captured->Value;
	return true;
}
