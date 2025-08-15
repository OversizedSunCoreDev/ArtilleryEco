#include "FBarragePrimitive.h"
#include "IsolatedJoltIncludes.h"
#include "BarrageDispatch.h"
#include "FBPhysicsInput.h"
#include "CoordinateUtils.h"
#include "FWorldSimOwner.h"
//this is a long way to go to keep the types from leaking but I think it's probably worth it.

//don't add inline. don't do it!
FBarragePrimitive::~FBarragePrimitive()
{
	//THIS WILL NOT HAPPEN UNTIL THE TOMBSTONE HAS EXPIRED AND THE LAST SHARED POINTER TO THE PRIMITIVE IS RELEASED.
	//Only the CleanTombs function in dispatch actually releases the shared pointer on the dispatch side
	//but an actor might hold a shared pointer to the primitive that represents it after that primitive has been
	//popped out of this.
	if (GlobalBarrage != nullptr && Me != FBShape::Character)
	//TODO: This prevented the double free but we may now not free at all?
	{
		GlobalBarrage->FinalizeReleasePrimitive(KeyIntoBarrage);
	}
	else if (Me == FBShape::Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("Character's last fblet is dealloc'd."));
	}
}

//-----------------
//the copy ops on call are intentional. they create a hold-open ref that allows us to be a little safer in many circumstances.
//once this thing is totally cleaned up, we can see about removing them and refactoring this.
//-----------------

void FBarragePrimitive::ApplyRotation(FQuat4d Rotator, FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			GameSimHoldOpen->ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
				FBPhysicsInput(Target->KeyIntoBarrage, 0, PhysicsInputType::Rotation,CoordinateUtils::ToBarrageRotation(Rotator), Target->Me));
		}
	}
}

//generally, this should be called from the same thread as update.
bool FBarragePrimitive::TryUpdateTransformFromJolt(FBLet Target, uint64 Time)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen)
		{
			JPH::BodyID result;
			if (GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result)) // ANY return value should get processed.....
			{
				//atm, characters cannot be inactive.
				//without an inner body to update from, they also require specialized handling.
				TSharedPtr<TransformUpdatesForGameThread> HoldOpen = GlobalBarrage->GameTransformPump;
				if (HoldOpen)
				{
					if (Target->Me == FBShape::Character)
					{
						//accumulate character update from OuterCharacter.
						//SNAAAAAAAAAAAKE
						TSharedPtr<FBCharacterBase>* CharacterRef = GameSimHoldOpen->CharacterToJoltMapping->Find(Target->KeyIntoBarrage);
						if (CharacterRef)
						{
							JPH::Ref<JPH::CharacterVirtual> CharacterPtr = CharacterRef->Get()->mCharacter;
							JPH::RVec3 Pos = CharacterPtr->GetPosition();
							JPH::Quat Rot = CharacterPtr->GetRotation();
							HoldOpen->Enqueue(TransformUpdate(
								Target->KeyOutOfBarrage,
								Time,
								CoordinateUtils::FromJoltRotation(Rot),
								CoordinateUtils::FromJoltCoordinates(Pos),
								0));
						}
					}
					else if (!result.IsInvalid() && GameSimHoldOpen->body_interface->IsActive(result)) //we should still exclude invalid bIDs other than characters.
					{
						//TODO: @Eliza, can we figure out if updating the transforms in place is threadsafe? that'd be vastly preferable
						//TODO: figure out how to make this less.... horrid.
							HoldOpen->Enqueue(TransformUpdate(
								Target->KeyOutOfBarrage,
								Time,
								CoordinateUtils::FromJoltRotation(GameSimHoldOpen->body_interface->GetRotation(result)),
								CoordinateUtils::FromJoltCoordinates(GameSimHoldOpen->body_interface->GetPosition(result)),
								0));
					}
				}
				return true;
			}
		}
	}
	return false;
}

FVector3f FBarragePrimitive::GetCentroidPossiblyStale(FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen)
		{
			JPH::BodyID result;
			if (GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result)
				&& GameSimHoldOpen->body_interface->IsActive(result))
			{
				return CoordinateUtils::FromJoltCoordinates(GameSimHoldOpen->body_interface->GetCenterOfMassPosition(result));
			}
		}
	}
	return FVector3f::ZeroVector;
}

FVector3f FBarragePrimitive::GetPosition(FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			JPH::BodyID result;
			if (GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result))
			{
				if (!result.IsInvalid() && Target->Me != FBShape::Character)
				{
					JPH::RVec3 pos = GameSimHoldOpen->body_interface->GetPosition(result);
					return CoordinateUtils::FromJoltCoordinates(pos);
				}
				if (Target->Me == FBShape::Character)
				{
					TSharedPtr<FBCharacterBase>* CharacterActual = GameSimHoldOpen->CharacterToJoltMapping->Find(Target->KeyIntoBarrage);
					if (CharacterActual && *CharacterActual)
					{
						return CoordinateUtils::FromJoltCoordinates(CharacterActual->Get()->mCharacter->GetPosition());
					}
				}
			}
		}
	}
	return FVector3f(NAN);
}

FVector3f FBarragePrimitive::GetVelocity(FBLet Target)
{
	if (IsNotNull(Target) && GlobalBarrage)
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		JPH::BodyID result;
		if (GameSimHoldOpen && GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result) && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			switch (Target->Me)
			{
			case FBShape::Character:
				{
					TSharedPtr<FBCharacterBase>* CharacterActual = GameSimHoldOpen->CharacterToJoltMapping->Find(Target->KeyIntoBarrage);
					if (CharacterActual && *CharacterActual)
					{
						return CoordinateUtils::FromJoltCoordinates(CharacterActual->Get()->mEffectiveVelocity);
					}
				}
				break;
			default:
				if (!result.IsInvalid())
				{
					return CoordinateUtils::FromJoltCoordinates(GameSimHoldOpen->body_interface->GetLinearVelocity(result));
				}
			}
		}
	}
	return FVector3f();
}

void FBarragePrimitive::SetVelocity(FVector3d Velocity, FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			JPH::Quat lastchance =  CoordinateUtils::ToBarrageVelocity(Velocity);
			lastchance = lastchance.IsNaN() ? JPH::Quat::sZero() : lastchance;
			GameSimHoldOpen->ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
				FBPhysicsInput(Target->KeyIntoBarrage, 0, PhysicsInputType::Velocity, lastchance, Target->Me));
		}
	}
}

void FBarragePrimitive::SetPosition(FVector Position, FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			JPH::Quat lastchance =  CoordinateUtils::ToBarrageVelocity(Position);
			lastchance = lastchance.IsNaN() ? JPH::Quat::sZero() : lastchance;
			GameSimHoldOpen->ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
				FBPhysicsInput(Target->KeyIntoBarrage, 0, PhysicsInputType::SetPosition, lastchance, Target->Me));
		}
	}
}

//Has no effect on characters. Use the agent's set gravity if available.
void FBarragePrimitive::SetGravityFactor(float GravityFactor, FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			GameSimHoldOpen->ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
				FBPhysicsInput(Target->KeyIntoBarrage, 0, PhysicsInputType::SetGravityFactor, JPH::Quat(0, 0, GravityFactor, 0), Target->Me));
		}
	}
}

//NO COORDINATE TRANSFORM IS PERFORMED. DO NOT USE THIS UNLESS YOU KNOW EXACTLY WHAT YOU ARE DOING.
void FBarragePrimitive::Apply_Unsafe(FQuat4d Any, FBLet Target, PhysicsInputType Type)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			GameSimHoldOpen->ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
				FBPhysicsInput(Target->KeyIntoBarrage, 0, Type, JPH::Quat(Any.X, Any.Y, Any.Z, Any.W), Target->Me));
		}
	}
}
//Type is defaulted.
void FBarragePrimitive::ApplyForce(FVector3d Force, FBLet Target, PhysicsInputType Type)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			GameSimHoldOpen->ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
				FBPhysicsInput(Target->KeyIntoBarrage, 0, Type,CoordinateUtils::ToBarrageForce(Force), Target->Me));
		}
	}
}

void FBarragePrimitive::SpeedLimit(FBLet Target, float TargetSpeed)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen)
		{
			JPH::BodyID result;
			// if they exist... we proceed. this replaces the older faulty check.				  curry for safety.
			if (GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result) && Target->Me == FBShape::Character) 
			{
				TSharedPtr<FBCharacterBase>* CharacterActual = GameSimHoldOpen->CharacterToJoltMapping->Find(Target->KeyIntoBarrage);
				if (CharacterActual && *CharacterActual)
				{
					CharacterActual->Get()->mMaxSpeed = TargetSpeed/100;
				}
			}
		}
	}
}

bool FBarragePrimitive::GetSpeedLimitIfAny(FBLet Target, float& OldSpeedLimit)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen)
		{
			JPH::BodyID result;
			// if they exist... we proceed. this replaces the older faulty check.				  curry for safety.
			if (GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result) && Target->Me == FBShape::Character) 
			{
				TSharedPtr<FBCharacterBase>* CharacterActual = GameSimHoldOpen->CharacterToJoltMapping->Find(Target->KeyIntoBarrage);
				if (CharacterActual && *CharacterActual)
				{
					OldSpeedLimit = CharacterActual->Get()->mMaxSpeed;
					return true;
				}
			}
		}
	}
	return false;
}

FBarragePrimitive::FBGroundState FBarragePrimitive::GetCharacterGroundState(FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			JPH::BodyID result;
			if (GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result))
			{
				// exists		is valid					is not a character.
				if (!result.IsInvalid() && Target->Me != FBShape::Character)
				{
					return FBGroundState::NotFound;
				}
				// exists	is a character.
				if (Target->Me == FBShape::Character)
				{
					TSharedPtr<FBCharacterBase>* CharacterActual = GameSimHoldOpen->CharacterToJoltMapping->Find(Target->KeyIntoBarrage);
					if (CharacterActual && *CharacterActual)
					{
						JPH::Ref<JPH::CharacterVirtual> CharVirtual = CharacterActual->Get()->mCharacter;
						return FromJoltGroundState(CharVirtual->GetGroundState());
					}
					return FBGroundState::NotFound;
				}
			}
		}
	}
	return FBGroundState::NotFound;;
}

FVector3f FBarragePrimitive::GetCharacterGroundNormal(FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			JPH::BodyID result;
			//todo: see if we need a better character check? their bid is fake atm.
			if (GameSimHoldOpen->BarrageToJoltMapping->find(Target->KeyIntoBarrage, result) && !result.IsInvalid() && Target->Me != FBShape::Character)
			{
				return FVector3f::ZeroVector;
			}
			if (Target->Me == FBShape::Character)
			{
				TSharedPtr<FBCharacterBase>* CharacterActual = GameSimHoldOpen->CharacterToJoltMapping->Find(Target->KeyIntoBarrage);
				if (CharacterActual && *CharacterActual)
				{
					JPH::Ref<JPH::CharacterVirtual> CharVirtual = CharacterActual->Get()->mCharacter;
					return CoordinateUtils::FromJoltUnitVector(CharVirtual->GetGroundNormal());
				}
				return FVector3f::ZeroVector;
			}
		}
	}
	return FVector3f::ZeroVector;
}

void FBarragePrimitive::SetCharacterGravity(FVector3d InVector, FBLet Target)
{
	if (GlobalBarrage && IsNotNull(Target))
	{
		TSharedPtr<FWorldSimOwner> GameSimHoldOpen = GlobalBarrage->JoltGameSim;
		if (GameSimHoldOpen && MyBARRAGEIndex < ALLOWED_THREADS_FOR_BARRAGE_PHYSICS)
		{
			GameSimHoldOpen->ThreadAcc[MyBARRAGEIndex].Queue->Enqueue(
				FBPhysicsInput(Target->KeyIntoBarrage, 0, PhysicsInputType::SetCharacterGravity,CoordinateUtils::ToBarrageForce(InVector), Target->Me));
		}
	}
}
