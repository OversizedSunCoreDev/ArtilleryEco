#include "Misc/AutomationTest.h"
#include "FWorldSimOwner.h"

BEGIN_DEFINE_SPEC (FWorldSimOwnerTests, "Artillery.Barrage.World Sim Owner Tests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC (FWorldSimOwnerTests)
void FWorldSimOwnerTests::Define ()
{
	Describe("Creating a World Sim Owner", [this]()
	{
		auto TestInitExitFunction = [](int _) { };
		FWorldSimOwner ClassUnderTest(0.0f, TestInitExitFunction);

		It("should the expected member variables", [this, &ClassUnderTest]()
		{
			TestTrue ("Barrage To Jolt Mapping", ClassUnderTest.BarrageToJoltMapping.IsValid ());
		});
	});
}