#include "CWFU_WorldSubsystem.h"
#include "CWFU_Content.h"
#include "CWFU_SteeringPatch.h"
#include "CyberWagonFirmwareModule.h"
#include "AvailabilityDependencies/FGSchematicPurchasedDependency.h"
#include "FGVehicle.h"
#include "Registry/ModContentRegistry.h"
#include "Unlocks/FGUnlockRecipe.h"
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

// The patched wagon gets the Truck's cargo space
static const TCHAR* TruckPath = TEXT("/Game/FactoryGame/Buildable/Vehicle/Truck/BP_Truck.BP_Truck_C");

static constexpr int32 FirmwareCouponCost = 25;
static constexpr int32 FirmwareCircuitBoards = 2;

// Tyre grip (Chaos FrictionForceMultiplier). The vanilla wagon uses the engine default of 2.0, which limits it to
// about 0.4 g: its drive force is far above that, so it spins its wheels and slides. 3.6 gives roughly 0.7 g,
// in line with a real Cybertruck's 0-60 mph in about 4 seconds.
static constexpr float FirmwareWheelFriction = 3.6f;

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
	if (!VanillaDescClass || !VanillaRecipeClass || !VanillaSchematicClass || !CircuitBoardClass || !PatchedVehicleClass)
	{
		UE_LOG(LogCyberWagonFirmware, Error, TEXT("Missing class: desc=%d recipe=%d schematic=%d circuit=%d vehicle=%d"),
			VanillaDescClass != nullptr, VanillaRecipeClass != nullptr, VanillaSchematicClass != nullptr,
			CircuitBoardClass != nullptr, PatchedVehicleClass != nullptr);
		return false;
	}

	// Our classes' CDOs are native and live in the disregard-for-GC pool, whose references are not traced by the
	// garbage collector. Anything only they point at (loaded classes, the unlock/dependency objects below) would
	// be collected on the next map change and leave dangling pointers, so root it explicitly.
	for (UClass* Class : { VanillaDescClass, VanillaRecipeClass, VanillaSchematicClass, CircuitBoardClass, PatchedVehicleClass })
	{
		Class->AddToRoot();
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
	Unlock->mRecipes = { UCWFU_Recipe::StaticClass() }; // AddRecipe()/Init() are editor-only
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
		VehicleCDO->mInventorySize =  FCWFU_INVENTORY_SLOTS;
		// Cargo: as many slots as the Truck (the vanilla wagon has 1). Wagons from older saves are grown on
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

	FCWFU_SteeringPatch::SetPatchedVehicleClass(PatchedVehicleClass);

	bSetupDone = true;
	UE_LOG(LogCyberWagonFirmware, Log, TEXT("Cyber Wagon firmware content ready (vehicle class %s)"), *PatchedVehicleClass->GetPathName());
	return true;
}
