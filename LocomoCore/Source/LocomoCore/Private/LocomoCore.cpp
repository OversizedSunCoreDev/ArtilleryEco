// Copyright Hedra Group. All Rights Reserved.


#include "LocomoCore.h"
#include "LocomoUtil.h"

#define LOCTEXT_NAMESPACE "FLocomoModuleAPI"



void FLocomoModuleAPI::StartupModule()
{
	//YOU CAN FORCE COMMAND LINE ARGS HERE. it should be early enough to work in all cases, but it will certainly work for PIE.
	auto num =  FPlatformMisc::NumberOfWorkerThreadsToSpawn();
	num = FMath::Max(num/2, 4);
	FCommandLine::Append(
		GetData(FString::Printf(TEXT(" -corelimit=%u "), num))
			);
}

void FLocomoModuleAPI::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	

IMPLEMENT_MODULE(FLocomoModuleAPI, LocomoCore);