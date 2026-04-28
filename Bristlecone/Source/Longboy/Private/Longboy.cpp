// Copyright Epic Games, Inc. All Rights Reserved.

#include "Longboy.h"
#include "QuicFactory.h"
#include "LongboyClient.h"
#include "MsQuicRuntimeModule.h"

#define LOCTEXT_NAMESPACE "FLongboyModule"

void FLongboyModule::StartupModule()
{
	if (!FMsQuicRuntimeModule::InitRuntime())
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("Failed to initialize MsQuic runtime. Longboy will not function."));
		return;
	}
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	QuicFactory = MakeUnique<FQuicFactory>();
}

void FLongboyModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	QuicFactory.Reset();
}


TUniquePtr<FLongboyClient> FLongboyModule::CreateClient(const FIPv4Endpoint& RemoteEndpoint)
{
	if (ensureAlwaysMsgf(QuicFactory.IsValid(), TEXT("Cannot create client because the QUIC factory failed to initialize."))) 
	{
		return MakeUnique<FLongboyClient>(QuicFactory->CreateClient(RemoteEndpoint));
	}
	
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLongboyModule, Longboy)
