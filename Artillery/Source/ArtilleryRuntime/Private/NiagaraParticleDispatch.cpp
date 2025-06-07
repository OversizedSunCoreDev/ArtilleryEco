﻿// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#include "NiagaraParticleDispatch.h"
#include "ArtilleryDispatch.h"
#include "NiagaraDataChannel.h"

bool UNiagaraParticleDispatch::RegistrationImplementation()
{
	UArtilleryDispatch::SelfPtr->SetParticleDispatch(this);
	SelfPtr = this;
	UE_LOG(LogTemp, Warning, TEXT("NiagaraParticleDispatch:Subsystem: Online"));
	return true;
}

void UNiagaraParticleDispatch::ArtilleryTick()
{
	
}

void UNiagaraParticleDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UNiagaraParticleDispatch::PostInitialize()
{
	Super::PostInitialize();
}

void UNiagaraParticleDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
}

void UNiagaraParticleDispatch::Deinitialize()
{
	Super::Deinitialize();
	NameToNiagaraSystemMapping->Empty();
	ParticleIDToComponentMapping->Empty();
	ComponentToParticleIDMapping->Empty();
	KeyToParticleParamMapping->Empty();
	auto Iter = ProjectileNameToNDCAsset.Get()->CreateIterator();
	for (; Iter; ++Iter)
	{
		ManagementPayload& ParticlePayload = Iter.Value();
		TObjectPtr<UNiagaraDataChannelAsset>& NDCAsset = ParticlePayload.Get<0>();
		NDCAsset->ClearInternalFlags(EInternalObjectFlags::Async);
		TObjectPtr<UNiagaraDataChannelWriter>& NDCWriter = ParticlePayload.Get<1>();
		NDCWriter->ClearInternalFlags(EInternalObjectFlags::Async);
	}
	ProjectileNameToNDCAsset->Empty();
	MyDispatch = nullptr;
}

bool UNiagaraParticleDispatch::IsStillActive(FParticleID PID)
{
	return ParticleIDToComponentMapping->Contains(PID);
}

UNiagaraSystem* UNiagaraParticleDispatch::GetOrLoadNiagaraSystem(FString NiagaraSystemLocation)
{
	if (NameToNiagaraSystemMapping->Contains(NiagaraSystemLocation))
	{
		return *NameToNiagaraSystemMapping->Find(NiagaraSystemLocation);
	}
	UNiagaraSystem* LoadedSystem = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemLocation, nullptr, LOAD_None, nullptr);
	NameToNiagaraSystemMapping->Add(NiagaraSystemLocation, LoadedSystem);
	return LoadedSystem;
}

FParticleID UNiagaraParticleDispatch::SpawnFixedNiagaraSystem(FString NiagaraSystemLocation, FVector Location, FRotator Rotation, FVector Scale, bool bAutoDestroy, ENCPoolMethod PoolingMethod, bool bAutoActivate, bool bPreCullCheck)
{
	FParticleID NewParticleID = ++ParticleIDCounter;

	UNiagaraSystem* NiagaraSystem = GetOrLoadNiagaraSystem(NiagaraSystemLocation);
	UNiagaraComponent* NewNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				NiagaraSystem,
				Location,
				Rotation,
				Scale,
				bAutoDestroy,
				bAutoActivate,
				PoolingMethod,
				bPreCullCheck);

	ParticleIDToComponentMapping->Add(NewParticleID, NewNiagaraComponent);
	ComponentToParticleIDMapping->Add(NewNiagaraComponent, NewParticleID);
	NewNiagaraComponent->OnSystemFinished.AddUniqueDynamic(this, &UNiagaraParticleDispatch::DeregisterNiagaraParticleComponent);

	return NewParticleID;
}
	

FParticleID UNiagaraParticleDispatch::SpawnAttachedNiagaraSystem(FString NiagaraSystemLocation,
																 FSkeletonKey AttachToComponentKey,
                                                                 FName AttachPointName,
                                                                 EAttachLocation::Type LocationType,
                                                                 FVector Location,
                                                                 FRotator Rotation,
                                                                 FVector Scale,
                                                                 bool bAutoDestroy,
                                                                 ENCPoolMethod PoolingMethod,
                                                                 bool bAutoActivate,
                                                                 bool bPreCullCheck)
{
	FParticleID NewParticleID = ++ParticleIDCounter;

	UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
	TSharedPtr<Kine> SceneCompKinePtr;
	//this is the line that was actually causing some of our crashes, as it was a direct access.
	//it should be fine now that we're rolled to lbc.
	TransformDispatch->ObjectToTransformMapping->find(AttachToComponentKey, SceneCompKinePtr);
	if(SceneCompKinePtr)
	{
		TSharedPtr<BoneKine> Bone = StaticCastSharedPtr<BoneKine>(SceneCompKinePtr);
		TWeakObjectPtr<USceneComponent> BoneSceneComp = Bone->MySelf.Get();

		UNiagaraSystem* NiagaraSystem = GetOrLoadNiagaraSystem(NiagaraSystemLocation);
		UNiagaraComponent* NewNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
					NiagaraSystem,
					BoneSceneComp.Get(),
					AttachPointName,
					Location,
					Rotation,
					Scale,
					LocationType,
					bAutoDestroy,
					PoolingMethod,
					bAutoActivate,
					bPreCullCheck);

		if (NewNiagaraComponent != nullptr)
		{
			TSharedPtr<TQueue<NiagaraVariableParam>>* ParamQueuePtr = KeyToParticleParamMapping->Find(FBoneKey(AttachToComponentKey));
			if (ParamQueuePtr != nullptr && ParamQueuePtr->IsValid())
			{
				TQueue<NiagaraVariableParam>* ParamQueue = ParamQueuePtr->Get();
		
				NiagaraVariableParam Param;
				while (ParamQueue->Dequeue(Param))
				{
					switch (Param.Type)
					{
					case Position:
						NewNiagaraComponent->SetVariablePosition(Param.VariableName, Param.VariableValue);
						break;
					default:
						UE_LOG(LogTemp, Error, TEXT("UNiagaraParticleDispatch::SpawnAttachedNiagaraSystemInternal: Parameter type [%d] is not implemented, cannot proceed."), Param.Type);
						throw;
					}
				}
			}

			BoneKeyToParticleIDMapping->Add(FBoneKey(AttachToComponentKey), NewParticleID);
			ParticleIDToComponentMapping->Add(NewParticleID, NewNiagaraComponent);
			ComponentToParticleIDMapping->Add(NewNiagaraComponent, NewParticleID);
			if(GetWorld())
			{
				NewNiagaraComponent->OnSystemFinished.AddUniqueDynamic(this, &UNiagaraParticleDispatch::DeregisterNiagaraParticleComponent);
			}
		}
	}
	return NewParticleID;
}

void UNiagaraParticleDispatch::DeregisterNiagaraParticleComponent(UNiagaraComponent* NiagaraComponent)
{
	if (ComponentToParticleIDMapping->Contains(NiagaraComponent))
	{
		FParticleID PID = ComponentToParticleIDMapping->FindAndRemoveChecked(NiagaraComponent);
		ParticleIDToComponentMapping->Remove(PID);
		// Potential perf concern, may want to set up reverse mapping
		const FBoneKey* ParticleBoneKey = BoneKeyToParticleIDMapping->FindKey(PID);
		if(ParticleBoneKey)
		{
			BoneKeyToParticleIDMapping->Remove(*ParticleBoneKey);
		}
	} 
}

void UNiagaraParticleDispatch::Activate(FParticleID PID)
{
	MyDispatch->RequestRouter->ParticleSystemActivatedOrDeactivated(PID, true, MyDispatch->GetShadowNow());
}

void UNiagaraParticleDispatch::Deactivate(FParticleID PID)
{
	MyDispatch->RequestRouter->ParticleSystemActivatedOrDeactivated(PID, false, MyDispatch->GetShadowNow());
}

void UNiagaraParticleDispatch::Activate(FBoneKey BoneKey)
{
	FParticleID* PID = BoneKeyToParticleIDMapping->Find(BoneKey);
	if (PID != nullptr)
	{
		this->Activate(*PID);
	}
}

void UNiagaraParticleDispatch::Deactivate(FBoneKey BoneKey)
{
	FParticleID* PID = BoneKeyToParticleIDMapping->Find(BoneKey);
	if (PID != nullptr)
	{
		this->Deactivate(*PID);
	}
}

void UNiagaraParticleDispatch::ActivateInternal(FParticleID PID) const
{
	TWeakObjectPtr<UNiagaraComponent>* NiagaraComponentPtr = ParticleIDToComponentMapping->Find(PID);
	if (NiagaraComponentPtr != nullptr)
	{
		NiagaraComponentPtr->Get()->Activate();
	}
}

void UNiagaraParticleDispatch::DeactivateInternal(FParticleID PID) const
{
	TWeakObjectPtr<UNiagaraComponent>* NiagaraComponentPtr = ParticleIDToComponentMapping->Find(PID);
	if (NiagaraComponentPtr != nullptr)
	{
		NiagaraComponentPtr->Get()->Deactivate();
	}
}

void UNiagaraParticleDispatch::QueueParticleSystemParameter(const FBoneKey& Key, const NiagaraVariableParam& Param) const
{
	TSharedPtr<TQueue<NiagaraVariableParam>>* ParameterQueuePtr = KeyToParticleParamMapping->Find(Key);
	if (ParameterQueuePtr != nullptr)
	{
		ParameterQueuePtr->Get()->Enqueue(Param);
	}
	else
	{
		TSharedPtr<TQueue<NiagaraVariableParam>>& NewQueue = KeyToParticleParamMapping->Add(Key, MakeShareable(new TQueue<NiagaraVariableParam>()));
		NewQueue.Get()->Enqueue(Param);
	}
}

void UNiagaraParticleDispatch::AddNDCReference(FName Name, TObjectPtr<UNiagaraDataChannelAsset> DataChannelAssetPtr) const
{
	TObjectPtr<UNiagaraDataChannelWriter> MyWriter = UNiagaraDataChannelLibrary::CreateDataChannelWriter(GetWorld(), DataChannelAssetPtr->Get(), FNiagaraDataChannelSearchParameters(), 1, true, true, true, "Held Writer");
	ProjectileNameToNDCAsset->Add(Name, ManagementPayload(DataChannelAssetPtr, MyWriter, KeyToRecordCuckoo()));
}

void UNiagaraParticleDispatch::UpdateNDCChannels() const
{
	UTransformDispatch* TD = GetWorld()->GetSubsystem<UTransformDispatch>();
	
	for (auto it = ProjectileNameToNDCAsset->CreateIterator(); it; ++it)
	{
		UNiagaraDataChannelWriter* ChannelWriter = it.Value().Get<1>();
		KeyToRecordCuckoo* KeyToRecordMap = &it.Value().Get<2>();
		
		if (KeyToRecordMap->size() > 0)
		{
			FNiagaraDataChannelSearchParameters SearchParams;
			ChannelWriter->InitWrite(SearchParams, KeyToRecordMap->size(), true, true, true, UNiagaraParticleDispatch::StaticClass()->GetName());

			for (auto& KeyToRecord : KeyToRecordMap->lock_table())
			{
				TOptional<FTransform3d> KeyTransform = TD->CopyOfTransformByObjectKey(KeyToRecord.first);
				if (KeyTransform.IsSet())
				{
					ChannelWriter->WritePosition(TEXT("Position"), 0, KeyTransform->GetLocation());
				}
			}
		}
	}
}

void UNiagaraParticleDispatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateNDCChannels();
}
