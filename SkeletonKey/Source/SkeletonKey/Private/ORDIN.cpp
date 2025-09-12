#include "ORDIN.h"
#include <stdexcept>

/**
* Please note the following special memory values. All of these are BAD, some are worse.
* 0xABABABAB : Used by Microsoft's HeapAlloc() to mark "no man's land" guard bytes after allocated heap memory
* 0xABADCAFE : A startup to this value to initialize all free memory to catch errant pointers
* 0xBAADF00D : Used by Microsoft's LocalAlloc(LMEM_FIXED) to mark uninitialised allocated heap memory
* 0xBADCAB1E : Error Code returned to the Microsoft eVC debugger when connection is severed to the debugger
* 0xBEEFCACE : Used by Microsoft .NET as a magic number in resource files
* 0xCCCCCCCC : Used by Microsoft's C++ debugging runtime library to mark uninitialised stack memory
* 0xCDCDCDCD : Used by Microsoft's C++ debugging runtime library to mark uninitialised heap memory
* 0xDDDDDDDD : Used by Microsoft's C++ debugging heap to mark freed heap memory
* 0xDEADDEAD : A Microsoft Windows STOP Error code used when the user manually initiates the crash.
* 0xFDFDFDFD : Used by Microsoft's C++ debugging heap to mark "no man's land" guard bytes before and after allocated heap memory
* 0xFEEEFEEE : Used by Microsoft's HeapFree() to mark freed heap memory 
 */
void ULongLivedRecords::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

UOrdinatePillar::UOrdinatePillar()
{
	SelfPtr = this;
	MyWorld = nullptr;
	if (!ORDINATION_Fallback.burnt)
	{
		ORDINATION_Fallback.burnt = true;
		auto& A = Data;
		Data = ORDINATION_Fallback;
		ORDINATION_Fallback = A;
	}

	if (MyWorld != nullptr)
	{
		MyWorld->IsForbidden = true;
		MyWorld->IsReady = false;
	}
	//we clear on fire so this is a precaution.
	for (ORDIN::InitSequence* Group : Data.GROUPS)
	{
		Group->Empty();
	}
	//we also fry the fallback.
	for (ORDIN::InitSequence* Group : ORDINATION_Fallback.GROUPS)
	{
		Group->Empty();
	}
}

UOrdinatePillar::~UOrdinatePillar()
{
	//we share our lifetime with all world subsystems.
	if (MyWorld != nullptr)
	{
		MyWorld->IsForbidden = true;
		MyWorld->IsReady = false;
	}
	ORDINATION_Fallback.burnt = false;
	Data.Subsystems.Empty();
	//and the fallback
	ORDINATION_Fallback.Subsystems.Empty();
}

void UOrdinatePillar::Deinitialize()
{
	if (MyWorld != nullptr)
	{
		MyWorld->IsForbidden = true;
		MyWorld->IsReady = false;
	}
	Super::Deinitialize();
	//we clear on fire so this is a precaution.
	for (ORDIN::InitSequence* Group : Data.GROUPS)
	{
		Group->Empty();
	}
	//we also fry the fallback.
	for (ORDIN::InitSequence* Group : ORDINATION_Fallback.GROUPS)
	{
		Group->Empty();
	}
	MyWorld->IsForbidden = true;
	MyWorld->IsReady = false;
}

//extremely crude bit of trickery to avoid the weird code gen chicanery of UE. normally, i'd diamond these or use concepts
//but I don't really want to pull in that much template metaprogramming here and I don't want to use the UE-side custom stuff.
void UOrdinatePillar::REGISTERLORD(int RegisterAs, ISkeletonLord* YourThisPointer, ICanReady* YourThisPointerAgain)
{
	if (YourThisPointer)
	{
		Data.Subsystems.Add(ORDIN::SubsystemKey(RegisterAs, YourThisPointerAgain));
	}
	else if (!ORDINATION_Fallback.burnt)
	{
		ORDINATION_Fallback.Subsystems.Add(ORDIN::SubsystemKey( RegisterAs, YourThisPointerAgain));
	}
}

void UOrdinatePillar::REGISTERORDER(int RegisterAs, int group, IKeyedConstruct* YourThisPointer)
{
	if (GIsRunning && ORDINATION_Fallback.burnt && this && GetWorld())
	{
		auto WContextType = GetWorld()->WorldType;
		if (WContextType == EWorldType::PIE || WContextType == EWorldType::Game)
		{
			if (group > 10 || group < 0)
			{
				throw std::invalid_argument("Invalid group");
			}
			auto forcealloc = ORDIN::SequencedKey(RegisterAs, YourThisPointer);
			Data.GROUPS[group]->Add(forcealloc);
		}
	}
	else if (!ORDINATION_Fallback.burnt)
	{
		if (group > 10 || group < 0)
		{
			throw std::invalid_argument("Invalid group");
		}
		ORDINATION_Fallback.GROUPS[group]->Add(ORDIN::SequencedKey(RegisterAs, YourThisPointer));
	}
}

void UOrdinatePillar::PostInitialize()
{
	
	if (GetWorld() && GetWorld()->IsGameWorld() && GEngine)
	{
		auto uplink = GEngine->GetEngineSubsystem<ULongLivedRecords>();
		if (uplink) // if uplink is not valid during the postinitialization of world subsystems, we are either in CDO build or a degraded state.
		{
			MyWorld = uplink->WorldRecordStart();
			MyWorld->IsReady = true;
			MyWorld->IsForbidden = false;
			Super::PostInitialize();
			Data.Subsystems.Sort();
			for (ORDIN::SubsystemKey Register : Data.Subsystems)
			{
				if (auto PossibleLord = Cast<ISkeletonLord>(Register.Value ))
				{
					PossibleLord->MyWorldState = MyWorld;
				}
				Register.Value->IsReady = false;
				Register.Value->AttemptRegister();
				MyWorld->IsReady = MyWorld->IsReady && Register.Value->IsReady;
			}
			MyWorld->IsReady = true;
			MyWorld->IsForbidden = false;
			MyWorldState = MyWorld;//setty set.
		}
	}
	else
	{
		//if we are NOT in a game world, we'll set it up so we cannot engage with the Artillery Machinery.
		MyWorld = std::make_shared<WorldRecord>(false, true, true);
		for (ORDIN::SubsystemKey Register : Data.Subsystems)
		{
			Register.Value->IsReady = false;
			if (auto PossibleLord = Cast<ISkeletonLord>(Register.Value ))
			{
				PossibleLord->MyWorldState = MyWorld;
			}
		}
	}
}

void UOrdinatePillar::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UOrdinatePillar::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (ORDIN::InitSequence* Group : Data.GROUPS)
	{
		Group->Sort();
		for (ORDIN::SequencedKey Register : *Group)
		{
			if (Register.Value != nullptr && !Register.Value->IsReady)
			{
				Register.Value->AttemptRegister();
			}
		}
	}
	//we clear once fired!
	for (ORDIN::InitSequence* Group : Data.GROUPS)
	{
		Group->Empty();
	}
	//we also fry the fallback.
	for (ORDIN::InitSequence* Group : ORDINATION_Fallback.GROUPS)
	{
		Group->Empty();
	}
}
