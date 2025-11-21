// Copyright Epic Games, Inc. All Rights Reserved.

// ReSharper disable CppRedundantParentheses
//they weren't fucking redundant.
// ReSharper disable CppRedundantElseKeywordInsideCompoundStatement
#pragma once

#include "CoreMinimal.h"
#include "AArtilleryController.h"
#include "ArtilleryBPLibs.h"
#include "ArtilleryDispatch.h"
#include "EAttributes.h"
#include "ArtilleryActorControllerConcepts.h"
#include "KeyCarry.h"
#include "PlayerKeyCarry.h"
#include "Curves/CurveVector.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "UObject/ScriptMacros.h"
#include "ABarragePlayerController.generated.h"

class UInputMappingContext;

/**
 * This is a "two-sided" controller that hides most of the machinery of Artillery for player control.
 * Thistle, an entirely separate plugin, performs a similar set of functions for non-player entities.
 */
UCLASS()
class ARTILLERYRUNTIME_API ABarragePlayerController : public AArtilleryController
{
	GENERATED_BODY()
public:
	TObjectPtr<UIArtilleryLocomotionDefault> DefaultInstance;

	//changing this is a powerful and dangerous option for repossessing very very very fast.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Identity)
	E_PlayerKEY TrueName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = State)
	bool IsReady = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Identity)
	UArtilleryDispatch* MyDispatch;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Controls)
	UCurveVector* AimModulator;
	
	IArtilleryLocomotionInterface* BindablePawn;
	//TODO: Refactor these to a more righteous place. AMEN.
	//UNLIKE IN UE DEFAULT, THESE REPRESENT PHYSICALISH PROPERTIES OF THE CAMERA
	//CABLING TAKES CARE OF LOOK SENSITIVITY BEFORE ARTILLERY EVER SEES STICK DATA
	//THIS IS MANDATORY. ALL CHANGES TO STICK DATA MUST BE COMPLETED BEFORE NETWORK TRANSMISSION
	//TO THE LIMIT OF MY KNOWLEDGE:
	//THERE IS NO WAY TO MAKE THIS WORK FUNDAMENTALLY DIFFERENTLY WITHOUT BLEEDING SPEED BY ADDING A R-a-W DATA DEPENDENCY
	//Please, please find one.
	//changing this at run time is an elegant approach to losing determinism very very very fast.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float PitchScale = 1.15f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float PitchNarrowScale = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float PitchWideScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float PitchNarrowThreshold = 0.525f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float PitchWideThreshold = 0.91f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float YawNarrowThreshold = 0.45f;

	//You guessed it...
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float YawWideThreshold = 0.9f;

	//or this!
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float YawScale = 1.15f;

	//or this!
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float YawNarrowScale = 0.80f;
	
	//or this!
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float YawWideScale = 1.8f;
	
	//this too!
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float RollScale = 1.0f;
	
	//and this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputHandling)
	float Tolerance = 1e-3f;

	// prolly gonna wanna wire up our friend the ArtilleryStateMachine with this new cleaner auto-reg design.
	// each artillerytickable is responsible for registering itself as the correct _thing_
	virtual void RegisterWithDispatch(FSkeletonKey AKey) override
	{
		MyDispatch->RegisterControllite(AKey, this); //we don't deregister yet.
		//let's see if this works, and I'll figure out that mess. I'll prolly end up using libcuckoo again.......
	}
	
	// Begin Actor interface

	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		IsReady = true;
	}

public:
	virtual void PostInitializeComponents() override
	{
		Super::PostInitializeComponents();
		AimModulator = ABarragePlayerController::GetAimCurves();
		//TIME TO CALL THE METATRON.
		MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
		if (MyDispatch)
		{
			Arty::Attr3MapPtr VectorAttributes = MakeShareable(new Arty::Attr3Map());
			VectorAttributes->Add(Arty::Attr3::ChaosControlVector, MakeShareable(new FConservedVector()));
			
			VectorAttributes->Add(Arty::Attr3::ArtInputDeltaUnitVector, MakeShareable(new FConservedVector()));
			
			VectorAttributes->Add(Arty::Attr3::UControllerOnlyLookVector, MakeShareable(new FConservedVector()));
			VectorAttributes->Add(Arty::Attr3::TrueLookVector, MakeShareable(new FConservedVector()));
			MyDispatch->RegisterOrAddVecAttribs(GetMyKey(), VectorAttributes);

		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ABarragePlayerController::PostInitializeComponents: This is an error unless the Controller is open in the editor."));
		}
	}

protected:
	virtual void FinishDestroy() override
	{
		if (MyDispatch)
		{
		}
		Super::FinishDestroy();
	}

	//if you want to handle the case where we get a key carry other than a player key carry, override this.
	//may be necessary actually for certain aiming modes or piloting stuff that isn't a full "character"
	virtual void OnPossessNonPlayer(APawn* aPawn) 
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("UIArtilleryLocomotionDefault: OnPossessNonPlayer is not yet implemented!!!!"));
	}

	//if you want to handle the case where we get a key carry other than a player key carry, override this.
	//may be necessary actually for certain aiming modes or piloting stuff that isn't a full "character"
	virtual void OnUnPossessNonPlayer(APawn* aPawn) 
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("UIArtilleryLocomotionDefault: OnPossessNonPlayer is not yet implemented!!!!"));
	}
	
	
	virtual void OnPossess(APawn* aPawn) override
	{
		MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>(); //be real sure.
		Super::OnPossess(aPawn);
		if (aPawn)
		{
			UPlayerKeyCarry* PossiblePlayerKeyCarry = aPawn->GetComponentByClass<UPlayerKeyCarry>();
			UKeyCarry* AnyKeyCarry = aPawn->GetComponentByClass<UKeyCarry>();
			
			if  (PossiblePlayerKeyCarry && AnyKeyCarry && AnyKeyCarry == PossiblePlayerKeyCarry)
			{
				if (aPawn->Implements<UArtilleryLocomotionInterface>())
				{
					IArtilleryLocomotionInterface* PossibleBindablePawn = static_cast<IArtilleryLocomotionInterface*>(aPawn->GetInterfaceAddress(UArtilleryLocomotionInterface::StaticClass()));
					if (PossibleBindablePawn)
					{
						BindablePawn = PossibleBindablePawn;
						auto Key = BindablePawn->GetMyKey();
						RegisterWithDispatch(Key);
						auto InitialRotationIfAny = MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::InitialRotationVec);
						
						auto PhysicsDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>(); //be real sure.
						if (InitialRotationIfAny && PhysicsDispatch)
						{
							auto FBLease = UArtilleryLibrary::GetLocalPlayerBarrageAgent();
							auto RotVec = InitialRotationIfAny->CurrentValue;
							//this allows the specification of an initial rotation for any possessed object. if the value is absent,
							//we just keep going. if the value is present, we check if it's a unit vector. If not, it's consumed or invalid.
							//if it's a unit vector, we rotate to it. easy peasy lemon lemon thing.
							//TODO: VERY IMPORTANT. Fix NLT 6/1/26
							//WARNING WARNING WARNING DETERMINISM HAZARD. THIS CODE MUST EVENTUALLY BE REPLACED OR
							//RUN AT A DETERMINISTIC POINT IN TIME.
							if (!InitialRotationIfAny->CurrentValue.IsZero())
							{
								MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::UControllerOnlyLookVector)->SetCurrentValue(RotVec);
								MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::TrueLookVector)->SetCurrentValue(RotVec);
								InitialRotationIfAny->SetCurrentValue(FVector3d::ZeroVector);//unit vectors are never zero. this indicates consumed.
							}
						}
						IdentPtr TheirControllerAttrib = MyDispatch->GetIdent(Key, Ident::CurrentController);
						if(TheirControllerAttrib && TheirControllerAttrib->CurrentValue != GetMyKey())
						{
							TheirControllerAttrib->SetCurrentValue(GetMyKey());
						}
					}
				}
				else
				{
					UE_LOG( LogTemp, Warning, TEXT("Non-pawns or things that don't implement the interface are not yet supported."));
				}
			}
			else if (AnyKeyCarry)
			{
				OnPossessNonPlayer(aPawn);
			}
		}
		ShouldArtilleryTick = true;
	}

	virtual void OnUnPossess() override
	{
		ShouldArtilleryTick = false;
		
		APawn* CurrentPawn = GetPawn();
		if (MyDispatch && CurrentPawn)
		{
			UPlayerKeyCarry* PossiblePlayerKeyCarry = CurrentPawn->GetComponentByClass<UPlayerKeyCarry>();
			UKeyCarry* AnyKeyCarry = CurrentPawn->GetComponentByClass<UKeyCarry>();
			
			if  (PossiblePlayerKeyCarry && AnyKeyCarry && AnyKeyCarry == PossiblePlayerKeyCarry)
			{
				IdentPtr TheirControllerAttrib = MyDispatch->GetIdent(AnyKeyCarry->GetMyKey(), Ident::CurrentController);
				if(TheirControllerAttrib && TheirControllerAttrib->CurrentValue != GetMyKey())
				{
					TheirControllerAttrib->SetCurrentValue(FSkeletonKey());
				}
			}
			else if (AnyKeyCarry)
			{
				OnUnPossessNonPlayer(CurrentPawn);
			}
			
		}
		Super::OnUnPossess();

		BindablePawn = DefaultInstance.Get(); 
	}

public:
	//Override this to change what modulates the aim in your derived classes then call it in your constructor.
	//or initializer. or remove it and just use a soft pointer.
	virtual TObjectPtr<UCurveVector> GetAimCurves()
	{
		static ConstructorHelpers::FObjectFinder<UCurveVector> BaseAimModulation(TEXT("/Artillery/BaseAimModulation.BaseAimModulation"));
		return BaseAimModulation.Object;
	}

	ABarragePlayerController(): TrueName(), MyDispatch(nullptr), AimModulator(nullptr), BindablePawn(nullptr)
	{
		AimModulator = ABarragePlayerController::GetAimCurves();  //bloody hell.
	}

	virtual void Tick(float DeltaTime) override
	{
		//super tick this.
		Super::Super::Super::Super::Tick(DeltaTime);
		UpdateRotation(DeltaTime);
	}

	//Defactored so that you can modify the rotation application without needing to modify the entire set rotation behavior
	//in case you want to just call Super or in case one of your parents has already included a super call and you'd like to
	//inherit their behavior but modify the rotation application.
	virtual void ApplyRotationToRootComponent()
	{
		if (RootComponent && RootComponent->IsUsingAbsoluteRotation())
		{
			RootComponent->SetWorldRotation(GetControlRotation());
		}
	}

	//this will be necessary. Unfortunately, it's not worth a general solve.
	virtual void ClobberForRollback()
	{
		//this will need to find a way to resolve the possible differences between look and true look
		//as well as cull distance problems that may emerge.
		//critically, we need a way to make sure that the outcome is deterministic on all machines.
		//I don't think that'll be particularly difficult, but we will need test coverage for sanity's sake.
		throw;
	}
	
	//this applies the rotation ONLY to the chaos presentation layer. There's only the tiniest bit of information backflow, and it's only used to
	//fix desynchronizations between this and the expectations.
	//You SHOULD NOT override this. If you wish to change how rotation is processed, you need to override the associated function.
	//But I can't see the future, and I've certainly been mad as hell when I couldn't override something like this.
	//Just know that overriding this risks determinism failures.
	virtual void SetControlRotation(const FRotator& NewRotation) override
	{
		if (!IsValidControlRotation(NewRotation))
		{
			logOrEnsureNanError(TEXT("AController::SetControlRotation attempted to apply NaN-containing or NaN-causing rotation! (%s)"), *NewRotation.ToString());
			return;
		}
		if (!ControlRotation.Equals(NewRotation, Tolerance))
		{
			ControlRotation = NewRotation;
			ApplyRotationToRootComponent();
		}
		else
		{
			//UE_LOG(LogPlayerController, Log, TEXT("Skipping SetControlRotation %s for %s (Pawn %s)"), *NewRotation.ToString(), *GetNameSafe(this), *GetNameSafe(GetPawn()));
		}
	}
	
	virtual void PlayerTick(float DeltaTime) override
	// ReSharper disable once CppRedundantEmptyDeclaration
	//it's worth being able to SEE that we glass this.
	{};
	
	/**
	* Consumes the accumulated rotation records. Unlike the default player controller, we queue-ish our rotations
	* because this is a two-sided controller that hides the threadedness.
	*/
	virtual void UpdateRotation(float DeltaTime) override
	{
		//Like a machine of no knowing, we blindly accept as true the look vector from artillery.
		//originally, we did all sorts of clever stuff. it sucked. don't do it. we can and should do SOME smoothing or interp' or slerp here but atm...
		// let's get it working first. may god have mercy on our souls. happy new year --J
		FRotator ViewRotation = MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::TrueLookVector)->CurrentValue.Rotation();
		SetControlRotation(ViewRotation);
		APawn* const P = GetPawnOrSpectator();
		if (P)
		{
			P->FaceRotation(ViewRotation, DeltaTime);
		}
	}
	
	/* Curve order:
	*	Result.X = VectorCurves[0].Eval(InTime);
	*	Result.Y = VectorCurves[1].Eval(InTime);
	*	Result.Z = VectorCurves[2].Eval(InTime);
	*/
	//Check ArtilleryTick
	virtual float ModulatePitch(float Val)
	{
		if (!IsLookInputIgnored() )
		{
			if (AimModulator)
			{
				auto& AimCurves = AimModulator->FloatCurves;
				return AimCurves[0].Eval(abs( Val)) * Val;
			}
			else
			{
				float absV = abs(Val);
				if (absV < PitchNarrowThreshold)
				{
					Val = Val * PitchNarrowScale;
				}
				else if (absV >= PitchNarrowThreshold && Val <= PitchWideThreshold)
				{
					Val = Val * PitchScale;
				}
				else if (absV > PitchWideThreshold)
				{
					Val = Val * PitchWideScale;
				}
				return Val;
			}
		}
		return  0.0f;
	}

	virtual float ModulateYaw(float Val)
	{
		if (!IsLookInputIgnored() )
		{
			if (AimModulator)
			{
				auto& AimCurves =  AimModulator->FloatCurves;
				return AimCurves[1].Eval(abs( Val)) * Val;
			}
			else
			{
				auto absV = abs(Val);
				if (absV < YawNarrowThreshold)
				{
					Val = Val * YawNarrowThreshold;
				}
				else if (absV >= YawNarrowThreshold && Val <= YawWideThreshold)
				{
					Val = Val * YawScale;
				}
				else if (absV > YawWideThreshold)
				{
					Val = Val * YawWideScale;
				}
			
				return Val;
			}
		}
		return 0.0f;
	}

	//we basically don't support roll right now. you want a game about planes, you make it. quats.
	virtual float ModulateRoll(float Val)
	{
		return  !IsLookInputIgnored() ? Val * RollScale : 0.0f;
	}

	//it should not be necessary to override this.
	//Calls the PlayerCameraManager's ProcessViewRotation function, which should have zero side-effects
	//if it does not, this will explode.
	virtual FRotator ModulateRotation(FRotator NewRotationDelta)
	{
		float TruePitch = ModulatePitch(NewRotationDelta.Pitch);
		float TrueYaw = ModulateYaw(NewRotationDelta.Yaw);
		float TrueRoll = ModulateRoll(NewRotationDelta.Roll);
		return FRotator(TruePitch, TrueYaw, TrueRoll);
	}
	
	//**************************************************
	//THIS COMPLETES OUR ICEPICK LOBOTOMY OF ENHANCED INPUT. AND MAY GOD HAVE MERCY ON OUR SOULS.
	//Some work will be necessary to make this truly threadsafe.
	//**************************************************
	// ReSharper disable once CppEnforceOverridingFunctionStyle
	void AddPitchInput(float Val) override
	{
	}

	// ReSharper disable once CppEnforceOverridingFunctionStyle
	void AddRollInput(float Val) override
	{
	}

	// ReSharper disable once CppEnforceOverridingFunctionStyle
	void AddYawInput(float Val) override
	{
	}

	//You really, really, really, really should not override this. If you do, you really should call super.
	//if you don't, have fun with that.
	virtual void ArtilleryTick(FArtilleryShell PreviousMovement, FArtilleryShell Movement, bool RunAtLeastOnce, bool Smear) override
	{
		IArtilleryLocomotionInterface* BoundPawn = BindablePawn; // save a copy of the pointer.
		// because we use key refs, as long as the object exists long enough to invoke the function, we should be okay
		// this EXACT problem is why delegates work how they do, but we don't need a general case solution, thank goodness
		// we just gotta solve this case for this behavior. [famous last words]
		if (BindablePawn != DefaultInstance.Get() && BindablePawn->IsReady())
		{
			if (ShouldArtilleryTick && IsReady && BoundPawn != DefaultInstance.Get()) //setting and checking a bool is automatic on all relevant systems.
			{
				FRotator RotDeltaInStickMagnitudes = FRotator( Movement.GetStickRightY(), //up down, which controls PITCH 
					Movement.GetStickRightX(), //left right, which controls YAW
					0);
				FVector3d CurrentView = MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::TrueLookVector)->CurrentValue;
				FRotator RotDeltaInDegrees = ModulateRotation(RotDeltaInStickMagnitudes);
				MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::ArtInputDeltaUnitVector)->SetCurrentValue(RotDeltaInDegrees.Vector());
				FRotator CurrentChaosView = GetControlRotation();
				//This should be used for error remediation and control resim only.
				MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::ChaosControlVector)->SetCurrentValue(CurrentChaosView.Vector());
				//not only might we face the problem of a stale pointer to nulled memory, we might also actually be operating on the _wrong_ pawn.
				//Here, though, we are in luck. We can't actually get an NPE here, because BoundPawn is already not the default and bindable is never nulled, only defaulted.
				//we can get no-ops, but no NPEs. a no-op is fine, and better than locking. We'll catch it on the resim <3
				if (ShouldArtilleryTick && BoundPawn == BindablePawn)
				{
					FRotator DeltaForFrictionless = RotDeltaInDegrees;
					BoundPawn->LookStateMachine(RotDeltaInDegrees);
					FRotator CurrentViewRotator = CurrentView.Rotation();
					FRotator CopyForFrictionless = CurrentViewRotator;
					//TODO: remove this once we have a good way to actually mimic this nicely..... It's a huge determinism hazard but
					//The camera manager is the "orthodox" UE construct that understands how far we can allow a camera to move and when\why.
					if (PlayerCameraManager)
					{
						PlayerCameraManager->ProcessViewRotation(1.0/ArtilleryTickHertz, CurrentViewRotator, RotDeltaInDegrees);
						PlayerCameraManager->ProcessViewRotation(1.0/ArtilleryTickHertz, CopyForFrictionless, DeltaForFrictionless);
					}
					MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::UControllerOnlyLookVector)->SetCurrentValue(CopyForFrictionless.Vector());
					MyDispatch->GetVecAttr(GetMyKey(), E_VectorAttrib::TrueLookVector)->SetCurrentValue(CurrentViewRotator.Vector());

					BoundPawn->LocomotionStateMachine(PreviousMovement, Movement, RunAtLeastOnce, Smear); //oh god.
				}
			}
		}
	}
};
