// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "ArtilleryControlComponent.h"

#include "ArtilleryDispatch.h"
#include "UArtilleryGameplayTagContainer.h"
#include "FAttributeMap.h"
#include "TransformDispatch.h"
#include "UEnemyMachine.generated.h"

UCLASS(DefaultToInstanced)
class ARTILLERYRUNTIME_API UEnemyMachine : public UArtilleryFireControl
{
	GENERATED_BODY()

public:
	//IF YOU DO NOT CALL THIS FROM THE GAMETHREAD, YOU WILL HAVE A BAD TIME.
	ActorKey CompleteRegistrationWithAILocomotionAndParent(TMap<AttribKey, double> Attributes, ActorKey Key)
	{
		MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
		TransformDispatch =  GetWorld()->GetSubsystem<UTransformDispatch>();
		InitAsNeededModifyingKey(Attributes, Key);//modifies parentkey!!!
		UArtilleryDispatch* ArtilleryDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
		ArtilleryDispatch->RequestRouter->MobileAI(Key, ArtilleryDispatch->GetShadowNow());
		return ParentKey;
	}
	
	//IF YOU DO NOT CALL THIS FROM THE GAMETHREAD, YOU WILL HAVE A BAD TIME.
	ActorKey InitAsNeededModifyingKey(TMap<AttribKey, double> Attributes, ActorKey Key)
	{
		//these are initialized earlier under all intended orderings, but we cannot ensure that this function will be called correctly
		//so we should do what we can to foolproof things. As long as the world subsystems are up, force-updating
		//here will either:
		//work correctly
		//fail fast
		//make a key yo
		ParentKey = Key;
		//UE_LOG(LogTemp, Warning, TEXT("EnemyMachine Parented: %llu"), Key.Obj);
		MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
		TransformDispatch =  GetWorld()->GetSubsystem<UTransformDispatch>();

		Usable = true;
		if (MyAttributes == nullptr)
		{
			MyAttributes = MakeShareable(new FAttributeMap(ParentKey, MyDispatch, Attributes));
		}
		else
		{
			MyAttributes->Add(Attributes);
		}
		if (MyTags == nullptr || !MyTags->ReadyToUse)
		{
			MyTags = NewObject<UArtilleryGameplayTagContainer>();
			MyTags->Initialize(Key, MyDispatch);
		}
		//UE_LOG(LogTemp, Warning, TEXT("Enemy Mana: %f"), MyDispatch->GetAttrib(Key, Attr::Mana)->GetCurrentValue());
		return Key;
	}                                

	virtual void PushGunToFireMapping(const FGunKey& ToFire) override
	{
		Super::PushGunToFireMapping(ToFire);
	}

	virtual void PopGunFromFireMapping(const FGunKey& ToRemove) override
	{
		Super::PopGunFromFireMapping(ToRemove);
	}

	virtual void InitializeComponent() override
	{
		Super::InitializeComponent();
	}

	//this happens post init but pre begin play, and the world subsystems should exist by this point.
	virtual void ReadyForReplication() override
	{
		Super::ReadyForReplication();
	}                               
	
	virtual void BeginPlay() override
	{
		Super::BeginPlay();
	}

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override
	{
		Super::OnComponentDestroyed(bDestroyingHierarchy);
	}
};
