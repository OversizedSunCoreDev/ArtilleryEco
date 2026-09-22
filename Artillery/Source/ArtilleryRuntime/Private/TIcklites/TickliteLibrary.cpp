// Fill out your copyright notice in the Description page of Project Settings.


#include "TickliteLibrary.h"

#include "ArtilleryGunBlueprint.h"
#include "FTSphereCast.h"

template <typename OuterType, typename... InnerType>
static OuterType* CreateTicklite(UArtilleryDispatch* Dispatch, InnerType&&... Args)
{
	return new OuterType(std::forward<InnerType>(Args)..., Dispatch->GetTickliteWorker());
}

template <typename OuterType, typename... InnerType>
static OuterType* CreateAndRegisterTicklite(UArtilleryDispatch* Dispatch, TicklitePhase Phase, InnerType&&... Args)
{
	OuterType* Ptr = CreateTicklite<OuterType>(Args...);
	
	Dispatch->RequestAddTicklite(Ptr, Phase);
	
	return Ptr;
}


EArtilleryFindResult UTickliteLibrary::K2_SphereCastTickLiteFromGun(const FGunKey& Owner, FSkeletonKey BarrageShapeOwner, float Length, const FVector& Location, const FVector& Direction, const FTickliteHitDelegate& Delegate)
{
	UArtilleryDispatch* Dispatch = FArtilleryDispatchThreadScope::ArtilleryDispatch;
	
	if (!ensure(Dispatch))
	{
		return EArtilleryFindResult::NotFound;
	}
	FBLet OwnerFiblet = Dispatch->BarrageDispatch->GetShapeRef(BarrageShapeOwner);

	if (!ensure(OwnerFiblet))
	{
		return EArtilleryFindResult::NotFound;
	}
	
	StructureFullTL_Dispatch(Dispatch,
	Caster, TL_SphereCast, FTSphereCast, 
	OwnerFiblet->KeyIntoBarrage,
	0.05f,
	Length,
	Location,
	Direction,
	[=](FVector RayStart, TSharedPtr<FHitResult> HitResultFromTicklite)
	{
		// do stuff
		auto GunOwner = Dispatch->GetPointerToGun(Owner);
		
		if (GunOwner && HitResultFromTicklite)
		{
			Delegate.ExecuteIfBound(RayStart, *HitResultFromTicklite);
		}
	});
	
	
	Dispatch->RequestAddTicklite(Caster, Early);

	/// ------------------------------------ vs:
	// CreateAndRegisterTicklite<TL_SphereCast>(
	// 	Dispatch,
	// 	Early,
	// 	FTSphereCast(BarrageKey,
	// 	             0.05f,
	// 	             Length,
	// 	             Location,
	// 	             Direction,
	// 	             [](FVector RayStart, TSharedPtr<FHitResult> HitResultFromTicklite)
	// 	             {
	// 	             }));
	

	
	
	return EArtilleryFindResult::Found;
}
