#include "FBristleconeSender.h"

#include "UBristleconeWorldSubsystem.h"
#include "Common/UdpSocketBuilder.h"

//these includes shouldn't be moved to the .h, due to odd declaration behaviors.
//the same pattern can be seen, executed differently, in the socket library.
//it looks like they basically "shade" them from being included in some TLUs.
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <qos2.h>

typedef int32 SOCKLEN;

#include "Windows/HideWindowsPlatformTypes.h"

#endif
#include <Runtime/Sockets/Private/BSDSockets/SocketsBSD.h>
#include "LongboyCrypto.h"

FBristleconeSender::FBristleconeSender()
: consecutive_zero_bytes_sent(0), running(false), SessionId(0), CipherKey(0), Crypto(nullptr) {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Sender: Constructing Bristlecone Sender"));

	target_endpoints = MakeShareable(new TArray<FIPv4Endpoint>());
	target_endpoints->Reserve(MAX_TARGET_COUNT);
	
}

FBristleconeSender::~FBristleconeSender() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Sender: Destructing Bristlecone Sender"));
}

void FBristleconeSender::SetSessionData(uint64 sessionId, uint64 cipherKey)
{
	// uh probs make this thread safe now
	SessionId = sessionId;
	CipherKey = cipherKey;
	if (Crypto == nullptr)
	{
		Crypto = new FLongboyCrypto(CipherKey);
	}
	else
	{
		delete Crypto;
		Crypto = new FLongboyCrypto(CipherKey);
	}
}

void FBristleconeSender::BindSource(TheCone::SendQueue QueueCandidate)
{
	Queue.Reset();
	Queue = QueueCandidate;
}

void FBristleconeSender::AddTargetAddress(FString target_address_str) {
	FIPv4Address target_address;
	FIPv4Address::Parse(target_address_str, target_address);
	FIPv4Endpoint target_endpoint = FIPv4Endpoint(target_address, GetDefault<UBristleconeConstants>()->ClientToServerSchemaPort);
	target_endpoints->Emplace(target_endpoint);
}

void FBristleconeSender::SetLocalSockets(
	const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket_high,
	const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket_low,
	const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket_adaptive
) {
	sender_socket_high = new_socket_high;
	sender_socket_low = new_socket_low;
	sender_socket_background = new_socket_adaptive;
}

void FBristleconeSender::SetWakeSender(FSharedEventRef NewWakeSender) {
	WakeSender = NewWakeSender;
}

void FBristleconeSender::ActivateDSCP()
{

	//this no-ops on many platforms.
	//TODO: If you want to take this to general production grade, this will need a behavior for most platforms.
	//Fortunately, Linux, mac, steam deck, and many other platforms will actually be simpler, as those allow dscp
	//to be set normally, instead of requiring qos manipulation.
	// 
	//The outliers are switch and Steam Datagram Relays, and I don't even know that you'd ever use bristlecone with SDR, as it's basically
	// a successor system with a narrow application space.
	
	//quite a lot of ungood things have to happen for us to do this. As a result, I'll be writing out what we're doing.
	// https://learn.microsoft.com/en-us/windows/win32/api/qos2/nf-qos2-qosaddsockettoflow
	//https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/netds/Qos/Qos2/qossample.c
	//This is probably the best example I can provide for WHAT is happening.
	//DSCP settings do appear to have a significant but small effect on behavior, contrary to popular wisdom.
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	QOS_VERSION Version;
	SOCKADDR_IN destination;
	HANDLE      QoSHandle = NULL;


	// Initialize the QoS version parameter.
	Version.MajorVersion = 1;
	Version.MinorVersion = 0;
	QOS_FLOWID     QoSFlowId = 0;
	destination.sin_family = AF_INET;
	destination.sin_port = target_endpoints->Last().Port;
	destination.sin_addr.s_addr = target_endpoints->Last().Address.Value;
	// Get a handle to the QoS subsystem. this requires us to have the qwave lib file loaded to resolve the symbol. Oddly, you can't load the dll.

	QOSCreateHandle(
		&Version,
		&QoSHandle);
	//this is necessary because fsocket does not have a get native, as not all abstracted sockets actually have a native file-like socket
	//our build mechanism has to break encapsulation pretty aggressively to resolve this, and it's quite ugly.
	SOCKET underlyingHigh = ((FSocketBSD*)(sender_socket_high.Get()))->GetNativeSocket();// Time to go for a very bad ride.
	SOCKET underlyingLow = ((FSocketBSD*)(sender_socket_low.Get()))->GetNativeSocket();
	SOCKET underlyingSPICY = ((FSocketBSD*)(sender_socket_background.Get()))->GetNativeSocket();
	//qwave MAY need destination and point. as a result, we had to wait to perform this until now.
	//https://learn.microsoft.com/en-us/windows/win32/api/qos2/ne-qos2-qos_traffic_type
	//The DSCP markings are the most effective part, so far as I can tell, but local routers often support 802.1.
	//we should switch to using https://learn.microsoft.com/en-us/windows/win32/api/qos2/nf-qos2-qossetflow
	//and revisit this. It appears that you can override bandwidth limits that may be automatically placed on flows
	// and get system RTT information for flows. Both of these would be useful. the windows QoS system is always running
	//to some extent and CAN delay traffic from being sent to meet traffic shaping goals.
	// see https://learn.microsoft.com/en-us/windows/win32/api/qos2/ne-qos2-qos_shaping
	// We'd like to modify this aggressively, but that's more testing and research than I can afford atm.
	
	//Codepoints worth testing still are: some combinations of (4, 7, 11, 18, 23)
	//which cisco docs indicate are supported AHBs. I'm not sure how to get windows to set and respect those without admin.

	//While there are other codings for gaming, our control flow is actually most similar to low bit-rate av flows
	//where jitter is crippling. We can't mark all our packets at codepoint 46, or we may get shaped and dropped for being
	//bad citizens, but we do want to mark one of our clones at 46.
	QOSAddSocketToFlow(QoSHandle, underlyingHigh, (SOCKADDR*)&destination, QOS_TRAFFIC_TYPE::QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &QoSFlowId);
	QoSFlowId = 0;
	QOSAddSocketToFlow(QoSHandle, underlyingLow, (SOCKADDR*)&destination, QOS_TRAFFIC_TYPE::QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &QoSFlowId);
	QoSFlowId = 0;
	//Control doesn't seem to actually set a respected value. Unfortunately, I can't find a way to set an arbitrary DSCP without admin on windows.
	QOSAddSocketToFlow(QoSHandle, underlyingSPICY, (SOCKADDR*)&destination, QOS_TRAFFIC_TYPE::QOSTrafficTypeBackground, QOS_NON_ADAPTIVE_FLOW, &QoSFlowId);
#endif
}

bool FBristleconeSender::Init() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Sender: Initializing Bristlecone Sender thread"));
	socket_subsystem.Reset(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM));
	
	running = true;
	return true;
}

uint32 FBristleconeSender::Run() {
	uint16 counter = 0;
	FControllerState sending_state;

	const auto Config = GetDefault<UBristleconeConstants>();
	const uint16 ClientToServerMapperPort = Config->ClientToServerSchemaMapperPort;
	const uint16 ClientToServerPort = Config->ClientToServerSchemaPort;
	const uint16 ServerToClientMapperPort = Config->ServerToClientMapperPort;
	
	while(sender_socket_high) {
		auto H1 = sender_socket_low;
		auto H2 = sender_socket_high;
		auto H3 = sender_socket_background;
		//perform before the wait so we don't waste time... pins open until loop complete.
		WakeSender->Wait(8);

		WakeSender->Reset();

		// Ping to the mapper sockets to update session state.
		{
			auto HoldOpen = target_endpoints;
			if (HoldOpen && H1 && H2 && H3)
			{
				// ; ABNF format of the messages we send to mapper ports.
				// <MAPPER_MESSAGE>             ::= <SESSION_ID> <MIRRORING>        ; notify server of a new port and endpoint mapping for this session, and how many mirror ports to expect.
				// 
				// <SESSION_MAPPER_HEARTBEAT>   ::= <SESSION_ID>                    ; heartbeat message, no mirroring info, just need to keep the session alive and let the mapper know we're still here.
				// 
				// <MIRROR_MAPPER_HEARTBEAT>    ::= <MIRROR_INDEX>                  ; heartbeat message for mirror, mirror index only needed if mirroring > 1
				// 
				// <DATAGRAM_MESSAGE>           ::= <HEADER> <PAYLOAD>              ; A full message, payloads contain game input.
				// 
				// <HEADER>                     ::= <CYCLE> <TIMESTAMP>             ; This is the format POST decryption using the shared cipher key.
				// 
				// <SESSION_ID>                 ::= 8*OCTET                         ; backhaul server assigned session id
				// <MIRRORING>                  ::= 1*OCTET                         ; number of mirror indices to activate
				// <CYCLE>                      ::= 2*OCTET                         ; cycle number for this packet, modulo 65536
				// <TIMESTAMP>                  ::= 2*OCTET                         ; sender small timestamp - small because we have tiny time ranges of validity
				// <PAYLOAD>                    ::= *OCTET                          ; game input data, size is compile time defined. Encrypted using the shared cipher key.
				for (auto& endpoint : (*HoldOpen)) {
					// same address, different port
					FIPv4Endpoint ActualEndpoint = FIPv4Endpoint(endpoint.Address, ClientToServerMapperPort);
					FIPv4Endpoint MirrorHeartbeatEndpoint = FIPv4Endpoint(endpoint.Address, ClientToServerPort);
					FIPv4Endpoint HeartbeatEndpoint = FIPv4Endpoint(endpoint.Address, ServerToClientMapperPort);
					int32 bytes_sent;
					// for now, session Id is static value of 1 in uint64 form.
					const struct _ {
						uint64 SessionId;
						uint8  Mirroring;
					} msg{ SessionId, 2U };
					constexpr size_t msg_size = sizeof(msg);
					auto holdopen = ActualEndpoint;
					bool packet_sent = H2->SendTo(reinterpret_cast<const uint8*>(&msg), msg_size,
						bytes_sent, *holdopen.ToInternetAddr());
					packet_sent = H1->SendTo(reinterpret_cast<const uint8*>(&msg), msg_size,
						bytes_sent, *holdopen.ToInternetAddr());
					packet_sent = H3->SendTo(reinterpret_cast<const uint8*>(&msg), msg_size,
						bytes_sent, *holdopen.ToInternetAddr());

					// TODO: perform over a time period
					holdopen = HeartbeatEndpoint;
					constexpr size_t msg_session_size = sizeof(msg.SessionId);
					constexpr size_t msg_session_offset = offsetof(decltype(msg), SessionId);
					packet_sent = H2->SendTo(reinterpret_cast<const uint8*>(&msg) + msg_session_offset, msg_session_size,
						bytes_sent, *holdopen.ToInternetAddr());
					packet_sent = H1->SendTo(reinterpret_cast<const uint8*>(&msg) + msg_session_offset, msg_session_size,
						bytes_sent, *holdopen.ToInternetAddr());
					packet_sent = H3->SendTo(reinterpret_cast<const uint8*>(&msg) + msg_session_offset, msg_session_size,
						bytes_sent, *holdopen.ToInternetAddr());

					// We sent the sessionId heartbeat, now the protocol wants some mirroring heartbests.
					holdopen = MirrorHeartbeatEndpoint;
					for (uint8 MirrorIdx = 0; MirrorIdx < msg.Mirroring; ++MirrorIdx)
					{
						constexpr size_t msg_mirror_idx_size = sizeof(MirrorIdx);
						packet_sent = H2->SendTo(reinterpret_cast<const uint8*>(&MirrorIdx), msg_mirror_idx_size,
							bytes_sent, *holdopen.ToInternetAddr());
						packet_sent = H1->SendTo(reinterpret_cast<const uint8*>(&MirrorIdx), msg_mirror_idx_size,
							bytes_sent, *holdopen.ToInternetAddr());
						packet_sent = H3->SendTo(reinterpret_cast<const uint8*>(&MirrorIdx), msg_mirror_idx_size,
							bytes_sent, *holdopen.ToInternetAddr());
					}

					if(bytes_sent == 0) {
						consecutive_zero_bytes_sent++;
					}
					else {
						consecutive_zero_bytes_sent = 0;
					}
				}
			}
		}

		// Update ring array
		//BRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR
		while(!Queue.Get()->IsEmpty())
		{
			++counter;
			sending_state.controller_arr = *Queue->Peek(); //assign by value or you'll have a bad time.
			packet_container.InsertNewDatagram(&sending_state);
			packet_container.GetPacket()->UpdateCycleOrMeta(counter);

			auto HoldOpen = target_endpoints;
			if(HoldOpen && H1 && H2 && H3)
			{
				for (auto& endpoint : (*target_endpoints)) {
					int32 bytes_sent;
					//we may want a mode to timestamp each individual packet during testing so we can get a unique rtt
					const FControllerStatePacket* current_controller_state = packet_container.GetPacket();
					// Create a working copy for right now, will update to do everything in place if possible, but this is easier to write for now.
					FControllerStatePacket working_copy = *current_controller_state;
					// log the current controller state as an array of int8 for debugging purposes
					const uint8* controller_state_bytes = reinterpret_cast<const uint8*>(&working_copy);
					uint8* mutable_controller_state_bytes = reinterpret_cast<uint8*>(&working_copy);
					constexpr size_t controller_state_size = sizeof(FControllerStatePacket);

					// TODO: figure out all of this to see if it can be encrypted in place versus allocation, or use an arena.
					Crypto->EncryptHeader(
						reinterpret_cast<const uint32*>(controller_state_bytes),
						reinterpret_cast<uint32*>(mutable_controller_state_bytes)
					);
					Crypto->EncryptBody(
						controller_state_bytes + sizeof(uint32),
						mutable_controller_state_bytes + sizeof(uint32),
						controller_state_size - sizeof(uint32)
					);

					auto holdopen = endpoint;
					bool packet_sent = H2->SendTo(controller_state_bytes, controller_state_size,
						bytes_sent, *holdopen.ToInternetAddr());
					packet_sent = H1->SendTo(controller_state_bytes, controller_state_size,
						bytes_sent, *holdopen.ToInternetAddr());
					packet_sent = H3->SendTo(controller_state_bytes, controller_state_size,
						bytes_sent, *holdopen.ToInternetAddr());


					if (bytes_sent == 0) {
						consecutive_zero_bytes_sent++;
					}
					else {
						consecutive_zero_bytes_sent = 0;
					}

				}
			}
			Queue->Dequeue();
		}
	}
	
	return 0;
}

void FBristleconeSender::Exit() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Sender: Exiting Bristlecone sender thread."));
	running = false;
	Cleanup();
}

void FBristleconeSender::Stop() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Sender: Stopping Bristlecone sender thread."));
	running = false;
	Cleanup();
}

void FBristleconeSender::Cleanup() {
	sender_socket_high = nullptr;
	sender_socket_low = nullptr;
	sender_socket_background = nullptr;
	const ISocketSubsystem* socket_subsystem_obj = socket_subsystem.Release();
	if (socket_subsystem_obj != nullptr) {
		socket_subsystem_obj = nullptr;
	}
	target_endpoints.Reset();
	running = false;
}

