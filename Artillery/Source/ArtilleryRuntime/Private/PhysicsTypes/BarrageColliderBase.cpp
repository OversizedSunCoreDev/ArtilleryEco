#include "PhysicsTypes/BarrageColliderBase.h"
#if WITH_EDITORONLY_DATA
#include "Debug/BarrageDebugComponent.h"
#endif

//CONSTRUCTORS
//--------------------
//do not invoke the default constructor unless you have a really good plan. in general, let UE initialize your components.

// Sets default values for this component's properties
UBarrageColliderBase::UBarrageColliderBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	MyObjectKey = 0;
	bAlwaysCreatePhysicsState = false;
	UPrimitiveComponent::SetNotifyRigidBodyCollision(false);
	bCanEverAffectNavigation = false;
	Super::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	Super::SetEnableGravity(false);
	Super::SetSimulatePhysics(false);
	bHiddenInGame = true;
}

void UBarrageColliderBase::SetBarrageBody(FBLet NewBody)
{
	MyBarrageBody = NewBody;
#if WITH_EDITORONLY_DATA
	if (BarrageDebugComponent)
	{
		BarrageDebugComponent->SetTargetBody(MyBarrageBody);
	}
#endif
}

//---------------------------------

void UBarrageColliderBase::InitializeComponent()
{
	Super::InitializeComponent();
}

//SETTER: Unused example of how you might set up a registration for an arbitrary key.
void UBarrageColliderBase::BeforeBeginPlay(FSkeletonKey TransformOwner)
{
	MyObjectKey = TransformOwner;
}

//Colliders must override this.
void UBarrageColliderBase::Register()
{
	PrimaryComponentTick.SetTickFunctionEnable(false);
}

#if WITH_EDITORONLY_DATA
void UBarrageColliderBase::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	UBarrageColliderBase* ThisComp = CastChecked<UBarrageColliderBase>(InThis);
	if (ThisComp->BarrageDebugComponent)
	{
		Collector.AddReferencedObject(ThisComp->BarrageDebugComponent);
	}
}

void UBarrageColliderBase::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	if (BarrageDebugComponent)
	{
		BarrageDebugComponent->DestroyComponent();
		BarrageDebugComponent = nullptr;
	}
}

void UBarrageColliderBase::UpdateDebugComponent()
{
	if (BarrageDebugComponent != nullptr)
	{
		// determine things of consequence that would require a redraw
		bool bNeedsUpdate = false;
		if (!BarrageDebugComponent->IsBodySame(MyBarrageBody))
		{
			BarrageDebugComponent->SetTargetBody(MyBarrageBody);
			bNeedsUpdate = true;
		}

		if (bNeedsUpdate)
		{
			BarrageDebugComponent->MarkRenderStateDirty();
		}
	}
}

#endif

void UBarrageColliderBase::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITORONLY_DATA
	AActor* Owner = GetOwner();
	if (Owner != nullptr)
	{
		if (!BarrageDebugComponent)
		{
			BarrageDebugComponent = NewObject<UBarrageDebugComponent>(Owner, NAME_None, RF_Transactional | RF_TextExportTransient);
			BarrageDebugComponent->SetTargetBody(MyBarrageBody);
			BarrageDebugComponent->SetupAttachment(this);
			BarrageDebugComponent->SetIsVisualizationComponent(true);
			BarrageDebugComponent->CreationMethod = EComponentCreationMethod::Instance;
			BarrageDebugComponent->RegisterComponentWithWorld(GetWorld());
		}
		UpdateDebugComponent();
	}
#endif
}

void UBarrageColliderBase::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Register(); // ...
#if WITH_EDITORONLY_DATA
	UpdateDebugComponent();
#endif
}

// Called when the game starts
void UBarrageColliderBase::BeginPlay()
{
	Super::BeginPlay();
	Register();
}

//TOMBSTONERS

void UBarrageColliderBase::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();
	if (GetWorld())
	{
		UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics && MyBarrageBody)
		{
			Physics->SuggestTombstone(MyBarrageBody);
			MyBarrageBody.Reset();
		}
	}
}

void UBarrageColliderBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (GetWorld())
	{
		UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics && MyBarrageBody)
		{
			Physics->SuggestTombstone(MyBarrageBody);
			MyBarrageBody.Reset();
		}
	}
}

void UBarrageColliderBase::SetTransform(const FTransform& NewTransform)
{
	Transform.SetTransform(NewTransform);
}
