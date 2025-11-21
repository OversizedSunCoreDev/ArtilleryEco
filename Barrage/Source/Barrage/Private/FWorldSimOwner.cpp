#include "FWorldSimOwner.h"

#include "BarrageContactListener.h"
#include "CoordinateUtils.h"
#include "Experimental/CollisionGroupUnaware_FleshBroadPhase.h"
#include "PhysicsCharacter.h"
#include "CastShapeCollectors/SphereCastCollector.h"
#include "CastShapeCollectors/SphereSearchCollector.h"
#include "CollisionDetectionFilters/FirstHitRayCastCollector.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseBruteForce.h"

using namespace JOLT;
//it's going to be quite tempting to make that initexit a const or a reference. don't.
// ReSharper disable once CppPassValueParameterByConstReference
FWorldSimOwner::FWorldSimOwner(float cDeltaTime, InitExitFunction JobThreadInitializer)
{
	DeltaTime = cDeltaTime;

	BarrageToJoltMapping = MakeShareable(new KeyToBody());
	BoxCache = MakeShareable(new BoundsToShape());
	CharacterToJoltMapping = MakeShareable(new TMap<FBarrageKey, TSharedPtr<FBCharacterBase>>());
	//mTestBroadPhase = std::make_shared<CollisionGroupUnaware_FleshBroadPhase>();
	// Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
	// This needs to be done before any other Jolt function is called.
	RegisterDefaultAllocator();

	//hey future friend! collision listeners, character collision, and character collision listeners live below. so...
	//if you are looking for character collision behavior, this is the line that sets it up.
	character_contact_listener = MakeShareable(new BarrageContactListener());
	contact_listener = character_contact_listener;
	Allocator = MakeShareable(new TempAllocatorImpl(AllocationArenaSize));
	physics_system = MakeShareable(new PhysicsSystem());
	// Install trace and assert callbacks
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

	// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
	// It is not directly used in this example but still required.
	Factory::sInstance = new Factory();

	// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
	// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
	// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
	RegisterTypes();

	// We need a job system that will execute physics jobs on multiple threads. Typically
	// you would implement the JobSystem interface yourself and let Jolt Physics run on top
	// of your own job scheduler. JobSystemThreadPool is a (pretty good) example implementation.
	job_system = MakeShareable(
		new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, 5));
	job_system->SetThreadInitFunction(JobThreadInitializer);
	
	
	// Now we can create the actual physics system.
	physics_system->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
	                     broad_phase_layer_interface, object_vs_broadphase_layer_filter,
	                     object_vs_object_layer_filter);  //, mTestBroadPhase.get()); this can be used to experiment with broadphases.
	physics_system->SetContactListener(contact_listener.Get());
	// The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
	// variant of this. We're going to use the locking version.
	body_interface = &physics_system->GetBodyInterface();


	// Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
	// You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
	// Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
	physics_system->OptimizeBroadPhase();

	// here's Andrea's transform into jolt.
	//	https://youtu.be/jhCupKFly_M?si=umi0zvJer8NymGzX&t=438
}

inline void FWorldSimOwner::SphereCast(
	double Radius,
	double Distance,
	FVector3d CastFrom,
	FVector3d Direction,
	TSharedPtr<FHitResult> OutHit,
	const BroadPhaseLayerFilter& BroadPhaseFilter,
	const ObjectLayerFilter& ObjectFilter,
	const BodyFilter& BodiesFilter) const
{
	check(OutHit.IsValid());
	OutHit->Init();
	// In order to denote whether we actually hit anything, we'll munge Jolt's uint32 BodyID values into
	// the int32 of `FHitResult::MyItem`. This should be fine.
	OutHit->MyItem = JPH::BodyID::cInvalidBodyID;

	ShapeCastSettings settings;
	settings.mUseShrunkenShapeAndConvexRadius = true;
	settings.mReturnDeepestPoint = true;

	JPH::SphereShape sphere(Radius);

	JPH::Vec3 JoltCastFrom = CoordinateUtils::ToJoltCoordinates(CastFrom);
	JPH::Vec3 JoltDirection = CoordinateUtils::ToJoltCoordinates(Direction) * Distance;
	
	JPH::RShapeCast ShapeCast(
		&sphere,
		JPH::Vec3::sReplicate(1.0f),
		JPH::RMat44::sTranslation(JoltCastFrom),
		JoltDirection);

	// Actually do the shapecast
	SphereCastCollector CastCollector(*(physics_system.Get()), ShapeCast);
	physics_system->GetNarrowPhaseQueryNoLock().CastShape(
		ShapeCast,
		settings,
		ShapeCast.mCenterOfMassStart.GetTranslation(),
		CastCollector,
		BroadPhaseFilter,
		ObjectFilter,
		BodiesFilter);

	if (CastCollector.mBody) {
		// Fill out the hit result
		FHitResult* HitResultPtr = OutHit.Get();

		HitResultPtr->MyItem = CastCollector.mBody->GetID().GetIndexAndSequenceNumber();
		HitResultPtr->bBlockingHit = true;
		
		FVector3f UnrealContactPos = CoordinateUtils::FromJoltCoordinates(CastCollector.mContactPosition);
		HitResultPtr->Location.Set(UnrealContactPos.X, UnrealContactPos.Y, UnrealContactPos.Z);
		HitResultPtr->ImpactPoint.Set(UnrealContactPos.X, UnrealContactPos.Y, UnrealContactPos.Z);
		HitResultPtr->Distance = (UnrealContactPos - FVector3f(CastFrom)).Length();

		JPH::Vec3& HitNormal = CastCollector.mContactNormal;
		FVector3f UnrealImpactNormal = CoordinateUtils::FromJoltUnitVector(HitNormal);
		HitResultPtr->ImpactNormal.Set(UnrealImpactNormal.X, UnrealImpactNormal.Y, UnrealImpactNormal.Z);
	}
}

inline void FWorldSimOwner::SphereSearch(
	const JPH::BodyID& CastingBody,
	const FVector3d& Location,
	double Radius,
	const JPH::BroadPhaseLayerFilter& BroadPhaseFilter,
	const JPH::ObjectLayerFilter& ObjectFilter,
	const JPH::BodyFilter& BodiesFilter,
	uint32* OutFoundObjectCount,
	TArray<uint32>& OutFoundObjectIDs) const
{
	JPH::Vec3 JoltLocation = CoordinateUtils::ToJoltCoordinates(Location);
	
	SphereSearchCollector Collector(physics_system.Get()->GetBodyLockInterfaceNoLock(), BodiesFilter);
	physics_system->GetBroadPhaseQuery().CollideSphere(JoltLocation, Radius, Collector, BroadPhaseFilter, ObjectFilter);

	(*OutFoundObjectCount) = Collector.BodyCount;
	for (uint32 FoundBodyIdx = 0; FoundBodyIdx < Collector.BodyCount; ++FoundBodyIdx)
	{
		OutFoundObjectIDs.Add(Collector.mBodies[FoundBodyIdx]->GetID().GetIndexAndSequenceNumber());
	}
}

inline void FWorldSimOwner::CastRay(FVector3d CastFrom, FVector3d Direction, const BroadPhaseLayerFilter& BroadPhaseFilter, const ObjectLayerFilter& ObjectFilter, const BodyFilter& BodiesFilter, TSharedPtr<FHitResult> OutHit) const
{
	check(OutHit.IsValid());
	OutHit->Init();
	// Use the same ID munging as we do in SphereCast
	OutHit->MyItem = JPH::BodyID::cInvalidBodyID;
	
	JPH::Vec3 JoltCastFromLocation = CoordinateUtils::ToJoltCoordinates(CastFrom);
	JPH::Vec3 JoltDirection = CoordinateUtils::ToJoltCoordinates(Direction);
	
	RRayCast Ray(JoltCastFromLocation, JoltDirection);
	RayCastResult CastResult;
	FirstHitRayCastCollector FirstHitCollector(Ray, CastResult, physics_system->GetBodyLockInterfaceNoLock(), BodiesFilter);
	
	physics_system->GetBroadPhaseQuery().CastRay(RayCast(Ray), FirstHitCollector, BroadPhaseFilter, ObjectFilter);

	if (FirstHitCollector.mHit.mBodyID != BodyID())
	{
		// Fill out the hit result
		FHitResult* HitResultPtr = OutHit.Get();

		HitResultPtr->MyItem = FirstHitCollector.mHit.mBodyID.GetIndexAndSequenceNumber();
		HitResultPtr->bBlockingHit = true;
		
		FVector3f UnrealContactPos = CoordinateUtils::FromJoltCoordinates(FirstHitCollector.mContactPosition);
		HitResultPtr->Location.Set(UnrealContactPos.X, UnrealContactPos.Y, UnrealContactPos.Z);
		HitResultPtr->ImpactPoint.Set(UnrealContactPos.X, UnrealContactPos.Y, UnrealContactPos.Z);
		HitResultPtr->Distance = (UnrealContactPos - FVector3f(CastFrom)).Length();
	}
}

inline EMotionType FWorldSimOwner::LayerToMotionTypeMapping(uint16 Layer)
{
	switch (Layer)
	{
	case Layers::NON_MOVING:
		return EMotionType::Static;
	case Layers::MOVING:
		return EMotionType::Dynamic;
	case Layers::HITBOX:
		return EMotionType::Kinematic;
	case Layers::PROJECTILE:
		return EMotionType::Kinematic;
	case Layers::ENEMYPROJECTILE:
		return EMotionType::Kinematic;
	case Layers::ENEMY:
		return EMotionType::Dynamic;
	case Layers::ENEMYHITBOX:
		return EMotionType::Kinematic;
	case Layers::CAST_QUERY:
		return EMotionType::Kinematic;
	case Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY:
		return EMotionType::Kinematic;
	case Layers::DEBRIS:
		return EMotionType::Dynamic;
	default:
		JPH_ASSERT(false);
		return EMotionType::Static;
	}
}

inline EMotionQuality LayerToMotionQualityMapping(uint16 Layer)
{
	switch (Layer)
	{
	case Layers::NON_MOVING:
		return EMotionQuality::Discrete;
	case Layers::MOVING:
		return EMotionQuality::LinearCast;
	case Layers::HITBOX:
		return EMotionQuality::Discrete;
	case Layers::PROJECTILE:
		return EMotionQuality::LinearCast;
	case Layers::ENEMYPROJECTILE:
		return EMotionQuality::LinearCast;
	case Layers::ENEMYHITBOX:
		return EMotionQuality::LinearCast;
	case Layers::ENEMY:
		return EMotionQuality::Discrete;
	case Layers::CAST_QUERY:
		return EMotionQuality::Discrete;
	case Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY:
		return EMotionQuality::Discrete;
	case Layers::DEBRIS:
		return EMotionQuality::Discrete;
	default:
		JPH_ASSERT(false);
		return EMotionQuality::Discrete;
	}
}

inline Ref<Shape> FWorldSimOwner::AttemptBoxCache(double JoltX, double JoltY, double JoltZ, float HEReduceMin)
{
	Vec3 Bounds(JoltX, JoltY, JoltZ);
	void* At = &Bounds;
	uint64 BoundsHash = HashBytes(At,sizeof(Bounds));
	if (!BoxCache->contains(BoundsHash))
	{
		Ref<Shape> NewShape = new BoxShape(Vec3(JoltX, JoltY, JoltZ), FMath::Min(HEReduceMin / 2.f, 0.01));
		BoxCache->insert_or_assign(BoundsHash, NewShape);
		return NewShape;
	}
	Ref<Shape> Result;
	if (BoxCache->find(BoundsHash, Result))
	{
		return Result;
	}
	Ref<Shape> NewShape = new BoxShape(Vec3(JoltX, JoltY, JoltZ), FMath::Min(HEReduceMin / 2.f, 0.01));
	BoxCache->insert_or_assign(BoundsHash, NewShape);
	return NewShape;
}

//we need the coordinate utils, but we don't really want to include them in the .h
inline FBarrageKey FWorldSimOwner::CreatePrimitive(FBBoxParams& ToCreate, uint16 Layer, bool IsSensor, bool forceDynamic, bool isMovable)
{
	//if movable, check if dynamic. if not movable but dynamic, come on guys.
	EMotionType MovementType = isMovable ?
		(forceDynamic ? EMotionType::Dynamic : LayerToMotionTypeMapping(Layer)) : EMotionType::Static;
	EMotionQuality MotionQuality = LayerToMotionQualityMapping(Layer);

	Vec3 HalfExtent(ToCreate.JoltX, ToCreate.JoltY, ToCreate.JoltZ);
	float HEReduceMin = HalfExtent.ReduceMin();
	if(MotionQuality == EMotionQuality::LinearCast)
	{
		HEReduceMin = 0.01;
	}

	//not really sure how much our cache helps us, but it could in theory improve GJK perf? Removed for perf testing.
	Ref<Shape> CachedShape = AttemptBoxCache(ToCreate.JoltX, ToCreate.JoltY, ToCreate.JoltZ, FMath::Min(HEReduceMin / 2.f, 0.02));

	// We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()
	// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
	BodyCreationSettings box_body_settings(CachedShape,
			 CoordinateUtils::ToJoltCoordinates(ToCreate.Offset.X, ToCreate.Offset.Y, ToCreate.Offset.Z) +
	                                       CoordinateUtils::ToJoltCoordinates(ToCreate.Point.GridSnap(1)),
	                                       Quat::sIdentity(),
	                                       MovementType, Layer);
	JPH::MassProperties msp;
	msp.ScaleToMass(ToCreate.MassClass); //actual mass in kg
	box_body_settings.mMassPropertiesOverride = msp;
	box_body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
	box_body_settings.mIsSensor = IsSensor;
	box_body_settings.mMotionQuality = MotionQuality;
	box_body_settings.mRestitution = 0.0;
	
	if (MovementType == EMotionType::Dynamic && (Layer == Layers::MOVING || Layer == Layers::ENEMY))
	{
		box_body_settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ | EAllowedDOFs::RotationY;
	}
	
	// Create the actual rigid body
	Body* box_body = body_interface->CreateBody(box_body_settings);
	// Note that if we run out of bodies this can return nullptr

	// Queue adding it
	AddInternalQueuing(box_body->GetID(), 0);// oh no. yeah this is.... this is for batching the add.
															//but really, we'd like to batch create.
															//but we can't do that without both proof that it's slow this way
															//and redoing barragekeys so that they aren't reversible with bodyIDs.
															//this is because we need the barrage key so stuff can queue operations against the primitive...
															//and without create, we don't have a bodyID. in fact, there's not a body at all yet.
															//this isn't a hard change, but it's a SERIOUS breaking change. we need more evidence before committing.
															// IMPORTANT. REVISIT. THAT MEANS YOU. WHOEVER YOU ARE.
	BodyID BodyIDTemp = box_body->GetID();
	FBarrageKey FBK = GenerateBarrageKeyFromBodyId(BodyIDTemp);
	//Barrage key is unique to WORLD and BODY. This is crushingly important.
	BarrageToJoltMapping->insert(FBK, BodyIDTemp);

	return FBK;
}

inline FBarrageKey FWorldSimOwner::CreatePrimitive(FBCapParams& ToCreate, uint16 Layer, bool IsSensor, bool forceDynamic, bool isMovable)
{
	//if movable, check if dynamic. if not movable but dynamic, come on guys.
	EMotionType MovementType = isMovable ?
		(forceDynamic ? EMotionType::Dynamic : LayerToMotionTypeMapping(Layer)) : EMotionType::Static;
	EMotionQuality MotionQuality = LayerToMotionQualityMapping(Layer);
	
	//not really sure how much our cache helps us, but it could in theory improve GJK perf? Removed for perf testing.
	//Ref<Shape> CachedShape = AttemptBoxCache(ToCreate.JoltX, ToCreate.JoltY, ToCreate.JoltZ, FMath::Min(HEReduceMin / 2.f, 0.01));
	Ref<Shape> NewShape = new CapsuleShape(ToCreate.JoltHalfHeightOfCylinder, ToCreate.JoltRadius);
	
	// We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()
	// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
	BodyCreationSettings cap_body_settings(NewShape,
	                                       CoordinateUtils::ToJoltCoordinates((FVector3f(ToCreate.point) + ToCreate.Offset).GridSnap(1)),
	                                       Quat::sIdentity(),
	                                       MovementType, Layer);
	JPH::MassProperties msp;
	msp.ScaleToMass(ToCreate.MassClass); //actual mass in kg
	cap_body_settings.mMassPropertiesOverride = msp;
	cap_body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
	cap_body_settings.mIsSensor = IsSensor;
	cap_body_settings.mMotionQuality = MotionQuality;
	cap_body_settings.mRestitution = 0.08;
	cap_body_settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ | EAllowedDOFs::RotationX | EAllowedDOFs::RotationY | EAllowedDOFs::RotationZ;
	
	// Create the actual rigid body
	Body* box_body = body_interface->CreateBody(cap_body_settings);
	// Note that if we run out of bodies this can return nullptr

	// Queue adding it
	AddInternalQueuing(box_body->GetID(), 0);// TODO: consider your life choices. you don't wanna sell death sticks.
	BodyID BodyIDTemp = box_body->GetID();
	FBarrageKey FBK = GenerateBarrageKeyFromBodyId(BodyIDTemp);
	//Barrage key is unique to WORLD and BODY. This is crushingly important.
	BarrageToJoltMapping->insert(FBK, BodyIDTemp);

	return FBK;
}

//we need the coordinate utils, but we don't really want to include them in the .h
inline FBarrageKey FWorldSimOwner::CreatePrimitive(FBCharParams& ToCreate, uint16 Layer)
{
	TSharedPtr<FBCharacter> NewCharacter = MakeShareable<FBCharacter>(new FBCharacter);
	NewCharacter->mHeightStanding = 2 * ToCreate.JoltHalfHeightOfCylinder;
	NewCharacter->mRadiusStanding = ToCreate.JoltRadius;
	NewCharacter->mInitialPosition = CoordinateUtils::ToJoltCoordinates(ToCreate.point);
	NewCharacter->mMaxSpeed = ToCreate.speed;
	if(NewCharacter->mInitialPosition.IsNearZero() || NewCharacter->mInitialPosition.IsNaN())
	{
		NewCharacter->mInitialPosition = Vec3::sZero();
	}
	NewCharacter->World = this->physics_system;
	NewCharacter->mDeltaTime = DeltaTime;
	NewCharacter->mForcesUpdate = Vec3::sZero();
	NewCharacter->mCharacterSettings.mCharacterPadding = 0.08f;
	NewCharacter->mListener = character_contact_listener;
	// Create the shape
	BodyID BodyIDTemp = NewCharacter->Create(&this->CharacterVsCharacterCollisionSimple);
	//AddInternalQueuing(BodyIDTemp, 0);// we can't figure this out yet. we'll have to set it later or rearch for data exposure reasons. --JMK, can kicka
	//Barrage key is unique to WORLD and BODY. This is crushingly important.
	FBarrageKey FBK = GenerateBarrageKeyFromBodyId(BodyIDTemp);
	BarrageToJoltMapping->insert(FBK, BodyIDTemp);
	CharacterToJoltMapping->Add(FBK, NewCharacter);
	return FBK;
}

inline FBarrageKey FWorldSimOwner::CreatePrimitive(FBSphereParams& ToCreate, uint16 Layer, bool IsSensor)
{
	EMotionType MovementType = LayerToMotionTypeMapping(Layer);
	BodyCreationSettings sphere_settings(new SphereShape(ToCreate.JoltRadius),
	                                     CoordinateUtils::ToJoltCoordinates(ToCreate.point.GridSnap(1)),
	                                     Quat::sIdentity(),
	                                     MovementType,
	                                     Layer);
	sphere_settings.mIsSensor = IsSensor;
	BodyID BodyIDTemp = body_interface->CreateBody(sphere_settings)->GetID();
	AddInternalQueuing(BodyIDTemp, 0);// we can't figure this out yet. we'll have to set it later or rearch for data exposure reasons. --JMK, can kicka
	FBarrageKey FBK = GenerateBarrageKeyFromBodyId(BodyIDTemp);
	//Barrage key is unique to WORLD and BODY. This is crushingly important.
	BarrageToJoltMapping->insert(FBK, BodyIDTemp);
	return FBK;
}

inline FBarrageKey FWorldSimOwner::CreatePrimitive(FBCapParams& ToCreate, uint16 Layer, bool IsSensor, FMassByCategory::BMassCategories MassClass)
{
	EMotionType MovementType = LayerToMotionTypeMapping(Layer);
	BodyCreationSettings cap_settings(new CapsuleShape(ToCreate.JoltHalfHeightOfCylinder, ToCreate.JoltRadius),
	                                  CoordinateUtils::ToJoltCoordinates(ToCreate.point.GridSnap(1)),
	                                  Quat::sIdentity(),
	                                  MovementType,
	                                  Layer);
	JPH::MassProperties msp;
	msp.ScaleToMass(MassClass); //actual mass in kg
	cap_settings.mMassPropertiesOverride = msp;
	cap_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
	cap_settings.mIsSensor = IsSensor;
	BodyID BodyIDTemp = body_interface->CreateBody(cap_settings)->GetID();
	AddInternalQueuing(BodyIDTemp, 0);// You know, it feels worse each time I use it.
	FBarrageKey FBK = GenerateBarrageKeyFromBodyId(BodyIDTemp);
	//Barrage key is unique to WORLD and BODY. This is crushingly important.
	BarrageToJoltMapping->insert(FBK, BodyIDTemp);
	return FBK;
}

//If you set layer to nonmoving, you should set movement type to nonmoving as well or you're going to have
//a really terrible time. It's not technically wrong to do this, so we don't throw, but there's no good
//reason I can think of. At this API level, intended for extremely advanced users,
//it is our policy to be no-throw wherever possible, and to permit behavior that
//is not rational so long as it is sane.
FBLet FWorldSimOwner::LoadComplexStaticMesh(FBTransform& MeshTransform,
                                            const UStaticMeshComponent* StaticMeshComponent,
                                            FSkeletonKey Outkey, Layers::EJoltPhysicsLayer Layer, EMotionType Movement, bool IsSensor, bool ForceActualMesh, FVector CenterOfMassTranslation)
{
	// using ParticlesType = Chaos::TParticles<Chaos::FRealSingle, 3>;
	// using ParticleVecType = Chaos::TVec3<Chaos::FRealSingle>;
	using ::CoordinateUtils;
	//why do we check render data here?
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh() || !StaticMeshComponent->GetStaticMesh()->GetRenderData())
	{
		
		UE_LOG(LogTemp, Warning, TEXT("Can't find body for setup..."));
		return nullptr;
	}
	EMotionQuality MotionQuality = LayerToMotionQualityMapping(Layer);
	UBodySetup* body = StaticMeshComponent->GetStaticMesh()->GetBodySetup();
	if (!body)
	{
		UE_LOG(LogTemp, Warning, TEXT("Body setup may be unsupported..."));
		return nullptr; // we don't accept anything but complex or primitive yet.
		//simple collision tends to use primitives, in which case, don't call this
		//or compound shapes which will get added back in.
	}
	TObjectPtr<UStaticMesh> CollisionMesh = StaticMeshComponent->GetStaticMesh();
	if (!CollisionMesh || ForceActualMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Falling back to ACTUAL MESH."));
		CollisionMesh = StaticMeshComponent->GetStaticMesh();
	}
	if (!CollisionMesh)
	{
		return nullptr;
	}
	if (!CollisionMesh->IsCompiling() || !CollisionMesh->IsPostLoadThreadSafe())
	{
		UBodySetup* collbody = CollisionMesh->GetBodySetup();
		if (collbody == nullptr)
		{
			return nullptr;
		}

		//Here we go!
		TArray<Chaos::FTriangleMeshImplicitObjectPtr>& MeshSet = collbody->TriMeshGeometries;
		JPH::VertexList JoltVerts;
		JPH::IndexedTriangleList JoltIndexedTriangles;
		uint32 tris = 0;
		for (Chaos::FTriangleMeshImplicitObjectPtr& Mesh : MeshSet)
		{
			tris += Mesh->Elements().GetNumTriangles();
		}
		JoltVerts.reserve(tris);
		JoltIndexedTriangles.reserve(tris);
		for (Chaos::FTriangleMeshImplicitObjectPtr& Mesh : MeshSet)
		{
			//indexed triangles are made by collecting the vertexes, then generating triples describing the triangles.
			//this allows the heavier vertices to be stored only once, rather than each time they are used. for large models
			//like terrain, this can be extremely significant. though, it's not truly clear to me if it's worth it.
			const Chaos::FTrimeshIndexBuffer& VertToTriBuffers = Mesh->Elements();
			const Chaos::TArrayCollectionArray<Chaos::TVector<float, 3>>& Verts = Mesh->Particles().X();
			if (VertToTriBuffers.RequiresLargeIndices())
			{
				for (auto& aTri : VertToTriBuffers.GetLargeIndexBuffer())
				{
					JoltIndexedTriangles.push_back(IndexedTriangle(aTri[2], aTri[1], aTri[0]));
				}
			}
			else
			{
				for (auto& aTri : VertToTriBuffers.GetSmallIndexBuffer())
				{
					JoltIndexedTriangles.push_back(IndexedTriangle(aTri[2], aTri[1], aTri[0]));
				}
			}

			for (auto& vtx : Verts)
			{
				//need to figure out how to defactor this without breaking typehiding or having to create a bunch of util.h files.
				//though, tbh, the util.h is the play. TODO: util.h ?
				JoltVerts.push_back(CoordinateUtils::ToJoltCoordinates(vtx));
			}
		}
		JPH::MeshShapeSettings FullMesh(JoltVerts, JoltIndexedTriangles);
		//just the last boiler plate for now.
		JPH::ShapeSettings::ShapeResult err = FullMesh.Create();
		if (err.HasError())
		{
			return nullptr;
		}
		//TODO: should we be holding the shape ref in gamesim owner?
		auto& shape = err.Get();
		
		BodyCreationSettings creation_settings;
		creation_settings.mMotionType = Movement;
		creation_settings.mObjectLayer = Layer;
		creation_settings.mFriction = 0.5f;
		creation_settings.mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
		creation_settings.mRestitution = 0;
		creation_settings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(shape->GetLocalBounds().GetExtent() *2, 1);
		creation_settings.mMassPropertiesOverride.mMass = EBWeightClasses::HugeEnemy;
		creation_settings.mMotionQuality = MotionQuality;
		creation_settings.mUseManifoldReduction = true;
		creation_settings.mIsSensor = IsSensor;
		creation_settings.mAllowSleeping = false;
		Shape::ShapeResult result = shape->ScaleShape(MeshTransform.GetJoltScale());
		if (result.HasError() || result.IsEmpty())
		{
			throw;
		}
		//reminder: translation and position do differ.
		if (CenterOfMassTranslation == FVector::ZeroVector && MeshTransform.GetRotationQuat() == FQuat4f::Identity)
		{
			creation_settings.SetShape(result.Get());
		}
		else
		{
			Ref<Shape> OriginAndRotationApplied = new RotatedTranslatedShape(CoordinateUtils::ToJoltCoordinates(CenterOfMassTranslation), CoordinateUtils::ToJoltRotation(MeshTransform.GetRotationQuat()),  result.Get());
			creation_settings.SetShape(OriginAndRotationApplied);
		}
		creation_settings.mPosition = CoordinateUtils::ToJoltCoordinates(MeshTransform.GetUnrealLocation());
		BodyID bID = body_interface->CreateBody(creation_settings)->GetID();
		AddInternalQueuing(bID, 0);// You know that scene where data tries alcohol, hates it, and immediately orders another?
		FBarrageKey FBK = GenerateBarrageKeyFromBodyId(bID);
		BarrageToJoltMapping->insert(FBK, bID);
		FBLet shared = MakeShareable(new FBarragePrimitive(FBK, Outkey));
		return shared;
	}
	return nullptr;
}

void FWorldSimOwner::StepSimulation()
{
	// Step the world
	TSharedPtr<JPH::TempAllocatorImpl> AllocHoldOpen = Allocator;
	TSharedPtr<JPH::JobSystemThreadPool> JobHoldOpen = job_system;
	TSharedPtr<JPH::PhysicsSystem> PhysicsHoldOpen = physics_system;

		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Physics Update");
		// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable
		// we run pretty fast, so... normally fine.
		constexpr int cCollisionSteps = 1;
		if (AllocHoldOpen && JobHoldOpen)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("Physics Update");

		PhysicsHoldOpen->Update(DeltaTime, cCollisionSteps, AllocHoldOpen.Get(), JobHoldOpen.Get());
		}
	
}

bool FWorldSimOwner::OptimizeBroadPhase()
{
	// Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
	// You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
	// Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
	TSharedPtr<JPH::PhysicsSystem> HoldOpen = physics_system;
	HoldOpen->OptimizeBroadPhase();
	return true;
}

FBarrageKey FWorldSimOwner::GenerateBarrageKeyFromBodyId(const BodyID& Input) const
{
	return GenerateBarrageKeyFromBodyId(Input.GetIndexAndSequenceNumber());
}

FBarrageKey FWorldSimOwner::GenerateBarrageKeyFromBodyId(const uint32 RawIndexAndSequenceNumberInput) const
{
	uint64_t KeyCompose = PointerHash(this);
	KeyCompose = KeyCompose << 32;
	KeyCompose |= RawIndexAndSequenceNumberInput;
	return FBarrageKey(KeyCompose);
}

FWorldSimOwner::~FWorldSimOwner()
{
	UnregisterTypes();
	Factory::sInstance = nullptr;

	//this is the canonical order.
	//grab our hold open.		
	TSharedPtr<JPH::PhysicsSystem> HoldOpen = physics_system;
	physics_system.Reset(); //cast it into the fire.
	CharacterToJoltMapping->Reset();//free characters so they don't double free inner shapes.
	std::this_thread::yield(); //Cycle.
	HoldOpen.Reset();
	job_system.Reset();
	Allocator.Reset();
}


void FWorldSimOwner::AddInternalQueuing(JPH::BodyID ToQueue, uint64 ordinant)
{
	//oh boy. ohhhhh boy. oh boy oh boy oh boy oh boy. we have made the big strangeness now.
	//TODO: does this need a hold open? dear god in heaven.
	ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
					FBPhysicsInput(ToQueue, ordinant, PhysicsInputType::ADD)); // oh boy. hhoo. this is NOT good. see fun story. you only need the ids to do adds.
}
bool FWorldSimOwner::UpdateCharacter(FBPhysicsInput& Update)
{
	FBarrageKey key = Update.Target;
	TSharedPtr<FBCharacterBase>* CharacterOuter = CharacterToJoltMapping->Find(key);
	//As you add handling for Characters with Inner Shapes, you'll need to use something like the line below.
	//Unfortunately, it's going to be a lot of work. Right now, there's a bug preventing us from doing it, something in the lifecycle.
	//auto CharacterInner = BarrageToJoltMapping->find(Update.Target.Get()->KeyIntoBarrage); 
	if (CharacterOuter)
	{
		auto HoldOpen = physics_system;

		(*CharacterOuter)->IngestUpdate(Update);
		return true;
	}
	return false;
}

//convenience function for bulk updates.
bool FWorldSimOwner::UpdateCharacters(TSharedPtr<TArray<FBPhysicsInput>> Array)
{
	for (FBPhysicsInput& update : *Array)
	{
		UpdateCharacter(update);
	}
	return true;
}
