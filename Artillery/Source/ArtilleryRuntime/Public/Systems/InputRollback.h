#pragma once
#include "ArtilleryCommonTypes.h"
#include "ArtilleryShell.h"

class UBristleconeWorldSubsystem;
class UBarrageDispatch;
class UCanonicalInputStreamECS;

//todo: assess if we still need this at this level of complexity.
class FInputRollback
{
public:
	FInputRollback();
	void Initialize(UWorld* InWorld);
	void ProcessPredictiveInputs(uint32 Sequence, ArtilleryTime TickliteNow);
	TMap<PlayerKey, FArtilleryShell> GetInputsForSequence(uint32 Sequence, ArtilleryTime TickliteNow);
	void SendInputsToServer(uint32 StartSequence, uint32 EndSequence, const TMap<PlayerKey, FArtilleryShell>& Inputs);
	void ParseAuthorityData();
	void ProcessBufferedInputs(uint32 Sequence);
	void ClearBuffers();
	void Shutdown();
	// TODO, Get rid of this, have a struct containing inputs, players and wether or not is provided by authority
	const TMap<uint32, TMap<PlayerKey, FArtilleryShell>>& GetAuthoritativeInputs() const { return AuthoritativeInputs; }
	const TMap<uint32, TMap<PlayerKey, FArtilleryShell>>& GetLocalInputHistory() const { return InputHistory; }
	bool IsNetInitialized() const { return bReceivedInitialNetUpdate; };
	uint32 GetLatestServerFrame() const { return ServerFrame; };
	void ClearBufferedInputs();
private:
	TMap<PlayerKey, FArtilleryShell> GetBufferedInputs(uint32 FrameNumber);
	TMap<uint32, TMap<PlayerKey, FArtilleryShell>> InputHistory;
	TMap<uint32, TMap<PlayerKey, FArtilleryShell>> AuthoritativeInputs;
	TMap<PlayerKey, uint64> LastProcessedInputIndex;
	TWeakObjectPtr<UCanonicalInputStreamECS> CanonicalInput;
	TWeakObjectPtr<UBristleconeWorldSubsystem> NetworkDispatch;
	TWeakObjectPtr<UWorld> GameWorld;
	TSharedPtr<TCircularQueue<TheCone::PacketElement>> InputSwapSlot;
	uint32 ServerFrame = 0;
	bool bIsServer = false;
	bool bReceivedInitialNetUpdate = false;
};
