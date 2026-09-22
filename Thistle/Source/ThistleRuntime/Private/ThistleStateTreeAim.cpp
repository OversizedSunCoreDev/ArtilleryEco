#include "ThistleStateTreeAim.h"

#include "Kismet/KismetMathLibrary.h"

EStateTreeRunStatus FAimTurret::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool Shuck = false;
	FVector location = InstanceData.ShuckPoi(Context,Shuck);

	UBarrageDispatch* AreWeBarraging = UBarrageDispatch::Get(Context.GetWorld());
	auto ArtilleryDispatch = UArtilleryDispatch::Get(Context.GetWorld());
	if (AreWeBarraging != nullptr)
	{
		bool found = true;
		FVector HereIAm = UArtilleryLibrary::GetLocation(ArtilleryDispatch, InstanceData.KeyOf, found);
		if (found)
		{
			if (!Shuck)
			{
				return EStateTreeRunStatus::Failed;
			}
			FRotator Rot = UKismetMathLibrary::FindLookAtRotation(HereIAm, location);
			Attr3Ptr MyRot = UArtilleryLibrary::GetAttr3Ptr(ArtilleryDispatch, InstanceData.KeyOf, E_VectorAttrib::AimVector);
			if (MyRot)
			{
				if (MyRot->CurrentValue.Equals(Rot.Vector(), 0.2f))
				{
					return EStateTreeRunStatus::Succeeded;
				}
			}
			else
			{
				// we do need to handle this but I actually don't know what we should do yet.
			}

			FVector3d unit = MyRot->CurrentValue.GetSafeNormal();
			FVector3d unit2 = Rot.Vector();
			static constexpr double SIXTY_DEGREES_AS_RADS = FMath::DegreesToRadians(60.f);
			if (acos(unit.Dot(unit2)) > SIXTY_DEGREES_AS_RADS)
			{
			  Rot =	FVector::SlerpVectorToDirection(unit, unit2, 0.4).Rotation();
			}
			UThistleBehavioralist::AttemptAimFromKey(UThistleBehavioralist::Get(Context.GetWorld()), InstanceData.KeyOf, Rot);
			return EStateTreeRunStatus::Running;
		}
	}
	return EStateTreeRunStatus::Failed;
}
