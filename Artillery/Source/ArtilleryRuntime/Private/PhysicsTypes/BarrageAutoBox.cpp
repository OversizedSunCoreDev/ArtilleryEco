#include "PhysicsTypes/BarrageAutoBox.h"



//CONSTRUCTORS
//--------------------
//do not invoke the default constructor unless you have a really good plan. in general, let UE initialize your components.

// Sets default values for this component's properties
UBarrageAutoBox::UBarrageAutoBox(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), MyMassClass(Weights::NormalEnemy)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	switch (Weight)
	{
	case EBWeightClasses::NormalEnemy: MyMassClass = Weights::NormalEnemy; break;
	case EBWeightClasses::BigEnemy: MyMassClass = Weights::BigEnemy; break;
	case EBWeightClasses::HugeEnemy: MyMassClass = Weights::HugeEnemy; break;
	default: MyMassClass = FMassByCategory(Weights::NormalEnemy); break;
	}

	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	MyParentObjectKey = 0;
	bAlwaysCreatePhysicsState = false;
	UPrimitiveComponent::SetNotifyRigidBodyCollision(false);
	bCanEverAffectNavigation = false;
	Super::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	Super::SetEnableGravity(false);
	Super::SetSimulatePhysics(false);
}

//KEY REGISTER, initializer, and failover.
//----------------------------------

bool UBarrageAutoBox::RegistrationImplementation()
{
	if (GetOwner())
	{
		if (MyParentObjectKey == 0)
		{
			if (GetOwner()->GetComponentByClass<UKeyCarry>())
			{
				MyParentObjectKey = GetOwner()->GetComponentByClass<UKeyCarry>()->GetMyKey();
			}

			if (MyParentObjectKey == 0)
			{
				uint32 val = PointerHash(GetOwner());
				MyParentObjectKey = ActorKey(val);
			}
		}

		if (!IsReady && MyParentObjectKey != 0) // this could easily be just the !=, but it's better to have the whole idiom in the example
		{
			UPrimitiveComponent* AnyMesh = GetOwner()->GetComponentByClass<UMeshComponent>();
			AnyMesh = AnyMesh ? AnyMesh : GetOwner()->GetComponentByClass<UPrimitiveComponent>();
			if (AnyMesh)
			{
				FVector extents = DiameterXYZ.IsNearlyZero() || DiameterXYZ.Length() <= 0.1 ? FVector::ZeroVector : DiameterXYZ;
				if (extents.IsZero())
				{
					FBoxSphereBounds Boxen = AnyMesh->GetLocalBounds();
					if (Boxen.BoxExtent.GetMin() >= 0.01)
					{
						// Multiply by the scale factor, then multiply by 2 since mesh bounds is radius not diameter
						extents = Boxen.BoxExtent * AnyMesh->GetComponentScale() * 2;
					}
					else
					{
						//I SAID BEHAAAAAAAAAAAVE.
						extents = FVector(1, 1, 1);
					}
				}
				auto offset = FVector3d(OffsetCenterToMatchBoundedShapeX, OffsetCenterToMatchBoundedShapeY, OffsetCenterToMatchBoundedShapeZ);
				UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
				FBBoxParams params = FBarrageBounder::GenerateBoxBounds(
					GetOwner()->GetActorLocation(),
					FMath::Max(extents.X, .1),
					FMath::Max(extents.Y, 0.1),
					FMath::Max(extents.Z, 0.1),
					offset,
					MyMassClass.Category);

				MyBarrageBody = Physics->CreatePrimitive(params, MyParentObjectKey, static_cast<uint16>(Layer), false, false, isMovable);
				if (MyBarrageBody)
				{
					AnyMesh->WakeRigidBody();
					IsReady = true;
					AnyMesh->SetSimulatePhysics(false);
					for (auto Child : this->GetAttachChildren())
					{
						Child->SetRelativeLocation_Direct(Child->GetRelativeLocation() - offset);
					}
				}
			}
		}
	}

	if (IsReady)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return true;
	}
	return false;
}


FBoxSphereBounds UBarrageAutoBox::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FBox(-DiameterXYZ, DiameterXYZ)).TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UBarrageAutoBox::CreateSceneProxy()
{
	// This render proxy is draws only how we wish to view the box in-editor, it
	// will not represent the actual physics shape if that is different within the
	// physics engine.
	class FBarrageBoxSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		FBarrageBoxSceneProxy(const UBarrageAutoBox* InComponent, FVector3f&& BarragePosition)
			: FPrimitiveSceneProxy(InComponent)
			, bDrawOnlyIfSelected(false)
			, BarragePosition(MoveTemp(BarragePosition))
			, BoxExtents(InComponent->DiameterXYZ)
			, bHasBarrageBody(InComponent->GetBarrageBody().IsValid())
		{
			bWillEverBeLit = false;

#if WITH_EDITOR
			struct FIterSink
			{
				FIterSink(const FEngineShowFlags InSelectedShowFlags)
					: SelectedShowFlags(InSelectedShowFlags)
				{
					SelectedShowFlagIndices.SetNum(FEngineShowFlags::SF_FirstCustom, false);
				}

				bool HandleShowFlag(uint32 InIndex, const FString& InName)
				{
					if (SelectedShowFlags.GetSingleFlag(InIndex) == true)
					{
						SelectedShowFlagIndices.PadToNum(InIndex + 1, false);
						SelectedShowFlagIndices[InIndex] = true;
					}

					return true;
				}

				bool OnEngineShowFlag(uint32 InIndex, const FString& InName)
				{
					return HandleShowFlag(InIndex, InName);
				}

				bool OnCustomShowFlag(uint32 InIndex, const FString& InName)
				{
					return HandleShowFlag(InIndex, InName);
				}

				const FEngineShowFlags SelectedShowFlags;

				TBitArray<> SelectedShowFlagIndices;
			};

			FIterSink Sink(ESFIM_All0);
			FEngineShowFlags::IterateAllFlags(Sink);
			SelectedShowFlagIndices = MoveTemp(Sink.SelectedShowFlagIndices);
#endif // WITH_EDITOR
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			static constexpr FColor UEBoxColor(128, 255, 128);
			static constexpr FColor BarrageColor(19, 240, 255);
			static constexpr float UELineThickness = 1.0f;
			static constexpr float BarrageLineThickness = .8f;

			QUICK_SCOPE_CYCLE_COUNTER(STAT_BoxSceneProxy_GetDynamicMeshElements);

			const FMatrix& LocalToWorld = GetLocalToWorld();

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];

					const FLinearColor DrawColor = GetViewSelectionColor(UEBoxColor, *View, IsSelected(), IsHovered(), false, IsIndividuallySelected());

					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
					DrawOrientedWireBox(PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetScaledAxis(EAxis::X), LocalToWorld.GetScaledAxis(EAxis::Y), LocalToWorld.GetScaledAxis(EAxis::Z), BoxExtents * 0.5f, DrawColor, SDPG_World, UELineThickness);

					if (bHasBarrageBody)
					{
						// Draw a ghost boxy thing at the position of the barrage body, it is assumed to be in world space already
						DrawOrientedWireBox(PDI, FBarragePrimitive::UpConvertFloatVector(BarragePosition), LocalToWorld.GetScaledAxis(EAxis::X), LocalToWorld.GetScaledAxis(EAxis::Y), LocalToWorld.GetScaledAxis(EAxis::Z), BoxExtents * 0.5f, BarrageColor, SDPG_World, BarrageLineThickness);
					}
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			const bool bProxyVisible = !bDrawOnlyIfSelected || IsSelected();

			// Should we draw this because collision drawing is enabled, and we have collision
			const bool bShowForCollision = View->Family->EngineShowFlags.Collision && IsCollisionEnabled();

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = (IsShown(View) && bProxyVisible) || bShowForCollision;
#if WITH_EDITOR
			bool bAreAllSelectedFlagsEnabled = true;
			for (TConstSetBitIterator<> It(SelectedShowFlagIndices); It; ++It)
			{
				bAreAllSelectedFlagsEnabled &= View->Family->EngineShowFlags.GetSingleFlag(It.GetIndex());
			}

			Result.bDrawRelevance &= bAreAllSelectedFlagsEnabled;
#endif // WITH_EDITOR
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = IsShadowCast(View);
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}
		virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
		uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	private:
		const uint32	bDrawOnlyIfSelected : 1;
		const uint32	bHasBarrageBody : 1;
		const FVector	BoxExtents;
		const FVector3f BarragePosition;
#if WITH_EDITOR
		TBitArray<>		SelectedShowFlagIndices;
#endif // WITH_EDITOR
	};

	return new FBarrageBoxSceneProxy(this, FBarragePrimitive::GetPosition(GetBarrageBody()));
}