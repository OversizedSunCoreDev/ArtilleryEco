// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "SkeletonTypes.h"
#include "FBarrageKey.h"
#include "FBPhysicsInputTypes.h"
#include "IsolatedJoltIncludes.h"
#include "seq/concurrent_map.hpp"

//Activate this to turn on the debug draw capability.
#define BARRAGE_DEBUG_ENABLED 0


//A Barrage shapelet accepts forces and transformations as though it were not managed by an evil secret machine
//and this allows us to pretty much Do The Right Thing. I've chosen to actually hide the specific kind of shape as an
//enum prop rather than a class parameter. The pack pragma makes me reluctant to use non-POD approaches.
//primitives MUST be passed by reference or pointer. doing otherwise means that you may have an out of date view
//of tombstone state, which is not particularly safe.
class BARRAGE_API FBarragePrimitive
{
	friend class UBarrageDispatch;

public:
	enum FBGroundState
	{
		OnGround, ///< Character is on the ground and can move freely.
		OnSteepGround,
		///< Character is on a slope that is too steep and can't climb up any further. The caller should start applying downward velocity if sliding from the slope is desired.
		NotSupported,
		///< Character is touching an object, but is not supported by it and should fall. The GetGroundXXX functions will return information about the touched object.
		InAir, ///< Character is in the air and is not touching anything.
		NotFound, ///< There's no character
	};

	static FBGroundState FromJoltGroundState(JPH::CharacterBase::EGroundState JoltGroundState)
	{
		switch (JoltGroundState)
		{
		case JPH::CharacterBase::EGroundState::OnGround:
			return FBGroundState::OnGround;
		case JPH::CharacterBase::EGroundState::OnSteepGround:
			return FBGroundState::OnSteepGround;
		case JPH::CharacterBase::EGroundState::NotSupported:
			return FBGroundState::NotSupported;
		case JPH::CharacterBase::EGroundState::InAir:
			return FBGroundState::InAir;
		}
		return FBGroundState::NotFound;
	}

	//you cannot safely reorder these or it will change the pack width.
	FBarrageKey KeyIntoBarrage;
	//This breaks our type dependency by using the underlying hashkey instead of the artillery type.
	//this is pretty risky, but it's basically necessary to avoid a dependency on artillery until we factor our types
	//into a type plugin, which is a wild but not unexpected thing to do.
	FSkeletonKey KeyOutOfBarrage;

	//Tombstone state. Used to ensure that we don't nullity sliced. 
	//0 is normal.
	//1 or more is dead and indicates that the primitive could be released at any time. These should be considered
	// opaque values as implementation is not final. a primitive is guaranteed to be tombstoned for at least
	// BarrageDispatch::TombstoneInitialMinimum cycles before it is released, so this can safely be
	// used in conjunction with null checks to ensure that short tasks can finish safely without
	// worrying about the exact structure of our lifecycles. it is also used for pool and rollback handling,
	// and the implementation will change as those come online in artillery.
	uint32 tombstone = 0; //4b 
	FBShape Me; //4b

	FBarragePrimitive(FBarrageKey Into, FSkeletonKey OutOf)
	{
		KeyIntoBarrage = Into;
		KeyOutOfBarrage = OutOf;
		tombstone = 0;
		Me = Uninitialized;
	}

	~FBarragePrimitive();
	//Note the use of shared pointers. Due to tombstoning, FBlets must always be used by reference.
	//this is actually why they're called FBLets, as they're rented (or let) shapes that are also thus both shapelets and shape-lets.

	typedef FBarragePrimitive FBShapelet;
	typedef TSharedPtr<FBShapelet> FBLet;

	//STATIC METHODS
	//-------------------------------
	//By and at large, these are static so that they can interact with FBLets, instead of the bare primitive. We don't
	//really want to ever encourage people to use those.
	//-------------------------------

	static void SetGravityFactor(float GravityFactor, FBLet Target);
	//immediately sets the velocity of object to given velocity vector
	static void SetVelocity(FVector3d Velocity, FBLet Target);
	static void SetPosition(FVector Position, FBLet Target);
	//transform forces transparently from UE world space to jolt world space
	//then apply them directly to the "primitive"
	static void ApplyForce(FVector3d Force, FBLet Target, PhysicsInputType Type = PhysicsInputType::OtherForce);
	//debug only!
	static TPair<FVector, FVector> GetLocalBounds(FBLet Target);
	//transform the quaternion from the UE ref to the Jolt ref
	//then apply it to the "primitive"
	static void ApplyRotation(FQuat4d Rotator, FBLet Target);
	//Applies any given quat as any given input to any given target. use of this is not recommended.
	static void Apply_Unsafe(FQuat4d Any, FBLet Target, PhysicsInputType Type);


	static FVector GetAngularVelocity(FBLet Target);
	static void ApplyTorque(FVector Torque, FBLet Target);


	//My current thinking:
	//This should be called from the gamethread, in the PULL model. it doesn't lock, but it will fail if the lock is held on that body
	//because we should _never_ block the game thread. unfortunately, this means I can't provide the code to actually use
	//this as part of jolt very easily at first, but I'll try to defactor whatever I built into a sample implementation for Barrage.
	//This is unsafe to call except during the exact place it is currently called because it avoids the lifecycle management.
	template<typename OwnerType, typename QueueType> static void TryUpdateTransformFromJolt(const FBarragePrimitive* Target, JPH::BodyID PassIn, TSharedPtr<OwnerType>& GameSimHoldOpen, TSharedPtr<QueueType>& HoldOpen,  uint64 Time);
	static FVector3f GetCentroidPossiblyStale(FBLet Target);
	static FVector3f GetPosition(FBLet Target);
	static FVector3f GetVelocity(FBLet Target);

	//in almost all cases, we recommend that you use the vector attributes, as rotation rarely actually provides
	//the information about facing, aim point, and similar that you might want it to. this is especially true for
	//characters which we almost never actually rotate.
	static FQuat OptimisticGetAbsoluteRotation(FBLet Target);
	//tombstoned primitives are treated as null even by live references, because while the primitive is valid
	//and operations against it can be performed safely, no new operations should be allowed to start.
	//the tombstone period is effectively a grace period due to the fact that we have quite a lot of different
	//timings in play. it should be largely unnecessary, but it's also a very useful semantic for any pooled
	//data and allows us to batch disposal nicely.
	//while it seems like a midframe tombstoning could lead to non-determinism,
	//physics mods are actually effectively applied all on one thread right before update kicks off thanks to StackUp()
	static bool IsNotNull(FBLet Target)
	{
		return Target != nullptr && Target->tombstone == 0;
	};

	static FVector3d UpConvertFloatVector(FVector3f InVector)
	{
		return FVector3d(InVector.X, InVector.Y, InVector.Z);
	}

	static FQuat4d UpConvertFloatQuat(FQuat4f InQuat)
	{
		return FQuat4d(InQuat.X, InQuat.Y, InQuat.Z, InQuat.W);
	}

	static void SpeedLimit(FBLet Target, float TargetSpeed);
	static bool GetSpeedLimitIfAny(FBLet Target, float& OldSpeedLimit);

	// If you call these with a non-character FBLet, they will always return false-y values.
	static FBGroundState GetCharacterGroundState(FBLet Target);
	static FVector3f GetCharacterGroundNormal(FBLet Target);

	static void SetCharacterGravity(FVector3d InVector, FBLet Target);

protected:
	static inline UBarrageDispatch* GlobalBarrage = nullptr;
};



typedef FBarragePrimitive FBShapelet;
typedef TSharedPtr<FBShapelet, ESPMode::ThreadSafe> FBLet;

//we could use this to save a shared ptr construction in the lifecycle update inner loop
//it's safe for us to pull a raw there because only during the resolution of that loop is there
//a chance for the pointed thango to be deallocated. This would be a marked and possibly annoying
//arch change but it would get us a concretized type for this AND remove an atomic, potentially
//improving memory _considerably_. at the cost of manually incrementing and decrementing the shared ptr referencer
//otoh: this does make what happens in tombs FAR more legible. grasping hand: not any less stupid though.
//otgoh: We could do away with tombs by using a tombstone stamp, which would significantly ease memory pressure.
//TODO: move this to the correct memory packing, christ!!!!!!
//or I could just be a big boy and use libcuckoo's find function I think.
/*

 * *Searches the table for @p key, and invokes @p fn on the value. @p fn is
* not allowed to modify the contents of the value if found.
*
*otherwise we use something like this:
typedef SharedPointerInternals::FSharedReferencer<ESPMode::ThreadSafe> PrecursorDefRef;
struct PossibleRecordType
{
	FBShapelet TheRealDeal;
	PrecursorDefRef* NeverDoThis;
};
*/
THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
typedef seq::concurrent_map<FBarrageKey, FBLet> KeyToFBLet;
JPH_SUPPRESS_WARNINGS

PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

