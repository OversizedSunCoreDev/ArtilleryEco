// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "FArtilleryGun.h"
#include "FGunKey.h"
#include "UObject/Object.h"
#include "ArtilleryGunBlueprint.generated.h"

class UArtilleryDispatch;
struct FGunInstanceKey;




struct FArtilleryDispatchThreadScope
{
	FArtilleryDispatchThreadScope(UArtilleryDispatch* InDispatch);;
	
	~FArtilleryDispatchThreadScope();

	static thread_local UArtilleryDispatch* ArtilleryDispatch;
	static thread_local UBarrageDispatch* BarrageDispatch;
#ifdef JPH_DEBUG_RENDERER
	static thread_local JPH::DebugRenderer* DebugRenderer;
#endif
};


/**
 * Blueprints designed to run in the artillery thread which is important but scary to rely on for thread safety
 * The #1 thing to understand here is that THESE SHOULD NOT HAVE DYNAMIC MEMBER VARIABLES! 
 * Having member variables is okay, but only if they never change from the default.
 * To use this is has to be in your artillery data table currently
 */
UCLASS(Abstract, Blueprintable, Const)
class ARTILLERYRUNTIME_API UArtilleryGunBlueprint : public UObject
{
	GENERATED_BODY()
	
	UArtilleryGunBlueprint();
public:
	// The GunKey is provided by Artillery
	UFUNCTION(BlueprintImplementableEvent)
	bool InitializeGun(UArtilleryDispatch* Artillery, const FGunKey& GunKey, FSkeletonKey GunInstance, FSkeletonKey Owner) const;
	
	UFUNCTION(BlueprintImplementableEvent)
	void PreFireGun(UArtilleryDispatch* Artillery, const FGunKey& GunKey, FSkeletonKey GunInstance, FSkeletonKey Owner) const;
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable,BlueprintPure=false, meta=(HideSelfPin="true"))
	void FireGun(UArtilleryDispatch* Artillery, const FGunKey& GunKey, FSkeletonKey GunInstance, FSkeletonKey Owner) const;
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable,BlueprintPure=false, meta=(HideSelfPin="true"))	
	void PostFireGun(UArtilleryDispatch* Artillery, const FGunKey& GunKey, FSkeletonKey GunInstance, FSkeletonKey Owner) const;
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnGunProjectileCollided(UArtilleryDispatch* Artillery, const FGunKey& GunKey, FSkeletonKey GunInstance, FSkeletonKey Owner, const FSkeletonKey& Projectile, const FSkeletonKey& HitEntity) const;
	
	// An experimental idea to sort of extend a struct of a parent gun.. we have many ways to make this better (does NOT do anything right now, just a nice example)
	UPROPERTY(EditDefaultsOnly, meta = (MetaStruct="/Script/ArtilleryRuntime.ArtilleryGun"))
	TSoftObjectPtr<UScriptStruct> ParentGunStruct;
	
	// UObject interface
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;

#if WITH_EDITOR
	virtual void PostCDOCompiled(const FPostCDOCompiledContext& Context) override;
	
	// Track these instances in the editor for reinstancing (somewhat evil but very powerful)
	void TrackGunPtr(struct FArtilleryGunBlueprintWrapper* GunPtr);
	void RemoveTrackedGunPtr(struct FArtilleryGunBlueprintWrapper* GunPtr);
	
	FCriticalSection MyGunInstancesForResinstancingCS;
	TSet<FArtilleryGunBlueprintWrapper*> MyGunInstancesForResinstancing_RequiresCriticalSection;
#endif
	// End of UObject interface
};


// The idea is to forward native C++ calls to a singleton bp instance of a UArtilleryGunBlueprint (maybe even just the cdo but I would rather not)
USTRUCT()
struct ARTILLERYRUNTIME_API FArtilleryGunBlueprintWrapper: public FArtilleryGun
{
	GENERATED_BODY()
	
	FArtilleryGunBlueprintWrapper() {};
	virtual ~FArtilleryGunBlueprintWrapper() override;
	FArtilleryGunBlueprintWrapper(const FGunKey& KeyFromDispatch, UArtilleryDispatch* Dispatch);
	

	UArtilleryGunBlueprint* GetObjectInstanceFromDispatchLoader();
	virtual void PreFireGun(FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile,
		bool VerifiedFrame = false,
		const EventBufferInfo FireAction = EventBufferInfo::Default()) override;
	virtual void FireGun(FArtilleryStates OutcomeStates, int DallyFramesToOmit, bool RerunDueToReconcile) override;
	virtual void PostFireGun(FArtilleryStates OutcomeStates, int DallyFramesToOmit, bool RerunDueToReconcile) override;
	virtual bool Initialize(const FGunKey& KeyFromDispatch,
		const bool MyCodeWillSetGunKey,
		UAGunBitBP* PF = nullptr,
		UAGunBitBP* PFC = nullptr,
		UAGunBitBP* F = nullptr,
		UAGunBitBP* FC = nullptr,
		UAGunBitBP* PtF = nullptr,
		UAGunBitBP* PtFc = nullptr,
		UAGunBitBP* FFC = nullptr) override;
	virtual void ProjectileCollided(const FSkeletonKey ProjectileKey, const FSkeletonKey HitEntity) override;

	TStrongObjectPtr<UArtilleryGunBlueprint> ArtilleryBlueprintObject;

	

};


