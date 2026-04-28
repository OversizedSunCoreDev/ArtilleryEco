// Fill out your copyright notice in the Description page of Project Settings.
#include "TransformDispatch.h"

#include "ORDIN.h"
#include "SwarmKine.h"

UTransformDispatch::UTransformDispatch()
{
	ObjectToTransformMapping = MakeShareable(new KineLookup(4096));
}

UTransformDispatch::~UTransformDispatch()
{
}

void UTransformDispatch::RegisterObjectToShadowTransform(FSkeletonKey Target, TObjectPtr<AActor> Self) const
{
	//explicitly cast to parent type.
	TSharedPtr<Kine> kine = MakeShareable<ActorKine>(new ActorKine(Self, Target));
	ObjectToTransformMapping->insert_or_assign(Target, kine);
}

void UTransformDispatch::RegisterSceneCompToShadowTransform(FBoneKey Target,
	TObjectPtr<USceneComponent> Original) const
{
	TSharedPtr<Kine> kine = MakeShareable<BoneKine>(new BoneKine(Original, Target));
	ObjectToTransformMapping->insert_or_assign(FSkeletonKey(Target), kine);
}

void UTransformDispatch::RegisterObjectToShadowTransform(FSkeletonKey Target, USwarmKineManager* Manager) const
{
	//explicitly cast to parent type.
	TSharedPtr<Kine> kine = MakeShareable<SwarmKine>(new SwarmKine(Manager, Target));
	ObjectToTransformMapping->insert_or_assign(Target, kine);
}

inline TSharedPtr<Kine> UTransformDispatch::GetKineByObjectKey(FSkeletonKey Target) const
{
	TSharedPtr<KinematicRef> ref;
	ObjectToTransformMapping->visit(Target, [&ref](auto& a){ref = a.second;});
	return ref ? ref : nullptr;
}


TSharedPtr<ActorKine> UTransformDispatch::GetActorKineByObjectKey(FSkeletonKey Target) const
{
	TSharedPtr<Kine> ref;
	ObjectToTransformMapping->visit(Target, [&ref](auto& a){ref = a.second;});
	// TODO: this isn't safe, will probably throw if its not actually an ActorKine
	return ref ? StaticCastSharedPtr<ActorKine>(ref) : nullptr;
}

TWeakObjectPtr<AActor> UTransformDispatch::GetAActorByObjectKey(FSkeletonKey Target) const
{
	TSharedPtr<ActorKine> ActorKinePtr = GetActorKineByObjectKey(Target);
	return ActorKinePtr.IsValid() && Target.IsValid() ? ActorKinePtr->MySelf : nullptr;
}

//actual release happens 
void UTransformDispatch::ReleaseKineByKey(FSkeletonKey Target)
{
	if(Target)
	{
		TSharedPtr<KineLookup> HoldOpen = ObjectToTransformMapping;
		if(HoldOpen)
		{
			HoldOpen->erase(Target);
		}
	}
}

TOptional<FTransform> UTransformDispatch::CopyOfTransformByObjectKey(FSkeletonKey Target) 
{
	TSharedPtr<KinematicRef> ref;
	ObjectToTransformMapping->visit(Target, [&ref](auto& a){ref = a.second;});
	return ref ? ref.Get()->CopyOfTransformLike() : TOptional<FTransform>();
}

TStatId UTransformDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTransformDispatch, STATGROUP_Tickables);
}

bool UTransformDispatch::RegistrationImplementation()
{
	UE_LOG(LogTemp, Warning, TEXT("Shadow Transforms Subsystem: Online"));
	SelfPtr = this;
	return true;
}

void UTransformDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UTransformDispatch::Deinitialize()
{
	SelfPtr = nullptr;
	ObjectToTransformMapping->clear();
	
	Super::Deinitialize();
}

void UTransformDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UTransformDispatch::PostInitialize()
{
	Super::PostInitialize();
}

void UTransformDispatch::PostLoad()
{
	Super::PostLoad();
}

void UTransformDispatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
