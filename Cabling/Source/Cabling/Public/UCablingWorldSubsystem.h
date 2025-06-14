// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCablingRunner.h"
#include "HAL/Runnable.h"
#include "CablingCommonTypes.h"
#include "KeyedConcept.h"
#include "TransformDispatch.h"
#include "UCablingWorldSubsystem.generated.h"

//Goal: The Cabling Subsystem maintains the cabling thread and provides the output of
//the control polling that it performs to the normal input system. Cabling is not
//intended to replace a full input system, just provide a threaded flow

//This is not a full dispatch, as it possesses no ECS like capabilities to expose.
UCLASS()
class  CABLING_API UCablingWorldSubsystem : public UTickableWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()
public:
	UCablingWorldSubsystem()
	{
	}

	static inline UCablingWorldSubsystem* SelfPtr = nullptr;
	
	//in general, this should be called very seldom, as it is a DESTRUCTIVE
	//and slightly unsafe operation. calling it outside of postinitialize
	//or beginplay is not recommended. instead, clients should get a reference
	//and change what queue they listen on rather than replacing this queue.
	void DestructiveChangeLocalOutboundQueue(Cabling::SendQueue NewlyAllocatedQueue); 
	constexpr static int OrdinateSeqKey = UTransformDispatch::OrdinateSeqKey  + ORDIN::Step;
	virtual bool RegistrationImplementation() override; 
	
protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	//THIS IS THE SHARED SESSION ID. THIS IS NOT IMPLEMENTED YET.
	//THESE ARE ONLY GUARANTEED TO BE UNIQUE PER REFLECTOR ATM.
	//ULTIMATELY, THESE SHOULD BE THE CURRENT MATCH ID AS BRISTLECONE
	//SHOULD NOT BE RUNNING OUTSIDE OF A MATCH.

	friend class UBristleconeWorldSubsystem;

	// Receiver information
	FCabling controller_runner;
	Cabling::SendQueue GameThreadControlQueue;
	Cabling::SendQueue CabledThreadControlQueue;
	TUniquePtr<FRunnableThread> controller_thread;
};
