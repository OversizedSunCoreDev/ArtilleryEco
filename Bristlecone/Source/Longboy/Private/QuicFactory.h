#pragma once

#include "QuicClient.h"

// GLobally manages the MsQuic API function table and provides helper functions for working with MsQuic.
// Once a client gets shipped from the factory, the lifecycle of the product is up to the consumer.
// factories are instanced so we do not fight the UE lifecycle, a factory instance should be owned by the module.
struct FQuicFactory
{
	FQuicFactory();
	~FQuicFactory();
	TUniquePtr<FQuicClient> CreateClient(const FIPv4Endpoint& RemoteEndpoint);

private:
	// MsQuic API function table, globally available for all clients created by this factory.
	const QUIC_API_TABLE* MsQuic = nullptr;

	// Client Registration handle as defined by MsQuic, globally available for all clients created by this factory.
	HQUIC Registration = nullptr;

	// Client Configuration handle as defined by MsQuic, globally available for all clients created by this factory.
	HQUIC CurrentConfiguration = nullptr;
};
