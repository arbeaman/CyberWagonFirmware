#include "CWFU_WorldSubsystem.h"
#include "CWFU_Content.h"
#include "CWFU_StationPatch.h"
#include "CWFU_SteeringPatch.h"
#include "CyberWagonFirmwareModule.h"
#include "AvailabilityDependencies/FGSchematicPurchasedDependency.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "FGVehicle.h"
#include "Registry/ModContentRegistry.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "WheeledVehicles/FGVehicleAutopilotComponent.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleMovementComponent.h"

static const int FCWFU_INVENTORY_SLOTS = 120;

static const FName ModReference(TEXT("CyberWagonFirmware"));

// Vanilla content (paths from CommunityResources/Docs)
static const TCHAR* VanillaDescPath = TEXT("/Game/FactoryGame/Buildable/Vehicle/Cyberwagon/Desc_CyberWagon.Desc_CyberWagon_C");
static const TCHAR* VanillaRecipePath = TEXT("/Game/FactoryGame/Recipes/Vehicle/Recipe_CyberWagon.Recipe_CyberWagon_C");
static const TCHAR* VanillaSchematicPath = TEXT("/Game/FactoryGame/Schematics/ResourceSink/ResourceSink_CyberWagon.ResourceSink_CyberWagon_C");
static const TCHAR* CircuitBoardPath = TEXT("/Game/FactoryGame/Resource/Parts/CircuitBoard/Desc_CircuitBoard.Desc_CircuitBoard_C");

// The mod's own vehicle: a Blueprint child of the vanilla Cyber Wagon (Testa_BP_WB), see README
static const TCHAR* PatchedVehiclePath = TEXT("/CyberWagonFirmware/BP_CyberWagonFirmware.BP_CyberWagonFirmware_C");

// On autopilot the patched wagon drives on the Truck's paths (Truck and Universal)
static const TCHAR* TruckPath = TEXT("/Game/FactoryGame/Buildable/Vehicle/Truck/BP_Truck.BP_Truck_C");

// The mod's stations: Blueprint children of the vanilla Truck Station (Build_TruckStation), see README
static const TCHAR* UnloadStationPath = TEXT("/CyberWagonFirmware/Build_CyberWagonUnload.Build_CyberWagonUnload_C");
static const TCHAR* LoadStationPath = TEXT("/CyberWagonFirmware/Build_CyberWagonLoad.Build_CyberWagonLoad_C");
static const TCHAR* TruckStationDescPath = TEXT("/Game/FactoryGame/Buildable/Factory/TruckStation/Desc_TruckStation.Desc_TruckStation_C");

static constexpr int32 FirmwareCouponCost = 25;
static constexpr int32 FirmwareCircuitBoards = 2;

struct FCWFU_StationIngredient
{
	const TCHAR* ItemPath;
	int32 Amount;
};

// Fixed cost of each station: twice a Truck Station (15 Modular Frame, 20 Rotor, 50 Cable) plus
// a Firmware Update wagon (10 Reinforced Iron Plate, 2 Circuit Board)
static const FCWFU_StationIngredient StationIngredients[] = {
	{ TEXT("/Game/FactoryGame/Resource/Parts/ModularFrame/Desc_ModularFrame.Desc_ModularFrame_C"), 30 },
	{ TEXT("/Game/FactoryGame/Resource/Parts/Rotor/Desc_Rotor.Desc_Rotor_C"), 40 },
	{ TEXT("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C"), 100 },
	{ TEXT("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C"), 10 },
	{ CircuitBoardPath, 2 },
};

// Tyre grip (Chaos FrictionForceMultiplier). The vanilla wagon uses the engine default of 2.0, which limits it to
// about 0.4 g: its drive force is far above that, so it spins its wheels and slides. 3.6 gives roughly 0.7 g,
// in line with a real Cybertruck's 0-60 mph in about 4 seconds.
static constexpr float FirmwareWheelFriction = 3.6f;

// Autopilot moves the wagon along the path spline without the wheel physics, so the manual driving performance is
// repeated here. The game's autopilot forces are absolute (kN), so they are calculated from the wagon's own mass.
static constexpr float AutopilotTopSpeed = 45.8f; // m/s, the manual top speed of 165 km/h
static constexpr float AutopilotAcceleration = 5.6f; // m/s^2, 0-100 km/h in about 5 seconds
static constexpr float AutopilotBraking = 6.9f; // m/s^2, about 0.7 g like the manual tyre grip
static constexpr float AutopilotEngineBraking = 1.4f; // m/s^2, part of AutopilotBraking
static constexpr float AutopilotCornering = 6.9f; // m/s^2, about 0.7 g like the manual tyre grip
static constexpr float AutopilotThrottleChangeRate = 2.0f; // 1/s, the default of 0.2 takes 5 seconds to reach full throttle
static constexpr float AirDensity = 1.2f; // kg/m^3

/** Empties a curve so it can be filled with our own keys. A curve asset assigned to it would otherwise take priority */
static FRichCurve& ResetCurve(FRuntimeFloatCurve& Curve)
{
	Curve.ExternalCurve = nullptr;
	Curve.EditorCurveData.Reset();
	return Curve.EditorCurveData;
}

/** Copy all settings of a vanilla wheel Blueprint into one of our wheel classes, then apply the firmware's grip changes */
static void SetupWheelClass(UClass* WheelClass, const UClass* VanillaWheelClass)
{
	UChaosVehicleWheel* Wheel = WheelClass->GetDefaultObject<UChaosVehicleWheel>();
	const UChaosVehicleWheel* VanillaWheel = VanillaWheelClass->GetDefaultObject<UChaosVehicleWheel>();
	for (TFieldIterator<FProperty> It(UFGVehicleWheel::StaticClass()); It; ++It)
	{
		if (!It->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Transient))
		{
			It->CopyCompleteValue_InContainer(Wheel, VanillaWheel);
		}
	}
	Wheel->FrictionForceMultiplier = FirmwareWheelFriction;
	// Limit drive force to the available grip instead of spinning the wheels, and keep the rear wheels from
	// locking under braking (vanilla only has ABS on the front)
	Wheel->bTractionControlEnabled = true;
	Wheel->bABSEnabled = true;
}

bool UCWFU_WorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCWFU_WorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UModContentRegistry>();

	if (!SetupContent())
	{
		UE_LOG(LogCyberWagonFirmware, Error, TEXT("Content setup failed, the Firmware Update will not be available"));
		return;
	}

	if (UModContentRegistry* Registry = UModContentRegistry::Get(GetWorld()))
	{
		Registry->RegisterSchematic(ModReference, UCWFU_Schematic::StaticClass());
	}
}

bool UCWFU_WorldSubsystem::SetupContent()
{
	static bool bSetupDone = false;
	if (bSetupDone)
	{
		return true;
	}

	UClass* VanillaDescClass = LoadClass<UFGVehicleDescriptor>(nullptr, VanillaDescPath);
	UClass* VanillaRecipeClass = LoadClass<UFGRecipe>(nullptr, VanillaRecipePath);
	UClass* VanillaSchematicClass = LoadClass<UFGSchematic>(nullptr, VanillaSchematicPath);
	UClass* CircuitBoardClass = LoadClass<UFGItemDescriptor>(nullptr, CircuitBoardPath);
	UClass* PatchedVehicleClass = LoadClass<AFGVehicle>(nullptr, PatchedVehiclePath);
	UClass* TruckClass = LoadClass<AFGWheeledVehicle>(nullptr, TruckPath);
	UClass* UnloadStationClass = LoadClass<AFGBuildableDockingStation>(nullptr, UnloadStationPath);
	UClass* LoadStationClass = LoadClass<AFGBuildableDockingStation>(nullptr, LoadStationPath);
	UClass* TruckStationDescClass = LoadClass<UFGBuildingDescriptor>(nullptr, TruckStationDescPath);
	if (!VanillaDescClass || !VanillaRecipeClass || !VanillaSchematicClass || !CircuitBoardClass || !PatchedVehicleClass || !TruckClass
		|| !UnloadStationClass || !LoadStationClass || !TruckStationDescClass)
	{
		UE_LOG(LogCyberWagonFirmware, Error, TEXT("Missing class: desc=%d recipe=%d schematic=%d circuit=%d vehicle=%d truck=%d unloadstation=%d loadstation=%d stationdesc=%d"),
			VanillaDescClass != nullptr, VanillaRecipeClass != nullptr, VanillaSchematicClass != nullptr,
			CircuitBoardClass != nullptr, PatchedVehicleClass != nullptr, TruckClass != nullptr,
			UnloadStationClass != nullptr, LoadStationClass != nullptr, TruckStationDescClass != nullptr);
		return false;
	}

	// Our classes' CDOs are native and live in the disregard-for-GC pool, whose references are not traced by the
	// garbage collector. Anything only they point at (loaded classes, the unlock/dependency objects below) would
	// be collected on the next map change and leave dangling pointers, so root it explicitly.
	for (UClass* Class : { VanillaDescClass, VanillaRecipeClass, VanillaSchematicClass, CircuitBoardClass, PatchedVehicleClass, TruckClass,
		UnloadStationClass, LoadStationClass, TruckStationDescClass })
	{
		Class->AddToRoot();
	}

	TArray<FItemAmount> StationCost;
	for (const FCWFU_StationIngredient& Ingredient : StationIngredients)
	{
		UClass* ItemClass = LoadClass<UFGItemDescriptor>(nullptr, Ingredient.ItemPath);
		if (!ItemClass)
		{
			UE_LOG(LogCyberWagonFirmware, Error, TEXT("Missing class %s"), Ingredient.ItemPath);
			return false;
		}
		ItemClass->AddToRoot();
		StationCost.Add(FItemAmount(ItemClass, Ingredient.Amount));
	}

	const UFGVehicleDescriptor* VanillaDesc = VanillaDescClass->GetDefaultObject<UFGVehicleDescriptor>();
	const UFGRecipe* VanillaRecipe = VanillaRecipeClass->GetDefaultObject<UFGRecipe>();
	const UFGSchematic* VanillaSchematic = VanillaSchematicClass->GetDefaultObject<UFGSchematic>();

	// Descriptor: take every native setting from the vanilla wagon (icons, build menu category/sub-category,
	// quick switch group, ...) so nothing the UI relies on is left empty, then apply our own identity on top
	UCWFU_Desc* Desc = GetMutableDefault<UCWFU_Desc>();
	static const TSet<FName> OwnProperties = {
		TEXT("mUseDisplayNameAndDescription"), TEXT("mDisplayName"), TEXT("mDescription"), TEXT("mAbbreviatedDisplayName"),
		TEXT("mVehicleClass"), TEXT("mMenuPriority") };
	for (TFieldIterator<FProperty> It(UFGVehicleDescriptor::StaticClass()); It; ++It)
	{
		if (!OwnProperties.Contains(It->GetFName()) && !It->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference))
		{
			It->CopyCompleteValue_InContainer(Desc, VanillaDesc);
		}
	}
	Desc->mMenuPriority = VanillaDesc->mMenuPriority + 0.5f;
	Desc->mVehicleClass = PatchedVehicleClass;

	// Recipe: vanilla wagon ingredients plus the circuit boards for the new firmware
	UCWFU_Recipe* Recipe = GetMutableDefault<UCWFU_Recipe>();
	Recipe->mIngredients = VanillaRecipe->mIngredients;
	Recipe->mIngredients.Add(FItemAmount(CircuitBoardClass, FirmwareCircuitBoards));
	Recipe->mProducedIn = VanillaRecipe->mProducedIn;

	// Stations: everything the build menu needs comes from the Truck Station's descriptor, except what they build
	const UFGBuildingDescriptor* TruckStationDesc = TruckStationDescClass->GetDefaultObject<UFGBuildingDescriptor>();
	const auto SetupStationDesc = [TruckStationDesc](UFGBuildingDescriptor* StationDesc, UClass* StationClass, float MenuPriorityOffset)
	{
		for (TFieldIterator<FProperty> It(UFGBuildingDescriptor::StaticClass()); It; ++It)
		{
			if (It->GetFName() != TEXT("mBuildableClass") && !It->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference))
			{
				It->CopyCompleteValue_InContainer(StationDesc, TruckStationDesc);
			}
		}
		StationDesc->mMenuPriority = TruckStationDesc->mMenuPriority + MenuPriorityOffset;
		StationDesc->mBuildableClass = StationClass;
	};
	SetupStationDesc(GetMutableDefault<UCWFU_UnloadStationDesc>(), UnloadStationClass, 0.1f);
	SetupStationDesc(GetMutableDefault<UCWFU_LoadStationDesc>(), LoadStationClass, 0.2f);

	for (UFGRecipe* StationRecipe : { static_cast<UFGRecipe*>(GetMutableDefault<UCWFU_UnloadStationRecipe>()), static_cast<UFGRecipe*>(GetMutableDefault<UCWFU_LoadStationRecipe>()) })
	{
		StationRecipe->mIngredients = StationCost;
		StationRecipe->mProducedIn = VanillaRecipe->mProducedIn; // the build gun
	}

	// Schematic: same shop category and icon as the vanilla wagon, paid in coupons
	UCWFU_Schematic* Schematic = GetMutableDefault<UCWFU_Schematic>();
	Schematic->mSchematicCategory = VanillaSchematic->mSchematicCategory; // the shop groups entries by this
	Schematic->mSubCategories = VanillaSchematic->mSubCategories;
	Schematic->mSchematicIcon = VanillaSchematic->mSchematicIcon;
	Schematic->mSmallSchematicIcon = VanillaSchematic->mSmallSchematicIcon;
	Schematic->mRelevantShopSchematics = { VanillaSchematicClass };
	Schematic->mCost.Reset();
	for (const FItemAmount& Cost : VanillaSchematic->mCost)
	{
		Schematic->mCost.Add(FItemAmount(Cost.ItemClass, FirmwareCouponCost)); // FICSIT Coupons
	}

	// The unlock and dependency types are Blueprint subclasses (BP_UnlockRecipe, BP_SchematicPurchasedDependency),
	// so instance whatever class the vanilla schematic uses rather than hardcoding their paths
	UClass* UnlockRecipeClass = nullptr;
	for (const UFGUnlock* Unlock : VanillaSchematic->mUnlocks)
	{
		if (Unlock && Unlock->IsA<UFGUnlockRecipe>())
		{
			UnlockRecipeClass = Unlock->GetClass();
			break;
		}
	}
	UClass* PurchasedDependencyClass = nullptr;
	for (const UFGAvailabilityDependency* Dependency : VanillaSchematic->mSchematicDependencies)
	{
		if (Dependency && Dependency->IsA<UFGSchematicPurchasedDependency>())
		{
			PurchasedDependencyClass = Dependency->GetClass();
			break;
		}
	}
	if (!UnlockRecipeClass || !PurchasedDependencyClass)
	{
		UE_LOG(LogCyberWagonFirmware, Error, TEXT("Could not find vanilla unlock/dependency classes on %s"), VanillaSchematicPath);
		return false;
	}

	UFGUnlockRecipe* Unlock = NewObject<UFGUnlockRecipe>(Schematic, UnlockRecipeClass);
	// AddRecipe()/Init() are editor-only. Unlocks are applied again on load, so saves that already own this schematic get the stations too.
	Unlock->mRecipes = { UCWFU_Recipe::StaticClass(), UCWFU_UnloadStationRecipe::StaticClass(), UCWFU_LoadStationRecipe::StaticClass() };
	Unlock->AddToRoot();
	Schematic->mUnlocks = { Unlock };

	// Only show up once the player owns the original wagon. You can't patch what you haven't bought.
	UFGSchematicPurchasedDependency* Dependency = NewObject<UFGSchematicPurchasedDependency>(Schematic, PurchasedDependencyClass);
	Dependency->mSchematics = { VanillaSchematicClass };
	Dependency->mRequireAllSchematicsToBePurchased = true;
	Dependency->AddToRoot();
	Schematic->mSchematicDependencies = { Dependency };

	// Handling clean-up for the patched wagon (the left-turn fix itself lives in FCWFU_SteeringPatch): plain
	// single-angle steering instead of the vanilla Ackermann setup, and a sane speed-vs-steering curve.
	// Vehicles copy this from the class default object when spawned or loaded, so only patched wagons change.
	if (AFGWheeledVehicle* VehicleCDO = Cast<AFGWheeledVehicle>(PatchedVehicleClass->GetDefaultObject()))
	{
		// Autopilot: the Truck's path preset decides which paths the wagon can drive on. The vanilla wagon has an
		// autopilot component, but no preset or curves, so it has no autopilot and no forces to move with.
		VehicleCDO->mVehiclePathPreset = TruckClass->GetDefaultObject<AFGWheeledVehicle>()->GetVehiclePathPreset();

		if (UFGVehicleAutopilotComponent* Autopilot = VehicleCDO->GetVehicleAutopilotComponent())
		{
			const float MassTonnes = Autopilot->mVehicleMass / 1000.0f; // tonnes * m/s^2 = kN

			// Enough force for the target acceleration on top of the air drag at top speed. Drops to zero at top speed.
			const float DragAtTopSpeed = 0.5f * AirDensity * Autopilot->mDragCoefficient * Autopilot->mVehicleCrossSectionalArea
				* FMath::Square(AutopilotTopSpeed) / 1000.0f;
			const float TractiveEffort = MassTonnes * AutopilotAcceleration + DragAtTopSpeed;
			FRichCurve& TractiveCurve = ResetCurve(Autopilot->mTractiveEffortCurve);
			TractiveCurve.AddKey(0.0f, TractiveEffort);
			TractiveCurve.AddKey(AutopilotTopSpeed - 1.0f, TractiveEffort);
			TractiveCurve.AddKey(AutopilotTopSpeed, 0.0f);

			// Engine braking fades out at walking speed, so the wagon stops on its brakes
			const float EngineBrakingEffort = MassTonnes * AutopilotEngineBraking;
			FRichCurve& EngineBrakingCurve = ResetCurve(Autopilot->mDynamicBrakingEffortCurve);
			EngineBrakingCurve.AddKey(0.0f, 0.0f);
			EngineBrakingCurve.AddKey(2.0f, EngineBrakingEffort);
			EngineBrakingCurve.AddKey(AutopilotTopSpeed, EngineBrakingEffort);
			Autopilot->mMaxAirBrakingEffort = MassTonnes * (AutopilotBraking - AutopilotEngineBraking);

			// Corner as fast as the grip allows: speed = sqrt(lateral acceleration * radius).
			// The curve's input is curvature in deg/cm, which is 0.573 / radius in metres.
			FRichCurve& CurvatureCurve = ResetCurve(Autopilot->mCurvatureSpeedLimitCurve);
			CurvatureCurve.AddKey(0.0f, AutopilotTopSpeed);
			for (const float Radius : { 3.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f })
			{
				CurvatureCurve.AddKey(0.573f / Radius, FMath::Min(FMath::Sqrt(AutopilotCornering * Radius), AutopilotTopSpeed));
			}

			// Let the throttle and the speed limit keep up with the above
			Autopilot->mAutopilotThrottleChangeRate = AutopilotThrottleChangeRate;
			Autopilot->mAutopilotSpeedLimitChangeRate = AutopilotBraking * 100.0f; // cm/s

			UE_LOG(LogCyberWagonFirmware, Log, TEXT("Autopilot ready: mass %.0f kg, tractive effort %.1f kN, top speed %.1f"),
				Autopilot->mVehicleMass, TractiveEffort, AFGWheeledVehicle::GetMaxAutopilotSpeedForVehicleType(PatchedVehicleClass));
		}

		// Set the vehicle inventory slots to FCWFU_INVENTORY_SLOTS
		VehicleCDO->mInventorySize =  FCWFU_INVENTORY_SLOTS;
		// Cargo: a fixed slot count (the vanilla wagon has 1). Wagons from older saves are grown on
		// BeginPlay by FCWFU_TuningPatch.
		/*
		if (const UClass* TruckClass = LoadClass<AFGWheeledVehicle>(nullptr, TruckPath))
		{
			VehicleCDO->mInventorySize = TruckClass->GetDefaultObject<AFGWheeledVehicle>()->mInventorySize;
		}
		else
		{
			UE_LOG(LogCyberWagonFirmware, Error, TEXT("Missing class %s, cargo space not changed"), TruckPath);
		}
		*/

		if (UFGWheeledVehicleMovementComponent* Movement = VehicleCDO->GetVehicleMovementComponent())
		{
			// Grip: swap the vanilla front/back wheel classes for our copies with more grip and traction control
			for (FChaosWheelSetup& WheelSetup : Movement->WheelSetups)
			{
				UClass* VanillaWheelClass = WheelSetup.WheelClass;
				if (!VanillaWheelClass || VanillaWheelClass->IsChildOf<UCWFU_FrontWheel>() || VanillaWheelClass->IsChildOf<UCWFU_RearWheel>())
				{
					continue;
				}
				const UChaosVehicleWheel* VanillaWheel = VanillaWheelClass->GetDefaultObject<UChaosVehicleWheel>();
				UClass* WheelClass = VanillaWheel->bAffectedBySteering ? UCWFU_FrontWheel::StaticClass() : UCWFU_RearWheel::StaticClass();
				SetupWheelClass(WheelClass, VanillaWheelClass);
				WheelSetup.WheelClass = WheelClass;
			}

			Movement->SteeringSetup.SteeringType = ESteeringType::SingleAngle;

			// The vanilla speed-vs-steering curve is deliberately jagged (e.g. 2.03 at 36 mph, 0.55 at 41 mph),
			// which makes the steering angle jump around as speed changes. Use the engine's standard curve.
			if (FRichCurve* SpeedCurve = Movement->SteeringSetup.SteeringCurve.GetRichCurve())
			{
				SpeedCurve->Reset();
				SpeedCurve->AddKey(0.f, 1.f);
				SpeedCurve->AddKey(20.f, 0.8f);
				SpeedCurve->AddKey(60.f, 0.4f);
				SpeedCurve->AddKey(120.f, 0.3f);
			}
		}
	}

	// A new instance of a Blueprint only copies the native properties that differed from the native defaults when the
	// class was loaded. The path preset was empty then, so without this rebuild wagons would never receive it.
	if (UBlueprintGeneratedClass* PatchedVehicleBlueprintClass = Cast<UBlueprintGeneratedClass>(PatchedVehicleClass))
	{
		PatchedVehicleBlueprintClass->UpdateCustomPropertyListForPostConstruction();
	}

	FCWFU_SteeringPatch::SetPatchedVehicleClass(PatchedVehicleClass);
	FCWFU_StationPatch::SetStationClasses(UnloadStationClass, LoadStationClass);

	bSetupDone = true;
	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Cyber Wagon firmware content ready (vehicle class %s)"), *PatchedVehicleClass->GetPathName());
	return true;
}
