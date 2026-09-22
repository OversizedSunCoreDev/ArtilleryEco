// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "IsolatedJoltIncludes.h"
#include "FBPhysicsInputTypes.h"

// *********************** IMPORTANT ***********************
// This struct must be pragma pack immune in order to properly work in Jolt.
// Please be careful if the size needs to be changed
static constexpr int FB_PHYSICS_INPUT_EXPECTED_SIZE = 48;
//
// this is the input record used for queuing a physics op. if that sounds like gibberish,
// take a look at the StackUp function and the transform dispatch.
// *********************************************************
struct FBPhysicsInput
{
	FBarrageKey Target;
	friend bool operator<(const FBPhysicsInput& Lhs, const FBPhysicsInput& RHS) 
	{
		if (Lhs.Target.KeyIntoBarrage < RHS.Target.KeyIntoBarrage)
			return true;
		if (Lhs.Sequence < RHS.Sequence)
			return true;
		if (Lhs.Action < RHS.Action)
			return true;
		return false;
	}

	static bool Less(const FBPhysicsInput& LHS, const FBPhysicsInput& RHS)
	{
		return LHS < RHS;
	}
	FBPhysicsInput(const FBarrageKey& Target, uint64 Sequence, PhysicsInputType Action, FBShape Metadata,
		const JPH::Quat& State)
		: Target(Target),
		  Sequence(Sequence),
		  Action(Action),
		  metadata(Metadata),
		  State(State)
	{
	}

	FBPhysicsInput() = default;
	uint64 Sequence; //used to order operations for determinism.
	PhysicsInputType Action;
	FBShape metadata;
	JPH::Quat State;

	FBPhysicsInput(FBarrageKey Affected, int Seq, PhysicsInputType PhysicsInput, JPH::Quat Quat, FBShape PrimitiveShape)
	{
		State = Quat;
		Target  = Affected;
		Sequence = Seq;
		Action = PhysicsInput;
		metadata = PrimitiveShape;
	}

	FBPhysicsInput(JPH::BodyID Affected, uint64 Seq, PhysicsInputType ThisBetterBeAdd): metadata(), State()
	{
		Target = Affected.GetIndexAndSequenceNumber(); //oh mother of god
		Sequence = Seq;
		Action = ThisBetterBeAdd;
	}


};
static_assert(sizeof(FBPhysicsInput) == FB_PHYSICS_INPUT_EXPECTED_SIZE);
