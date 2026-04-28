// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "ThistleTypes.h"
#include "StateTreeConditionBase.h"
#include "ThistleStateTreeConditions.generated.h"

using namespace ThistleTypes;

USTRUCT(meta = (Hidden))
struct THISTLERUNTIME_API FThistleCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()
};

USTRUCT()
struct THISTLERUNTIME_API FArtilleryKeyComparisonInstanceData
{
	GENERATED_BODY()

	/** Key to use. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FSkeletonKey SourceKey;

	/** Key to use. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FSkeletonKey TargetKey;
};

/**
 * HasTag condition
 * Succeeds if the tag container has the specified tag.
 * 
 * Condition can be used with multiple configurations:
 *	Does TagContainer {"A.1"} has Tag "A" ?
 *		exact match 'false' will SUCCEED
 *		exact match 'true' will FAIL
 */
USTRUCT(DisplayName="Has Tag", Category="Artillery Gameplay Tags")
struct THISTLERUNTIME_API FArtilleryTagMatchCondition : public FThistleCondition
{
	GENERATED_BODY()
	using FInstanceDataType = F_TTagInstanceData;

	FArtilleryTagMatchCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** If true, the tag has to be exactly present, if false then TagContainer will include its parent tags while matching */
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bExactMatch = false;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

/**
 * Check Attribute condition
 */
USTRUCT(DisplayName="Check Attribute", Category="Artillery Attributes")
struct THISTLERUNTIME_API FArtilleryAttributeValueCondition : public FThistleCondition
{
	GENERATED_BODY()

	using FInstanceDataType = F_TAttributeInstanceData;

	FArtilleryAttributeValueCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	bool Test(float Value, float Target) const;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** If true, the tag has to be exactly present, if false then TagContainer will include its parent tags while matching */
	UPROPERTY(EditAnywhere, Category = Condition)
	E_AttribConditionOperand Operation = TreeOperand::Equal;

	UPROPERTY(EditAnywhere, Category = Condition)
	float TestValue = 0.0;
};

/**
 * Compare Attribute Between Keys condition
 */
USTRUCT(DisplayName="Compare Attributes", Category="Artillery Attributes")
struct THISTLERUNTIME_API FArtilleryAttributeCompareCondition : public FArtilleryAttributeValueCondition
{
	GENERATED_BODY()

	using FInstanceDataType = F_TAttributeInstanceData;

	FArtilleryAttributeCompareCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** Should we default to test value if the attribute is missing on the target? */
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bFallbackToTestValue = false;
	
	UPROPERTY(EditAnywhere, Category = Condition)
	FSkeletonKey TestKey;
};

/**
 * Compare Attribute Of Related Key. Comparisons take the form RelatedKey->Attribute [comparison] [TestValue|InputKey->Attribute]
 */
USTRUCT(DisplayName="Compare Attributes Between Related Keys", Category="Artillery Attributes")
struct THISTLERUNTIME_API FArtilleryCompareRelatedCondition : public FArtilleryAttributeValueCondition
{
	GENERATED_BODY()

	using FInstanceDataType = F_TAttributeInstanceData;

	FArtilleryCompareRelatedCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** If this is set, we'll compare the target key's attribute against the same attribute for the related key. */
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bCompareWithTargetKeyAttribute = false;

	/** The relationship between the target key and the key whose attribute we want to test.*/
	UPROPERTY(EditAnywhere, Category = Condition)
	E_IdentityAttrib Relationship = {};
};

/**
 * Compare Keys. Invalid keys return false. Keys of different runtime key types will also return false.
 */
USTRUCT(DisplayName="Compare Artillery Keys", Category="Artillery Attributes")
struct THISTLERUNTIME_API FArtilleryCompareKeys : public FThistleCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FArtilleryKeyComparisonInstanceData;

	FArtilleryCompareKeys() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT(meta = (DisplayName = "LOS Check"))
struct THISTLERUNTIME_API FCheckLoStoPoI : public FThistleCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FThistleSphereCastInstanceData;
	
	FCheckLoStoPoI() = default;

	UPROPERTY(EditAnywhere, Category = Condition, meta = (ToolTip = "This should be set generously. The target point will be the centroid. But we're comparing the target to the impact point."))
	double DistanceFromTargetToTolerate = 10;
	
protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};