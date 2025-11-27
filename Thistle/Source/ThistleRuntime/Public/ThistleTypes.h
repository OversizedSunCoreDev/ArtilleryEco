// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "ArtilleryBPLibs.h"
#include "EAttributes.h"
#include "GameplayTagContainer.h"
#include "SkeletonTypes.h"
#include "ThistleTypes.generated.h"

namespace ThistleTypes
{
	const FName ContextKey = TEXT("Key Of Controlled Entity");
}

/** CurrentValue.[EAttribConditionOperand](TestValue)  */
UENUM(BlueprintType)
enum class E_AttribConditionOperand : uint8
{
	/** > */
	Greater,
	/** < */
	Less,
	/** == */
	Equal,
	/** <= */
	LessOrEqual,
	/** >= */
	GreaterOrEqual,
};

/** Use POI Key, Vector, Or Both (Key Location+Vector)*/
UENUM(BlueprintType)
enum  class  E_PointOfInterestMode : uint8
{
	VectorOnly, //The vector contains the exact worldspace position of the POI
	KeyOnly, //The PointOfInterestKey is set, and a valid transform can be retrieved from it
	KeyRelativeVec, //Both are set, and the vector is relative to the position of the key's transform.
};

USTRUCT(BlueprintType, meta = (DisplayName = "Skeleton Key"))
struct THISTLERUNTIME_API F_ArtilleryKeyInstanceData
{
	GENERATED_BODY()

	/** Key to use. Unlike basic skeleton keys, this exposes a UProp and supports reflection.*/
	UPROPERTY(VisibleAnywhere, Category = Input)
	FSkeletonKey KeyOf;

	F_ArtilleryKeyInstanceData& operator=(const FSkeletonKey& rhs)
	{
		KeyOf = rhs;
		return *this;
	}

	explicit operator FSkeletonKey() const { return KeyOf; }
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_K2KInstanceData
{
	GENERATED_BODY()

	/** Key to use. */
	UPROPERTY(VisibleAnywhere, Category = Input)
	FSkeletonKey InputKey;
	UPROPERTY(EditAnywhere, Category = Output)
	FSkeletonKey OutputKey;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_KOutOnlyInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	FSkeletonKey OutputKey;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_RelatedKey
{
	GENERATED_BODY()
	/** Key to use. */
	UPROPERTY(VisibleAnywhere, Category = Input)
	FSkeletonKey InputKey;

	UPROPERTY(EditAnywhere, Category = Input)
	E_IdentityAttrib Relationship;

	UPROPERTY(EditAnywhere, Category = Output)
	bool Found;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FSkeletonKey OutputKey;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_SetRelatedKey
{
	GENERATED_BODY()

	/** Source IS_RELATED_BY Relationship TO Updated Key*/
	UPROPERTY(EditAnywhere, Category = Input)
	FSkeletonKey SourceKey; //read only

	/** Relationship connects source to the updated key*/
	UPROPERTY(EditAnywhere, Category = Input)
	E_IdentityAttrib Relationship;

	/** Related Key IS_THE relationship OF Source
	 *  For example, Related Key is the TARGET of Source
	 *				-> player is the Target of Tank
	 *  This is stored as Tank Target Player - because we map From Verb To in the standard fact triple style
	 */
	UPROPERTY(EditAnywhere, Category = Input)
	FSkeletonKey UpdateToRelatedKey;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_TTagInstanceData : public F_ArtilleryKeyInstanceData
{
	GENERATED_BODY()

	/** Tag to check for in the container. */
	UPROPERTY(EditAnywhere, Category = Input)
	FGameplayTag Tag;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_TAttributeInstanceData : public F_ArtilleryKeyInstanceData
{
	GENERATED_BODY()

	/** Tag to check for in the container. */
	UPROPERTY(EditAnywhere, Category = Input)
	E_AttribKey AttributeName;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_TAttributeSetData : public F_TAttributeInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	float Value;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_TVec3InstanceData: public  F_ArtilleryKeyInstanceData
{
	GENERATED_BODY()
		
	/** The vector, if any. Assumed to be worldspace. */
	UPROPERTY(EditAnywhere, Category = Input)
	FVector Vec3;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_TPOIInstanceData: public  F_ArtilleryKeyInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	E_PointOfInterestMode Mode = E_PointOfInterestMode::KeyOnly;
	
	UPROPERTY(VisibleAnywhere, Category = Input, meta = (Optional))
	FSkeletonKey PointOfInterestKey;
	/** The vector, if any. Assumed to be worldspace. */

	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional))
	FVector Vec3 = FVector(NAN, NAN, NAN);

	FVector ShuckPoi(bool& ShuckedSuccessfully) const
	{
		switch (Mode) {
		case E_PointOfInterestMode::VectorOnly:
			ShuckedSuccessfully = !(Vec3.ContainsNaN());
			return Vec3;
		case E_PointOfInterestMode::KeyOnly:
			return UArtilleryLibrary::K2_GetBarrageLocIfAny(PointOfInterestKey, ShuckedSuccessfully);
		case E_PointOfInterestMode::KeyRelativeVec:
			FVector RequiresNANCheck = UArtilleryLibrary::K2_GetBarrageLocIfAny(PointOfInterestKey, ShuckedSuccessfully);
			if (ShuckedSuccessfully)
			{
				if (!Vec3.ContainsNaN())
				{
					ShuckedSuccessfully = true; // this is already set, but this is C++. be explicit.
					return RequiresNANCheck + Vec3;
				}
				ShuckedSuccessfully = false;
				return RequiresNANCheck; // in THEORY you might be able to recover here, so we provide the most we can.
			}
			break;
		}
		
		//this could be a default, but the switch case above is already a mess
		ShuckedSuccessfully = false;
		return FVector::Zero();
	}
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API F_TPOIInstanceNavData : public  F_TPOIInstanceData
{
	GENERATED_BODY()
};

//originally, this was named POIntToPOInt. I know. I know.
USTRUCT(BlueprintType)
struct THISTLERUNTIME_API FPointToPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	F_TPOIInstanceData Source;

	UPROPERTY(EditAnywhere, Category = Parameter)
	F_TPOIInstanceData Target;
};

USTRUCT(BlueprintType)
struct THISTLERUNTIME_API FThistleSphereCastInstanceData : public FPointToPoint
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Radius;

	TSharedPtr<FHitResult> HitResultCache = MakeShared<FHitResult>();
	UPROPERTY(EditAnywhere, Category = Parameter)
	int TicksBetweenCastRefresh = 2;

	//You generally really want this. It's almost always the source key, but not ALWAYS.
	//Once in a while though, you'll be casting from empty air without an actual source body.
	UPROPERTY(EditAnywhere, Category = Parameter)
	FSkeletonKey SourceBodyKey_SetOrRegret;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FHitResult Outcome;
};

namespace ThistleTypes
{
	typedef E_PointOfInterestMode InterestMode;
	typedef E_AttribConditionOperand TreeOperand;
	//Don't use this generally. It's worse than just a skeleton key. useful for casting down to tho

	static bool TreeOperandTest(float Value, float Target, TreeOperand Operation)
	{
		if (Value == Target && (Operation == TreeOperand::Equal || Operation == TreeOperand::LessOrEqual || Operation == TreeOperand::GreaterOrEqual)) 
		{
			return true;
		}
		if (Value > Target && (Operation == TreeOperand::Greater || Operation == TreeOperand::GreaterOrEqual))
		{
			return true;
		}
		if (Value < Target && (Operation == TreeOperand::Less || Operation == TreeOperand::LessOrEqual))
		{
			return true;
		}
		return false;
	}
}

