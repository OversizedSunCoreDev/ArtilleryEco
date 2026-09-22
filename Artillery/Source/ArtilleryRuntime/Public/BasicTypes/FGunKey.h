// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "skeletonize.h"
#include "SkeletonTypes.h"
#include "CoreTypes.h"
#include "FGunKey.generated.h"

USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FGunKey
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (GetOptions = "ArtilleryRuntime.ArtilleryDeveloperSettings.GetGunNamesForEditorSelection"))
	FName GunDefinitionID; //this will need to be human searchable
	//FUN STORY: BLUEPRINT CAN'T USE UINT64.
	
	UPROPERTY(Transient, VisibleInstanceOnly)
	FGunInstanceKey GunInstanceID;
	
	// ReSharper disable once CppRedundantMemberInitializer
	// THIS DOES NOT DO THE SAME THING AS INSTANCE 0
	FGunKey() : GunDefinitionID("M6D"), GunInstanceID()
	{
	}

	// ReSharper disable once CppRedundantMemberInitializer
	// THIS DOES NOT DO THE SAME THING AS INSTANCE 0
	FGunKey(const FName& Name) : GunDefinitionID(Name), GunInstanceID()
	{
	}
	
	explicit FGunKey(const FName& Name, FGunInstanceKey id) : GunDefinitionID(Name), GunInstanceID(id)
	{
	}
	
	FGunKey(const FName& Name, uint32_t id) : GunDefinitionID(Name), GunInstanceID(id)
	{
	}
	

	operator FSkeletonKey() const
	{	
		
		return FSkeletonKey( 
			//this causes the gun definition id to reside in the meta layer, and uses the POTENTIALLY EMPTY guninstance id for the hash bits.
			//
			SFIX_ImprintKeyDependency(GetTypeHash(GunDefinitionID), GunInstanceID, SKELLY::SFIX_GunOrAbilityPrototypeKey)
			);
	}
	
	friend uint32 GetTypeHash(const FGunKey Other)
	{
		//WARNING: TYPEHASH CONSIDERED HARMFUL
		//originally, we just used typehash for, well, typehash.
		//it was extremely not fine. TypeHash is fucked for scalars of 4 bytes or less. don't use it.
		return HashCombineFast(GetTypeHash(Other.GunDefinitionID), FMMM::FastHash32(Other.GunInstanceID));
	}

	bool IsValidInstance() const { return GunInstanceID.Obj != 0; }

	static FGunKey Invalid() { return FGunKey(); }
};

static bool operator==(FGunKey const& lhs, FGunKey const& rhs) {
	return (lhs.GunDefinitionID == rhs.GunDefinitionID) && (lhs.GunInstanceID == rhs.GunInstanceID);
}

//when sorted, gunkeys follow their instantiation order!
static bool operator<(FGunKey const& lhs, FGunKey const& rhs) {
	return (lhs.GunInstanceID < rhs.GunInstanceID);
}

//see you soon, chief...
static const FGunKey DefaultGunKey = FGunKey("M6D");