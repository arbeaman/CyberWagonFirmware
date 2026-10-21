#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCyberWagonFirmware, Log, All);

class FCyberWagonFirmwareModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
};
