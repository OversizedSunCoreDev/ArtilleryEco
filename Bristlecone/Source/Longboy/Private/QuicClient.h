#pragma once

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#endif

// Inspired by
// Engine\Plugins\Media\PixelStreaming\Source\PixelStreaming\Private\WebRTCIncludes.h

// C5105: One of the files included by "msquic.h" has a macro expansion
//		  producing defined, which has undefined behavior
#pragma warning(push)
#pragma warning(disable: 5105)

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif


// This define is needed to provide optional local non-encrypted traffic
#define QUIC_API_ENABLE_INSECURE_FEATURES

#include <msquic.h>
#include <stdio.h>
#include <stdlib.h>


#ifdef __clang__
#pragma clang diagnostic pop
#endif

#pragma warning(pop)


#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

#define UI UI_ST
#include <openssl/pem.h>
#include <openssl/x509.h>
#undef UI


#if PLATFORM_WINDOWS
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "Interfaces/IPv4/IPv4Endpoint.h"


DECLARE_LOG_CATEGORY_EXTERN(LongboyQuicClient, Log, All);

// Queued messages from the QUIC stream for processing in the main thread
struct FStreamData
{
	uint64 StreamId;
	TArray<uint8> Data;

	// Implicit contract of the Queue requires an empty constructor
	FStreamData()
		: StreamId(0)
		, Data()
	{
	}

	FStreamData(uint64 InStreamId, const uint8* InData, int32 InDataLength)
		: StreamId(InStreamId)
		, Data(InData, InDataLength)
	{
	}

	FStreamData(uint64 InStreamId, TArray<uint8>&& InData)
		: StreamId(InStreamId)
		, Data(MoveTemp(InData))
	{
	}

	FStreamData(const FStreamData& Other)
		: StreamId(Other.StreamId)
		, Data(Other.Data)
	{
	}

	FStreamData& operator=(const FStreamData& Other)
	{
		if (this != &Other)
		{
			StreamId = Other.StreamId;
			Data = Other.Data;
		}
		return *this;
	}

	FStreamData(FStreamData&& Other) noexcept
		: StreamId(Other.StreamId)
		, Data(MoveTemp(Other.Data))
	{
	}

	FStreamData& operator=(FStreamData&& Other) noexcept
	{
		if (this != &Other)
		{
			StreamId = Other.StreamId;
			Data = MoveTemp(Other.Data);
		}
		return *this;
	}

	// Construct with empty data for a given stream id with reserved space for a given amount of data. This is useful for preallocating buffers for incoming data.
	FStreamData(uint64 InStreamId, int32 ReservedSize)
		: StreamId(InStreamId)
		, Data()
	{
		Data.Reserve(ReservedSize);
	}

	FStreamData(uint64 InStreamId)
		: StreamId(InStreamId)
		, Data()
	{
	}

	bool operator==(const FStreamData& Other) const
	{
		return StreamId == Other.StreamId;
	}

	bool operator!=(const FStreamData& Other) const
	{
		return StreamId != Other.StreamId;
	}
};

namespace Quicky
{
	// Thanks https://github.com/EpicGames/UnrealEngine/blob/dbd4567bb1443f0155e8df4c6118d02f361f73f3/Engine/Plugins/Experimental/QuicMessaging/Source/QuicMessagingTransport/Private/QuicUtils.h

	typedef long HRESULT; //Windows-specific, but we want to avoid including Windows headers in the public interface

	/**
	 * Convert HRESULT to FString.
	 *
	 * @note Most HRESULT values are overriden/extended by msquic
	 * https://github.com/microsoft/msquic/blob/main/docs/TSG.md#understanding-error-codes
	 * https://github.com/microsoft/msquic/blob/main/src/inc/msquic_winuser.h
	 */
	inline FString ConvertResult(HRESULT Result) { return FString::Printf(TEXT("0x%lx"), Result); }


	inline FString GetEndpointErrorString(HRESULT Error)
	{
#ifndef CASE2TEXT
#define CASE2TEXT(x) case x: return TEXT(#x)
#endif
		switch (Error)
		{
			CASE2TEXT(QUIC_STATUS_SUCCESS);
			CASE2TEXT(QUIC_STATUS_OUT_OF_MEMORY);
			CASE2TEXT(QUIC_STATUS_INVALID_PARAMETER);
			CASE2TEXT(QUIC_STATUS_INVALID_STATE);
			CASE2TEXT(QUIC_STATUS_NOT_SUPPORTED);
			CASE2TEXT(QUIC_STATUS_NOT_FOUND);
			CASE2TEXT(QUIC_STATUS_BUFFER_TOO_SMALL);
			CASE2TEXT(QUIC_STATUS_HANDSHAKE_FAILURE);
			CASE2TEXT(QUIC_STATUS_ABORTED);
			CASE2TEXT(QUIC_STATUS_ADDRESS_IN_USE);
			CASE2TEXT(QUIC_STATUS_INVALID_ADDRESS);
			CASE2TEXT(QUIC_STATUS_CONNECTION_TIMEOUT);
			CASE2TEXT(QUIC_STATUS_CONNECTION_IDLE);
			CASE2TEXT(QUIC_STATUS_INTERNAL_ERROR);
			CASE2TEXT(QUIC_STATUS_CONNECTION_REFUSED);
			CASE2TEXT(QUIC_STATUS_UNREACHABLE);
			CASE2TEXT(QUIC_STATUS_TLS_ERROR);
			CASE2TEXT(QUIC_STATUS_USER_CANCELED);
			CASE2TEXT(QUIC_STATUS_ALPN_NEG_FAILURE);
			CASE2TEXT(QUIC_STATUS_STREAM_LIMIT_REACHED);
			CASE2TEXT(QUIC_STATUS_PROTOCOL_ERROR);
			CASE2TEXT(QUIC_STATUS_VER_NEG_ERROR);
			default:
				return ConvertResult(Error);
		}
	}

	// The rest here is for MsQuic -> UE5 conversion
	uint64 GetStreamId(const QUIC_API_TABLE* Api, HQUIC StreamHandle)
	{
		check(Api != nullptr);

		uint64_t StreamId = 0;
		uint32_t Size = sizeof(StreamId); // We need a pointer to this?

		if (QUIC_FAILED(Api->GetParam(
			StreamHandle,
			QUIC_PARAM_STREAM_ID,
			&Size,
			&StreamId
		)))
		{
			StreamId = UINT64_MAX;
		}

		return StreamId;
	}
}

uint32 GetTypeHash(const FStreamData& Data)
{
	return GetTypeHash(Data.StreamId);
}

using FQuicClientBuffer = TArray<uint8>;


/**
* Our Quic client will behave as follows:
* - Ingest data from a QUIC stream in a separate thread,
* - Internally enqueue received data in a non-locking way
* - Merge streamId data into a single buffer on the "Single Thread" tick
* -- Any full messages received in the buffer will be emitted as a full event with raw data for application level deserialization and handling.
* - Enqueue any outgoing data and perform the streaming internally in a separate thread as well
**/
struct FQuicClient
{
private:
	const QUIC_API_TABLE* MsQuic;
	HQUIC ClientConnectionHandle;

	// Only the receive streams need in memory buffering as our app logic emits fully materialized messages. MSQuic already works in a non-blocking async manner.
	TQueue<FQuicClientBuffer, EQueueMode::Spsc> PendingAppDataQueue; // Single producer (this), single consumer (App)
	// messages that we are currently compiling from the stream, single thread use.
	TSet<FStreamData> IncompleteMessages;

protected:
	// The QUIC connection handle as defined by MsQuic.
	_Function_class_(QUIC_CONNECTION_CALLBACK)
	static QUIC_STATUS ClientConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event);

	// The QUIC stream handle as defined by MsQuic.
	_Function_class_(QUIC_STREAM_CALLBACK)
	static QUIC_STATUS ClientStreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event);

	// Applevel callbacks for connection events
	void OnConnectionConnected(HQUIC Connection);
	void OnConnectionShutdownComplete(HQUIC Connection, bool bAppCloseInProgress);
	void OnConnectionShutdownByTransport(HQUIC Connection, uint32_t ErrorCode);
	void OnConnectionShutdownByPeer(HQUIC Connection, uint32_t ErrorCode);

	// Applevel callbacks for stream events
	void OnStreamSendComplete(HQUIC Stream, QUIC_STREAM_EVENT* Event);
	void OnStreamReceive(HQUIC Stream, QUIC_STREAM_EVENT* Event);
	void OnStreamPeerSendAborted(HQUIC Stream);
	void OnStreamShutdownComplete(HQUIC Stream);
public:
	~FQuicClient();

	// Instantiates and runs the thread.
	FQuicClient(HQUIC InQuicConfig
		, const QUIC_API_TABLE* InMsQuic
		, HQUIC InRegistration
		, const FIPv4Endpoint InRemoteEndpoint
	);

	inline bool IsFine() const
	{
		return ClientConnectionHandle != nullptr;
	}

	// Thread safe methods for the app to call
	bool Send(TArray<uint8>&& Data);

	// Receive data from the internal queue. Returns true if data was available. This will be a fully buffered message from the stream.
	bool Receive(TArray<uint8>& OutData);

	void ForceShutdown(); // Circumnavigate RAII and force this client to stop.
};