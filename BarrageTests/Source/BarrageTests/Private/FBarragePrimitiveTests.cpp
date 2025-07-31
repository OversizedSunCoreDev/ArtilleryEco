#include "Misc/AutomationTest.h"
#include "FBarragePrimitive.h"

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
}