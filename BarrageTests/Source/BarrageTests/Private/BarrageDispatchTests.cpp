#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "BarrageDispatch.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "FBarrageKey.h"
#include "FBPhysicsInput.h"
#include "FBShapeParams.h"
#include "PhysicsFilters/FastObjectLayerFilters.h"
#include "CoordinateUtils.h"

/*
* This helper function offloads step functions intended to be run OFF the Game Thread.
* It will then await the results on the Game Thread. The results are void
*/
void BlockingWaitOnAsyncWorldSimulation(UBarrageDispatch* Dispatch)
{
	// static lock object to ensure thread safety for parallel calls natural to the internal testing framework
	static FCriticalSection CriticalSection;

	// Create a future that will be immediately waited on the game thread
	auto Task = Async(EAsyncExecution::Thread, [Dispatch]()
		{
			FScopeLock Lock(&CriticalSection);
			// This runs in a separate thread
			// Perform the step world sequence
			Dispatch->StackUp();
			Dispatch->StepWorld(333, 0);
			Dispatch->StepWorld(333, 1);
			Dispatch->StepWorld(333, 2);
			Dispatch->StepWorld(1000, 0);
			Dispatch->StepWorld(1000, 1);
			Dispatch->StepWorld(1000, 2);
			Dispatch->StepWorld(2000, 0);
			Dispatch->StepWorld(2000, 1);
			Dispatch->StepWorld(2000, 2);
			Dispatch->BroadcastContactEvents();
		});

	Task.WaitFor(FTimespan::FromSeconds(30)); // Wait for up to 30 seconds
	return;
}

BEGIN_DEFINE_SPEC(FBarrageDispatchTests, "Artillery.Barrage.Barrage Dispatch Tests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UBarrageDispatch* BarrageDispatch;
	UWorld* DummyWorldOuter;
END_DEFINE_SPEC(FBarrageDispatchTests)
void FBarrageDispatchTests::Define()
{
	BeforeEach([this]()
		{
			// Do not fully initialize the world, just enough to satisfy the subsystem requirements
			DummyWorldOuter = NewObject<UWorld>(); // indirect dependency for some barrage function implementations

			BarrageDispatch = NewObject<UBarrageDispatch>(DummyWorldOuter);
			TestNotNull("BarrageDispatch subsystem should be created", BarrageDispatch);
			BarrageDispatch->RegistrationImplementation();
			BarrageDispatch->GrantWorkerFeed(0);
			BarrageDispatch->GrantClientFeed();
		});
	AfterEach([this]()
		{
			if (BarrageDispatch)
			{
				BarrageDispatch->ConditionalBeginDestroy();
				BarrageDispatch = nullptr;
			}

			if (DummyWorldOuter)
			{
				DummyWorldOuter->ConditionalBeginDestroy();
				DummyWorldOuter = nullptr;
			}
		});
	Describe("Barrage Dispatch Tests", [this]()
		{
			Describe("System Initialization", [this]()
				{
					It("Should initialize subsystem correctly", [this]()
						{
							TestTrue("SelfPtr should be set after initialization", UBarrageDispatch::SelfPtr != nullptr);
							TestTrue("Thread accumulator not be 0", BarrageDispatch->ThreadAccTicker != 0);
							TestTrue("Worker thread accumulator not be 0", BarrageDispatch->WorkerThreadAccTicker != 0);
						});

					It("Should initialize transform and contact pumps", [this]()
						{
							TestTrue("GameTransformPump should be valid", BarrageDispatch->GameTransformPump.IsValid());
							TestTrue("ContactEventPump should be valid", BarrageDispatch->ContactEventPump.IsValid());
						});
				});

			Describe("Primitive Creation", [this]()
				{
					It("Should create box primitives", [this]()
						{
							FSkeletonKey OutKey;
							const FVector3d Point(0, 0, 0);
							const double Dimensions = 100.0;

							FBBoxParams BoxParams = FBarrageBounder::GenerateBoxBounds(
								Point,
								Dimensions,
								Dimensions,
								Dimensions
							);

							FBLet Result = BarrageDispatch->CreatePrimitive(BoxParams, OutKey, 0);

							TestTrue("Box primitive creation should succeed", FBarragePrimitive::IsNotNull(Result));
							TestTrue("Box primitive should have valid key", Result->KeyIntoBarrage != 0);
							TestTrue("Box primitive should have matching skeleton key", Result->KeyOutOfBarrage == OutKey);
						});

					It("Should create sphere primitives", [this]()
						{
							FSkeletonKey OutKey;
							const FVector3d Point(0, 0, 0);
							const double Radius = 50.0;

							FBSphereParams SphereParams = FBarrageBounder::GenerateSphereBounds(Point, Radius);
							FBLet Result = BarrageDispatch->CreatePrimitive(SphereParams, OutKey, 0);

							TestTrue("Sphere primitive creation should succeed", FBarragePrimitive::IsNotNull(Result));
							TestTrue("Sphere primitive should have valid key", Result->KeyIntoBarrage != 0);
							TestTrue("Sphere primitive should have matching skeleton key", Result->KeyOutOfBarrage == OutKey);

						});

					It("Should create capsule primitives", [this]()
						{
							FSkeletonKey OutKey;
							UE::Geometry::FCapsule3d Capsule(FVector3d(0, 0, 0), FVector3d(0, 0, 100), 50.0);

							FBCapParams CapsuleParams = FBarrageBounder::GenerateCapsuleBounds(Capsule);
							FBLet Result = BarrageDispatch->CreatePrimitive(CapsuleParams, OutKey, 0);

							TestTrue("Capsule primitive creation should succeed", FBarragePrimitive::IsNotNull(Result));
							TestTrue("Capsule primitive should have valid key", Result->KeyIntoBarrage != 0);
						});

					It("Should create a projectile", [this]()
						{
							FSkeletonKey OutKey;
							FBBoxParams BoxParams = FBarrageBounder::GenerateBoxBounds(
								FVector3d(0, 0, 0),
								100.0,
								100.0,
								100.0
							);

							FBLet Result = BarrageDispatch->CreateProjectile(BoxParams, OutKey, Layers::MOVING);

							TestTrue("Projectile creation should succeed", FBarragePrimitive::IsNotNull(Result));
							TestTrue("Projectile should have valid key", Result->KeyIntoBarrage != 0);
						});
				});

			Describe("Physics", [this]()
				{
					Describe("Collision Filtering", [this]()
						{
							It("Should perform a sphere cast", [this]
								{
									const double Radius = 50.0;
									const double Distance = 100.0;
									const FVector3d CastFrom(0, 0, 0);
									const FVector3d Direction(1, 0, 0);
									TSharedPtr<FHitResult> OutHit = MakeShared<FHitResult>();

									// Create an object to hit first
									FSkeletonKey BoxKey;
									FBBoxParams BoxParams = FBarrageBounder::GenerateBoxBounds(
										FVector3d(90, 0, 0),
										50.0,
										50.0,
										50.0
									);
									FBLet Box = BarrageDispatch->CreatePrimitive(BoxParams, BoxKey, Layers::MOVING);

									// Create filters using the object we just created
									auto BroadPhaseFilter = BarrageDispatch->GetDefaultBroadPhaseLayerFilter(Layers::MOVING);
									auto ObjectFilter = BarrageDispatch->GetDefaultLayerFilter(Layers::MOVING);
									auto BodiesFilter = BarrageDispatch->GetFilterToIgnoreSingleBody(FBarrageKey());

									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);

									BarrageDispatch->SphereCast(
										Radius,
										Distance,
										CastFrom,
										Direction,
										OutHit,
										BroadPhaseFilter,
										ObjectFilter,
										BodiesFilter
									);

									// Note: Actual hit testing would require setting up the physics world properly
									TestTrue("SphereCast operation should return a blocking hit", OutHit->bBlockingHit);
								});
							It("Should performa a sphere search", [this]
								{
									const FVector3d Location(0, 0, 0);
									const double Radius = 70.0;
									uint32 OutFoundObjectCount = 0;
									TArray<uint32> OutFoundObjects;
									// Create an object to search for
									FSkeletonKey BoxKeys[2];
									FBBoxParams BoxParams = FBarrageBounder::GenerateBoxBounds(
										FVector3d(0, 0, 0),
										50.0,
										50.0,
										50.0
									);
									FBLet Box = BarrageDispatch->CreatePrimitive(BoxParams, BoxKeys[0], Layers::MOVING);

									BoxParams = FBarrageBounder::GenerateBoxBounds(
										FVector3d(30, 0, 0),
										50.0,
										50.0,
										50.0
									);
									BarrageDispatch->CreatePrimitive(BoxParams, BoxKeys[1], Layers::MOVING);
									// Create filters using the object we just created
									auto BroadPhaseFilter = BarrageDispatch->GetDefaultBroadPhaseLayerFilter(Layers::MOVING);
									auto ObjectFilter = BarrageDispatch->GetDefaultLayerFilter(Layers::MOVING);
									auto BodiesFilter = BarrageDispatch->GetFilterToIgnoreSingleBody(Box->KeyIntoBarrage);

									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);

									BarrageDispatch->SphereSearch(
										Box->KeyIntoBarrage,
										Location,
										Radius,
										BroadPhaseFilter,
										ObjectFilter,
										BodiesFilter,
										&OutFoundObjectCount,
										OutFoundObjects
									);

									TestTrue("SphereSearch should find at least one object", OutFoundObjectCount > 0);
								});
							It("Should perform a ray cast", [this]()
								{
									const FVector3d RayStart(0, 0, 0);
									const FVector3d RayEnd(100, 0, 0);
									TSharedPtr<FHitResult> OutHit = MakeShared<FHitResult>();

									// Create an object to hit first
									FSkeletonKey BoxKey;
									FBBoxParams BoxParams = FBarrageBounder::GenerateBoxBounds(
										FVector3d(90, 0, 0),
										50.0,
										50.0,
										50.0
									);
									FBLet Box = BarrageDispatch->CreatePrimitive(BoxParams, BoxKey, Layers::MOVING);
									// Create filters using the object we just created
									auto BroadPhaseFilter = BarrageDispatch->GetDefaultBroadPhaseLayerFilter(Layers::MOVING);
									auto ObjectFilter = BarrageDispatch->GetDefaultLayerFilter(Layers::MOVING);
									auto BodiesFilter = BarrageDispatch->GetFilterToIgnoreSingleBody(FBarrageKey());

									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);

									BarrageDispatch->CastRay(
										RayStart,
										RayEnd - RayStart,
										BroadPhaseFilter,
										ObjectFilter,
										BodiesFilter,
										OutHit
									);

									// Note: Actual hit testing would require setting up the physics world properly
									TestTrue("RayCast operation should return a blocking hit", OutHit->bBlockingHit);

									// 65 is correct, box is at 90, the 50 diam given is in UE coordinates CONVERTED to Jolt half extents e.g. 90 - (50 / 2) = 65
									TestEqual("RayCast hit distance should be correct", OutHit->Distance, 65.0f);
									TestEqual("RayCast hit location should be correct", OutHit->Location, FVector(65.0f, 0.0f, 0.0f));
								});
						});

					Describe("Collision Events", [this]()
						{
							It("Should trigger a contact added event", [this]()
								{
									// Bind to the contact added delegate
									bool bContactEventTriggered = false;
									auto lambda = [&bContactEventTriggered](const BarrageContactEvent&)
										{
											bContactEventTriggered = true;
										};
									BarrageDispatch->OnBarrageContactAddedDelegate.AddLambda(lambda);


									// Setup primitives to collide
									FSkeletonKey Sphere1Key, Sphere2Key;
									auto Sphere1Params = FBarrageBounder::GenerateSphereBounds(FVector3d(0, 0, 0), 50.0);
									FBLet Sphere1 = BarrageDispatch->CreatePrimitive(
										Sphere1Params,
										Sphere1Key,
										Layers::MOVING
									);

									auto Sphere2Params = FBarrageBounder::GenerateSphereBounds(FVector3d(100, 0, 0), 50.0);
									FBLet Sphere2 = BarrageDispatch->CreatePrimitive(
										Sphere2Params,
										Sphere2Key,
										Layers::MOVING
									);

									// Spawn into world then set on a collision course with each other
									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);

									// Set the primitive bodies to move towards each other
									FBarragePrimitive::SetVelocity(FVector3d(50, 0, 0), Sphere1);
									FBarragePrimitive::SetVelocity(FVector3d(-50, 0, 0), Sphere2);

									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);

									TestTrue("Contact added event should be triggered", bContactEventTriggered);
								});

							xIt("Should trigger a contact persisted event", [this]()
								{
									// Bind to the contact persisted delegate
									bool bContactEventTriggered = false;
									auto lambda = [&bContactEventTriggered](const BarrageContactEvent& ContactEvent)
										{
											bContactEventTriggered = true;
										};
									BarrageDispatch->OnBarrageContactPersistedDelegate.AddLambda(lambda);

									// Setup primitives to collide
									FSkeletonKey Sphere1Key, Sphere2Key;
									auto Sphere1Params = FBarrageBounder::GenerateSphereBounds(FVector3d(0, 0, 0), 50.0);
									FBLet Sphere1 = BarrageDispatch->CreatePrimitive(
										Sphere1Params,
										Sphere1Key,
										Layers::MOVING
									);
									auto Sphere2Params = FBarrageBounder::GenerateSphereBounds(FVector3d(25, 0, 0), 50.0);
									FBLet Sphere2 = BarrageDispatch->CreatePrimitive(
										Sphere2Params,
										Sphere2Key,
										Layers::MOVING,
										true
									);
									// Spawn into world then set on a collision course with each other
									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);
									// Set the primitive bodies to move towards each other
									FBarragePrimitive::SetVelocity(FVector3d(10, 0, 0), Sphere1);
									FBarragePrimitive::SetVelocity(FVector3d(-10, 0, 0), Sphere2);
									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);

									FBarragePrimitive::SetVelocity(FVector3d(0, 0, 0), Sphere1);
									FBarragePrimitive::SetVelocity(FVector3d(0, 0, 0), Sphere2);
									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);

									// Process contact events
									TestTrue("Contact persisted event should be triggered", bContactEventTriggered);
								});

							It("Should trigger a contact ended event", [this]()
								{
									// Bind to the contact ended delegate
									bool bContactEventTriggered = false;
									auto lambda = [&bContactEventTriggered](const BarrageContactEvent&)
										{
											bContactEventTriggered = true;
										};
									BarrageDispatch->OnBarrageContactRemovedDelegate.AddLambda(lambda);
									// Setup primitives to collide
									FSkeletonKey Sphere1Key, Sphere2Key;
									auto Sphere1Params = FBarrageBounder::GenerateSphereBounds(FVector3d(0, 0, 0), 50.0);
									FBLet Sphere1 = BarrageDispatch->CreatePrimitive(
										Sphere1Params,
										Sphere1Key,
										Layers::MOVING
									);
									auto Sphere2Params = FBarrageBounder::GenerateSphereBounds(FVector3d(100, 0, 0), 50.0);
									FBLet Sphere2 = BarrageDispatch->CreatePrimitive(
										Sphere2Params,
										Sphere2Key,
										Layers::MOVING
									);
									// Spawn into world then set on a collision course with each other
									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);
									// Set the primitive bodies to move towards each other
									FBarragePrimitive::SetVelocity(FVector3d(50, 0, 0), Sphere1);
									FBarragePrimitive::SetVelocity(FVector3d(-50, 0, 0), Sphere2);
									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);
									// Now set them to move apart
									FBarragePrimitive::SetVelocity(FVector3d(-50, 0, 0), Sphere1);
									FBarragePrimitive::SetVelocity(FVector3d(50, 0, 0), Sphere2);
									BlockingWaitOnAsyncWorldSimulation(BarrageDispatch);
									TestTrue("Contact ended event should be triggered", bContactEventTriggered);
								});
						});
				});
		});
}

BEGIN_DEFINE_SPEC(FBarrageDispatchWorldDependentTests, "Artillery.Barrage.Barrage Dispatch with World Tests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* TestWorld;
UBarrageDispatch* BarrageDispatch;
END_DEFINE_SPEC(FBarrageDispatchWorldDependentTests)
void FBarrageDispatchWorldDependentTests::Define()
{
	BeforeEach([this]()
		{
			// Create a new world for testing
			TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
			TestTrue("Test world should be created", TestWorld != nullptr);

			if (TestWorld)
			{
				FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
				WorldContext.SetCurrentWorld(TestWorld);

				// Initialize the world
				FURL URL;
				TestWorld->InitializeActorsForPlay(URL);
				TestWorld->BeginPlay();

				BarrageDispatch = TestWorld->GetSubsystem<UBarrageDispatch>();
				TestNotNull("BarrageDispatch subsystem should be created", BarrageDispatch);
			}
		});

	Describe("System Initialization", [this]()
		{
			It("Should initialize subsystem correctly", [this]()
				{
					TestTrue("SelfPtr should be set after initialization", UBarrageDispatch::SelfPtr != nullptr);
					TestTrue("Thread accumulator not be 0", BarrageDispatch->ThreadAccTicker != 0);
					TestTrue("Worker thread accumulator not be 0", BarrageDispatch->WorkerThreadAccTicker != 0);
				});

			It("Should initialize transform and contact pumps", [this]()
				{
					TestTrue("GameTransformPump should be valid", BarrageDispatch->GameTransformPump.IsValid());
					TestTrue("ContactEventPump should be valid", BarrageDispatch->ContactEventPump.IsValid());
				});
		});

	Describe("Primitive Creation in World", [this]()
		{
			It("Should create a complex static mesh from an Actor with a UStaticMeshComponent", [this]()
				{
					FSkeletonKey OutKey;
					AActor* TestActor = TestWorld->SpawnActor<AActor>();
					UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(TestActor);
					MeshComponent->SetStaticMesh(LoadObject<UStaticMesh>(MeshComponent, TEXT("/Artillery/TestMeshes/SM_Chair.SM_Chair")));
					MeshComponent->RegisterComponent();
					TestActor->AddInstanceComponent(MeshComponent);

					FBTransform MeshTransform;
					MeshTransform.SetTransform(MeshComponent->GetComponentTransform());
					FBLet Result = BarrageDispatch->LoadComplexStaticMesh(MeshTransform, MeshComponent, OutKey);

					TestTrue("Complex static mesh creation should succeed", FBarragePrimitive::IsNotNull(Result));
					TestTrue("Complex static mesh should have valid key", Result->KeyIntoBarrage != 0);
				});
		});

	AfterEach([this]()
		{
			if (TestWorld)
			{
				TestWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(TestWorld);
				TestWorld = nullptr;
				BarrageDispatch = nullptr;
			}
		});
}