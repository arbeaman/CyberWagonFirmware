#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CWFU_WorldSubsystem.generated.h"

/**
 * Finishes setting up the mod's content against the vanilla Cyber Wagon assets and registers the
 * AWESOME Shop schematic with SML. Runs for every game world, before the save is loaded.
 */
UCLASS()
class CYBERWAGONFIRMWARE_API UCWFU_WorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Patches the CDOs of our descriptor, recipe and schematic. Only needs to happen once per process */
	static bool SetupContent();
};
