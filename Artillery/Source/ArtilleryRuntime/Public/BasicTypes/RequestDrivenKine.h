#pragma once
#include "ArtilleryDispatch.h"
#include "Kines.h"
#include "NeedA.h"

//shares the lifecycle of the owner.
struct ManagedRequestingKine
{
public:
	USceneComponent* MySelf;
	FBoneKey Key;
	bool Readyish = false;
	
	bool QueueUpdate(ArtilleryTime Stamp, FVector Pos, FRotator Rot, USceneComponent* NewSelf)
	{
		if (UArtilleryDispatch::SelfPtr && UArtilleryDispatch::SelfPtr->RequestRouter && Readyish)
		{
			UArtilleryDispatch::SelfPtr->RequestRouter->SceneComponentMoved(Key, Stamp,  Pos,  Rot);
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
		if (UTransformDispatch::SelfPtr && MyNewSelf)
		{
			Key = FBoneKey(PointerHash(MyNewSelf + MYSTIC_STANDARDIZED_OFFSET)) ;
			UTransformDispatch::SelfPtr->RegisterSceneCompToShadowTransform(Key, MyNewSelf);
			Readyish = true;
		}
		else
		{
			Readyish = false;
		}
	}

	ManagedRequestingKine(USceneComponent* MySelf)
	{
		Registration(MySelf);
	}

	ManagedRequestingKine(): MySelf(nullptr)
	{
		Readyish = false;
	}

	~ManagedRequestingKine()
	{
		Readyish = false;
		if (UTransformDispatch::SelfPtr)
		{
			UTransformDispatch::SelfPtr->ReleaseKineByKey(FSkeletonKey(Key));
		}
	}
};