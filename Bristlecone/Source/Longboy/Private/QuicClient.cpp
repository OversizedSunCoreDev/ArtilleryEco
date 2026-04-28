#include "QuicClient.h"

DEFINE_LOG_CATEGORY(LongboyQuicClient);

FQuicClient::FQuicClient(HQUIC InQuicConfig
	, const QUIC_API_TABLE* InMsQuic
	, HQUIC InRegistration
	, const FIPv4Endpoint InRemoteEndpoint
)
	: MsQuic(InMsQuic)
	, ClientConnectionHandle(nullptr)
{
	QUIC_STATUS Status;

	if (QUIC_FAILED(Status = MsQuic->ConnectionOpen(
		InRegistration,
		ClientConnectionCallback,
		this,
		&ClientConnectionHandle
	)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("MsQuic->ConnectionOpen failed: %s"), *Quicky::GetEndpointErrorString(Status));
	}

	if(QUIC_FAILED(Status = MsQuic->ConnectionStart(
		ClientConnectionHandle,
		InQuicConfig,
		QUIC_ADDRESS_FAMILY_UNSPEC,
		TCHAR_TO_UTF8(*InRemoteEndpoint.Address.ToString()),
		InRemoteEndpoint.Port
	)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("MsQuic->ConnectionStart failed: %s"), *Quicky::GetEndpointErrorString(Status));
	}

	UE_LOG(LongboyQuicClient, Log, TEXT("Started QUIC client connection to %s"), *InRemoteEndpoint.ToString());
}

FQuicClient::~FQuicClient()
{
	ForceShutdown();
}

void FQuicClient::ForceShutdown()
{
	if (MsQuic != nullptr && ClientConnectionHandle != nullptr)
	{
		MsQuic->ConnectionShutdown(ClientConnectionHandle, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
		MsQuic->ConnectionClose(ClientConnectionHandle);
		ClientConnectionHandle = nullptr;
	}
}

// Primary MsQuic connection callback and dispatch for connection events.
_Function_class_(QUIC_CONNECTION_CALLBACK)
QUIC_STATUS FQuicClient::ClientConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event)
{
	FQuicClient* ClientContext = static_cast<FQuicClient*>(Context);
	switch (Event->Type)
	{

	case QUIC_CONNECTION_EVENT_CONNECTED:
		ClientContext->OnConnectionConnected(Connection);
		break;
	case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
		ClientContext->OnConnectionShutdownByTransport(Connection, Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
		break;
	case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
		ClientContext->OnConnectionShutdownByPeer(Connection, Event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
		break;
	case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
		ClientContext->OnConnectionShutdownComplete(Connection, Event->SHUTDOWN_COMPLETE.AppCloseInProgress);
		break;
	case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
	{
		ClientContext->MsQuic->SetCallbackHandler(Event->PEER_STREAM_STARTED.Stream, reinterpret_cast<void*>(ClientStreamCallback), Context);
		break;
	}

	case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
	case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED:
	case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
	case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
	case QUIC_CONNECTION_EVENT_IDEAL_PROCESSOR_CHANGED:
	case QUIC_CONNECTION_EVENT_LOCAL_ADDRESS_CHANGED:
	case QUIC_CONNECTION_EVENT_PEER_ADDRESS_CHANGED:
	case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED:
	case QUIC_CONNECTION_EVENT_PEER_NEEDS_STREAMS:
	case QUIC_CONNECTION_EVENT_RESUMED:
	case QUIC_CONNECTION_EVENT_STREAMS_AVAILABLE:
	default:
		UE_LOG(LongboyQuicClient, Warning, TEXT("Received unhandled connection event : % d"), Event->Type);
		break;
	}

	return QUIC_STATUS_SUCCESS;
}

_Function_class_(QUIC_STREAM_CALLBACK)
QUIC_STATUS FQuicClient::ClientStreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event)
{
	FQuicClient* ClientContext = static_cast<FQuicClient*>(Context);
	switch (Event->Type)
	{
	case QUIC_STREAM_EVENT_SEND_COMPLETE:
		ClientContext->OnStreamSendComplete(Stream, Event);
		break;
	case QUIC_STREAM_EVENT_RECEIVE:
		ClientContext->OnStreamReceive(Stream, Event);
		return QUIC_STATUS_PENDING;
	case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
		ClientContext->OnStreamPeerSendAborted(Stream);
		break;
	case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
		ClientContext->OnStreamShutdownComplete(Stream);
		break;
	case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
	case QUIC_STREAM_EVENT_START_COMPLETE:
	case QUIC_STREAM_EVENT_PEER_ACCEPTED:
	case QUIC_STREAM_EVENT_PEER_RECEIVE_ABORTED:
	case QUIC_STREAM_EVENT_IDEAL_SEND_BUFFER_SIZE:
	case QUIC_STREAM_EVENT_SEND_SHUTDOWN_COMPLETE:
	default:
		UE_LOG(LongboyQuicClient, Warning,
			TEXT("Received unhandled stream event: %d"), Event->Type);
		break;
	}

	return QUIC_STATUS_SUCCESS;
}

// Callbacks for connection and stream events.
void FQuicClient::OnConnectionConnected(HQUIC Connection)
{
	//QUIC_ADDR RemoteAddress;
	//uint32_t AddressSize = sizeof(RemoteAddress);
	//MsQuic->GetParam(Connection, QUIC_PARAM_CONN_REMOTE_ADDRESS, &AddressSize, &RemoteAddress);
	// log it
	UE_LOG(LongboyQuicClient, Log, TEXT("QUIC connection established"));
}

void FQuicClient::OnConnectionShutdownComplete(HQUIC Connection, bool bAppCloseInProgress)
{

}

void FQuicClient::OnConnectionShutdownByTransport(HQUIC Connection, uint32_t ErrorCode)
{

}

void FQuicClient::OnConnectionShutdownByPeer(HQUIC Connection, uint32_t ErrorCode)
{
}

// Callbacks for stream events
void FQuicClient::OnStreamSendComplete(HQUIC Stream, QUIC_STREAM_EVENT* Event)
{
}

void FQuicClient::OnStreamReceive(HQUIC Stream, QUIC_STREAM_EVENT* Event)
{
	const bool bIsEndOfStream = (Event->RECEIVE.Flags & QUIC_RECEIVE_FLAG_FIN) != 0;
	const uint64_t StreamId = Quicky::GetStreamId(MsQuic, Stream);

	// not sure if I like this... what is this, an alloc that may get trashed?
	FStreamData IncomingData(StreamId, Event->RECEIVE.TotalBufferLength);
	IncomingData = IncompleteMessages.FindOrAdd(IncomingData);
	for (uint32_t BufferIndex = 0; BufferIndex < Event->RECEIVE.BufferCount; ++BufferIndex)
	{
		check(BufferIndex < Event->RECEIVE.BufferCount);
		const QUIC_BUFFER& Buffer = Event->RECEIVE.Buffers[BufferIndex];
		IncomingData.Data.Append(Buffer.Buffer, Buffer.Length);
	}

	if (bIsEndOfStream)
	{
		// Wonder if I can do a swap here instead in some way...
		IncompleteMessages.Remove(IncomingData);
		PendingAppDataQueue.Enqueue(MoveTemp(IncomingData.Data));
	}
}

void FQuicClient::OnStreamPeerSendAborted(HQUIC Stream)
{
	if(MsQuic != nullptr)
	{
		MsQuic->StreamShutdown(Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
	}
}

void FQuicClient::OnStreamShutdownComplete(HQUIC Stream)
{
}

bool FQuicClient::Send(TArray<uint8>&& Data)
{
	// Create a new stream for each outgoing data buffer
	HQUIC NewStream = nullptr;
	QUIC_STATUS Status;
	if (QUIC_FAILED(Status = MsQuic->StreamOpen(
		ClientConnectionHandle,
		QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
		ClientStreamCallback,
		this,
		&NewStream
	)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("Failed to open new QUIC stream for outgoing data: %s"), *Quicky::ConvertResult(Status));
		return false;
	}
	if (QUIC_FAILED(MsQuic->StreamStart(
		NewStream,
		QUIC_STREAM_START_FLAG_NONE
	)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("Failed to start new QUIC stream for outgoing data: %s"), *Quicky::ConvertResult(Status));
		MsQuic->StreamClose(NewStream);
		return false;
	}

	// Prepare the buffer for sending
	QUIC_BUFFER SendBuffer;
	SendBuffer.Buffer = Data.GetData();
	SendBuffer.Length = Data.Num();

	if (QUIC_FAILED(MsQuic->StreamSend(
		NewStream,
		&SendBuffer,
		1,
		QUIC_SEND_FLAG_FIN,
		this
	)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("Failed to send data on QUIC stream: %s"), *Quicky::ConvertResult(Status));
		MsQuic->StreamShutdown(NewStream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
		MsQuic->StreamClose(NewStream);
		return false;
	}

	return true;
}

bool FQuicClient::Receive(TArray<uint8>& OutData)
{
	FQuicClientBuffer IncomingBuffer;
	if (PendingAppDataQueue.Dequeue(IncomingBuffer))
	{
		OutData = MoveTemp(IncomingBuffer);
		return true;
	}
	return false;
}