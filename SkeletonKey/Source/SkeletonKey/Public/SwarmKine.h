#pragma once

#include "SkeletonTypes.h"
#include "Kines.h"
#include "CoreMinimal.h"
#include "InstanceDataTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include "LibSeq/seq/concurrent_map.hpp"
typedef seq::concurrent_map<int32, FSkeletonKey> LibCIntFSK;
typedef seq::concurrent_map<FSkeletonKey, int32> LibCFSKInt;
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END
#include "SwarmKine.generated.h"

class UObject;

//interface that adds support for owning swarm kines. used for managing many many meshes at a time.
//generally, 
UCLASS()
class SKELETONKEY_API USwarmKineManager : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()
	
public:
	virtual ~USwarmKineManager() override;
	typedef int32 IDTYPE;
	TSharedPtr<TCircularQueue<IDTYPE>> ToRemove;
	
	USwarmKineManager()
	{
		PrimaryComponentTick.bCanEverTick = true;
		ToRemove = MakeShareable(new TCircularQueue<IDTYPE>(2048));
		KeyToMesh = MakeShareable(new LibCFSKInt());
		MeshToKey = MakeShareable(new LibCIntFSK());
		KeyToSceneComponent = MakeShareable(new TMap<FSkeletonKey, TObjectPtr<USceneComponent>>());
		bDisableCollision = true;
		UPrimitiveComponent::SetSimulatePhysics(false);
	}

	// No chaos physics for you
	virtual bool ShouldCreatePhysicsState() const override { return false; }
	
	virtual TOptional<FTransform> GetTransformCopy(FSkeletonKey Target)
	{
	 	int32 m;
		FTransform ref;
		//bool FoundBodyID = (bool)BarrageToJoltMapping->visit(Key, [&result](auto& a) { result = a.second; });
		if(KeyToMesh->visit(Target, [&m](auto& a) { m = a.second; })
			&& GetInstanceTransform(GetInstanceIndexForId(FPrimitiveInstanceId(m)), ref, true))
		{
			return ref;
		}
		return TOptional<FTransform>();
	}
	
	virtual bool SetTransformOnInstance(FSkeletonKey Target, FTransform Update)
	{
	 	int32 m;
		if (!Update.ContainsNaN())
		{
			if(KeyToMesh->visit(Target, [&m](auto& a) { m = a.second; }))
			{
				TObjectPtr<USceneComponent> OptionalLinkedComponent = KeyToSceneComponent->FindRef(Target);
				if (OptionalLinkedComponent && OptionalLinkedComponent.Get())
				{
					OptionalLinkedComponent->SetWorldLocationAndRotationNoPhysics(Update.GetLocation(), Update.Rotator());
				}
				return UpdateInstanceTransform(GetInstanceIndexForId(FPrimitiveInstanceId(m)), Update, true, false, true);
			}
		}
		return false;
	};
	
	virtual FSkeletonKey GetKeyOfInstance(FPrimitiveInstanceId Target)
	{
		FSkeletonKey m;
		//KeyToMesh->visit(Target, [&m](auto& a) { m = a.second; })
		return MeshToKey->visit(Target.Id, [&m](auto& a) { m = a.second; }) ? m : FSkeletonKey();
	};

	virtual void AddToMap(FPrimitiveInstanceId MeshId, FSkeletonKey Key)
	{
		KeyToMesh->insert_or_assign(Key, MeshId.Id);
		MeshToKey->insert_or_assign(MeshId.Id, Key);
		
	}

	virtual void AddToMapDbg(FPrimitiveInstanceId MeshId, FSkeletonKey Key)
	{
		auto a = KeyToMesh->insert_or_assign(Key, MeshId.Id);
		auto b = MeshToKey->insert_or_assign(MeshId.Id, Key);
	}

	void QueueRemoveInstanceById(int I)
	{
		ToRemove->Enqueue(I);
	};

	virtual void CleanupInstance(const FSkeletonKey Target)
	{
		auto HoldOpen = KeyToMesh;
		if (HoldOpen)
		{
			int32 m;
			size_t found = KeyToMesh->visit(Target, [&m](auto& a) { m = a.second; });
			if (found && MeshToKey->erase(m))
			{
				QueueRemoveInstanceById(m);
				KeyToMesh->erase(Target);
				TObjectPtr<USceneComponent> Out;
				while(KeyToSceneComponent->RemoveAndCopyValue(Target, Out))
				{
					Out->ClearInternalFlags(EInternalObjectFlags::Async);
					Out->ConditionalBeginDestroy();
				}
			}
		}
	}

	virtual TWeakObjectPtr<USceneComponent> GetSceneComponentForInstance(const FSkeletonKey InstanceKey)
	{
		TObjectPtr<USceneComponent> m = KeyToSceneComponent->FindRef(InstanceKey);
		if (m && m.Get())
		{
			return m;
		}

		TObjectPtr<USceneComponent> NewSceneComponent = TObjectPtr<USceneComponent>(NewObject<USceneComponent>(this)); //async flags are automatically added.
		TOptional<FTransform> ExistingTransform = GetTransformCopy(InstanceKey);
		if (ExistingTransform.IsSet())
		{
			NewSceneComponent->SetWorldLocationAndRotationNoPhysics(ExistingTransform->GetLocation(), ExistingTransform->Rotator());
		}
		
		KeyToSceneComponent->Add(InstanceKey, NewSceneComponent);
		return NewSceneComponent;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		int32 Key = 0;
		while (ToRemove->Dequeue(Key))
		{
			RemoveInstanceById(FPrimitiveInstanceId(Key));
		}
	};
	
	void BeginDestroy() override;

private:
	TSharedPtr<LibCFSKInt> KeyToMesh;
	TSharedPtr<LibCIntFSK> MeshToKey;
	TSharedPtr<TMap<FSkeletonKey, TObjectPtr<USceneComponent>>> KeyToSceneComponent;
};

inline USwarmKineManager::~USwarmKineManager()
{
}

inline void USwarmKineManager::BeginDestroy()
{
	Super::BeginDestroy();
	TSharedPtr<TMap<FSkeletonKey, TObjectPtr<USceneComponent>>> hold = KeyToSceneComponent;
	// ReSharper disable once CppTemplateArgumentsCanBeDeduced - removing template typing causes compiler error
	for(TTuple<FSkeletonKey, TObjectPtr<USceneComponent>> x : *hold)
	{
		x.Value->ClearInternalFlags(EInternalObjectFlags::Async);
		x.Value->ConditionalBeginDestroy();
	}
	this->ClearInternalFlags(EInternalObjectFlags::Async);
}


class SwarmKine : public Kine
{
	TWeakObjectPtr<USwarmKineManager> MyManager;

public:
	explicit SwarmKine(const TWeakObjectPtr<USwarmKineManager>& MyManager, const FSkeletonKey& MeshInstanceKey)
		: MyManager(MyManager)
	{
		MyKey  = MeshInstanceKey;
	}

	virtual void SetTransformlike(FTransform Input) override
	{
		MyManager->SetTransformOnInstance(MyKey, Input);
	}

	virtual void SetLocationAndRotation(FVector3d Loc, FQuat4d Rot) override
	{
		TOptional<FTransform> m = MyManager->GetTransformCopy(MyKey);
		if(m.IsSet())
		{
			m->SetLocation(Loc);
			m->SetRotation(Rot);
			MyManager->SetTransformOnInstance(MyKey, m.GetValue());
		}	
	}

	virtual void SetLocationAndRotationWithScope(FVector3d Loc, FQuat4d Rot) override
	{
		TOptional<FTransform> m = MyManager->GetTransformCopy(MyKey);
		if(m.IsSet())
		{
			m->SetLocation(Loc);
			m->SetRotation(Rot);
			MyManager->SetTransformOnInstance(MyKey, m.GetValue());
		}	
	};

	virtual void SetLocation(FVector3d Location) override
	{
		TOptional<FTransform> m = MyManager->GetTransformCopy(MyKey);
		if(m.IsSet())
		{
			m->SetLocation(Location);
			MyManager->SetTransformOnInstance(MyKey, m.GetValue());
		}	
	}

	virtual void SetRotation(FQuat4d Rotation) override
	{
		TOptional<FTransform> m = MyManager->GetTransformCopy(MyKey);
		if(m.IsSet())
		{
			m->SetRotation(Rotation);
			MyManager->SetTransformOnInstance(MyKey, m.GetValue());
		}	
	}
	
protected:
	virtual TOptional<FTransform> CopyOfTransformlike_Impl() override
	{
		return MyManager->GetTransformCopy(MyKey);
	}
};
