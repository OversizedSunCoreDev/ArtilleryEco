#pragma once
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "FControllerState.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "BristleconeCommonTypes.h"

class FBristleconeReceiver : public FRunnable {
public:
	FBristleconeReceiver();

	void BindSink(TheCone::RecvQueue QueueCandidate);
	void BindStatsSink(TheCone::TimestampQueue QueueCandidate);
	void SetSessionData(uint64 InSessionId, uint64 InCipherKey);
	virtual ~FBristleconeReceiver() override;

	void SetLocalSocket(const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket);
	
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;

	//public so it can be changed more easily in the future during runtime
	//FObjects don't really have props, so not dealing with this atm.
	bool LogOnReceive;

private:
	void Cleanup();
	int64 SeenCycles;
	int64 HighestSeen;
	TSharedPtr<FSocket, ESPMode::ThreadSafe> receiver_socket;
	TArray<uint8> received_data;
	TheCone::RecvQueue Queue;
	TheCone::TimestampQueue PacketStats;
	TheCone::CycleTracking MySeen;
	TUniquePtr<ISocketSubsystem> socket_subsystem;
	bool running;
	uint64 SessionId;
	uint64 CipherKey;
	struct FLongboyCrypto* Crypto;
};
