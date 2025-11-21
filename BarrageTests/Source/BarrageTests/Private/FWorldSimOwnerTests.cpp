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

	Describe("Broad Phase Layer Interface Implementation", [this]()
		{
			TSharedPtr<JPH::BroadPhaseLayerInterface> BroadPhaseLayerInterface = MakeShared<FWorldSimOwner::BPLayerInterfaceImpl>();

			It("should map object layers to broad phase layers as expected", [this, BroadPhaseLayerInterface]()
				{
					const int32 NumDefinedLayers = Layers::NUM_LAYERS;
					for (int32 Layer = 0; Layer < NumDefinedLayers; ++Layer)
					{
						JPH::BroadPhaseLayer Result = BroadPhaseLayerInterface->GetBroadPhaseLayer(Layer);
						switch (Layer)
						{
						case Layers::NON_MOVING:
							TestEqual("NON_MOVING maps to NON_MOVING broad phase layer", Result, JOLT::BroadPhaseLayers::NON_MOVING);
							break;
						case Layers::MOVING:
						case Layers::ENEMY:
						case Layers::ENEMYHITBOX:
						case Layers::HITBOX:
						case Layers::PROJECTILE:
						case Layers::ENEMYPROJECTILE:
						case Layers::CAST_QUERY:
						case Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY:
							TestEqual("MOVING and related layers map to MOVING broad phase layer", Result, JOLT::BroadPhaseLayers::MOVING);
							break;
						case Layers::DEBRIS:
							TestEqual("DEBRIS maps to DEBRIS broad phase layer", Result, JOLT::BroadPhaseLayers::DEBRIS);
							break;
						default:
							TestFalse("All defined layers should be handled in the switch", true);
							break;
						}
					}
				});

			It("should report the correct number of broad phase layers", [this, BroadPhaseLayerInterface]()
				{
					const unsigned int NumBroadPhaseLayers = BroadPhaseLayerInterface->GetNumBroadPhaseLayers();
					TestEqual("The number of broad phase layers is as expected", NumBroadPhaseLayers, JOLT::BroadPhaseLayers::NUM_LAYERS);
				});
		});

	Describe("Object vs. Broad Phase Layer Filter Implementation", [this]()
		{
			TSharedPtr<JPH::ObjectVsBroadPhaseLayerFilter> ObjectVsBroadPhaseLayerFilter = MakeShared<FWorldSimOwner::ObjectVsBroadPhaseLayerFilterImpl>();
			// lazy and inference is too
			using T = TArray < JPH::BroadPhaseLayer>;
			using P = TArray < TPair < JPH::ObjectLayer, T>>;

			P PositiveExpectations
			{
				{ Layers::NON_MOVING, T{ JOLT::BroadPhaseLayers::MOVING, JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::MOVING, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::MOVING } },
				{ Layers::HITBOX, T{ } },
				{ Layers::PROJECTILE, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::MOVING } },
				{ Layers::ENEMYPROJECTILE, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::MOVING } },
				{ Layers::ENEMY, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::MOVING } },
				{ Layers::ENEMYHITBOX, T{ JOLT::BroadPhaseLayers::MOVING } },
				{ Layers::CAST_QUERY, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::MOVING } },
				{ Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::MOVING } },
				{ Layers::DEBRIS, T{ JOLT::BroadPhaseLayers::NON_MOVING } }
			};

			P NegativeExpectations
			{
				{ Layers::NON_MOVING, T{ JOLT::BroadPhaseLayers::NON_MOVING } },
				{ Layers::MOVING, T{ JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::HITBOX, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::PROJECTILE, T{ JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::ENEMYPROJECTILE, T{ JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::ENEMY, T{ JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::ENEMYHITBOX, T{ JOLT::BroadPhaseLayers::NON_MOVING, JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::CAST_QUERY, T{ JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, T{ JOLT::BroadPhaseLayers::DEBRIS } },
				{ Layers::DEBRIS, T{ JOLT::BroadPhaseLayers::MOVING, JOLT::BroadPhaseLayers::DEBRIS } }
			};

			It("should return true for a collision when expected", [this, PositiveExpectations, ObjectVsBroadPhaseLayerFilter]()
				{
					for (const auto& Pair : PositiveExpectations)
					{
						for (const auto& BroadPhaseLayer : Pair.Value)
						{
							bool Result = ObjectVsBroadPhaseLayerFilter->ShouldCollide(Pair.Key, BroadPhaseLayer);
							FString ObjectLayerName = StaticEnum<EPhysicsLayer>()->GetNameStringByValue(static_cast<int64>(Pair.Key));
							TestTrue(FString::Printf(TEXT("Object Layer %s should collide with Broad Phase Layer %d"), *ObjectLayerName, BroadPhaseLayer.GetValue()), Result);
						}
					}
				});

			It("should return false for a collision when expected", [this, NegativeExpectations, ObjectVsBroadPhaseLayerFilter]()
				{
					for (const auto& Pair : NegativeExpectations)
					{
						for (const auto& BroadPhaseLayer : Pair.Value)
						{
							bool Result = ObjectVsBroadPhaseLayerFilter->ShouldCollide(Pair.Key, BroadPhaseLayer);
							FString ObjectLayerName = StaticEnum<EPhysicsLayer>()->GetNameStringByValue(static_cast<int64>(Pair.Key));
							TestFalse(FString::Printf(TEXT("Object Layer %s should NOT collide with Broad Phase Layer %d"), *ObjectLayerName, BroadPhaseLayer.GetValue()), Result);
						}
					}
				});
		});
			

	Describe("Object Layer Pair Filter Implementation", [this]()
		{
			// lazy and inference is too
			using T = TArray < JPH::ObjectLayer>;
			using P = TArray < TPair < JPH::ObjectLayer, T>>;

			P PositiveExpectations
			{
				{ Layers::MOVING, T{ Layers::NON_MOVING, Layers::MOVING, Layers::ENEMY, Layers::ENEMYPROJECTILE, Layers::CAST_QUERY } },
				{ Layers::NON_MOVING, T{ Layers::MOVING, Layers::CAST_QUERY, Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, Layers::DEBRIS, Layers::ENEMY, Layers::ENEMYPROJECTILE, Layers::MOVING, Layers::PROJECTILE } },
				{ Layers::ENEMY, T{ Layers::NON_MOVING, Layers::MOVING, Layers::ENEMY, Layers::PROJECTILE, Layers::CAST_QUERY }},
				{ Layers::ENEMYHITBOX, T{ Layers::MOVING, Layers::PROJECTILE, Layers::CAST_QUERY }},
				{ Layers::HITBOX, T{ Layers::PROJECTILE, Layers::ENEMYPROJECTILE, Layers::CAST_QUERY } },
				{ Layers::PROJECTILE, T{ Layers::NON_MOVING, Layers::ENEMYHITBOX, Layers::ENEMY, Layers::HITBOX, Layers::CAST_QUERY } },
				{ Layers::ENEMYPROJECTILE, T{ Layers::NON_MOVING, Layers::MOVING, Layers::HITBOX } },
				{ Layers::CAST_QUERY, T{ Layers::NON_MOVING, Layers::MOVING, Layers::ENEMY, Layers::ENEMYHITBOX, Layers::HITBOX, Layers::PROJECTILE, Layers::ENEMYPROJECTILE } },
				{ Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, T{ Layers::NON_MOVING } },
				{ Layers::DEBRIS, T{ Layers::NON_MOVING }}
			};

			P NegativeExpectations
			{
				{ Layers::MOVING, T{ Layers::HITBOX, Layers::DEBRIS, Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, Layers::PROJECTILE } },
				{ Layers::NON_MOVING, T{ Layers::NON_MOVING, Layers::HITBOX } },
				{ Layers::ENEMY, T{ Layers::ENEMYPROJECTILE, Layers::ENEMYHITBOX } },
				{ Layers::ENEMYHITBOX, T{Layers::NON_MOVING, Layers::ENEMY, Layers::ENEMYPROJECTILE, Layers::HITBOX, Layers::ENEMYHITBOX } },
				{ Layers::HITBOX, T{  Layers::NON_MOVING, Layers::MOVING, Layers::ENEMY, Layers::ENEMYHITBOX, Layers::HITBOX } },
				{ Layers::PROJECTILE, T{ Layers::PROJECTILE, Layers::DEBRIS } },
				{ Layers::ENEMYPROJECTILE, T{ Layers::ENEMYPROJECTILE, Layers::ENEMYHITBOX, Layers::PROJECTILE, Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, Layers::DEBRIS } },
				{ Layers::CAST_QUERY, T{ } },
				{ Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, T{ Layers::MOVING, Layers::ENEMYHITBOX, Layers::HITBOX, Layers::PROJECTILE, Layers::ENEMYPROJECTILE, Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY, Layers::DEBRIS } },
				{ Layers::DEBRIS, T{ Layers::MOVING, Layers::HITBOX, Layers::PROJECTILE, Layers::ENEMYPROJECTILE, Layers::CAST_QUERY, Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY,Layers::DEBRIS } }
			};

			It("should cover all layers in expectations", [this, PositiveExpectations, NegativeExpectations]()
				{
					const int32 NumDefinedLayers = Layers::NUM_LAYERS;
					TSet<JPH::ObjectLayer> CoveredLayersPerLayer;
					for (int32 Layer = 0; Layer < NumDefinedLayers; ++Layer)
					{
						bool FoundInPositive = false;
						for (const auto& Pair : PositiveExpectations)
						{
							if (Pair.Key == Layer)
							{
								FoundInPositive = true;
								for(const auto& CoveredLayer : Pair.Value)
								{
									CoveredLayersPerLayer.Add(CoveredLayer);
								}
								break;
							}
						}
						bool FoundInNegative = false;
						for (const auto& Pair : NegativeExpectations)
						{
							if (Pair.Key == Layer)
							{
								FoundInNegative = true;
								for (const auto& CoveredLayer : Pair.Value)
								{
									CoveredLayersPerLayer.Add(CoveredLayer);
								}
								break;
							}
						}

						// convert the JPH::ObjectLayer (uint8) to an EPhysicsLayer which has the ability to get the Enum name using UE libraries
						FString Name = StaticEnum<EPhysicsLayer>()->GetNameStringByValue(static_cast<int64>(Layer));
						TestTrue(FString::Printf(TEXT("Layer %s is covered in positive expectations"), *Name), FoundInPositive);
						TestTrue(FString::Printf(TEXT("Layer %s is covered in negative expectations"), *Name), FoundInNegative);
						TestEqual("should cover all layers, including self", CoveredLayersPerLayer.Num(), NumDefinedLayers);
					}
				});

			It("should return true for a pair that is allowed", [this, PositiveExpectations]()
				{
					FWorldSimOwner::ObjectLayerPairFilterImpl ClassUnderTest;
					for (const auto& Pair : PositiveExpectations)
					{
						for (const auto& TestLayer : Pair.Value)
						{
							FString TestedLayer = StaticEnum<EPhysicsLayer>()->GetNameStringByValue(static_cast<int64>(Pair.Key));
							FString AgainstLayer = StaticEnum<EPhysicsLayer>()->GetNameStringByValue(static_cast<int64>(TestLayer));
							TestTrue(FString::Printf(TEXT("Layer %s should collide with %s"), *TestedLayer, *AgainstLayer), ClassUnderTest.ShouldCollide(Pair.Key, TestLayer));
						}
					}
				});

			It("should return false for a pair that is not allowed", [this, NegativeExpectations]()
				{
					FWorldSimOwner::ObjectLayerPairFilterImpl ClassUnderTest;
					for (const auto& Pair : NegativeExpectations)
					{
						for (const auto& TestLayer : Pair.Value)
						{
							FString TestedLayer = StaticEnum<EPhysicsLayer>()->GetNameStringByValue(static_cast<int64>(Pair.Key));
							FString AgainstLayer = StaticEnum<EPhysicsLayer>()->GetNameStringByValue(static_cast<int64>(TestLayer));
							TestFalse(FString::Printf(TEXT("Layer %s should collide with %s"), *TestedLayer, *AgainstLayer), ClassUnderTest.ShouldCollide(Pair.Key, TestLayer));
						}
					}
				});
		});
}