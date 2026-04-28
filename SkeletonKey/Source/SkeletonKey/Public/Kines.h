#pragma once

#include <functional>

#include "SkeletonTypes.h"


class KineData 
{
public:
	virtual ~KineData() = default;
	FSkeletonKey MyKey; 
	//I don't recommend adding data here, but this base class is provided in case you do not use barrage and need a place to keep shadow or
	//scratch transforms.
};


//Kines serve to connect skeleton keys and their associated simulation data to the transforms of various UE level constructs.
//So an actor kine connects a skeleton key to the transform of an actor. You can actually think of a kine as a fact about
//a key, rather than the other way around, and they're the capability that the transform dispatch offers.
//in that sense, the transform dispatch is just another ECS pillar, and kines are just the function objects that it tracks

class KinematicRef : public KineData
{
public:
	TOptional<FTransform> CopyOfTransformLike()
	{
		if(MyKey)
		{
			return  CopyOfTransformlike_Impl();
		}
		return TOptional<FTransform>();
	}
	virtual void SetTransformlike(FTransform Input) = 0;
	virtual void SetLocation( FVector3d Location) = 0;
	virtual void SetRotation (FQuat4d Rotation) = 0;
	virtual void SetLocationAndRotation(FVector3d Loc, FQuat4d Rot) = 0;
	virtual void SetLocationAndRotationWithScope(FVector3d Loc, FQuat4d Rot) = 0;
	bool IsNull() const { return MyKey == 0;}
protected:
	virtual TOptional<FTransform> CopyOfTransformlike_Impl() = 0;
};
//You can assess the broad type of the Kine's semantic meaning with the hidden runtime type infix found in every ObjectKey.
//This constant-time single-layer reflection trick allows runtime typesafety without allowing deep hierarchy.
using Kine = KinematicRef;

class ActorKine;

class ActorKine : public Kine
{
	//static inline FActorComponentTickFunction FALSEHOOD_MALEVOLENCE_TRICKERY = FActorComponentTickFunction();
public:
	TWeakObjectPtr<AActor> MySelf;
	
	explicit ActorKine(const TWeakObjectPtr<AActor>& MySelf, const ActorKey& Target)
		: MySelf(MySelf)
	{
		MyKey = Target;
	}

	virtual void SetLocationAndRotation(FVector3d Loc, FQuat4d Rot) override
	{
		TObjectPtr<AActor> Pin;
		Pin = MySelf.Get();
		if(Pin)
		{
			auto Ref = Pin->GetRootComponent();
			if(Ref)
			{
				auto transform = Ref->GetComponentTransform();
				transform.SetLocation(Loc);
				transform.SetRotation(Rot);
				Ref->SetWorldTransform(transform);
				Ref->MarkRenderTransformDirty();
			}
		}
	}

	virtual void SetLocationAndRotationWithScope(FVector3d Loc, FQuat4d Rot) override
	{
		TObjectPtr<AActor> Pin;
		Pin = MySelf.Get();
		if(Pin)
		{
			auto Ref = Pin->GetRootComponent();
			if(Ref)
			{
				Ref->SetWorldLocationAndRotationNoPhysics(Loc,Rot.Rotator());
			}
		}
	}
	virtual TOptional<FTransform> CopyOfTransformlike_Impl() override
	{
		TObjectPtr<AActor> Pin;
		Pin = MySelf.Get();
		if(Pin)
		{
			return Pin->GetActorTransform();
		}
		return TOptional<FTransform>();
	}

	virtual void SetTransformlike(FTransform Input) override
	{
		TObjectPtr<AActor> Pin;
		Pin = MySelf.Get();
		if(Pin)
		{
			Pin->SetActorTransform(Input, false, nullptr, ETeleportType::None);
		}
	}

	virtual void SetLocation(FVector3d Location) override
	{
		TObjectPtr<AActor> Pin;
		Pin = MySelf.Get();
		if(Pin)
		{

			auto Ref = Pin->GetRootComponent();
			if(Ref)
			{
				auto transform = Ref->GetComponentTransform();
				transform.SetLocation(Location);
				Ref->SetWorldTransform(transform);
			}
		}
	}

	virtual void SetRotation(FQuat4d Rotation) override
	{
		TObjectPtr<AActor> Pin;
		Pin = MySelf.Get();
		if(Pin)
		{
			
			auto Ref = Pin->GetRootComponent();
			if(Ref)
			{
				auto transform = Ref->GetComponentTransform();
				transform.SetRotation(Rotation);
				Ref->SetWorldTransform(transform);
			}
		}
	}
};

class BoneKine : public Kine
{
public:
	TWeakObjectPtr<USceneComponent> MySelf;
	
	explicit BoneKine(const TWeakObjectPtr<USceneComponent>& MySelf, const FBoneKey& Target)
		: MySelf(MySelf)
	{
		MyKey = Target;
	}

	virtual void SetLocationAndRotation(FVector3d Loc, FQuat4d Rot) override
	{
		TObjectPtr<USceneComponent> Pin = MySelf.Get();
		if(Pin)
		{
			Pin->SetWorldLocationAndRotationNoPhysics(Loc, Rot.Rotator());
		}
	}
	virtual void SetLocationAndRotationWithScope(FVector3d Loc, FQuat4d Rot) override
	{
		TObjectPtr<USceneComponent> Pin = MySelf.Get();
		if(Pin)
		{
			Pin->SetWorldLocationAndRotationNoPhysics(Loc, Rot.Rotator());
		}
	}

	virtual TOptional<FTransform> CopyOfTransformlike_Impl() override
	{
		TObjectPtr<USceneComponent> Pin = MySelf.Get();
		if(Pin)
		{
			return Pin->GetComponentTransform();
		}
		return TOptional<FTransform>();
	}

	virtual void SetTransformlike(FTransform Input) override
	{
		TObjectPtr<USceneComponent> Pin = MySelf.Get();
		if(Pin)
		{
			Pin->SetWorldTransform(Input, false, nullptr, ETeleportType::None);
		}
	}

	virtual void SetLocation(FVector3d Location) override
	{
		TObjectPtr<USceneComponent> Pin = MySelf.Get();
		if(Pin)
		{

			auto transform = Pin->GetComponentTransform();
			transform.SetLocation(Location);
			Pin->SetWorldTransform(transform);
	
		}
	}

	virtual void SetRotation(FQuat4d Rotation) override
	{
		TObjectPtr<USceneComponent> Pin = MySelf.Get();
		if(Pin)
		{
			auto transform = Pin->GetComponentTransform();
			transform.SetRotation(Rotation);
			Pin->SetWorldTransform(transform);
		}
	}
};


//@todo fill this out (will be done very shortly once the queue to push up is in place)
class SkeletalMeshKine : public Kine
{
public:
	
	virtual void SetBonePose(const TArray<FTransform>& ComponentSpaceTransforms, const TArray<FTransform>& BoneSpaceTransforms);
	
	virtual void SetTransformlike(FTransform Input) override;
	virtual void SetLocation(FVector3d Location) override;
	virtual void SetRotation(FQuat4d Rotation) override;
	virtual void SetLocationAndRotation(FVector3d Loc, FQuat4d Rot) override;
	virtual void SetLocationAndRotationWithScope(FVector3d Loc, FQuat4d Rot) override;

protected:
	virtual TOptional<FTransform> CopyOfTransformlike_Impl() override;

public:
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComp;
};
