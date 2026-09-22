#include "ArtilleryRuntime/Public/Debugging/ArtilleryDebugger.h"

#include "ABarragePlayerController.h"
#include "FArtilleryGun.h"
#include "imgui.h"

static TAutoConsoleVariable<bool> CVarEnableImGuiArtilleryOverview(
	TEXT("imgui.Enable.Artillery.Overview"),
	false,
	TEXT(""),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarEnableImGuiArtilleryInput(
	TEXT("imgui.Enable.Artillery.Input"),
	0,
	TEXT("Enable or Disable ImGui Rendering.\n0: Off\n1: On"),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarEnableImGuiArtilleryNetwork(
	TEXT("imgui.Enable.Artillery.Network"),
	0,
	TEXT("Enable or Disable ImGui Rendering.\n0: Off\n1: On"),
	ECVF_Default
);

TArray<FInputStreamDebugData> ArtilleryDebugger::ServerInputStreamDebugData;
TArray<FInputMismatchEvent> ArtilleryDebugger::MismatchHistory;

void ArtilleryDebugger::CaptureInputDebugData()
{
    if (!DispatchOwner->ArtilleryAsyncWorldSim.ContingentInputECSLinkage)
    {
        return;
    }

    InputStreamDebugData.Empty();

    InputProcessingStats.TotalPlayers = 0;
    InputProcessingStats.TotalProcessedInputs = 0;
    InputProcessingStats.LocomotionBufferSize = 0;
    InputProcessingStats.AbilityEventsCount = 0;
    InputProcessingStats.CurrentSequenceNumber = DispatchOwner->ArtilleryAsyncWorldSim.SeqNumber;
    InputProcessingStats.BurstDropDetected = DispatchOwner->burstDropDetected;
    InputProcessingStats.MissedInputCount = 0;

    ABarragePlayerController* LocalPC = Cast<ABarragePlayerController>(DispatchOwner->GetWorld()->GetFirstPlayerController());
    const bool bIsValidPC = (LocalPC != nullptr);
    const bool bIsServer   = (bIsValidPC && LocalPC->HasAuthority());
    const bool bIsLocal    = (bIsValidPC && LocalPC->IsLocalController());

    for (auto& Pair : DispatchOwner->ArtilleryAsyncWorldSim.ContingentInputECSLinkage->SessionPlayerToStreamMapping)
    {
        const PlayerKey pkey = Pair.Key;
        const InputStreamKey skey = Pair.Value;

        TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> stream =
            DispatchOwner->ArtilleryAsyncWorldSim.ContingentInputECSLinkage->GetStream(skey);

        FInputStreamDebugData StreamData;
        StreamData.PlayerID = pkey;
        StreamData.StreamKey = skey;
        StreamData.IsValid  = stream.IsValid();

        if (stream.IsValid())
        {
            StreamData.HighestInput       = stream->highestInput;
            StreamData.PendingInputs      = (StreamData.HighestInput > StreamData.LastProcessedInput)
                                              ? (StreamData.HighestInput - StreamData.LastProcessedInput)
                                              : 0;
            StreamData.ProcessedThisTick  = 0;
            StreamData.MissedInputs       = 0;

            if (stream->highestInput > 0)
            {
                auto LastInput = stream->peek(stream->highestInput - 1);
                if (LastInput.has_value())
                {
                    StreamData.LastRawInput = FString::Printf(TEXT("0x%016llX"), LastInput->MyInputActions);
                }
            }
        }
        else
        {
            StreamData.HighestInput       = 0;
            StreamData.LastProcessedInput = 0;
            StreamData.PendingInputs      = 0;
            StreamData.ProcessedThisTick  = 0;
            StreamData.MissedInputs       = 0;
            StreamData.LastRawInput       = TEXT("N/A");
        }

        InputStreamDebugData.Add(StreamData);
        InputProcessingStats.TotalPlayers++;
    }

    if (bIsServer)
    {
        ServerInputStreamDebugData = InputStreamDebugData;
    }

    if (!bIsServer && bIsLocal && ServerInputStreamDebugData.Num() > 0)
    {
        CompareRawInputs();
    }
}

void ArtilleryDebugger::Initialize(UArtilleryDispatch* InDispatchOwner)
{
	NetworkDebug = MakeShared<ArtilleryNetworkDebug>();
	DispatchOwner = InDispatchOwner;
	check(DispatchOwner.Get());

	UWorld* World = DispatchOwner->GetWorld();
	NetworkDebug->Initialize(
			World->GetSubsystem<UBristleconeWorldSubsystem>(),
			World->GetSubsystem<UCablingWorldSubsystem>(),
			World->GetSubsystem<UCanonicalInputStreamECS>());
}

void ArtilleryDebugger::Draw(float DeltaTime)
{
	
	if (CVarEnableImGuiArtilleryNetwork.GetValueOnGameThread() != 0)
	{
		NetworkDebug->Update(DeltaTime);
		NetworkDebug->Draw();
	}

	DrawImGuiOverview();
	
	DrawImGuiInputDebug();
}

void ArtilleryDebugger::DrawImGuiOverview()
{
	if (!CVarEnableImGuiArtilleryOverview->GetBool())
	{
		return;
	}
	
	const ImGui::FScopedContext ScopedContext;
	if (!ScopedContext)
	{
		return;
	}
	
	ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
	
	const bool bIsOnScreen = ImGui::Begin("Artillery Overview");
	ON_SCOPE_EXIT
	{
		ImGui::End();
	};
	if (!bIsOnScreen)
	{
		return;
	}
	



	UArtilleryDispatch* DispatchOwnerPtr = DispatchOwner.Get();
	if (!DispatchOwnerPtr)
	{
		ImGui::Text("ArtilleryDebugger::DispatchOwner is invalid!");
		return;
	}

	DispatchOwnerPtr->ArtilleryAsyncWorldSim.QueueFunctionFromAnyThreadAndWait([=]
	{
		TSharedPtr<const AttrCuckoo> AttributeMapping = DispatchOwnerPtr->GetAttributeSetToDataMapping();

		TSharedPtr<const TMap<FSkeletonKey, TSharedPtr<FArtilleryGun>>> GunsByKey = DispatchOwnerPtr->GetGunByKeyMap();
		
		TSharedPtr<const TMap<FSkeletonKey, Attr3MapPtr>> VectorMapping = DispatchOwnerPtr->GetVectorSetToDataMapping();

		
		// @todo this might miss things that don't have float atributes?
		int32 IncrementingIndex = 0;
		AttributeMapping->visit_all([&](auto& AttribeMapElem)
		{
			FSkeletonKey Key = AttribeMapElem.first;
			AttrMapPtr Attribute = AttribeMapElem.second;
			IncrementingIndex++;
			ImGui::PushID(IncrementingIndex);
			ON_SCOPE_EXIT
			{
				ImGui::PopID();
			};
			
			ImGui::Separator();
			
			FAnsiString NodeName = {};

			switch (GET_SK_TYPE(Key))
			{
			case SKELLY::SFIX_GunOrAbilityInstance:
			case SKELLY::SFIX_GunOrAbilityPrototypeKey:
			{
				auto FoundGunPtr = GunsByKey->Find(Key);
				if (*FoundGunPtr)
				{
					auto& GunName = (*FoundGunPtr)->MyGunKey.GunDefinitionID;
					NodeName = FAnsiString::Printf("%ls, (%s)", *GunName.ToString(), *Key.PrettyPrintAnsi());

				}
				break;
			}
			case SKELLY::SFIX_ActorOrActorlike:
			{
				auto FoundPtr = DispatchOwnerPtr->TransformDispatch->GetActorKineByObjectKey(Key);
				if (FoundPtr)
				{
					const AActor* ActorPtr = FoundPtr->MySelf.Get();
					if (ActorPtr)
					{
						
						NodeName = FAnsiString::Printf("%ls, (%s)", *ActorPtr->GetActorNameOrLabel(), *Key.PrettyPrintAnsi());
					}
				}
				break;
			}
			default: ;
			}

			
			if (NodeName.IsEmpty())
			{
				NodeName = Key.PrettyPrintAnsi();
			}
			
			if (ImGui::TreeNode(*NodeName))
			{
				ON_SCOPE_EXIT
				{
					ImGui::TreePop();
				};
				
				if (ImGui::BeginTable("Attribute Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthFixed, 250.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableHeadersRow();

					for (auto& [AttributeKey, AttributeData] : *Attribute)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(TCHAR_TO_ANSI(*StaticEnum<E_AttribKey>()->GetNameStringByValue(static_cast<uint64>(AttributeKey))));
						ImGui::TableNextColumn();
						ImGui::Text("%f", AttributeData->GetCurrentValue());
					}

					const TSharedPtr<TMap<E_VectorAttrib, TSharedPtr<FConservedVector>>>* VectorAttributes = VectorMapping->Find(Key);
					
					if (VectorAttributes && *VectorAttributes)
					{
						for (auto& [AttributeKey, AttributeData] : **VectorAttributes)
						{
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(TCHAR_TO_ANSI(*StaticEnum<E_VectorAttrib>()->GetNameStringByValue(static_cast<uint64>(AttributeKey))));
							ImGui::TableNextColumn();
							ImGui::Text("%s", TCHAR_TO_ANSI(*AttributeData->CurrentValue.ToCompactString()));
						}
						
					}
					ImGui::EndTable();
				}
				
			}
		});
	});
	
}

void ArtilleryDebugger::DrawImGuiInputDebug()
{
	if (CVarEnableImGuiArtilleryInput.GetValueOnGameThread() == 0)
	{
		return;
	}

	CaptureInputDebugData();

	const ImGui::FScopedContext ScopedContext;
	if (!ScopedContext)
		return;

	static bool bShowWindow = true;
	ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

	ImGui::Begin("Mismatch List");

	if (MismatchHistory.Num() == 0)
	{
		ImGui::Text("No mismatches recorded.");
	}
	else
	{
		// Only display the last x mismatch events (adjust x as needed)
		static const int32 NumEventsToDisplay = 10;
		int32 StartIndex = FMath::Max(0, MismatchHistory.Num() - NumEventsToDisplay);

		// Iterate in reverse order (latest events first)
		for (int32 EventIndex = MismatchHistory.Num() - 1; EventIndex >= StartIndex; --EventIndex)
		{
			const FInputMismatchEvent& Event = MismatchHistory[EventIndex];
			ImGui::Text("Event #%d at %.2f sec", EventIndex, Event.TimeSeconds);

			// For each player in this event, print each field mismatch
			for (const FPlayerInputCompare& PlayerCompare : Event.MismatchedPlayers)
			{
				for (const FRawInputValue& Field : PlayerCompare.InputValues)
				{
					// Only show entries that don't match
					if (!Field.IsMatch)
					{
						ImGui::Text("Player %d | %s: Client = %s, Server = %s",
									static_cast<int>(PlayerCompare.PlayerID),
									TCHAR_TO_UTF8(*Field.InputName),
									TCHAR_TO_UTF8(*Field.ClientValue),
									TCHAR_TO_UTF8(*Field.ServerValue));
					}
				}
			}
			ImGui::Separator();
		}
	}

	ImGui::End();
}

void ArtilleryDebugger::CompareRawInputs()
{
    RawInputComparisons.Empty();

    ABarragePlayerController* LocalPC = Cast<ABarragePlayerController>(DispatchOwner->GetWorld()->GetFirstPlayerController());
    if (!LocalPC || !LocalPC->IsLocalController() || ServerInputStreamDebugData.Num() == 0)
    {
        return;
    }

    for (const FInputStreamDebugData& LocalEntry : InputStreamDebugData)
    {
        const FInputStreamDebugData* ServerEntryPtr = ServerInputStreamDebugData.FindByPredicate(
            [&LocalEntry](const FInputStreamDebugData& Elem)
            {
                return (Elem.PlayerID == LocalEntry.PlayerID) &&
                       (Elem.StreamKey == LocalEntry.StreamKey);
            }
        );

        if (!ServerEntryPtr || LocalEntry.PlayerID != LocalPC->TrueName)
        {
            continue;
        }

        bool bHighestMatch = (LocalEntry.HighestInput == ServerEntryPtr->HighestInput);
        bool bRawInputMatch = (LocalEntry.LastRawInput == ServerEntryPtr->LastRawInput);
        bool bSequenceMatches = bHighestMatch && bRawInputMatch;

        if (!bSequenceMatches)
        {
            FPlayerInputCompare PlayerCompare;
            PlayerCompare.PlayerID = LocalEntry.PlayerID;

            {
                FRawInputValue Value;
                Value.SequenceNumber = LocalEntry.HighestInput; // for reference
                Value.InputName      = TEXT("HighestInput");
                Value.ClientValue    = FString::FromInt(LocalEntry.HighestInput);
                Value.ServerValue    = FString::FromInt(ServerEntryPtr->HighestInput);
                Value.IsMatch        = bHighestMatch;
                PlayerCompare.InputValues.Add(Value);
            }

            {
                FRawInputValue Value;
                Value.SequenceNumber = 0;
                Value.InputName      = TEXT("LastRawInput");
                Value.ClientValue    = LocalEntry.LastRawInput;
                Value.ServerValue    = ServerEntryPtr->LastRawInput;
                Value.IsMatch        = bRawInputMatch;
                PlayerCompare.InputValues.Add(Value);
            }

            RawInputComparisons.Add(PlayerCompare);
        }
    }

    if (RawInputComparisons.Num() > 0)
    {
        FInputMismatchEvent NewEvent;
        NewEvent.TimeSeconds = DispatchOwner->GetWorld() ? DispatchOwner->GetWorld()->GetTimeSeconds() : 0.f;
        NewEvent.MismatchedPlayers = RawInputComparisons;

        MismatchHistory.Add(NewEvent);

        const int32 MaxHistoryEntries = 50;
        if (MismatchHistory.Num() > MaxHistoryEntries)
        {
            int32 OverBy = MismatchHistory.Num() - MaxHistoryEntries;
            MismatchHistory.RemoveAt(0, OverBy, /*bAllowShrink=*/false);
        }
    }
}
