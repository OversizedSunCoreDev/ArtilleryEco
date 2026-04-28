#pragma once

#include <functional>
#include "Ticklite.h"
#include "ArtilleryDispatch.h"
#include "FArtilleryTicklitesThread.h"
#include "FWorldSimOwner.h"

class FTSphereCast : public UArtilleryDispatch::TL_ThreadedImpl
{
	uint32 TicksRemaining;
	FBarrageKey ShapeCastSourceObject;
	float Radius;
	float Distance;
	FVector RayStart;
	FVector RayDirection;
	TSharedPtr<FHitResult> HitResultPtr;
	std::function<void(FVector, TSharedPtr<FHitResult>)> Callback;

public:
	FTSphereCast() : TicksRemaining(2), ShapeCastSourceObject(0), Radius(0.01), Distance(5000), RayStart(),
	                 RayDirection(),
	                 Callback(nullptr)
	{
		HitResultPtr = MakeShared<FHitResult>();
	}

	FTSphereCast(
		FBarrageKey ShapeCastSource,
		float SphereRadius,
		float CastDistance,
		const FVector& StartLocation,
		const FVector& Direction,
		const std::function<void(FVector, TSharedPtr<FHitResult>)> CallbackFunc
	)
		: TicksRemaining(2),
		  ShapeCastSourceObject(ShapeCastSource),
		  Radius(SphereRadius), Distance(CastDistance),
		  RayStart(StartLocation),
		  RayDirection(Direction),
		  Callback(CallbackFunc)
	{
		HitResultPtr = MakeShared<FHitResult>();
	}

	FTSphereCast(
		FBarrageKey ShapeCastSource,
		float SphereRadius,
		float CastDistance,
		const FVector& StartLocation,
		const FVector& Direction,
		const std::function<void(FVector, TSharedPtr<FHitResult>)> CallbackFunc,
		uint16_t TicksRemaining
	)
		: TicksRemaining(TicksRemaining),
		  ShapeCastSourceObject(ShapeCastSource),
		  Radius(SphereRadius), Distance(CastDistance),
		  RayStart(StartLocation),
		  RayDirection(Direction),
		  Callback(CallbackFunc)
	{
		HitResultPtr = MakeShared<FHitResult>();
	}

	void TICKLITE_StateReset()
	{
	}

	void TICKLITE_Calculate()
	{
		UBarrageDispatch* Physics = this->ADispatch->DispatchOwner->GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics)
		{
			const JPH::DefaultBroadPhaseLayerFilter BroadPhaseFilter = Physics->GetDefaultBroadPhaseLayerFilter(
				Layers::CAST_QUERY);
			const JPH::DefaultObjectLayerFilter ObjectLayerFilter = Physics->GetDefaultLayerFilter(Layers::CAST_QUERY);
			const JPH::IgnoreSingleBodyFilter BodyFilter = Physics->GetFilterToIgnoreSingleBody(ShapeCastSourceObject);

			Physics->SphereCast(Radius, Distance, RayStart, RayDirection, HitResultPtr, BroadPhaseFilter,
			                    ObjectLayerFilter, BodyFilter);

			if (Callback && HitResultPtr->MyItem != JPH::BodyID::cInvalidBodyID)
			{
				Callback(RayStart, HitResultPtr);
			}
		}
	}

	void TICKLITE_Apply()
	{
		--TicksRemaining;
	}

	void TICKLITE_CoreReset()
	{
	}

	bool TICKLITE_CheckForExpiration()
	{
		return TicksRemaining == 0;
	}

	void TICKLITE_OnExpiration()
	{
	}
};

using TL_SphereCast = Ticklites::Ticklite<FTSphereCast>;


class FTSphereSearch : public UArtilleryDispatch::TL_ThreadedImpl
{
	uint32 TicksRemaining;
	FBarrageKey ShapeCastSourceObject;
	float JoltRadius;
	FVector RayStart;
	uint32 FoundDuringCalculate = 0;

	TArray<uint32> BodyIDsFound;
	std::function<void(FVector, FBLet)> Callback;
	Layers::EJoltPhysicsLayer InLayer;

public:
	FTSphereSearch() : TicksRemaining(2), ShapeCastSourceObject(0), JoltRadius(0.1), RayStart(),
	                   Callback(nullptr), InLayer()
	{
		BodyIDsFound.Reserve(MAX_FOUND_OBJECTS);
	}

	FTSphereSearch(
		FBarrageKey ShapeCastSource,
		float SphereRadius,
		const FVector& StartLocation,
		Layers::EJoltPhysicsLayer Layer,
		const std::function<void(FVector, FBLet)> CallbackFunc
	)
		: TicksRemaining(2),
		  ShapeCastSourceObject(ShapeCastSource),
		  JoltRadius(SphereRadius),
		  RayStart(StartLocation),
		  Callback(CallbackFunc),
		  InLayer(Layer)
	{
		BodyIDsFound.Reserve(MAX_FOUND_OBJECTS);
	}

	FTSphereSearch(
		FBarrageKey ShapeCastSource,
		float SphereRadius,
		const FVector& StartLocation,
		Layers::EJoltPhysicsLayer Layer,
		const std::function<void(FVector, FBLet)> CallbackFunc,
		uint16_t TicksRemaining
	)
		: TicksRemaining(TicksRemaining),
		  ShapeCastSourceObject(ShapeCastSource),
		  JoltRadius(SphereRadius),
		  RayStart(StartLocation),
		  Callback(CallbackFunc),
		  InLayer(Layer)
	{
		BodyIDsFound.Reserve(MAX_FOUND_OBJECTS);
	}

	void TICKLITE_StateReset()
	{
	}

	void TICKLITE_Calculate()
	{
		UBarrageDispatch* Physics = this->ADispatch->DispatchOwner->GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics)
		{
			InLayer = Layers::CAST_QUERY;
			const JPH::DefaultBroadPhaseLayerFilter BroadPhaseFilter = Physics->
				GetDefaultBroadPhaseLayerFilter(InLayer);
			const JPH::DefaultObjectLayerFilter ObjectLayerFilter = Physics->GetDefaultLayerFilter(InLayer);
			const JPH::IgnoreSingleBodyFilter BodyFilter = Physics->GetFilterToIgnoreSingleBody(ShapeCastSourceObject);

			Physics->SphereSearch(0, RayStart, JoltRadius, BroadPhaseFilter, ObjectLayerFilter, BodyFilter,
			                      &FoundDuringCalculate, BodyIDsFound);
		}
	}

	void TICKLITE_Apply()
	{
		UBarrageDispatch* Physics = this->ADispatch->DispatchOwner->GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics)
		{
			if (FoundDuringCalculate > 0 && !BodyIDsFound.IsEmpty())
			{
				for (auto id : BodyIDsFound)
				{
					FBarrageKey BodyBarrageKey = Physics->GenerateBarrageKeyFromBodyId(id);
					FBLet BodyObjectFiblet = Physics->GetShapeRef(BodyBarrageKey);
					if (BodyObjectFiblet && FBarragePrimitive::IsNotNull(BodyObjectFiblet) && Callback != nullptr)
					{
						Callback(RayStart, BodyObjectFiblet);
					}
				}
			}
		} //even if physics is gone, you are still permitted your rest, little ghost.
		--TicksRemaining;
	}

	void TICKLITE_CoreReset()
	{
	}

	bool TICKLITE_CheckForExpiration()
	{
		return TicksRemaining == 0;
	}

	void TICKLITE_OnExpiration()
	{
	}
};

using TL_SphereSearch = Ticklites::Ticklite<FTSphereSearch>;
