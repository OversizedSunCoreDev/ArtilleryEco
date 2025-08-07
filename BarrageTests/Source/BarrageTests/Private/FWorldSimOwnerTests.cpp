#include "Misc/AutomationTest.h"
#include "FWorldSimOwner.h"
#include "FBShapeParams.h"
#include "PhysicsFilters/FastBroadphaseLayerFilter.h"
#include "PhysicsFilters/FastObjectLayerFilters.h"
#include "Jolt/Physics/Body/BodyFilter.h"

BEGIN_DEFINE_SPEC (FWorldSimOwnerTests, "Artillery.Barrage.World Sim Owner Tests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC (FWorldSimOwnerTests)
void FWorldSimOwnerTests::Define ()
{
	Describe("A World Sim Owner", [this]()
	{
		auto TestInitExitFunction = [](int _) { };
		TSharedPtr<FWorldSimOwner> ClassUnderTest = MakeShared<FWorldSimOwner>(0.333f, TestInitExitFunction);

		It("should initialize with the expected member variables", [this, ClassUnderTest]()
		{
			TestTrue ("Barrage To Jolt Mapping is valid", ClassUnderTest->BarrageToJoltMapping.IsValid ());
			TestTrue ("Box Cache is valid", ClassUnderTest->BoxCache.IsValid ());
			TestTrue ("Character To Jolt Mapping is valid", ClassUnderTest->CharacterToJoltMapping.IsValid ());
			TestTrue ("Character To Jolt Mapping is empty", ClassUnderTest->CharacterToJoltMapping->IsEmpty ());
			TestTrue ("Allocator is Valid", ClassUnderTest->Allocator.IsValid ());
			TestTrue ("Job System is Valid", ClassUnderTest->job_system.IsValid ());
			TestTrue ("Contact listener is valid", ClassUnderTest->contact_listener.IsValid ());
			TestTrue ("Physics System is valid", ClassUnderTest->physics_system.IsValid ());
			TestNearlyEqual ("Delta Time is assigned using given value", ClassUnderTest->DeltaTime, 0.333f);
			TestEqual ("Body Interface is assigned cached pointer to physics system", ClassUnderTest->body_interface, &ClassUnderTest->physics_system->GetBodyInterface ());
		});

		It ("should perform simple sphere tests", [this, ClassUnderTest] ()
		{
			// Define the sphere test parameters and out params
			const double GivenRadius = 2.;
			const double GivenDistance = 10.;
			const FVector3d GivenCastFrom = FVector3d::ZeroVector;
			const FVector3d GivenDirection = FVector3d::XAxisVector;
			TSharedPtr<FHitResult> ActualHitResult = MakeShared<FHitResult> ();
			const FastExcludeBroadphaseLayerFilter GivenBroadPhaseFilter;
			const FastExcludeObjectLayerFilter GivenObjectLayerFilter;
			const JPH::BodyFilter GivenBodyFilter;

			// Create a body that will cause a hit with the test
			FBSphereParams GivenSphereParams{ GivenDirection * (GivenDirection * GivenDistance * 0.75), GivenRadius * 2 };
			FBarrageKey SimpleSpherePrimitive = ClassUnderTest->CreatePrimitive (GivenSphereParams, Layers::NON_MOVING);
			// TODO: This test fails in isolation because of indexing errors into the ThreadAcc for Barrage

			ClassUnderTest->SphereCast
			(
				GivenRadius,
				GivenDistance,
				GivenCastFrom,
				GivenDirection,
				ActualHitResult,
				GivenBroadPhaseFilter,
				GivenObjectLayerFilter,
				GivenBodyFilter
			);
			
			TestTrue ("A hit occurs", ActualHitResult->bBlockingHit);
		});
	});
}