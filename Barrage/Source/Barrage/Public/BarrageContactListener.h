// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BarrageDispatch.h"
#include "IsolatedJoltIncludes.h"

class BarrageContactListener : public JPH::ContactListener, public JPH::CharacterContactListener
{
public:
	BarrageContactListener(UBarrageDispatch* Barrage);
	UBarrageDispatch* MyBarrage = nullptr;
	virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset,
	                                              const JPH::CollideShapeResult& inCollisionResult) override;

	virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
	                            JPH::ContactSettings& ioSettings) override;

	virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
	                                JPH::ContactSettings& ioSettings) override;

	
	//despite the odd name, a character contact is an encapsulation of a character's contact with any rigid body, not an encapsulation of contact with a character.
	virtual void OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::CharacterContact &inContact, JPH::CharacterContactSettings &ioSettings) override;
	//virtual void			OnContactAdded(	const CharacterVirtual *inCharacter,
	//										const CharacterContact &inContact, 
	//										CharacterContactSettings &ioSettings) 
	//										{ /* Default do nothing */ }
	virtual void OnCharacterContactAdded(const JPH::CharacterVirtual *inCharacter,
											const JPH::CharacterContact &inContact,
											JPH::CharacterContactSettings &ioSettings) override;
	virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;
};
