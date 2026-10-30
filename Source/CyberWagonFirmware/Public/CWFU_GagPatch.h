#pragma once

#include "CoreMinimal.h"

/**
 * Removes the vanilla Cyber Wagon's deliberate malfunctions, for patched wagons only.
 *
 * - Shake: every 7.5 seconds the Blueprint's SpaceShipFeature event kicks the wagon's mesh with a large, mostly
 *   upward AddImpulseAtLocation aimed at one of its BumpLoc marker components (a corner picked at random).
 *   Impulses aimed at a BumpLoc marker are cancelled; every other impulse goes through.
 * - Sticky throttle: every ~6 seconds the StickyThrottleFailure event may start StickThrottleToFloor, which then
 *   runs every frame for 10-20 seconds and accelerates the wagon to top speed. Both events are blocked.
 */
class CYBERWAGONFIRMWARE_API FCWFU_GagPatch
{
public:
	static void Install();
};
