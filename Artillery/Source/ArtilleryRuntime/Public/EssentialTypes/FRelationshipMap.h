// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"

#include "ArtilleryCommonTypes.h"
#include "ArtilleryDispatch.h"
#include "FRelationshipMap.generated.h"

USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FRelationshipMap
{
	GENERATED_BODY()
	
	IdMapPtr MyRelationships;
	ActorKey ParentKey;
	UArtilleryDispatch* MyDispatch = nullptr;
	bool ReadyToUse = false;

	// Don't use this default constructor, this is a bad
	FRelationshipMap()
	{
		
	};

	FRelationshipMap(ActorKey ParentKeyIn, UArtilleryDispatch* MyDispatchIn, TMap<Ident, FSkeletonKey> DefaultAttributesIn)
	{
		Initialize(ParentKeyIn, MyDispatchIn, DefaultAttributesIn);
	};

	void Initialize(ActorKey ParentKeyIn, UArtilleryDispatch* MyDispatchIn, TMap<Ident, FSkeletonKey> DefaultAttributesIn)
	{
		this->ParentKey = ParentKeyIn;
		this->MyDispatch = MyDispatchIn;

		this->MyRelationships = MakeShareable(new IdentityMap());
		
		//TODO: swap this to loading values from a data table, and REMOVE this fallback.
		//If we want defaults, those defaults should ALSO live in a data table, that way when a defaulting bug screws us
		//maybe we can fix it without going through a full cert using a data only update.
		for(auto x : DefaultAttributesIn)
		{
			MyRelationships->Add(x.Key, MakeShareable(new FConservedAttributeKey));
			MyRelationships->FindChecked(x.Key)->SetBaseValue(x.Value);
			MyRelationships->FindChecked(x.Key)->SetCurrentValue(x.Value);
		}

		MyDispatch->RegisterOrAddRelationships(ParentKey, MyRelationships);

		ReadyToUse = true;
	};
	
	~FRelationshipMap()
	{
		if (MyRelationships != nullptr)
		{
			MyDispatch->DeregisterRelationships(ParentKey);
		}
	}
};