#pragma once

#include "CoreMinimal.h"
#include "FGRecipe.h"
#include "FGSchematic.h"
#include "Resources/FGVehicleDescriptor.h"
#include "CWFU_Content.generated.h"

/**
 * Item/build descriptor for the patched wagon. Icons, build-menu category and the vehicle class are
 * filled in at runtime by UCWFU_WorldSubsystem, since they live in Blueprint assets.
 */
UCLASS()
class CYBERWAGONFIRMWARE_API UCWFU_Desc : public UFGVehicleDescriptor
{
	GENERATED_BODY()
public:
	UCWFU_Desc();

protected:
	// The vehicle descriptor may otherwise pull its name from the (vanilla) vehicle actor
	virtual FText GetItemNameInternal() const override;
	virtual FText GetItemDescriptionInternal() const override;
};

/** Build gun recipe for the patched wagon. Ingredients are set at runtime by UCWFU_WorldSubsystem */
UCLASS()
class CYBERWAGONFIRMWARE_API UCWFU_Recipe : public UFGRecipe
{
	GENERATED_BODY()
public:
	UCWFU_Recipe();
};

/**
 * AWESOME Shop entry. Requires the vanilla Cyber Wagon to have been purchased first; cost, icon,
 * shop category, unlock and dependency objects are set at runtime by UCWFU_WorldSubsystem.
 */
UCLASS()
class CYBERWAGONFIRMWARE_API UCWFU_Schematic : public UFGSchematic
{
	GENERATED_BODY()
public:
	UCWFU_Schematic();
};
