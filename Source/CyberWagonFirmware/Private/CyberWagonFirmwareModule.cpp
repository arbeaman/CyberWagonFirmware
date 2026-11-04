#include "CyberWagonFirmwareModule.h"
#include "CWFU_GagPatch.h"
#include "CWFU_StationPatch.h"
#include "CWFU_SteeringPatch.h"
#include "CWFU_TuningPatch.h"

DEFINE_LOG_CATEGORY(LogCyberWagonFirmware);

void FCyberWagonFirmwareModule::StartupModule()
{
	// Native hooks crash the editor, so only install them in the packaged game
	if (!WITH_EDITOR)
	{
		FCWFU_SteeringPatch::Install();
		FCWFU_GagPatch::Install();
		FCWFU_TuningPatch::Install();
		FCWFU_StationPatch::Install();
	}
}

IMPLEMENT_GAME_MODULE(FCyberWagonFirmwareModule, CyberWagonFirmware);
