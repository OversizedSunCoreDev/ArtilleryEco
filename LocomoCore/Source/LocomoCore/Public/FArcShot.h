// Copyright Hedra Group.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "Math/InterpCurve.h"
#include <limits>
#include "FArcshot.generated.h"

USTRUCT()
struct LOCOMOCORE_API FSimpleArcShot
{
	GENERATED_BODY()
	
	FInterpCurveVector Arc;
	
	FSimpleArcShot() {}
	
	FSimpleArcShot(FVector Origin, double ApogeeHeight, FVector Target, double TimeToTarget)
	{
		Arc.AddPoint(0.0, Origin);
		
		FVector AlongThisPath = Origin - Target;
		AlongThisPath.Z = 0;
		
		double GoalHeight = FMath::Max(Origin.Z, Target.Z);
		double ArcManipulation = GoalHeight / ApogeeHeight;
		
		// how you split the behaviors here is basically what determines how "smart" the aim is.
		// this boy is pretty dumb. it'll look BASICALLY ok.
		double ApogeeMoment = FMath::Max(0.2, ArcManipulation);
		if(ArcManipulation >= 1)
		{
			ApogeeMoment = 0.5;
			ApogeeHeight = GoalHeight + ApogeeHeight;
		}
		
		FVector Mid = (AlongThisPath * ApogeeMoment) + Origin;
		Mid.Z = ApogeeHeight;
		Arc.AddPoint(TimeToTarget * ApogeeMoment, Mid);
		
		Arc.AddPoint(TimeToTarget, Target);
		Arc.Points[0].InterpMode = CIM_CurveAuto;
		Arc.Points[1].InterpMode = CIM_CurveAuto;
		Arc.Points[2].InterpMode = CIM_CurveAuto;
		Arc.bIsLooped = false;
		Arc.AutoSetTangents();
	}

	FVector Get(double time) const
	{
		return Arc.Eval(time);
	}

	FVector GetCurveTangent(double time) const
	{
		return Arc.EvalDerivative(time);
	}
};
