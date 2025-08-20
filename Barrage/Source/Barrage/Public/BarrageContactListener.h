// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IsolatedJoltIncludes.h"

class BarrageContactListener : public JPH::ContactListener, public JPH::CharacterContactListener
{
public:
	BarrageContactListener()
	{
	}
	
	virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset,
	                                              const JPH::CollideShapeResult& inCollisionResult) override;

	virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
	                            JPH::ContactSettings& ioSettings) override;

	virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
	                                JPH::ContactSettings& ioSettings) override;
	virtual void OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::BodyID& inBodyID2,
		const JPH::SubShapeID& inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal,
		JPH::CharacterContactSettings& ioSettings) override;
	virtual void OnCharacterContactAdded(const JPH::CharacterVirtual* inCharacter,
		const JPH::CharacterVirtual* inOtherCharacter, const JPH::SubShapeID& inSubShapeID2,
		JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal,
		JPH::CharacterContactSettings& ioSettings) override;
	virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;
};
