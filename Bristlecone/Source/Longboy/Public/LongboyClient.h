#pragma once

#include "CoreMinimal.h"

struct FQuicClient;

struct LONGBOY_API FLongboyClient
{
private:
	TUniquePtr<FQuicClient> QuicClient = nullptr;
	uint64_t SessionId;
	uint64_t CipherKey;

public:
	FLongboyClient(TUniquePtr<FQuicClient>&& QuicClient);
	~FLongboyClient();

	FORCEINLINE uint64_t GetSessionId() const { return SessionId; }
	FORCEINLINE uint64_t GetCipherKey() const { return CipherKey; }
	FORCEINLINE bool IsValidSession() const { return SessionId != TNumericLimits<uint64>::Max() && CipherKey != TNumericLimits<uint64>::Max(); }
};
