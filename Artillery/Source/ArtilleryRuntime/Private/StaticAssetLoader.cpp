#include "StaticAssetLoader.h"

#include "ArtilleryDeveleperSettings.h"
#include "ArtilleryGunBlueprint.h"
#include "ArtilleryRuntimeModule.h"
#include "FArtilleryGun.h"
#include "FGunDefinitionRow.h"

void UStaticGunLoader::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	
	Definitions = GetDefault<UArtilleryDeveloperSettings>()->LoadMainGunsDataTable();
	if (!ensureMsgf(Definitions, TEXT("Artillery UStaticGunLoader::Initialize failed to find a valid gun datatable")))
	{
		return;
	}
	
	Definitions->ForeachRow<FGunDefinitionRow>(
		TEXT("UStaticGunLoader::Initialize"),
		[this](const FName& Key, const FGunDefinitionRow& RowDefinition) mutable
	{
		if (!RowDefinition.LoadableCPP.IsNull())
		{
			UScriptStruct* StructMetadata = RowDefinition.LoadableCPP.LoadSynchronous();
			if (ensure(StructMetadata))
			{
				UE_LOG(LogArtillery, Verbose, TEXT("GunLoader: Loaded UScriptStruct: [%s]"), *StructMetadata->GetName());
				void* container = FMemory::Malloc(StructMetadata->GetStructureSize());
				StructMetadata->InitializeStruct(container); //init & constructor

				UE_LOG(LogArtillery, Verbose, TEXT("GunLoader: Initing USSI..."));
				TSharedPtr<FArtilleryGun> Form = MakeShareable(static_cast<FArtilleryGun*>(container));
				if (Form.IsValid())
				{
					auto Data = static_cast<FArtilleryGun*>(container);
					ZardozMapping.Add(StructMetadata->GetFName(), StructMetadata);
					CommonNameToProperNameMapping.Add(RowDefinition.GunDefinitionId, StructMetadata->GetFName());
				}
			}
		}
			
		if (!RowDefinition.LoadableBP.IsNull())
		{
			UClass* BPGunClass = RowDefinition.LoadableBP.LoadSynchronous();
			if (ensure(BPGunClass))
			{
				UArtilleryGunBlueprint* Singleton = NewObject<UArtilleryGunBlueprint>(this, BPGunClass);
				BPGunNamesToSingletons.Add(FName(RowDefinition.GunDefinitionId), Singleton);
			}
		}
		
			
		if (RowDefinition.LoadableCPP.IsNull() && RowDefinition.LoadableBP.IsNull())
		{
			UE_LOG(LogArtillery, Warning, TEXT("GunLoader: Gun row: [%s] has neither a BP or C++ loadable"), *Key.ToString());
		}
	});
}

void UStaticGunLoader::Deinitialize()
{
	Super::Deinitialize();
	
	// Currently the guns contain a strong pointer to their singleton instance, which means you should be careful with actually making sure guns are... disarmed early
	BPGunNamesToSingletons.Reset();
}

TSharedPtr<FArtilleryGun> UStaticGunLoader::GetNewInstanceUninitialized(const FName& RequestedGunDefinitionID)
{
	FName* TrueName = CommonNameToProperNameMapping.Find(RequestedGunDefinitionID);
	if (TrueName)
	{
		UScriptStruct** GhostlyGun = ZardozMapping.Find(*TrueName);
		if (GhostlyGun)
		{
			UScriptStruct* BindMetadata = *GhostlyGun;
			if (GhostlyGun) //todo: figure out if this is a serious lifecycle risk. As these are available during game instance initialization, I think we're okay? but...
			{
				//UE_LOG(LogTemp, Warning, TEXT("GunLoader: Loaded UScriptStruct: [%s]"), *BindMetadata->GetName());
				void* container = FMemory::Malloc(BindMetadata->GetStructureSize());
				BindMetadata->InitializeStruct(container); //init & constructor

				//UE_LOG(LogTemp, Warning, TEXT("GunLoader: Initing USSI..."));
				return MakeShareable(static_cast<FArtilleryGun*>(container));
			}
		}
	}
	return nullptr;
}

UStaticGunLoader::~UStaticGunLoader()
{
}
