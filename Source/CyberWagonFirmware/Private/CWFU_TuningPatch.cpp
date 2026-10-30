#include "CWFU_TuningPatch.h"
#include "CWFU_SteeringPatch.h"
#include "CyberWagonFirmwareModule.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "FGInventoryComponent.h"
#include "Patching/NativeHookManager.h"
#include "UObject/ObjectKey.h"
#include "WheeledVehicles/FGWheeledVehicle.h"

namespace
{
	/** One 0-100 km/h run per wagon: armed when standing still, timed from the moment it starts moving */
	struct FSprint
	{
		bool bArmed = false;
		double StartTime = -1.0;
		float PeakKmh = 0.0f;
	};
	TMap<FObjectKey, FSprint> Sprints;

	TMap<FName, double> LastSetterLog;

	void LogSetter(const UActorComponent* Movement, const TCHAR* Setter, int32 WheelIndex, float Value)
	{
		if (!Movement || !FCWFU_SteeringPatch::IsPatched(Cast<AFGWheeledVehicle>(Movement->GetOwner())))
		{
			return;
		}
		const double Now = FPlatformTime::Seconds();
		double& Last = LastSetterLog.FindOrAdd(Setter, -1000.0);
		if (Now - Last >= 1.0)
		{
			Last = Now;
			UE_LOG(LogCyberWagonFirmware, Warning, TEXT("[Tuning] %s(wheel %d, %.3f) called on the wagon at runtime"), Setter, WheelIndex, Value);
		}
	}
}

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

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGWheeledVehicle::Tick, GetMutableDefault<AFGWheeledVehicle>(), [](AFGWheeledVehicle* Self, float DeltaTime)
	{
		if (!FCWFU_SteeringPatch::IsPatched(Self) || !Self->GetDriver())
		{
			return;
		}
		const float Kmh = Self->GetVelocity().Size() * 0.036f;
		FSprint& Sprint = Sprints.FindOrAdd(FObjectKey(Self));
		const double Now = FPlatformTime::Seconds();
		if (Kmh < 1.0f)
		{
			Sprint = FSprint();
			Sprint.bArmed = true;
		}
		else if (Sprint.bArmed)
		{
			if (Sprint.StartTime < 0.0)
			{
				Sprint.StartTime = Now;
			}
			Sprint.PeakKmh = FMath::Max(Sprint.PeakKmh, Kmh);
			if (Kmh >= 100.0f)
			{
				UE_LOG(LogCyberWagonFirmware, Log, TEXT("[Tuning] 0-100 km/h in %.1f s"), Now - Sprint.StartTime);
				Sprint.bArmed = false;
			}
			else if (Kmh < Sprint.PeakKmh - 5.0f)
			{
				Sprint.bArmed = false; // lifted off or braked, not a full-throttle run
			}
		}
	});

	SUBSCRIBE_METHOD(UChaosWheeledVehicleMovementComponent::SetWheelFrictionMultiplier,
		[](auto& Scope, UChaosWheeledVehicleMovementComponent* Self, int WheelIndex, float Friction)
	{
		LogSetter(Self, TEXT("SetWheelFrictionMultiplier"), WheelIndex, Friction);
	});
	SUBSCRIBE_METHOD(UChaosWheeledVehicleMovementComponent::SetWheelSlipGraphMultiplier,
		[](auto& Scope, UChaosWheeledVehicleMovementComponent* Self, int WheelIndex, float Multiplier)
	{
		LogSetter(Self, TEXT("SetWheelSlipGraphMultiplier"), WheelIndex, Multiplier);
	});
	SUBSCRIBE_METHOD(UChaosWheeledVehicleMovementComponent::SetTractionControlEnabled,
		[](auto& Scope, UChaosWheeledVehicleMovementComponent* Self, int WheelIndex, bool bEnabled)
	{
		LogSetter(Self, TEXT("SetTractionControlEnabled"), WheelIndex, bEnabled);
	});
	SUBSCRIBE_METHOD(UChaosWheeledVehicleMovementComponent::SetMaxEngineTorque,
		[](auto& Scope, UChaosWheeledVehicleMovementComponent* Self, float Torque)
	{
		LogSetter(Self, TEXT("SetMaxEngineTorque"), -1, Torque);
	});
	SUBSCRIBE_METHOD(UChaosWheeledVehicleMovementComponent::SetDragCoefficient,
		[](auto& Scope, UChaosWheeledVehicleMovementComponent* Self, float Drag)
	{
		LogSetter(Self, TEXT("SetDragCoefficient"), -1, Drag);
	});
	SUBSCRIBE_METHOD(UChaosWheeledVehicleMovementComponent::SetDownforceCoefficient,
		[](auto& Scope, UChaosWheeledVehicleMovementComponent* Self, float Downforce)
	{
		LogSetter(Self, TEXT("SetDownforceCoefficient"), -1, Downforce);
	});

	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Tuning patch installed"));
}
