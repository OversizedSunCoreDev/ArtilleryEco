// Fill out your copyright notice in the Description page of Project Settings.


#include "UBristleconeWorldSubsystem.h"

#include "Longboy.h"
#include "UBristleconeConstants.h"

bool UBristleconeWorldSubsystem::RegistrationImplementation()
{
	UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: Inbound and Outbound Queues set to null."));
	QueueOfReceived = nullptr;
	QueueToSend = nullptr;
	ReceiveTimes = nullptr;
	SelfBind = nullptr;
	DebugSend = nullptr;
	//TODO @maslabgamer: does this leak memory?
	const UBristleconeConstants* ConfigVals = GetDefault<UBristleconeConstants>();
	LogOnReceive = ConfigVals->log_receive_c;
	FString address = ConfigVals->default_address_c.IsEmpty() ? "34.207.0.66" : ConfigVals->default_address_c;
	// Dependent on Longboy Backhaul. Not sure if we wanted each player or each game instance to have their own session/client, but for now, we'll just have one client per world.
	auto& LongboyModule = FModuleManager::GetModuleChecked<FLongboyModule>("Longboy");
	FIPv4Address IPv4Address;
	if (!FIPv4Address::Parse(ConfigVals->BackhaulAddress, IPv4Address))
	{
		UE_LOG(LogTemp, Error, TEXT("UBristleconeWorldSubsystem: Failed to parse default address from config."));
		return true; //we registered successfully but did not initialize successfully.
	}
	
	
	// @todo this should definitely not be on the main thread at least in the editor, this sleeps for 30 seconds in the editor
	// I am guessing this init step could move to the bristlecone runners? 
	if (GetWorld()->GetNetMode() != NM_Standalone) 
	{
		// This will establish a session.
		Longboy = LongboyModule.CreateClient(FIPv4Endpoint(IPv4Address, ConfigVals->BackhaulPort));
	}

	
	if(Longboy == nullptr || !Longboy->IsValidSession())
	{
		UE_LOG(LogTemp, Error, TEXT("UBristleconeWorldSubsystem: Failed to establish Longboy session."));
		return true; // this is a non-fatal failure.
	}
	sender_runner.AddTargetAddress(address);
	UE_LOG(LogTemp, Warning,
	       TEXT("BCN will not start unless another subsystem creates and binds queues during PostInitialize."));
	UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: Subsystem world initialized"));
	UCablingWorldSubsystem* Cabling = UCablingWorldSubsystem::SelfPtr;
	if (Cabling == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UBristleconeWorldSubsystem: No Cabling subsystem to connect to!"));
	}
	QueueToSend = Cabling->CabledThreadControlQueue;
	//this is a really odd take on the RAII model where it's to the point where FSharedEventRef should NEVER be alloc'd with new.
	//As is, we know that the event ref objects share the lifecycle of their owners exactly, being alloc'd and dealloced with them.
	//so we set ours equal to theirs, and the ref count will only drop when we are fully deinit'd
	Cabling->controller_runner.WakeTransmitThread = WakeSender;
	UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: World beginning play prep"));
	if (!QueueToSend.IsValid())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "Bristlecone:Subsystem: Inbound queue not allocated. Debug mode only. Connect a controller next time?"
		       ));
		QueueToSend = MakeShareable(new IncQ(256));
		DebugSend = QueueToSend;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: Good bind for send queue."));
	}

	if (!QueueOfReceived.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: No bind for received queue. Self-binding for debug."));
		QueueOfReceived = MakeShareable(new PacketQ(256));
		SelfBind = QueueOfReceived;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: Good bind for received queue."));
	}

	local_endpoint = FIPv4Endpoint(FIPv4Address::Any, DEFAULT_PORT);
	FUdpSocketBuilder socket_factory = FUdpSocketBuilder(TEXT("Bristlecone.Receiver.Socket"))
	                                   .AsNonBlocking()
	                                   .AsReusable()
	                                   .BoundToEndpoint(local_endpoint)
	                                   .WithReceiveBufferSize(CONTROLLER_STATE_PACKET_SIZE * 25)
	                                   .WithSendBufferSize(CONTROLLER_STATE_PACKET_SIZE * 25);
	socketHigh = MakeShareable(socket_factory.Build());
	socketLow = MakeShareable(socket_factory.Build());
	socketBackground = MakeShareable(socket_factory.Build());

	sender_runner.SetWakeSender(WakeSender);
	// start sender thread
	//TODO: refactor this to allow proper data driven construction.
	sender_runner.BindSource(QueueToSend);
	sender_runner.SetSessionData(Longboy->GetSessionId(), Longboy->GetCipherKey());
	sender_runner.SetLocalSockets(socketHigh, socketLow, socketBackground);
	sender_runner.ActivateDSCP();
	sender_thread.Reset(FRunnableThread::Create(&sender_runner, TEXT("Bristlecone.Sender")));

	// Start receiver thread
	ReceiveTimes = MakeShareable(new TimestampQ(140));
	receiver_runner.BindStatsSink(ReceiveTimes);
	receiver_runner.SetSessionData(Longboy->GetSessionId(), Longboy->GetCipherKey());
	receiver_runner.LogOnReceive = LogOnReceive;
	receiver_runner.SetLocalSocket(socketHigh);
	receiver_runner.BindSink(QueueOfReceived);

	receiver_thread.Reset(FRunnableThread::Create(&receiver_runner, TEXT("Bristlecone.Receiver")));

	return true;
}

void UBristleconeWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UBristleconeWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	if ([[maybe_unused]] const UWorld* World = InWorld.GetWorld())
	{
	}
}

void UBristleconeWorldSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: Deinitializing Bristlecone subsystem"));

	if (sender_thread)
	{
		sender_thread->Kill();
		WakeSender->Trigger();
	}
	if (receiver_thread)
	{
		receiver_thread->Kill();
	}

	if (socketHigh.IsValid())
	{
		socketHigh.Get()->Close();
	}
	if (socketLow.IsValid())
	{
		socketLow.Get()->Close();
	}
	if (socketBackground.IsValid())
	{
		socketBackground.Get()->Close();
	}
	socketHigh = nullptr;
	socketLow = nullptr;
	socketBackground = nullptr;

	//this will all need to be refactored, but tbh, I'm not sure we'll keep this for long enough to do it.
	FSocket* sender_socket_obj = socketHigh.Get();
	if (sender_socket_obj != nullptr)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(sender_socket_obj);
	}
	sender_socket_obj = socketLow.Get();
	if (sender_socket_obj != nullptr)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(sender_socket_obj);
	}
	sender_socket_obj = socketBackground.Get();
	if (sender_socket_obj != nullptr)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(sender_socket_obj);
	}
	Super::Deinitialize();
}

void UBristleconeWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	++logTicker;
	if (logTicker >= 30)
	{
		logTicker = 0;
		if (LogOnReceive)
		{
			double sum = 0;
			double sent = 0;
			while (ReceiveTimes != nullptr && ReceiveTimes->IsEmpty() != true)
			{
				++sent;
				sum += ReceiveTimes->Peek()->first;
				ReceiveTimes->Dequeue();
			}
			//UE_LOG(LogTemp, Warning, TEXT("Bristlecone: Average Latency, %lf for %lf packets"), (sum / sent), sent);
		}
	}
	//UE_LOG(LogTemp, Warning, TEXT("Bristlecone:Subsystem: Subsystem world ticked"));
}

TStatId UBristleconeWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFBristleconeWorldSubsystem, STATGROUP_Tickables);
}
