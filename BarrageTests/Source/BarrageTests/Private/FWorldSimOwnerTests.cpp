#include "Misc/AutomationTest.h"
#include "FWorldSimOwner.h"
#include "FBShapeParams.h"
#include "PhysicsFilters/FastBroadphaseLayerFilter.h"
#include "PhysicsFilters/FastObjectLayerFilters.h"
#include "Jolt/Physics/Body/BodyFilter.h"

BEGIN_DEFINE_SPEC (FWorldSimOwnerTests, "Artillery.Barrage.World Sim Owner Tests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
TSharedPtr<FWorldSimOwner> ClassUnderTest;
END_DEFINE_SPEC (FWorldSimOwnerTests)
void FWorldSimOwnerTests::Define ()
{
	BeforeEach([this]()
	{
		auto TestInitExitFunction = [this](int _)
		{
			if (ClassUnderTest.IsValid())
			{
				ClassUnderTest->WorkerAcc[0] = FBOutputFeed(std::this_thread::get_id(), 8);
				MyWORKERIndex = 0;

				ClassUnderTest->ThreadAcc[0] = FWorldSimOwner::FBInputFeed(std::this_thread::get_id(), 8);
				MyBARRAGEIndex = 0;
			}
		};
		ClassUnderTest = MakeShared<FWorldSimOwner>(0.016f, TestInitExitFunction);
	});

	Describe("A World Sim Owner", [this]()
	{
		It("should initialize with the expected member variables", [this]()
		{
			TestTrue ("Barrage To Jolt Mapping is valid", ClassUnderTest->BarrageToJoltMapping.IsValid ());
			TestTrue ("Box Cache is valid", ClassUnderTest->BoxCache.IsValid ());
			TestTrue ("Character To Jolt Mapping is valid", ClassUnderTest->CharacterToJoltMapping.IsValid ());
			TestTrue ("Character To Jolt Mapping is empty", ClassUnderTest->CharacterToJoltMapping->IsEmpty ());
			TestTrue ("Allocator is Valid", ClassUnderTest->Allocator.IsValid ());
			TestTrue ("Job System is Valid", ClassUnderTest->job_system.IsValid ());
			TestTrue ("Contact listener is valid", ClassUnderTest->contact_listener.IsValid ());
			TestTrue ("Physics System is valid", ClassUnderTest->physics_system.IsValid ());
			TestNearlyEqual ("Delta Time is assigned using given value", ClassUnderTest->DeltaTime, 0.016f);
			TestEqual ("Body Interface is assigned cached pointer to physics system", ClassUnderTest->body_interface, &ClassUnderTest->physics_system->GetBodyInterface ());
		});

		Describe("when creating primitives", [this]()
		{
			FBarrageKey ActualKey;
			AfterEach([this, &ActualKey]()
			{
				ClassUnderTest->FinalizeReleasePrimitive(ActualKey);
				// Clear the thread accumulator after each test
				ClassUnderTest->ThreadAcc[0].Queue->Empty();
				ClassUnderTest->WorkerAcc[0].Queue->Empty();
			});

			It("should enqueue an event to create a box", [this, &ActualKey]()
			{
				FBBoxParams GivenBoxParams{ FVector3d::ZeroVector, 10.f, 10.f, 10.f, FVector3f::ZeroVector, FMassByCategory::BMassCategories::MostScenery};
				ActualKey = ClassUnderTest->CreatePrimitive(GivenBoxParams, Layers::NON_MOVING);
				TestTrue ("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

				FBPhysicsInput ActualUpdate;
				TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[0].Queue->Dequeue(ActualUpdate));
				TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);
				TestEqual("The event target is the expected key", ActualUpdate.Target, ActualKey);
			});

			It("should enqueue an event to create a sphere", [this, &ActualKey]()
			{
				FBSphereParams GivenSphereParams{ FVector3d::ZeroVector, 10.f };
				ActualKey = ClassUnderTest->CreatePrimitive(GivenSphereParams, Layers::NON_MOVING);
				TestTrue("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

				FBPhysicsInput ActualUpdate;
				TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[0].Queue->Dequeue(ActualUpdate));
				TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);
				TestEqual("The event target is the expected key", ActualUpdate.Target, ActualKey);
			});

			It("should enqueue an event to create a cap", [this, &ActualKey]()
			{
				FBCapParams GivenCapParams{ FVector3d::ZeroVector, 10.f, 5.f };
				ActualKey = ClassUnderTest->CreatePrimitive(GivenCapParams, Layers::NON_MOVING);
				TestTrue("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

				FBPhysicsInput ActualUpdate;
				TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[0].Queue->Dequeue(ActualUpdate));
				TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);
				TestEqual("The event target is the expected key", ActualUpdate.Target, ActualKey);
			});

			It("should enqueue an event to create a character", [this, &ActualKey]()
			{
				FBCharParams GivenCharacterParams{ FVector3d::ZeroVector, 180.f, 40.f, 0.f };
				ActualKey = ClassUnderTest->CreatePrimitive(GivenCharacterParams, Layers::MOVING);
				TestTrue("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

				FBPhysicsInput ActualUpdate;
				TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[0].Queue->Dequeue(ActualUpdate));
				TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);
				TestEqual("The event target is the expected key", ActualUpdate.Target, ActualKey);
			});
		});

		Describe("when performing casts", [this]()
		{
			FBarrageKey BoxPrimitiveKey;
			BeforeEach([this, &BoxPrimitiveKey]()
			{
				// Create a sphere primitive to test against
				FBSphereParams GivenSphereParams{ FVector3d::XAxisVector * 50., 10.f };
				BoxPrimitiveKey = ClassUnderTest->CreatePrimitive(GivenSphereParams, Layers::NON_MOVING);

				// Take the event from the queue and process it
				FBPhysicsInput ActualUpdate;
				TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[0].Queue->Dequeue(ActualUpdate));

				JPH::BodyID BoxBodyID = JPH::BodyID(ActualUpdate.Target.KeyIntoBarrage);
				ClassUnderTest->body_interface->AddBodiesFinalize(&BoxBodyID, 1,
					ClassUnderTest->body_interface->AddBodiesPrepare(&BoxBodyID, 1),
					JPH::EActivation::Activate);

				ClassUnderTest->OptimizeBroadPhase();
				ClassUnderTest->StepSimulation();
			});

			AfterEach([this, &BoxPrimitiveKey]()
			{
				ClassUnderTest->FinalizeReleasePrimitive(BoxPrimitiveKey);
				// Clear the thread accumulator after each test
				ClassUnderTest->ThreadAcc[0].Queue->Empty();
				ClassUnderTest->WorkerAcc[0].Queue->Empty();
			});

			It("should perform simple sphere tests", [this]()
			{
				// Define the sphere test parameters and out params
				const double GivenRadius = 2.;
				const double GivenDistance = 100.;
				const FVector3d GivenCastFrom = FVector3d::ZeroVector;
				const FVector3d GivenDirection = FVector3d::XAxisVector;
				TSharedPtr<FHitResult> ActualHitResult = MakeShared<FHitResult>();
				const FastExcludeBroadphaseLayerFilter GivenBroadPhaseFilter;
				const FastExcludeObjectLayerFilter GivenObjectLayerFilter;
				const JPH::BodyFilter GivenBodyFilter;

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

				TestTrue("A hit occurs", ActualHitResult->bBlockingHit);
			});

			It("should perform simple ray tests", [this]()
			{
				// Define the box test parameters and out params
				const FVector3d GivenCastFrom = FVector3d::ZeroVector;
				const FVector3d GivenDirection = FVector3d::XAxisVector;
				TSharedPtr<FHitResult> ActualHitResult = MakeShared<FHitResult>();
				const FastExcludeBroadphaseLayerFilter GivenBroadPhaseFilter;
				const FastExcludeObjectLayerFilter GivenObjectLayerFilter;
				const JPH::BodyFilter GivenBodyFilter;

				ClassUnderTest->CastRay(
					GivenCastFrom,
					GivenDirection,
					GivenBroadPhaseFilter,
					GivenObjectLayerFilter,
					GivenBodyFilter,
					ActualHitResult
				);

				TestTrue("A hit occurs", ActualHitResult->bBlockingHit);
			});
		});
	});
}