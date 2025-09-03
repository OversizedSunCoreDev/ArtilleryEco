#include "Misc/AutomationTest.h"
#include "FBarragePrimitive.h"

/**
* These tests cover the basic functionality of the FBarragePrimitive class. The static helpers for the instances
* is found in the BarageDispatchTests. This is because there is a protected friend relationship between the two
* and the static global pointer to the dispatch is required for many of the static functions to work.
**/

BEGIN_DEFINE_SPEC (FBarragePrimitiveTests, "Artillery.Barrage.Barrage Primitive Tests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC (FBarragePrimitiveTests)
void FBarragePrimitiveTests::Define ()
{
	Describe ("Initial State of a Primitive", [this]()
	{
		It ("should use the given keys, not marked for tombstoning, and uninitialized.", [this]()
		{
			FBarrageKey GivenIntoKey;
			FSkeletonKey GivenOutOfKey;
			FBarragePrimitive ClassUnderTest (GivenIntoKey, GivenOutOfKey);

			TestEqual ("KeyIntoBarrage", ClassUnderTest.KeyIntoBarrage, GivenIntoKey);
			TestEqual ("KeyOutOfBarrage", ClassUnderTest.KeyOutOfBarrage, GivenOutOfKey);
			TestEqual ("Tombstone", ClassUnderTest.tombstone, 0);
			TestEqual ("Me", ClassUnderTest.Me, FBShape::Uninitialized);
		});
	});

	Describe ("Static Functions", [this] ()
	{
		Describe("FromJoltGroundState", [this]()
		{
			/**
			There exists no value of JPH::CharacterBase::EGroundState in which
			FBarragePrimitive::FBGroundState::NotFound exists. This holds true
			so long as the definition of JPH::CharacterBase::EGroundState does
			not change.
			**/
			const TTuple<FString, FBarragePrimitive::FBGroundState, JPH::CharacterBase::EGroundState> StatePairsToTest[] =
			{ 
				{ "Ground", FBarragePrimitive::FBGroundState::OnGround, JPH::CharacterBase::EGroundState::OnGround},
				{ "SteepGround", FBarragePrimitive::FBGroundState::OnSteepGround, JPH::CharacterBase::EGroundState::OnSteepGround },
				{ "NotSupported", FBarragePrimitive::FBGroundState::NotSupported, JPH::CharacterBase::EGroundState::NotSupported },
				{ "InAir", FBarragePrimitive::FBGroundState::InAir, JPH::CharacterBase::EGroundState::InAir },
			};

			It ("should map the given enumeration value from Jolt to Barrage", [this, StatePairsToTest]
			{
				for (const auto& TestPair : StatePairsToTest)
				{
					const FBarragePrimitive::FBGroundState Expected = TestPair.Get<1> ();
					const FBarragePrimitive::FBGroundState Actual = FBarragePrimitive::FromJoltGroundState (TestPair.Get<2> ());
					TestEqual (TestPair.Get<0> (), Expected, Actual);
				}
			});
		});

	});
}