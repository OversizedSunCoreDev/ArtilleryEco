// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArtillery, All, All)

class FArtilleryRuntimeModule : public IModuleInterface
{
public:
	//~IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End of IModuleInterface
};
