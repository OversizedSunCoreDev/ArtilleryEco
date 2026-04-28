#include "LongboyClient.h"
#include "QuicClient.h"

FLongboyClient::FLongboyClient(TUniquePtr<FQuicClient>&& InQuicClient)
	: QuicClient(MoveTemp(InQuicClient))
	, SessionId(TNumericLimits<uint64>::Max())
	, CipherKey(TNumericLimits<uint64>::Max())
{
	// Similar to the Rust client implementation, RAII includes establishing the session.
	
	// peek data for 30 seconds to allow the connection to establish and data to be received
	if (QuicClient.IsValid() && QuicClient->IsFine())
	{
		FTimespan DeltaTime = FTimespan::FromSeconds(0);
		FTimespan MaxTime = FTimespan::FromSeconds(30);
		while (DeltaTime < MaxTime && !IsValidSession())
		{
			// The Longboy protocol is quiet strict, once a client is connected,
			// and has gone through the Quic handshakes, the first thing the client
			// should receive is a session message containing the session id and cipher key.
			// If we don't receive this within 30 seconds, something has gone wrong
			// and we should probably disconnect.
			TArray<uint8> ReceivedData;
			if (QuicClient->Receive(ReceivedData))
			{
				// ABNF of message content:
				// SessionMessage = SessionId CipherKey
				// SessionId = 16 bytes hex string
				// CipherKey = 16 bytes hex string
				if (ReceivedData.Num() == sizeof(uint64) * 2)
				{

					uint8* DataPtr = ReceivedData.GetData();
					uint8* SessionIdPtr = DataPtr;
					uint8* CipherKeyPtr = DataPtr + sizeof(uint64);

					SessionId = *reinterpret_cast<uint64*>(SessionIdPtr);
					CipherKey = *reinterpret_cast<uint64*>(CipherKeyPtr);
				}
			}

			FPlatformProcess::Sleep(0.1f);
			DeltaTime += FTimespan::FromSeconds(0.1f);
		}

		if(IsValidSession())
		{
			// print masked hex of the cipher key.
			FString MaskedCipherKey = FString::Printf(TEXT("0x%016llx"), CipherKey);
			MaskedCipherKey[MaskedCipherKey.Len() - 1] = '*';
			MaskedCipherKey[MaskedCipherKey.Len() - 2] = '*';
			MaskedCipherKey[MaskedCipherKey.Len() - 3] = '*';
			MaskedCipherKey[MaskedCipherKey.Len() - 4] = '*';
			UE_LOG(LongboyQuicClient, Log, TEXT("Successfully established session with SessionId: %llu and CipherKey: %s"), SessionId, *MaskedCipherKey);
		}
		else
		{
			UE_LOG(LongboyQuicClient, Warning, TEXT("Failed to receive valid session message within timeout period. Closing connection."));
			QuicClient->ForceShutdown();
		}
	}
	// Log?
}

FLongboyClient::~FLongboyClient()
{

}