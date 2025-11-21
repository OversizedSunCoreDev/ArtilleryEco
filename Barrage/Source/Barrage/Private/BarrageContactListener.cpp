#include "BarrageContactListener.h"
#include "BarrageDispatch.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "BarrageContactEvent.h"

using namespace JPH;

	ValidateResult BarrageContactListener::OnContactValidate(const Body& inBody1, const Body& inBody2, RVec3Arg inBaseOffset,
												 const CollideShapeResult& inCollisionResult)
	{
		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	void BarrageContactListener::OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold,
								ContactSettings& ioSettings)
	{
		if (UBarrageDispatch::SelfPtr)
		{
			UBarrageDispatch::SelfPtr->HandleContactAdded(inBody1, inBody2, inManifold, ioSettings);
		}
	}

	void BarrageContactListener::OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold,
									ContactSettings& ioSettings)
	{
		// TODO: Honestly, this event fires way too often and we don't really need it so... let's just not for now

		/**
		if (BarrageDispatch.IsValid())
		{
			BarrageDispatch->HandleContactPersisted(inBody1, inBody2, inManifold, ioSettings);
		}
		**/
	}

//TODO: This is a JANKY hack for character collisions and basically doesn't include enough info to really get
//fine grained about stuff.
void BarrageContactListener::OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::BodyID& inBodyID2,
	const JPH::SubShapeID& inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal,
	JPH::CharacterContactSettings& ioSettings)
{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Report Contact");
		if (UBarrageDispatch::SelfPtr)
		{
			auto Ent1 = BarrageContactEntity(
				UBarrageDispatch::SelfPtr->GenerateBarrageKeyFromBodyId(inCharacter->GetInnerBodyID()) 
					); // hey so good chance this is our problem.
			
			auto Ent2 = BarrageContactEntity(UBarrageDispatch::SelfPtr->GenerateBarrageKeyFromBodyId(inBodyID2));
			//in general, almost nothing should push the character without explicitly invoking one of the apply forces functions.
			//the exception RIGHT NOW is other non-enemy non-hitboxed movers, and terrain. this is in part because the player weighs 1 Unit
			//to make the math for forces easier. That is Not Very Many Units.
			if (Ent2.MyLayer != Layers::MOVING && Ent2.MyLayer != Layers::NON_MOVING)
			{
				ioSettings.mCanPushCharacter = false; //TODO validate push/apply 
			}
			if (Ent2.MyLayer == Layers::NON_MOVING)
			{
				ioSettings.mCanReceiveImpulses = false; // no push world plz.
			}
			UBarrageDispatch::SelfPtr->HandleContactAdded(Ent1, Ent2);
		}
}

//TODO: this needs replaced when we go to multiplayer.
void BarrageContactListener::OnCharacterContactAdded(const JPH::CharacterVirtual* inCharacter,
	const JPH::CharacterVirtual* inOtherCharacter, const JPH::SubShapeID& inSubShapeID2,
	JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings& ioSettings)
{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Report Contact");
		if (UBarrageDispatch::SelfPtr)
		{
			auto Ent1 = BarrageContactEntity(UBarrageDispatch::SelfPtr->GenerateBarrageKeyFromBodyId(inCharacter->GetInnerBodyID()));
			
			auto Ent2 = BarrageContactEntity(UBarrageDispatch::SelfPtr->GenerateBarrageKeyFromBodyId(inOtherCharacter->GetInnerBodyID()));
			UBarrageDispatch::SelfPtr->HandleContactAdded(Ent1, Ent2);
		}  
}

void BarrageContactListener::OnContactRemoved(const SubShapeIDPair& inSubShapePair)
	{
		if (UBarrageDispatch::SelfPtr)
		{
			UBarrageDispatch::SelfPtr->HandleContactRemoved(inSubShapePair);
		}
	}
