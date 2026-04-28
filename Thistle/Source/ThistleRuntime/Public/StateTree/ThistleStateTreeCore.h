// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

//"IPropertyAccessEditor.h"
//"StateTreeEditorPropertyBindings.h" // this defines interfaces for data collection on bindables
//"StateTreeEditorData.h"	//this provides the editor-side implementation of the Property Owner

#include "StateTreeTaskBase.h"		//this defines the basic form of an actual tree task.
#include "StateTreeExecutionContext.h"
#include "ThistleBehavioralist.h"
#include "ThistleStateTreeSchema.h"
#include "ThistleTypes.h"
#include "Components/StateTreeComponent.h"
//"StateTreePropertyRef.h"	//this is the ref used to bind properties.
//"StateTreePropertyRefHelpers.h" // here are some "helpers" for refs.
//reference matter can be found at the bottom.
#include "ThistleStateTreeCore.generated.h"
//StateTreeTaskBlueprintBase

//as always, the tests are helpful: StateTreeTestTypes.h
//A ton of the actual stuff is over in the GAMEPLAY statetrees module.
//UE_5.4\Engine\Plugins\Runtime\GameplayStateTree
//Lost a bit of time to that, so if you're looking for code and ref matter, do check there too.
//--J

using namespace ThistleTypes;

USTRUCT(meta = (Hidden))
struct THISTLERUNTIME_API FTTaskBase : public FStateTreeTaskBase
{
	GENERATED_BODY()
};

USTRUCT()
struct THISTLERUNTIME_API FStoreRelationship : public FTTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = F_SetRelatedKey;

protected:
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
};

USTRUCT()
struct THISTLERUNTIME_API FSetTagOfKey : public FTTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = F_TTagInstanceData;
	
protected:
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
};

USTRUCT()
struct THISTLERUNTIME_API FRemoveTagFromKey : public FTTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = F_TTagInstanceData;
	
protected:
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
};

USTRUCT()
struct THISTLERUNTIME_API FStoreToAttribute : public FTTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = F_TAttributeSetData;
	
protected:
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
};

