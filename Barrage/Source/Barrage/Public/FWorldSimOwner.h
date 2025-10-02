// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

//unity build isn't fond of this, but we really want to completely contain these types and also prevent any collisions.
//there's other ways to do this, but the correct way is a namespace so far as I know.
// see: https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine?application_version=5.4#namespaces

#include "BarrageDispatch.h"
#include "FBShapeParams.h"
#include "FBarrageKey.h"
#include "FBPhysicsInput.h"
#include "SkeletonTypes.h"
#include "EPhysicsLayer.h"
//#include "Experimental/CollisionGroupUnaware_FleshBroadPhase.h"
#include "BarrageContactListener.h"
#include "IsolatedJoltIncludes.h"

// All Jolt symbols are in the JPH namespace

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char* inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
}

// We're also using STL classes in this example
#ifdef JPH_ENABLE_ASSERTS
// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
	// Breakpoint
	return true;
};
#endif // JPH_ENABLE_ASSERTS

class FBCharacterBase
{
public:
	virtual ~FBCharacterBase() = default;

	JPH::Ref<JPH::Shape> InnerStandingShape;
	JPH::RVec3 mInitialPosition = JPH::RVec3::sZero();
	float mHeightStanding = 1.35f;
	float mMaxSpeed = 15.0f;
	float mRadiusStanding = 0.3f;
	JPH::CharacterVirtualSettings mCharacterSettings;
	JPH::CharacterVirtual::ExtendedUpdateSettings mUpdateSettings;
	// Accumulated during IngestUpdate
	JPH::Quat mThrottleModel = JPH::Quat(100, 100, 100, 100);
	JPH::Vec3 mLocomotionUpdate = JPH::Vec3::sZero();
	JPH::Vec3 mForcesUpdate = JPH::Vec3::sZero();
	JPH::Vec3 mGravity = JPH::Vec3::sZero();
	JPH::Quat mCapsuleRotationUpdate = JPH::Quat::sIdentity();
	JPH::Ref<JPH::CharacterVirtual> mCharacter = JPH::Ref<JPH::CharacterVirtual>();
	float mDeltaTime = 0.01; //set this yourself or have a bad time.
	TSharedPtr<BarrageContactListener> mListener;
	// Calculated effective velocity after a step
	JPH::Vec3 mEffectiveVelocity = JPH::Vec3::sZero();
	virtual void IngestUpdate(FBPhysicsInput& input) = 0;
	virtual void StepCharacter() = 0;
	
	TSharedPtr<JPH::PhysicsSystem, ESPMode::ThreadSafe> World;
protected:
	friend class FWorldSimOwner;
};

class BARRAGE_API FWorldSimOwner
{
	// If you want your code to compile using single or double precision write 0.0_r to get a Real value that compiles to double or float depending if JPH_DOUBLE_PRECISION is set or not.


public:
	
	mutable bool Optimized = false;
	//members are destructed first in, last out.
	//https://stackoverflow.com/questions/2254263/order-of-member-constructor-and-destructor-calls
	//BodyId is actually a freaking 4byte struct, so it's _worse_ potentially to have a pointer to it than just copy it.
	TSharedPtr<KeyToBody> BarrageToJoltMapping;
	TSharedPtr<BoundsToShape> BoxCache;
	TSharedPtr<TMap<FBarrageKey, TSharedPtr<FBCharacterBase>>> CharacterToJoltMapping;
	//std::shared_ptr<JPH::CollisionGroupUnaware_FleshBroadPhase> mTestBroadPhase;
	
	 /*
	 * 
	 * @return true if found a BodyID in the map, false if we did not find one and `result` is false
	 */
	bool GetBodyIDOrDefault(FBarrageKey Key, JPH::BodyID& result) const
	{
		bool FoundBodyID = BarrageToJoltMapping->find(Key, result);
		if(!FoundBodyID)
		{
			result = JPH::BodyID(); // invalid BUT characters HAVE NO FLESSSSSSSH BLEHHHHHH (seriously, without an inner shape, they lack a body)
		}
		return FoundBodyID;
	}

	const unsigned int AllocationArenaSize = 256 * 1024 * 1024;
	TSharedPtr<JPH::TempAllocatorImpl> Allocator;
	// List of active characters in the scene so they can collide
	//https://github.com/jrouwe/JoltPhysics/blob/e3ed3b1d33f3a0e7195fbac8b45b30f0a5c8a55b/Jolt/Physics/Character/CharacterVirtual.h#L143
	//note this is the only extant character collision handler, and it is not very efficient since it doesn't use a BSP.
	//This will need to go as soon as we have more than three characters or so.
	JPH::CharacterVsCharacterCollisionSimple CharacterVsCharacterCollisionSimple;
	// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
	// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
	// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
	// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
	// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
	
	/// Class that determines if two object layers can collide
	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
		{
			switch (inObject1)
			{
			// TODO: in future if we want to enforce principle of hitbox vs. movement colliders being different,
			// could force all entities to have both a moving physics shape + a hitbox physics shape and remove collision between MOVING and {PROJECTILE, CAST_QUERY}
			case Layers::NON_MOVING:
				return inObject2 != Layers::NON_MOVING && inObject2 != Layers::HITBOX && inObject2 != Layers::ENEMYHITBOX; // Non-moving collides with all moving stuff EXCEPT hitbox
			case Layers::MOVING:
				return inObject2 == Layers::NON_MOVING || inObject2 == Layers::MOVING || inObject2 == Layers::ENEMY || inObject2 == Layers::ENEMYPROJECTILE || inObject2 == Layers::CAST_QUERY; // Moving collides with everything but hitboxes and debris
			case Layers::ENEMY:
				return inObject2 == Layers::NON_MOVING || inObject2 == Layers::MOVING || inObject2 == Layers::ENEMY || inObject2 == Layers::PROJECTILE || inObject2 == Layers::CAST_QUERY;
			case Layers::ENEMYHITBOX: //bonkfree is an emergency option that causes enemies to freely collide. It can be used in conjunction with other layers to create variable hitboxing for enemies for pathing, players, environment, and other enemies.
				return  inObject2 == Layers::PROJECTILE || inObject2 == Layers::CAST_QUERY || inObject2 == Layers::HITBOX; 
			case Layers::HITBOX:
				return inObject2 == Layers::PROJECTILE ||  inObject2 == Layers::ENEMYHITBOX || inObject2 == Layers::ENEMYPROJECTILE || inObject2 == Layers::CAST_QUERY; // Hitboxes only collide with projectiles and cast_queries
			case Layers::PROJECTILE:
				return inObject2 == Layers::NON_MOVING || inObject2 == Layers::ENEMYHITBOX || inObject2 == Layers::ENEMY || inObject2 == Layers::HITBOX || inObject2 == Layers::CAST_QUERY;
			case Layers::ENEMYPROJECTILE:
				return (inObject2 != Layers::ENEMYPROJECTILE) && (inObject2 == Layers::NON_MOVING || inObject2 == Layers::MOVING || inObject2 == Layers::HITBOX);
			case Layers::CAST_QUERY:
				return inObject2 == Layers::NON_MOVING || inObject2 == Layers::MOVING || inObject2 == Layers::ENEMY || inObject2 == Layers::ENEMYHITBOX || inObject2 == Layers::HITBOX || inObject2 == Layers::PROJECTILE  || inObject2 == Layers::ENEMYPROJECTILE;
			case Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY:
				return inObject2 == Layers::NON_MOVING;
			case Layers::DEBRIS:
				return inObject2 == Layers::NON_MOVING; // Debris only hits static non-moving stuff (environment)
			default:
				// Jolt is sad you did not define a layer's collision properties :(
				JPH_ASSERT(false);
				return false;
			}
		}
	};
	
	class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			// Create a mapping table from object to broad phase layer
			mObjectToBroadPhase[Layers::NON_MOVING] = JOLT::BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = JOLT::BroadPhaseLayers::MOVING;
			mObjectToBroadPhase[Layers::HITBOX] = JOLT::BroadPhaseLayers::MOVING;
			mObjectToBroadPhase[Layers::PROJECTILE]	= JOLT::BroadPhaseLayers::MOVING;
			mObjectToBroadPhase[Layers::ENEMYPROJECTILE]	= JOLT::BroadPhaseLayers::ENEMYHITBOX;
			mObjectToBroadPhase[Layers::ENEMYHITBOX]	= JOLT::BroadPhaseLayers::ENEMYHITBOX;
			mObjectToBroadPhase[Layers::ENEMY]	= JOLT::BroadPhaseLayers::MOVING;
			mObjectToBroadPhase[Layers::CAST_QUERY]	= JOLT::BroadPhaseLayers::MOVING;
			mObjectToBroadPhase[Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY]	= JOLT::BroadPhaseLayers::MOVING;
			mObjectToBroadPhase[Layers::DEBRIS]	= JOLT::BroadPhaseLayers::DEBRIS;
		}

		virtual unsigned int GetNumBroadPhaseLayers() const override
		{
			return JOLT::BroadPhaseLayers::NUM_LAYERS;
		}

		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
		{
			JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
			return mObjectToBroadPhase[inLayer];
		}
  
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
		{
			switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
			{
			case static_cast<JPH::BroadPhaseLayer::Type>(JOLT::BroadPhaseLayers::NON_MOVING): return "NON_MOVING";
			case static_cast<JPH::BroadPhaseLayer::Type>(JOLT::BroadPhaseLayers::MOVING): return "MOVING";
			case static_cast<JPH::BroadPhaseLayer::Type>(JOLT::BroadPhaseLayers::DEBRIS): return "DEBRIS";
			default: JPH_ASSERT(false);
				return "INVALID";
			}
		}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	private:
		JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	/// Class that determines if an object layer can collide with a broadphase layer
	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
		{
			switch (inLayer1)
			{
			case Layers::NON_MOVING:
				return inLayer2 != JOLT::BroadPhaseLayers::NON_MOVING;
			case Layers::MOVING:
				return inLayer2 != JOLT::BroadPhaseLayers::DEBRIS;
			case Layers::HITBOX:
				return false;
			case Layers::PROJECTILE:
				return inLayer2 != JOLT::BroadPhaseLayers::DEBRIS;
			case Layers::ENEMYPROJECTILE:
				return inLayer2 != JOLT::BroadPhaseLayers::DEBRIS && inLayer2 != JOLT::BroadPhaseLayers::ENEMYHITBOX;
			case Layers::ENEMY:
				return inLayer2 != JOLT::BroadPhaseLayers::DEBRIS;
			case Layers::ENEMYHITBOX: //TODO: do we need to modify this to get true freedom from bonks?
				return inLayer2 != JOLT::BroadPhaseLayers::DEBRIS && inLayer2 != JOLT::BroadPhaseLayers::ENEMYHITBOX;
			case Layers::CAST_QUERY:
				return inLayer2 != JOLT::BroadPhaseLayers::DEBRIS;
			case Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY:
				return inLayer2 != JOLT::BroadPhaseLayers::DEBRIS;
			case Layers::DEBRIS:
				return inLayer2 == JOLT::BroadPhaseLayers::NON_MOVING;
			default:
				JPH_ASSERT(false);
				return false;
			}
		}
	};
	
	// An example activation listener
	class MyBodyActivationListener : public JPH::BodyActivationListener
	{
	public:
		virtual void OnBodyActivated(const JPH::BodyID& inBodyID, uint64 inBodyUserData) override
		{
		}

		virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, uint64 inBodyUserData) override
		{
		}
	};

	using FBInputFeed = FeedMap<FBPhysicsInput>;
	FBOutputFeed WorkerAcc[ALLOWED_THREADS_FOR_BARRAGE_PHYSICS];
	FBInputFeed ThreadAcc[ALLOWED_THREADS_FOR_BARRAGE_PHYSICS];

	TSharedPtr<JPH::JobSystemThreadPool> job_system;
	// Create mapping table from object layer to broadphase layer
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	BPLayerInterfaceImpl broad_phase_layer_interface;

	// Create class that filters object vs broadphase layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
	
	using InitExitFunction = std::function<void(int)>;
	// Create class that filters object vs object layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	ObjectLayerPairFilterImpl object_vs_object_layer_filter;

	// A contact listener gets notified when bodies (are about to) collide, and when they separate again.
	// Note that this is called from a job so whatever you do here needs to be thread safe.
	//normally, the contact listener and the bcontact listener will be the same. the character contact listener is used
	//ONLY for characters, which may not be a shock. these are split as you might want to give it unique behavior.
	TSharedPtr<JPH::ContactListener> contact_listener;
	
	TSharedPtr<BarrageContactListener> character_contact_listener;

	float DeltaTime = 0.01; //You should set this or pass it in.

	//this is actually a member of the physics system
	JPH::BodyInterface* body_interface;

	// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const unsigned int cMaxBodies = 65536;

	// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
	// mutexes are cheap! (they aren't)
	const unsigned int cNumBodyMutexes = 128;//for a LOT of reasons, we actually want the locks on the bodies to be quite granular.

	// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
	// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
	// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const unsigned int cMaxBodyPairs = 65536;

	// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
	// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
	// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
	const unsigned int cMaxContactConstraints = 16384;
	
	//do not move this up. see C++ standard ~ 12.6.2
	TSharedPtr<JPH::PhysicsSystem> physics_system;

	FWorldSimOwner(float cDeltaTime, InitExitFunction JobThreadInitializer);
	void SphereCast(
		double Radius,
		double Distance,
		FVector3d CastFrom,
		FVector3d Direction,
		//JPH::BodyID& CastingBody,
		TSharedPtr<FHitResult> OutHit,
		const JPH::BroadPhaseLayerFilter& BroadPhaseFilter,
		const JPH::ObjectLayerFilter& ObjectFilter,
		const JPH::BodyFilter& BodiesFilter) const;
	void SphereSearch(
		const JPH::BodyID& CastingBody,
		const FVector3d& Location,
		double Radius,
		const JPH::BroadPhaseLayerFilter& BroadPhaseFilter,
		const JPH::ObjectLayerFilter& ObjectFilter,
		const JPH::BodyFilter& BodiesFilter,
		uint32* OutFoundObjectCount,
		TArray<uint32>& OutFoundObjectIDs) const;

	// Cast a ray at something and get the first thing it hits
	void CastRay(FVector3d CastFrom, FVector3d Direction, const JPH::BroadPhaseLayerFilter& BroadPhaseFilter, const JPH::ObjectLayerFilter& ObjectFilter, const JPH::BodyFilter& BodiesFilter, TSharedPtr<FHitResult> OutHit) const;

	JPH::Ref<JPH::Shape> AttemptBoxCache(double JoltX, double JoltY, double JoltZ, float HEReduceMin);
	//we could use type indirection or inheritance, but the fact of the matter is that this is much easier
	//to understand and vastly vastly faster. it's also easier to optimize out allocations, and it's very
	//very easy to read for people who are probably already drowning in new types.
	//finally, it allows FBShapeParams to be a POD and so we can reason about it really easily.
	FBarrageKey CreatePrimitive(FBBoxParams& ToCreate, uint16 Layer, bool IsSensor = false, bool forceDynamic = false, bool isMovable = true);
	FBarrageKey CreatePrimitive(FBCharParams& ToCreate, uint16 Layer);
	FBarrageKey CreatePrimitive(FBSphereParams& ToCreate, uint16 Layer, bool IsSensor = false);
	FBarrageKey CreatePrimitive(FBCapParams& ToCreate, uint16 Layer, bool IsSensor = false, FMassByCategory::BMassCategories MassClass = FMassByCategory::BMassCategories::MostEnemies);

	//Under normal circumstances, you will _not_ want to set the layer and movement to anything else.
	//the use of static mesh colliders is quite slow, and primitive shapes or compound primitives work for most purposes
	//The exception is non-moving static mesh colliders, which are well optimized in Jolt. However, you may wish to have specialized
	//collision available in many cases, at least for narrow phase collision.
	//A good example of this is the bubble shields we use. There are other ways to do them, but nothing that's as elegant from
	//a designer's perspective.
	FBLet LoadComplexStaticMesh(FBTransform& MeshTransform,
		const UStaticMeshComponent* StaticMeshComponent,
		FSkeletonKey Outkey,
		Layers::EJoltPhysicsLayer Layer = Layers::EJoltPhysicsLayer::NON_MOVING,
		JPH::EMotionType Movement = JPH::EMotionType::Static,
		 bool IsSensor = false, bool ForceActualMesh = false);

	//This'll be trouble.
	//https://www.youtube.com/watch?v=KKC3VePrBOY&lc=Ugw9YRxHjcywQKH5LO54AaABAg
	void StepSimulation();

	//Broad Phase is the first pass in the engine's cycle, and the optimization used to accelerate it breaks down as objects are added. As a result, when you have time after adding objects,
	//you should call optimize broad phase. You should also batch object creation whenever possible, but we don't support that well yet.
	//Generally, as we add and remove objects, we'll want to perform this, but we really don't want to run it every tick. We can either use trigger logic or a cadenced ticklite
	bool OptimizeBroadPhase();

	void FinalizeReleasePrimitive(FBarrageKey BarrageKey)
	{
		//TODO return owned Joltstuff to pool or dealloc
		JPH::BodyID result;
		//as we add character handling, it'll be extremely difficult to do it here.
		if (BarrageToJoltMapping->find(BarrageKey, result) && !result.IsInvalid()) 
		{
			body_interface->RemoveBody(result);
			body_interface->DestroyBody(result);
		}
		BarrageToJoltMapping->erase(BarrageKey);
	}
	
	FBarrageKey GenerateBarrageKeyFromBodyId(const JPH::BodyID& Input) const;
	FBarrageKey GenerateBarrageKeyFromBodyId(const uint32 RawIndexAndSequenceNumberInput) const;
	~FWorldSimOwner();
	bool UpdateCharacter(FBPhysicsInput& Update);
	bool UpdateCharacters(TSharedPtr<TArray<FBPhysicsInput>> Array);
private:
	//don't. not unless you understand deeply. that includes me. yes, I know, future jake, you think you're smart.
	//maybe. but this was a... decision.
	void
	AddInternalQueuing(JPH::BodyID ToQueue, uint64 ordinant);
	
};
