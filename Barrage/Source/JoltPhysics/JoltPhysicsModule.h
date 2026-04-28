// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogJolt, Log, All);

class FJoltPhysicsModule: public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	static FString GetJoltConfigString();
};