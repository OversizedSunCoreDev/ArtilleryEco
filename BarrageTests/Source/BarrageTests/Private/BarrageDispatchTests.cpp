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

BEGIN_DEFINE_SPEC(FBarrageDispatchTests, "Artillery.Barrage.Barrage Dispatch Tests", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

UWorld* TestWorld;
UBarrageDispatch* BarrageDispatch;

END_DEFINE_SPEC(FBarrageDispatchTests)

void FBarrageDispatchTests::Define()
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
		});

		Describe("Physics Queries", [this]()
		{
			It("Should perform sphere casts against a box", [this]()
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
				auto BodiesFilter = BarrageDispatch->GetFilterToIgnoreSingleBody (FBarrageKey ());

				BarrageDispatch->StackUp ();
				BarrageDispatch->StepWorld (0, 0);
				BarrageDispatch->StepWorld (0, 1);
				BarrageDispatch->StepWorld (1, 0);
				BarrageDispatch->StepWorld (1, 1);

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

			It("Should perform ray casts against a box", [this]()
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
				auto BodiesFilter = BarrageDispatch->GetFilterToIgnoreSingleBody (FBarrageKey ());
				BarrageDispatch->StackUp ();
				BarrageDispatch->StepWorld (0, 0);
				BarrageDispatch->StepWorld (0, 1);
				BarrageDispatch->StepWorld (1, 0);
				BarrageDispatch->StepWorld (1, 1);
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
				TestEqual ("RayCast hit distance should be correct", OutHit->Distance, 65.0f);
				TestEqual ("RayCast hit location should be correct", OutHit->Location, FVector (65.0f, 0.0f, 0.0f));
			});
		});
	});

	AfterEach([this]()
	{
		if (TestWorld)
		{
			TestWorld->DestroyWorld(true);
			TestWorld = nullptr;
		}
		BarrageDispatch = nullptr;
	});
}