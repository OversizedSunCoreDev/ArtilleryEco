#pragma once
#include "FBristleconePacket.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "FControllerState.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

#include "BristleconeCommonTypes.h"


class FBristleconeSender : public FRunnable {
public:
	FBristleconeSender();
	
	virtual ~FBristleconeSender() override;
	void BindSource(TheCone::SendQueue Queue);
	void SetSessionData(uint64 sessionId, uint64 cipherKey);
	void AddTargetAddress(FString target_address_str);
	void SetLocalSockets(
		const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket_high,
		const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket_low,
		const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket_adaptive
	);
	void SetWakeSender(FSharedEventRef NewWakeSender);

	void ActivateDSCP();
	
	virtual bool Init() final;
	virtual uint32 Run() final;
	virtual void Exit() final;
	virtual void Stop() final;

private:
	void Cleanup();

	FBristleconePacketContainer<FControllerState, 3> packet_container;
;
	TSharedPtr<FSocket, ESPMode::ThreadSafe> sender_socket_high;
	TSharedPtr<FSocket, ESPMode::ThreadSafe> sender_socket_low;
	TSharedPtr<FSocket, ESPMode::ThreadSafe> sender_socket_background;
	TSharedPtr<TArray<FIPv4Endpoint>> target_endpoints;

	FSharedEventRef WakeSender;

	TUniquePtr<ISocketSubsystem> socket_subsystem;
	TSharedPtr<TCircularQueue<uint64_t>> Queue;
	uint8 consecutive_zero_bytes_sent;
	bool running;

	// I am pretty sure these should be immutable
	uint64 SessionId;
	uint64 CipherKey;

	struct FLongboyCrypto* Crypto;
};
