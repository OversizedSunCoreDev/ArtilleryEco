#include "PhysicsCharacter.h"

using namespace JOLT;

JPH::BodyID FBCharacter::Create(JPH::CharacterVsCharacterCollision* CVCColliderSystem)
{
	JPH::BodyID ret = BodyID();
	// Create capsule
	if(World) 
	{
		//WorldSimOwner manages the lifecycle of the physics characters. we don't have a proper destructor in here yet, I'm just trying to get this UP for now.
		if(World)
		{
			mGravity = Vec3(0, -9.80, 0);
			mCharacterSettings.mMass = 1000;
			Ref<Shape> capsule = new CapsuleShape(0.5f * mHeightStanding, mRadiusStanding);
			Ref<Shape> capsuleB = new CapsuleShape(0.5f * mHeightStanding, mRadiusStanding);
			mCharacterSettings.mEnhancedInternalEdgeRemoval = true;
			
			mCharacterSettings.mShape = RotatedTranslatedShapeSettings(
				Vec3(0, 0.5f * mHeightStanding + mRadiusStanding, 0), Quat::sIdentity(), capsule).Create().Get();
			// Configure supporting volume
			mCharacterSettings.mSupportingVolume = Plane(Vec3::sAxisY(), -mHeightStanding);
			mForcesUpdate = Vec3::sZero();
			mCharacterSettings.mInnerBodyLayer = Layers::EJoltPhysicsLayer::MOVING;
			// Accept contacts that touch the lower sphere of the capsule
			// If you want to create character WITH innerbodyshape - don't try to reduce, reuse, or recycle here.
			InnerStandingShape =  RotatedTranslatedShapeSettings(
				Vec3(0, 0.5f * mHeightStanding + mRadiusStanding, 0), Quat::sIdentity(), capsuleB).Create().Get();
			mCharacterSettings.mInnerBodyShape = InnerStandingShape;
			mCharacter = new CharacterVirtual(&mCharacterSettings, mInitialPosition, Quat::sIdentity(), 0, World.Get());
			mCharacter->SetCharacterVsCharacterCollision(CVCColliderSystem); // see https://github.com/jrouwe/JoltPhysics/blob/e3ed3b1d33f3a0e7195fbac8b45b30f0a5c8a55b/UnitTests/Physics/CharacterVirtualTests.cpp#L759
			mEffectiveVelocity = Vec3::sZero();
			ret = mCharacter->GetInnerBodyID(); //I am going to regret this somehow. Update: I did.
			mCharacter->SetListener(mListener.Get());
		}
	}
	return ret;
}

void FBCharacter::StepCharacter()
{
	// Determine new basic velocity
	Vec3 MyVelo = mCharacter->GetLinearVelocity();
	Vec3 current_vertical_velocity = Vec3(0, MyVelo.GetY(), 0);
	Vec3 current_planar_velocity = Vec3(MyVelo.GetX(), 0, MyVelo.GetZ());
	Vec3 ground_velocity = mCharacter->GetGroundVelocity();
	Vec3 new_velocity = Vec3::sZero(); // start with nothing.

	//build carry-over.
	//this ensures small forces won't knock us off the ground, vastly reducing jitter.
	if (mCharacter->GetGroundState() == CharacterVirtual::EGroundState::OnGround)
	// And not moving away from ground
	{
		if(ground_velocity.IsNearZero())
		{//during initial settling, we want to maintain velocity.
			new_velocity += current_planar_velocity;
		}
		else
		{// Assume ground velocity when on ground for multiple ticks.
			new_velocity += ground_velocity;
		}
	}
	else
	{
		new_velocity += current_planar_velocity + current_vertical_velocity;
	}

	//Throttle carry-over
	new_velocity *= mThrottleModel.GetX();
	// Gravity
	if(World)
	{
		new_velocity += mGravity * mDeltaTime * mThrottleModel.GetY();
	}
	
	new_velocity += mLocomotionUpdate * mThrottleModel.GetZ();
	mLocomotionUpdate = Vec3::sZero();
	new_velocity += mForcesUpdate * mThrottleModel.GetW();
	mForcesUpdate = Vec3::sZero();
	
	RVec3 start_pos = GetPosition();
	//allow single frame exccession
	//TODO: factor this into a stupid constant.
	float SpeedLimit = min(new_velocity.Length(), (mMaxSpeed * 1.20f));
	Vec3 clamped = (new_velocity.Normalized() * SpeedLimit);
	
	mCharacter->SetLinearVelocity(clamped);
	// Update the character position. splitting this into two half-length updates allows you to get VERY
	// fine grained control by moving them around with respect to the clamp and update.
    {
    	TempAllocatorMalloc allocator;
    	mCharacter->ExtendedUpdate(mDeltaTime,
    							   mGravity,
    							   mUpdateSettings,
    							   World->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
    							   World->GetDefaultLayerFilter(Layers::MOVING),
    							   IgnoreSingleBodyFilter(mCharacter->GetInnerBodyID()),
    							   {},
    							   allocator);
    }

	// Update character velocity for carry over.
	SpeedLimit = min(new_velocity.Length(), (mMaxSpeed));
	clamped = (new_velocity.Normalized() * SpeedLimit);
	
	mCharacter->SetLinearVelocity(clamped);
	mEffectiveVelocity = Vec3(GetPosition() - start_pos) / mDeltaTime;
}

void FBCharacter::IngestUpdate(FBPhysicsInput& input)
{
	switch (input.Action)
	{
	case PhysicsInputType::Rotation:
		mCapsuleRotationUpdate = input.State;
		break;
	case PhysicsInputType::OtherForce:
		// TODO: IDK this is a kludge for now since we removed the 100.0 divide by in CoordinateUtils::ToBarrageForce
		mForcesUpdate += input.State.GetXYZ() / 100.0;
		break;
	case PhysicsInputType::SelfMovement:
		mLocomotionUpdate += input.State.GetXYZ()/ 100.0;
		break;
	case PhysicsInputType::Throttle:
		//Throttle controls the four key forces acting on a character by scaling them.
		mThrottleModel = input.State;
		break;
	case PhysicsInputType::SetCharacterGravity:
		mGravity = input.State.GetXYZ()/ 100.0;
		break;
	case PhysicsInputType::SetPosition:
		SetPosition(input.State.GetXYZ());	
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("FBCharacter::IngestUpdate: Received unimplemented input.Action = [%d]"), input.Action);
	}
}
