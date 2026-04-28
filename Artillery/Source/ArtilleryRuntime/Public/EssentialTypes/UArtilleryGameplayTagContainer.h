// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"

#include "ArtilleryCommonTypes.h"
#include "ArtilleryDispatch.h"
#include "UArtilleryGameplayTagContainer.generated.h"

UENUM(BlueprintType)
/** Whether a tag was added or removed, used in callbacks */
namespace ArtilleryGameplayTagChange {
	enum Type : int
	{		
		/** Event happens when tag is added */
		Added,

		/** Event happens when tag is removed */
		Removed
	};
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnArtilleryGameplayTagChanged, const FSkeletonKey, TargetKey, const FGameplayTag, Tag, const ArtilleryGameplayTagChange::Type, TagChangeType);

//ArtilleryGameplayTagContainers serve as a wrapper for the more general borrow-based lifecycle we offer for the
//fgameplaytagcontainer. This assumes ownership and causes the underlying tagcontainer to share its lifecycle.
UCLASS(BlueprintType)
class ARTILLERYRUNTIME_API UArtilleryGameplayTagContainer : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly)
	FSkeletonKey ParentKey;
	TSharedPtr<FGameplayTagContainer> TagsToAddDuringInitialization;
	FConservedTags MyTags;
	UArtilleryDispatch* MyDispatch = nullptr;
	bool ReadyToUse = false;
	bool ReferenceOnlyMode = true;
	
	// Don't use this default constructor, this is a bad
	UArtilleryGameplayTagContainer()
	{
		if (!HasAnyFlags(RF_ClassDefaultObject)) 
		{
			TagsToAddDuringInitialization = MakeShareable(new FGameplayTagContainer());
		}
	}

	//reference only mode changes the container to act more like you might expect in a blueprint, by not taking ownership of the
	//underlying tag collection. Instead, it will bind to the existing tag collection, if any, and the tag collection will survive
	//the destruction of that container. I've spent a bit of time thinking about this, and it's a pretty despicable hack.
	//On the other hand, it vastly simplifies rollback. On the grasping hand, due to the nature of the key lifecycle, we really need
	//to pick what level construct the tags share their lifecycle with. I don't want to get into a situation where we have a full owner
	//system, either. I think we'll probably end up splitting this into separate ContainerOwner, ContainerRef, and ContainerDisplay
	//classes. That seems the sanest.
	//TODO: revisit NLT 6/8/25
	void Initialize(FSkeletonKey ParentKeyIn, UArtilleryDispatch* MyDispatchIn, bool ReferenceOnly = false)
	{
		this->ParentKey = ParentKeyIn;
		this->MyDispatch = MyDispatchIn;
		bool Found = false;
		auto reference = MyDispatch->GetOrRegisterConservedTags(ParentKeyIn, Found);
		ReferenceOnlyMode = Found == false ?
			false : ReferenceOnly;
		MyTags = reference;
		ReadyToUse = true;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ContainerAddTag", DisplayName = "Add tag to container"),  Category="Artillery|Tags")
	void AddTag(const FGameplayTag& TagToAdd)
	{
		// Only add if the tag doesn't already exist
		if (ensure(MyTags) && !MyTags->Find(TagToAdd))
		{
			MyDispatch->AddTagToEntity(ParentKey, TagToAdd);
		}
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ContainerRemoveTag", DisplayName = "Remove tag from container"),  Category="Artillery|Tags")
	void RemoveTag(const FGameplayTag& TagToRemove)
	{
		// Only remove if the tag does already exist
		if (ensure(MyTags) && MyTags->Find(TagToRemove))
		{
			MyDispatch->RemoveTagFromEntity(ParentKey, TagToRemove);
		}
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ContainerHasTag", DisplayName = "Does Container have tag?"),  Category="Artillery|Tags")
	bool HasTag(const FGameplayTag& TagToCheck) const
	{
		return MyTags->Find(TagToCheck);
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ContainerHasTag", DisplayName = "Does Container have tag?"),  Category="Artillery|Tags")
	bool HasTagExact(const FGameplayTag& TagToCheck) const
	{
		return MyTags->Find(TagToCheck);
	}

	// TODO: expose more of the gameplay tag container's functionality
	virtual ~UArtilleryGameplayTagContainer() override
	{
		if (MyTags)
		{
			//this exploits a quirk of the GC collection sequencing for our mutual friend and allows us to detect
			//a number of things we otherwise shouldn't know.
			TSharedPtr<UnderlyingTagReverse> LiveCycle = MyTags->DecoderRing.Pin();
			if (MyDispatch && !ReferenceOnlyMode && LiveCycle)
			{
				MyDispatch->DeregisterGameplayTags(ParentKey);
			}
		}
	}
};