#include "FBristleconeReceiver.h"
#include "LongboyCrypto.h"


FBristleconeReceiver::FBristleconeReceiver() : MySeen(0x0b1), running(false), SessionId(0), CipherKey(0), Crypto(nullptr) {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Receiver: Constructing Bristlecone Receiver"));
}

void FBristleconeReceiver::BindSink(TheCone::RecvQueue QueueCandidate)
{
	Queue.Reset();
	Queue = QueueCandidate;
}

void FBristleconeReceiver::BindStatsSink(TheCone::TimestampQueue QueueCandidate)
{
	PacketStats.Reset();
	PacketStats = QueueCandidate;
}

FBristleconeReceiver::~FBristleconeReceiver() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Receiver: Destructing Bristlecone Receiver"));
}

void FBristleconeReceiver::SetLocalSocket(const TSharedPtr<FSocket, ESPMode::ThreadSafe>& new_socket) {
	receiver_socket = new_socket;
}

void FBristleconeReceiver::SetSessionData(uint64 InSessionId, uint64 InCipherKey) {
	SessionId = InSessionId;
	CipherKey = InCipherKey;
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

bool FBristleconeReceiver::Init() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Receiver: Initializing Bristlecone receiver thread"));
	socket_subsystem.Reset(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM));
	SeenCycles = 0;
	HighestSeen = 0;
	running = true;
	return true;
}

uint32 FBristleconeReceiver::Run() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Receiver: Running receiver thread"));
	const TSharedRef<FInternetAddr> targetAddr = socket_subsystem->CreateInternetAddr();
	FString localNID = FGenericPlatformMisc::GetLoginId();

	//if you use a system like this, the reflector will need the unhashed ids AND the session id.
	//dummy getsession crashes in shipping builds on purpose. it's a mock. this will ultimately be reworked to replace
	//timestamp or cycle, depending on what we find more reliable, since the TCP backhaul maps timestamp onto cycle.
	//it's not actually needed, interestingly, but it does make life a ton more convenient and we want to stay word aligned.
	//likely timestamp as (nanos >> K) & 0xFFFFFFFF or the lower 32 bits of the nanosecond timestamp, discarding the 
	//first K binary digits to help remove jitter's effect.
	uint32_t ThinHash = FTextLocalizationResource::HashString(localNID, CipherKey);
	MySeen = TheCone::CycleTracking(ThinHash);
	const FTimespan Period(120000); //we wait 12ms at a stop. we don't have anything to do while we aren't waiting, but I don't trust it.
	while (running && receiver_socket) {
		TheCone::Packet_tpl receiving_state;
	
		uint32 socket_data_size;
		while (receiver_socket.IsValid() && receiver_socket->HasPendingData(socket_data_size)) {
			int32 bytes_read = 0;

			received_data.SetNumUninitialized(FMath::Min(socket_data_size, 65507u));
			receiver_socket->RecvFrom(received_data.GetData(), received_data.Num(), bytes_read, *targetAddr);

			const auto packet_size = sizeof(decltype(receiving_state));
			if(bytes_read == packet_size)
			{
				continue;
			}

			Crypto->DecryptHeader(
				reinterpret_cast<const uint32*>(received_data.GetData()),
				reinterpret_cast<uint32*>(received_data.GetData())
			);
			Crypto->DecryptBody(
				reinterpret_cast<const uint8*>(received_data.GetData()) + sizeof(uint32),
				reinterpret_cast<uint8*>(received_data.GetData()) + sizeof(uint32),
				bytes_read - sizeof(uint32)
			);

			memcpy(&receiving_state, received_data.GetData(), bytes_read);

			//this & logging are VERY slow, like potentially reordering our perceived timings slow. We need to be careful as hell interacting
			//with time and logging, since we're now operating in the lock-sensitive time regime. we'll need a solution.
			const uint64_t cycle = receiving_state.GetCycleMeta();
			//we keep a mask of the 64 cycles before the highest seen to make sure we don't emit more than once.
			//if it's higher, we slide forwards and don't need to check the mask. That's handled in the BitTracker
			if (!MySeen.Update(cycle))
			{
				continue;
			}
			if (LogOnReceive)
			{
				uint32_t lsbTime = NarrowClock::getSlicedMicrosecondNow();;
				TheCone::CycleTimestamp v = TheCone::CycleTimestamp(lsbTime - receiving_state.GetTransferTime(), receiving_state.GetCycleMeta());
				PacketStats->Enqueue(v); // p sure this doesn't leak memory? @Eliza, TODO: please sanity check me?
			}
			Queue.Get()->Enqueue(receiving_state);//this actually provokes a copy, which can be removed, I think, by not doing the mcpy
			

		}
	
		receiver_socket.IsValid() ? receiver_socket.Get()->Wait(ESocketWaitConditions::WaitForRead, Period) : 0;
	}
	receiver_socket = nullptr;//revise this, it's not super safe even with threadsafe smart pointers, but it'll hold for now.
	return 0;
}

void FBristleconeReceiver::Exit() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Receiver: Stopping Bristlecone receiver thread."));
	Cleanup();
}

void FBristleconeReceiver::Stop() {
	UE_LOG(LogTemp, Display, TEXT("Bristlecone:Receiver: Stopping Bristlecone receiver thread."));
	Cleanup();
}

void FBristleconeReceiver::Cleanup() {
	const ISocketSubsystem* socket_subsystem_obj = socket_subsystem.Release();
	if (socket_subsystem_obj != nullptr) {
		socket_subsystem_obj = nullptr;
	}
	running = false;
}
