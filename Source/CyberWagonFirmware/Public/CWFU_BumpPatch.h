#pragma once

#include "CoreMinimal.h"

/**
 * Removes the vanilla Cyber Wagon's periodic shake.
 *
 * Every 7.5 seconds the Blueprint's SpaceShipFeature event kicks the wagon's mesh with a large, mostly upward
 * AddImpulseAtLocation aimed at one of its BumpLoc marker components (a corner picked at random). For patched
 * wagons, impulses aimed at a BumpLoc marker are cancelled; every other impulse goes through.
 */
class CYBERWAGONFIRMWARE_API FCWFU_BumpPatch
{
public:
	static void Install();
};
