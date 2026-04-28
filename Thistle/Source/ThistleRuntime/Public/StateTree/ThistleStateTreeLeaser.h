#pragma once
#include "StateTreeExecutionContext.h"
#include "ThistleStateTreeSchema.h"
#include "ThistleTypes.h"
#include "Components/StateTreeComponent.h"
#include "Guns/SampleKick.h"

#include "ThistleStateTreeLeaser.generated.h"
////////////////////////////////////////////////////////////////////////
// Thistle State Tree Lease is a Cadenced StateTree holder. It provides registration and execution context support
// but ultimately it is a is a two-sided actor component that connects to the Thistle Behavioralist ECS Pillar.
// This allows Thistle Director StateTrees to access both the UE presentation state and the Artillery deterministic tick.
// Like other two-sided components, it is a TickHeavy, which implements the Artillery Tick method.

//DETERMINISM RISK: this could persist between matches if we get real unlucky.
thread_local static uint8 UThistleStateTreeLeaseGroupMeter = 0;
UCLASS(Blueprintable, ClassGroup = AI, HideCategories = (Activation, Collision), meta = (BlueprintSpawnableComponent))
class THISTLERUNTIME_API UThistleStateTreeLease : public UStateTreeComponent, public ITickHeavy
{
	GENERATED_BODY()
	
public:
	virtual void BeginDestroy() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** @return a value if the state tree reference can be used by the component or the error why it's not a valid reference. */
	virtual TValueOrError<void, FString> HasValidStateTreeReference() const override;
	virtual FStateTreeReference GetGuard();
protected:
	virtual void OnUnregister() override;

public:
	EStateTreeRunStatus CurrentRunStatus;
	UPROPERTY(EditAnywhere, Category = Parameter)
	F_ArtilleryKeyInstanceData InstanceOwnerKey;

	UPROPERTY(EditAnywhere, Category = Parameter)
	uint8 CycleGroup;
	
	UPROPERTY(EditAnywhere)
	bool UseGroupOffset = false;
	
#if WITH_GAMEPLAY_DEBUGGER
	virtual FString GetDebugInfoString() const override;
#endif // WITH_GAMEPLAY_DEBUGGER
	
	virtual void InitializeComponent() override;
	FOnCollectStateTreeExternalData BindDel;
	virtual bool CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> Descs, TArrayView<FStateTreeDataView> OutDataViews) const override;
	virtual void OnRegister() override;
	virtual bool RegistrationImplementation() override;
	virtual void OnClusterMarkedAsPendingKill() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	UThistleStateTreeLease(const FObjectInitializer& ObjectInitializer);

protected:
	virtual ~UThistleStateTreeLease() override;

public:
	virtual void BeginPlay() override;

	virtual FSkeletonKey GetMyKey() const override;

	// we may finally need to shim our executor via IGameplayTaskOwnerInterface 
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override;

protected:
	virtual bool SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors = false) override;
	void GenerateGroupOffset();

public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void ArtilleryTick(uint64_t TicksSoFar) override;

	virtual TSubclassOf<UStateTreeSchema> GetSchema() const override
	{
		return UThistleStateTreeSchema::StaticClass();
	}
};
