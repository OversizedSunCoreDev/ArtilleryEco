// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kines.h"
#include "ORDIN.h"
#include "QuaternionQuantizer.h"
#include "SkeletonTypes.h"
#include "SwarmKine.h"
#include "seq/concurrent_map.hpp"
#include "Subsystems/WorldSubsystem.h"

THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
typedef seq::concurrent_map<FSkeletonKey, TSharedPtr<Kine>> KineLookup;
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

#include "TransformDispatch.generated.h"
//1p1c ring buffer that trades space for reduced atomics.
class TransformUpdateRing : public
ExportTemplateStream::FConservedStream<29491, 32768, TransformUpdate, TransformUpdate>
{
public:
	virtual void Add(TransformUpdate shell, long SentAt) override
	{
		this->CurrentHistory[this->highestInput] = shell;
		++(this->highestInput);
	}
	virtual void Add(TransformUpdate Copy) override
	{
		this->CurrentHistory[this->highestInput] = Copy;
		++(this->highestInput);
	}

	void AddMove(const TransformUpdate& input)
	{
		this->CurrentHistory[this->highestInput] = input;
		++(this->highestInput);
	}

	//there's gotta be a genuinely fast way to do this.
	inline void AddMove(const FSkeletonKey& ObjectKey,
	const uint64& sequence,
	const FQuat4f& Rotation,// this alignment looks wrong. Like outright wrong.
	const FVector3f& Position)
	{
		this->CurrentHistory[this->highestInput].sequence = sequence;
		this->CurrentHistory[this->highestInput].Rotation = Rotation;
		this->CurrentHistory[this->highestInput].ObjectKey = ObjectKey;
		this->CurrentHistory[this->highestInput].Position = Position;
		++(this->highestInput);
	}
	
	
	//encap for ease of use
	uint64 ConsumerOnlyLastReadScratch = 0;
};
using TransformUpdatesForGameThread = TransformUpdateRing;
/**
 * Transform dispatch is the core use case of the skeleton key system. It allows you to assign a key to an arbitrary
 * transform, effectively letting you create implicit objects and actors. This also lets you later use Artillery to bind
 * attributes to any of these transforms. I recognize that this is a strange model compared to the UE norm, but it's actually
 * not particularly different from the mesh managers, lightweight instances, and proxies, and in fact,
 * I expect we'll integrate with said systems.
 *
 * Effectively, this separates ephemeral data-driven mutations from the UObject type system without breaking type safety.
 */
UCLASS()
class SKELETONKEY_API UTransformDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()
	FSkeletonKey DefaultObjectKey;
	UTransformDispatch();
	virtual ~UTransformDispatch() override;


public:

	constexpr static double MagicTolerance = 0.57;
	constexpr static double MysticTolerance = 0.065;
	constexpr static int OrdinateSeqKey = ORDIN::FirstSeqKey;
	//honestly, it's gonna get used everywhere. You break it, you buy it.
	static inline UTransformDispatch* SelfPtr = nullptr;
	void RegisterObjectToShadowTransform(FSkeletonKey Target, TObjectPtr<AActor> Original) const;
	void RegisterSceneCompToShadowTransform(FBoneKey Target, TObjectPtr<USceneComponent> Original) const;
	void RegisterObjectToShadowTransform(FSkeletonKey Target, USwarmKineManager* Manager) const;

	//this provides support for new kinds of kines transparent to skeletonkey. kine bravely!
	//for many many kines, manager is going to be a self pointer, but not all!
	template <class KineType, class TargetKeyType, class TargetTypeManager>
	void RegisterObjectToShadowTransform(TargetKeyType Target, TargetTypeManager Manager) const
	{
		//explicitly cast to parent type.
		TSharedPtr<Kine> kine = MakeShareable<KineType>(new KineType(Manager, Target));
		ObjectToTransformMapping->insert_or_assign(Target, kine);
	}

	TSharedPtr<Kine> GetKineByObjectKey(FSkeletonKey Target) const;
	TSharedPtr<ActorKine> GetActorKineByObjectKey(FSkeletonKey Target) const;
	TWeakObjectPtr<AActor> GetAActorByObjectKey(FSkeletonKey Target) const;
	
	//OBJECT TO TRANSFORM MAPPING IS CALLED FROM MANY THREADS
	//Unfortunately, we ended up needed to hide an actor ref inside the Kine. Libcuckoo does help here, since we at least won't
	//get partial record writes, but we ultimately need a way to make that safer than it is.
	//TODO Can we get away from the actor ref? It's the last real barrier between us and true thread safety.
	TSharedPtr<KineLookup> ObjectToTransformMapping;
	void ReleaseKineByKey(FSkeletonKey Target);

	//right now, this is only a helper method, but if we add the read-only copy in the kine itself, we could conceivably
	//use this as a one-frame conserve without a lock. 
	TOptional<FTransform3d> CopyOfTransformByObjectKey(FSkeletonKey Target);

	//it's not clear if this can be made safe to call off gamethread. It's an unfortunate state of affairs to be sure.
	bool ApplyTransformUpdates(TSharedPtr<TransformUpdateRing> TransformUpdateQueue);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	//BEGIN OVERRIDES
	
	virtual void Deinitialize() override;
	virtual TStatId GetStatId() const override;
	virtual bool RegistrationImplementation() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void PostInitialize() override;
	virtual void PostLoad() override;
	virtual void Tick(float DeltaTime) override;
};

inline bool UTransformDispatch::ApplyTransformUpdates(TSharedPtr<TransformUpdateRing> TransformUpdateQueue)
{
	if(GetWorld() && !GetWorld()->bPostTickComponentUpdate)
	{
		//process updates from barrage.
		auto HoldOpen = TransformUpdateQueue;

		//This applies the update from Jolt. If this runs to slow, we have three options
		//1) switch to using render proxies per
		//https://www.youtube.com/watch?v=JaCf2Qmvy18
		//2) add tick instruction batching, allowing this to execute in batched fashion
		//3) add the parallel execute machinery in for "end of frame sync" or parallel execute.
		//we likely actually want to use FPrimitiveSceneProxy and other proxies
		
		GetWorld()->bPostTickComponentUpdate = 0;
		while(HoldOpen && HoldOpen->ConsumerOnlyLastReadScratch < HoldOpen->highestInput && !GetWorld()->bPostTickComponentUpdate)
		{
			auto Update = HoldOpen->get(HoldOpen->ConsumerOnlyLastReadScratch);
			
			if(Update.has_value() && !GetWorld()->bPostTickComponentUpdate)
			{
				try
				{
					if(TSharedPtr<Kine> BindOriginal = this->GetKineByObjectKey(Update->ObjectKey) )
					{
						//kinescope would normally be passed in, but we've removed that idiom.
						auto CurrentDisplayTransform = BindOriginal->CopyOfTransformLike();
						if (CurrentDisplayTransform.IsSet())
						{
							FQuat4d Rr = FQuat4d(Update->Rotation.GetNormalized() );
							//todo: jitter handling should probably prevent update EMISSION.
							auto dotR = CurrentDisplayTransform.GetValue().GetRotation() | Rr;
							FVector3d Loc = FVector3d(Update->Position);
							if ((1-(dotR*dotR) >= MysticTolerance) || FVector::Dist(CurrentDisplayTransform->GetLocation(), Loc) >= MagicTolerance)
							{
								BindOriginal->SetLocationAndRotationWithScope(
									Loc,
									Rr);
							}
						}
					}
					
				}
				catch (...)
				{
					return false; //we'll be back! we'll be back!!!!
				}
			}
		++HoldOpen->ConsumerOnlyLastReadScratch;

		}
		return true;
	}
	return false;
}
