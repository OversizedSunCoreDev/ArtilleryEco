#include "Misc/AutomationTest.h"
#include "FWorldSimOwner.h"
#include "FBShapeParams.h"
#include "PhysicsFilters/FastBroadphaseLayerFilter.h"
#include "PhysicsFilters/FastObjectLayerFilters.h"
#include "Jolt/Physics/Body/BodyFilter.h"

BEGIN_DEFINE_SPEC(FWorldSimOwnerTests, "Artillery.Barrage.World Sim Owner Tests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
TSharedPtr<FWorldSimOwner> ClassUnderTest = MakeShared<FWorldSimOwner>(0.016f, [this](int threadId)
	{
		if (ClassUnderTest.IsValid())
		{
			ClassUnderTest->WorkerAcc[threadId] = FBOutputFeed(std::this_thread::get_id(), 512);
			ClassUnderTest->ThreadAcc[threadId] = FWorldSimOwner::FBInputFeed(std::this_thread::get_id(), 512);
			MyWORKERIndex = threadId;
			MyBARRAGEIndex = threadId;
		}
	});
END_DEFINE_SPEC(FWorldSimOwnerTests)
void FWorldSimOwnerTests::Define()
{
	constexpr static int32 FORCED_THREAD_INDEX = 0;
	BeforeEach([this]()
		{
			MyBARRAGEIndex = FORCED_THREAD_INDEX;
			MyWORKERIndex = FORCED_THREAD_INDEX;
		});

	Describe("A World Sim Owner", [this]()
		{
			It("should initialize with the expected member variables", [this]()
				{
					TestTrue("Barrage To Jolt Mapping is valid", ClassUnderTest->BarrageToJoltMapping.IsValid());
					TestTrue("Box Cache is valid", ClassUnderTest->BoxCache.IsValid());
					TestTrue("Character To Jolt Mapping is valid", ClassUnderTest->CharacterToJoltMapping.IsValid());
					TestTrue("Character To Jolt Mapping is empty", ClassUnderTest->CharacterToJoltMapping->IsEmpty());
					TestTrue("Allocator is Valid", ClassUnderTest->Allocator.IsValid());
					TestTrue("Job System is Valid", ClassUnderTest->job_system.IsValid());
					TestTrue("Contact listener is valid", ClassUnderTest->contact_listener.IsValid());
					TestTrue("Physics System is valid", ClassUnderTest->physics_system.IsValid());
					TestNearlyEqual("Delta Time is assigned using given value", ClassUnderTest->DeltaTime, 0.016f);
					TestEqual("Body Interface is assigned cached pointer to physics system", ClassUnderTest->body_interface, &ClassUnderTest->physics_system->GetBodyInterface());
				});

			Describe("when creating primitives", [this]()
				{

					FBarrageKey ActualKey;
					AfterEach([this, &ActualKey]()
						{
							ClassUnderTest->ThreadAcc[FORCED_THREAD_INDEX].Queue->Empty();
							ClassUnderTest->WorkerAcc[FORCED_THREAD_INDEX].Queue->Empty();
						});

					It("should enqueue an event to create a box", [this, &ActualKey]()
						{
							FBBoxParams GivenBoxParams{ FVector3d::ZeroVector, 10.f, 10.f, 10.f, FVector3f::ZeroVector, FMassByCategory::BMassCategories::MostScenery };
							ActualKey = ClassUnderTest->CreatePrimitive(GivenBoxParams, Layers::NON_MOVING);
							TestTrue("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

							FBPhysicsInput ActualUpdate;
							TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[FORCED_THREAD_INDEX].Queue->Dequeue(ActualUpdate));
							TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);

							JPH::BodyID ResultBodyID;
							bool Found = ClassUnderTest->GetBodyIDOrDefault(ActualKey, ResultBodyID);
							TestTrue("Jolt body ID found for Barrage key", Found);
						});

					It("should enqueue an event to create a sphere", [this, &ActualKey]()
						{
							FBSphereParams GivenSphereParams{ FVector3d::ZeroVector, 10.f };
							ActualKey = ClassUnderTest->CreatePrimitive(GivenSphereParams, Layers::NON_MOVING);
							TestTrue("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

							FBPhysicsInput ActualUpdate;
							TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[FORCED_THREAD_INDEX].Queue->Dequeue(ActualUpdate));
							TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);
						});

					It("should enqueue an event to create a cap", [this, &ActualKey]()
						{
							FBCapParams GivenCapParams{ FVector3d::ZeroVector, 10.f, 5.f };
							ActualKey = ClassUnderTest->CreatePrimitive(GivenCapParams, Layers::NON_MOVING);
							TestTrue("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

							FBPhysicsInput ActualUpdate;
							TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[FORCED_THREAD_INDEX].Queue->Dequeue(ActualUpdate));
							TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);
						});

					xIt("should enqueue an event to create a character", [this, &ActualKey]()
						{
							FBCharParams GivenCharacterParams{ FVector3d::ZeroVector, 180.f, 40.f, 0.f };
							ActualKey = ClassUnderTest->CreatePrimitive(GivenCharacterParams, Layers::MOVING);
							TestTrue("The returned key is valid", ActualKey.KeyIntoBarrage != 0);

							FBPhysicsInput ActualUpdate;
							TestTrue("There is an event in the queue", ClassUnderTest->ThreadAcc[FORCED_THREAD_INDEX].Queue->Dequeue(ActualUpdate));
							TestEqual("The event is an add", ActualUpdate.Action, PhysicsInputType::ADD);
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
							if (ClassUnderTest->ThreadAcc[FORCED_THREAD_INDEX].Queue->Dequeue(ActualUpdate))
							{
								JPH::BodyID BoxBodyID = JPH::BodyID(ActualUpdate.Target.KeyIntoBarrage);
								ClassUnderTest->body_interface->AddBodiesFinalize(&BoxBodyID, 1,
									ClassUnderTest->body_interface->AddBodiesPrepare(&BoxBodyID, 1),
									JPH::EActivation::Activate);

								ClassUnderTest->OptimizeBroadPhase();
								ClassUnderTest->StepSimulation();
							}
						});

					AfterEach([this, &BoxPrimitiveKey]()
						{
							ClassUnderTest->FinalizeReleasePrimitive(BoxPrimitiveKey);
							// Clear the thread accumulator after each test
							ClassUnderTest->ThreadAcc[FORCED_THREAD_INDEX].Queue->Empty();
							ClassUnderTest->WorkerAcc[FORCED_THREAD_INDEX].Queue->Empty();
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

					It("should perform a sphere search", [this, &BoxPrimitiveKey]()
						{
							// Define the sphere search parameters and out params
							const JPH::BodyID GivenCastingBody = JPH::BodyID();
							const FVector3d GivenLocation = FVector3d::XAxisVector * 50.;
							const double GivenRadius = 20.;
							const FastExcludeBroadphaseLayerFilter GivenBroadPhaseFilter;
							const FastExcludeObjectLayerFilter GivenObjectLayerFilter;
							const JPH::BodyFilter GivenBodyFilter;
							uint32 ActualFoundObjectCount = 0;
							TArray<uint32> ActualFoundObjectIDs;
							ClassUnderTest->SphereSearch(
								GivenCastingBody,
								GivenLocation,
								GivenRadius,
								GivenBroadPhaseFilter,
								GivenObjectLayerFilter,
								GivenBodyFilter,
								&ActualFoundObjectCount,
								ActualFoundObjectIDs
							);
							TestEqual("One object is found", ActualFoundObjectCount, 1);

							uint32 ExpectedObjectID = ClassUnderTest->BarrageToJoltMapping->find(BoxPrimitiveKey).GetIndexAndSequenceNumber();
							TestEqual("The found object is the expected one", ActualFoundObjectIDs[0], ExpectedObjectID);
						});
				});
		});

	Describe("Object Layer Pair Filter Implementation", [this]()
		{
			// lazy and inference is too
			using T = TArray < JPH::ObjectLayer>;
			using P = TArray < TPair < JPH::ObjectLayer, T>>;

			It("should return true for a pair that is allowed", [this]()
				{
					FWorldSimOwner::ObjectLayerPairFilterImpl ClassUnderTest;
					P PositiveExpectations
					{
						{ Layers::MOVING, T{ Layers::NON_MOVING, Layers::BONKFREEENEMY, Layers::MOVING, Layers::ENEMY, Layers::ENEMYPROJECTILE, Layers::CAST_QUERY } },
						{ Layers::NON_MOVING, T{ Layers::MOVING, Layers::BONKFREEENEMY, Layers::CAST_QUERY, Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, Layers::DEBRIS, Layers::ENEMY, Layers::ENEMYPROJECTILE, Layers::MOVING, Layers::PROJECTILE } }
					};

					for (const auto& Pair : PositiveExpectations)
					{
						for (const auto& TestLayer : Pair.Value)
						{
							TestTrue(FString::Printf(TEXT("Layer %d should collide with %d"), Pair.Key, TestLayer), ClassUnderTest.ShouldCollide(Pair.Key, TestLayer));
						}
					}
				});

			It("should return false for a pair that is not allowed", [this]()
				{
					FWorldSimOwner::ObjectLayerPairFilterImpl ClassUnderTest;
					P NegativeExpectations
					{
						{ Layers::MOVING, T{ Layers::HITBOX, Layers::DEBRIS, Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, Layers::PROJECTILE } },
						{ Layers::NON_MOVING, T{ Layers::NON_MOVING, Layers::HITBOX } }
					};
					for (const auto& Pair : NegativeExpectations)
					{
						for (const auto& TestLayer : Pair.Value)
						{
							TestFalse(FString::Printf(TEXT("Layer %d should not collide with %d"), Pair.Key, TestLayer), ClassUnderTest.ShouldCollide(Pair.Key, TestLayer));
						}
					}
				});
		});
}