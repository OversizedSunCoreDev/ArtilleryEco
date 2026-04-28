#include "QuicFactory.h"

const QUIC_BUFFER Alpn = { sizeof("longboyquic") - 1, (uint8_t*)"longboyquic" };

FQuicFactory::FQuicFactory()
{
	QUIC_STATUS Status;
	// Open the MsQuic API and get the function table.
	if (QUIC_FAILED(Status = MsQuicOpen2(&MsQuic)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("MsQuicOpen2 failed: %s"), *Quicky::GetEndpointErrorString(Status));
		return;
	}

	const QUIC_REGISTRATION_CONFIG RegConfig = { "longboy", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
	// Create a client registration with MsQuic. This is required before creating any connections.
	if (QUIC_FAILED(Status = MsQuic->RegistrationOpen(&RegConfig, &Registration)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("MsQuic->RegistrationOpen failed: %s"), *Quicky::GetEndpointErrorString(Status));
		MsQuicClose(MsQuic);
		MsQuic = nullptr;
		return;
	}

	QUIC_SETTINGS QuicSettings = { {0} };

	/** Client idle timeout disabled */
	QuicSettings.IdleTimeoutMs = 0;
	QuicSettings.IsSet.IdleTimeoutMs = 1;

	/** Client connection timeout (5 seconds) */
	QuicSettings.DisconnectTimeoutMs = 5000;
	QuicSettings.IsSet.DisconnectTimeoutMs = 1;

	/** Allow peer to open max unidirectional streams */
	QuicSettings.PeerUnidiStreamCount = std::numeric_limits<uint16_t>::max();
	QuicSettings.IsSet.PeerUnidiStreamCount = 1;

	/** Disable sendbuffering to avoid copying outbound data and delays in sending. */
	QuicSettings.SendBufferingEnabled = 0;
	QuicSettings.IsSet.SendBufferingEnabled = 1;

	if (QUIC_FAILED(Status = MsQuic->ConfigurationOpen(
		Registration,
		&Alpn,
		1, // Alpn count
		&QuicSettings,
		sizeof(QuicSettings),
		nullptr,
		&CurrentConfiguration
	)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("MsQuic->ConfigurationOpen failed: %s"), *Quicky::GetEndpointErrorString(Status));
		MsQuic->RegistrationClose(Registration);
		MsQuicClose(MsQuic);
		MsQuic = nullptr;
		return;
	}

	QUIC_CREDENTIAL_CONFIG CredConfig;
	FMemory::Memset(&CredConfig, 0, sizeof(CredConfig));
	CredConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
	CredConfig.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
#if !UE_BUILD_SHIPPING
	CredConfig.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
#endif

	if(QUIC_FAILED(Status = MsQuic->ConfigurationLoadCredential(CurrentConfiguration, &CredConfig)))
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("MsQuic->ConfigurationLoadCredential failed: %s"), *Quicky::GetEndpointErrorString(Status));
		MsQuic->ConfigurationClose(CurrentConfiguration);
		MsQuic->RegistrationClose(Registration);
		MsQuicClose(MsQuic);
		MsQuic = nullptr;
		return;
	}
}

FQuicFactory::~FQuicFactory()
{
	if (MsQuic != nullptr)
	{
		MsQuic->ConfigurationClose(CurrentConfiguration);
		MsQuic->RegistrationClose(Registration);
		MsQuicClose(MsQuic);
	}
}

TUniquePtr<FQuicClient> FQuicFactory::CreateClient(const FIPv4Endpoint& RemoteEndpoint)
{
	if (MsQuic == nullptr || Registration == nullptr || CurrentConfiguration == nullptr)
	{
		UE_LOG(LongboyQuicClient, Error, TEXT("Cannot create QUIC client because MsQuic failed to initialize."));
		return nullptr;
	}
	return MakeUnique<FQuicClient>(CurrentConfiguration, MsQuic, Registration, RemoteEndpoint);
}