// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "FBarragePrimitive.h"
#include "Components/ActorComponent.h"
#include "BarrageColliderBase.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UBarrageColliderBase : public UPrimitiveComponent, public ICanReady
{
	GENERATED_BODY()

private:
#if WITH_EDITORONLY_DATA
	TObjectPtr<class UBarrageDebugComponent> BarrageDebugComponent;

	void UpdateDebugComponent();
#endif

	FBLet MyBarrageBody = nullptr;
public:
	FSkeletonKey MyParentObjectKey;
	FSkeletonKey MyObjectKey;
	bool IsReady = false;
	
	// Sets default values for this component's properties
	UBarrageColliderBase(const FObjectInitializer& ObjectInitializer);
	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeforeBeginPlay(FSkeletonKey TransformOwner);
	//Colliders must override this.
	virtual bool RegistrationImplementation() override;
	virtual void Register();
	virtual void OnRegister() override;

	virtual void OnDestroyPhysicsState() override;
	
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SetTransform(const FTransform& NewTransform);

	void SetBarrageBody(FBLet NewBody);
	FBLet GetBarrageBody() const { return MyBarrageBody; }

#if WITH_EDITORONLY_DATA
	static ARTILLERYRUNTIME_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#endif

protected:
	FBTransform Transform;
};
