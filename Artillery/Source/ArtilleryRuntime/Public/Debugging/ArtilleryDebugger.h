#pragma once
#include "ArtilleryCommonTypes.h"


class ArtilleryNetworkDebug;
class UArtilleryDispatch;

struct FPacketHistoryEntry {
	uint64 SequenceNumber;
	uint64 Timestamp;
	bool Received;
};

// Static packet history for both client and server
static TMap<PlayerKey, TArray<FPacketHistoryEntry>> ClientPacketHistory;
static TMap<PlayerKey, TArray<FPacketHistoryEntry>> ServerPacketHistory;

struct FInputStreamDebugData
{
	PlayerKey PlayerID;
	InputStreamKey StreamKey;
	uint64 HighestInput;
	uint64 LastProcessedInput;
	uint32 MissedInputs;
	uint32 ProcessedThisTick;
	uint32 PendingInputs;
	bool IsValid;
	FString LastRawInput;
};

struct FRawInputValue
{
	uint64 SequenceNumber;
	FString InputName;
	FString ClientValue;
	FString ServerValue;
	bool IsMatch;
};

struct FPlayerInputCompare
{
	PlayerKey PlayerID;
	TArray<FRawInputValue> InputValues;
};

struct FInputMismatchEvent
{
	float TimeSeconds;
	TArray<FPlayerInputCompare> MismatchedPlayers;
};

struct FInputProcessingStats
{
	int32 TotalPlayers;
	int32 TotalProcessedInputs;
	int32 LocomotionBufferSize;
	int32 AbilityEventsCount;
	bool TripleBufferIsDirty;
	uint64 CurrentSequenceNumber;
	uint64 MissedInputCount;
	bool BurstDropDetected;
};


class ArtilleryDebugger
{
public:
	void Initialize(UArtilleryDispatch* InDispatchOwner);
	void Draw(float DeltaTime);
private:
	void DrawImGuiOverview();
	void DrawImGuiInputDebug();
	void CaptureInputDebugData();
	void CompareRawInputs();
	static TArray<FInputStreamDebugData> ServerInputStreamDebugData;
	static TArray<FInputMismatchEvent> MismatchHistory;
	TArray<FInputStreamDebugData> InputStreamDebugData;
	TArray<FPlayerInputCompare> RawInputComparisons;
	FInputProcessingStats InputProcessingStats;
	TSharedPtr<ArtilleryNetworkDebug> NetworkDebug;

private:
	TWeakObjectPtr<UArtilleryDispatch> DispatchOwner;
};
