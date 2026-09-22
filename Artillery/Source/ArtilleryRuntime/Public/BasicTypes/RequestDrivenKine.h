#pragma once
#include "ArtilleryDispatch.h"
#include "RequestRouter.h"

//shares the lifecycle of the owner.
struct ManagedRequestingKine
{
public:
	USceneComponent* MySelf = nullptr;
	UArtilleryDispatch* MyDispatch = nullptr;
	TWeakObjectPtr<UTransformDispatch> TransformDispatch = nullptr;
	FBoneKey Key;
	bool Readyish = false;
	
	bool QueueUpdate(ArtilleryTime Stamp, FVector Pos, FRotator Rot, USceneComponent* NewSelf)
	{
		if (MyDispatch && MyDispatch->RequestRouter && Readyish)
		{
			MyDispatch->RequestRouter->SceneComponentMoved(Key, Stamp,  Pos,  Rot);
			return true;
		}
		else
		{
			Registration(NewSelf);
			return false;
		}
	}

	void Registration(USceneComponent* MyNewSelf)
	{
		MySelf=MyNewSelf;
		auto TransformDispatchPtr = MyDispatch->GetWorld()->GetSubsystem<UTransformDispatch>();
		TransformDispatch = TransformDispatchPtr;
		if (TransformDispatchPtr && MyNewSelf)
		{
			Key = FBoneKey(PointerHash(MyNewSelf + MYSTIC_STANDARDIZED_OFFSET)) ;
			TransformDispatch->RegisterSceneCompToShadowTransform(Key, MyNewSelf);
			Readyish = true;
		}
		else
		{
			Readyish = false;
		}
	}

	ManagedRequestingKine(UArtilleryDispatch* Dispatch, USceneComponent* MySelf)
	{
		
		MyDispatch = Dispatch;
		TransformDispatch = Dispatch->GetWorld()->GetSubsystem<UTransformDispatch>();
		Registration(MySelf);
	}

	ManagedRequestingKine(UArtilleryDispatch* Dispatch): MySelf(nullptr)
	{
		Readyish = false;
		MyDispatch = Dispatch;
		TransformDispatch = Dispatch->GetWorld()->GetSubsystem<UTransformDispatch>();
	}
	
	ManagedRequestingKine(): MySelf(nullptr)
	{
		Readyish = false;
		MyDispatch = nullptr;
		TransformDispatch = nullptr;
	}

	~ManagedRequestingKine()
	{
		Readyish = false;
		if (auto TransformDispatchPtr = TransformDispatch.Get())
		{
			TransformDispatchPtr->ReleaseKineByKey(FSkeletonKey(Key));
		}
	}
};