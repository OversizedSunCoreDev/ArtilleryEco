// Fill out your copyright notice in the Description page of Project Settings.


#include "JoltPhysicsModule.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include "Jolt/ConfigurationString.h"

#ifdef JPH_EXTERNAL_PROFILE
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Jolt/Core/Profiler.h"
#endif

#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR
#include "HAL/LowLevelMemTracker.h"
LLM_DEFINE_TAG(Jolt);
#endif

DEFINE_LOG_CATEGORY(LogJolt)

static void UnrealJoltTrace([[maybe_unused]] const char *inFMT, ...)
{
	char Buffer[1024];
	va_list Args;
	va_start(Args, inFMT);
	vsnprintf(Buffer, sizeof(Buffer), inFMT, Args);
	va_end(Args);

	UE_LOG(LogJolt, Log, TEXT("%hs"), Buffer);
};

void FJoltPhysicsModule::StartupModule()
{
	// Register allocation hook
#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR
	// Unreal: custom allocators and low level memory tracking (just in case)
	// I am unsure if it's worth using unreal allocators here or not. Definitely worth profiling a bit
	JPH::Allocate = [](size_t inSize)
	{
		LLM_SCOPE_BYTAG(Jolt);
		return FMemory::Malloc(inSize);
	};
	
	JPH::Reallocate = [](void* inBlock, size_t inOldSize, size_t inNewSize)
	{
		LLM_SCOPE_BYTAG(Jolt);
		// what on earth is inOldSize for?
		return FMemory::Realloc(inBlock, inNewSize);
	};

	JPH::Free = [](void* inBlock)
	{
		LLM_SCOPE_BYTAG(Jolt);
		FMemory::Free(inBlock);
	};
	
	JPH::AlignedAllocate = [](size_t inSize, size_t inAlignment)-> void* {
		LLM_SCOPE_BYTAG(Jolt);
		return FMemory::Malloc(inSize, inAlignment);
	};
	
	JPH::AlignedFree = [](void* inBlock)
	{
		LLM_SCOPE_BYTAG(Jolt);
		FMemory::Free(inBlock);
	};
#endif
	
	JPH::Trace = UnrealJoltTrace;
	
	// Profiling callbacks. 
	// Currently jolt traces shape querying down to individual triangles which is a bit much...
	// It will probably be more reasonable to just profile the higher-level task workers if you find this is slow
#ifdef JPH_EXTERNAL_PROFILE
	/// Functions called when a profiler measurement starts or stops, need to be overridden by the user.
	JPH::ProfileStartMeasurement = [](const char *inName, uint32 inColor, uint8 *ioUserData)
	{
		FCpuProfilerTrace::OutputBeginDynamicEvent(inName);
	};
	
	JPH::ProfileEndMeasurement = [](uint8 *ioUserData)
	{
		FCpuProfilerTrace::OutputEndEvent();
	};
#endif
	
#ifdef JPH_ENABLE_ASSERTS
	// Callback for asserts, connect this to your own assert handler if you have one
	JPH::AssertFailed = [](const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
	{
		// Arguably should be ensure always... I would prefer to just use unreal's macro directly in Jolt almost
		ensureMsgf(false, TEXT("Jolt assert: %hs, %hs, %hs:%i"), inExpression, inMessage, inFile, inLine);
		return true;
	};
#endif
	
	// Create a factory
	JPH::Factory::sInstance = new JPH::Factory();

	// Register all Jolt physics types
	JPH::RegisterTypes();
	
	UE_LOG(LogJolt, Log, TEXT("Unreal Jolt module loaded! Jolt build configuration: %s"), *GetJoltConfigString());
}

void FJoltPhysicsModule::ShutdownModule()
{
	JPH::UnregisterTypes();

	// Destroy the reflection factory (we sure hope it's still here)
	if (ensure(JPH::Factory::sInstance)) {
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
}

FString FJoltPhysicsModule::GetJoltConfigString() 
{
	return JPH::GetConfigurationString();
}

IMPLEMENT_MODULE(FJoltPhysicsModule, JoltPhysics)