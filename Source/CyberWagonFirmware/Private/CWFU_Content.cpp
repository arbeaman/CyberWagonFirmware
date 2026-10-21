#include "CWFU_Content.h"

#define LOCTEXT_NAMESPACE "CyberWagonFirmware"

static FText GetWagonName()
{
	return LOCTEXT("WagonName", "Cyber Wagon (Firmware Update)");
}

static FText GetWagonDescription()
{
	return LOCTEXT("WagonDescription",
		"Absolutely indestructible.\r\nNow with Left Turn™ support.\r\n\r\n"
		"Firmware 2.0 patch notes:\r\n- Added steering in the second direction\r\n- Known issue: still not indestructible");
}

UCWFU_Desc::UCWFU_Desc()
{
	mUseDisplayNameAndDescription = true;
	mDisplayName = GetWagonName();
	mDescription = GetWagonDescription();
	mStackSize = EStackSize::SS_ONE;
	mCanBeDiscarded = true;
}

FText UCWFU_Desc::GetItemNameInternal() const
{
	return GetWagonName();
}

FText UCWFU_Desc::GetItemDescriptionInternal() const
{
	return GetWagonDescription();
}

UCWFU_Recipe::UCWFU_Recipe()
{
	mDisplayNameOverride = true;
	mDisplayName = GetWagonName();
	mProduct.Add(FItemAmount(UCWFU_Desc::StaticClass(), 1));
	mManufactoringDuration = 1.0f;
}

UCWFU_Schematic::UCWFU_Schematic()
{
	mType = ESchematicType::EST_ResourceSink;
	mDisplayName = GetWagonName();
	mDescription = LOCTEXT("SchematicDescription",
		"An over-the-air update for your Cyber Wagon. Unlocks the long-requested ability to turn left.");
	mTechTier = 1;
	mMenuPriority = 1.5f; // right after the vanilla Cyber Wagon (1.0)
	mTimeToComplete = 0.0f;
	mIsPlayerSpecific = false;
	mHiddenUntilDependenciesMet = true;
	mDependenciesBlocksSchematicAccess = true;
}

#undef LOCTEXT_NAMESPACE
