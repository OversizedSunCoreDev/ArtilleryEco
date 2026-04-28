#include "FCablingRunner.h"
#include "Windows/WindowsSystemIncludes.h"
#include <timeapi.h>
#include <bitset>
#include <thread>

#include "CablingCommonTypes.h"
#include "FStatefulPatternMatcher.h"
#include "MatchableTagTypes.h"
#include "UnsignedNarrowTime.h"

using std::bitset;

FCabling::FCabling()
{
	running = false;
	GuessedInputCount = 0;
}

FCabling::~FCabling()
{
}

bool FCabling::Init()
{
	UE_LOG(LogTemp, Display, TEXT("FCabling: Initializing Control Intercept thread"));
	running = true;
	GuessedInputCount = 0;
	return true;
}

bool FCabling::SendNew(bool sent, uint64_t priorReading, uint64_t currentRead)
{
	if (!sent && currentRead != priorReading)
	{
		// push to both queues.
		this->CabledThreadControlQueue.Get()->Enqueue(currentRead);
		this->GameThreadControlQueue.Get()->Enqueue(currentRead);
		WakeTransmitThread->Trigger();
		return true;
	}
	return sent;
}

bool FCabling::SendIfWindowEdge(bool sent, int seqNumber, uint64_t currentRead, const uint32_t sendHertzFactor)
{
	if (
		//if we haven't sent, we do have a reading...
		!sent && currentRead != 0 &&
		//and we're out of bloosy time.
		seqNumber % sendHertzFactor != 0)
	{
		//push to both queues.
		this->CabledThreadControlQueue.Get()->Enqueue(currentRead);
		this->GameThreadControlQueue.Get()->Enqueue(currentRead);
		WakeTransmitThread->Trigger();
		return true;
	}
	return sent;
}

uint64_t FCabling::FromKeyboardAndMouseState(uint32_t keyCount, GameInputKeyState (&states)[16],
                                             const GameInputMouseButtons& mouseButtons, float MouseX, float MouseY)
{
	double xMagnitude = 0.0;
	double yMagnitude = 0.0;

	FCableInputPacker boxing;
	boxing.buttons = 0;

	for (uint32_t i = 0; i < keyCount; i++)
	{
		if (states[i].codePoint == 0 && states[i].scanCode == 0)
		{
			break; //0,0 is indicates end of valid data per api doc.
		}
		//https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
		// W
		if (states[i].codePoint == 0x57)
		{
			yMagnitude += 1.0;
		}
		// A
		if (states[i].virtualKey == 0x41)
		{
			xMagnitude -= 1.0;
		}
		// S
		if (states[i].codePoint == 0x53)
		{
			yMagnitude -= 1.0;
		}
		// D
		if (states[i].codePoint == 0x44)
		{
			xMagnitude += 1.0;
		}
		if (states[i].codePoint == VK_SPACE)
		{
			boxing.buttons.set(10, true);
			// boxing.buttons.set(11, true);
		}
	}


	boxing.lx = boxing.IntegerizedStick(xMagnitude);
	boxing.ly = boxing.IntegerizedStick(yMagnitude);
	boxing.rx = boxing.IntegerizedStick(MouseX);
	boxing.ry = boxing.IntegerizedStick(MouseY);

	if ((mouseButtons & GameInputMouseButtons::GameInputMouseLeftButton) != 0)
	{
		boxing.buttons.set(13, true);
	}
	if ((mouseButtons & GameInputMouseButtons::GameInputMouseRightButton) != 0)
	{
		boxing.buttons.set(12, true);
	}

	uint64_t currentRead = boxing.PackImpl();
	//don't check events because we may set an event to indicate that we're on keeb input....
	return currentRead;
}


uint64_t FCabling::FromGamePadState(GameInputGamepadState state)
{
	FCableInputPacker boxing;
	//very fun story. unless you explicitly import and use std::bitset
	//the wrong thing happens here. I'm not going to speculate on why, because
	//I don't think I can do so without swearing extensively.
	boxing.lx = boxing.IntegerizedStick(state.leftThumbstickX);
	boxing.ly = boxing.IntegerizedStick(state.leftThumbstickY);
	boxing.rx = boxing.IntegerizedStick(state.rightThumbstickX);
	boxing.ry = boxing.IntegerizedStick(state.rightThumbstickY);
	boxing.buttons = static_cast<uint32_t>(state.buttons); //strikingly, there's no paddle field.
	boxing.buttons.set(12, (state.leftTrigger > 0.55)); //check the bitfield.
	boxing.buttons.set(13, (state.rightTrigger > 0.55));

	bool HasFlick = MatchingTools::FlickDetect<FlickBuffer*>(
		boxing.GetStickLeftXAsACSN(),
		boxing.GetStickLeftYAsACSN(),
		GuessedInputCount,
		(GuessedInputCount - MatchingTools::DefaultFlickTickWidth),
		&RingForGamepadKeybinds);
	if (HasFlick)
	{
		boxing.buttons.set(14, true);
	}

	//we'll need a little damping over these which is why they're classed as virtual.
	boxing.buttons.set(15, (state.buttons & GameInputGamepadLeftThumbstick) != 0);
	
	boxing.buttons.set(16, (state.buttons & GameInputGamepadRightThumbstick) != 0);
	this->RingForGamepadKeybinds.add(GuessedInputCount, boxing);
	uint64_t currentRead = boxing.PackImpl();

	//because we deadzone and integerize, we actually have a pretty good idea
	//of when input actually changes. 2048 positions for the stick along each axis
	//actually looks like it's enough to give us precise movement while still
	//excising some amount of jitter. Because we always round down, you have to move
	//fully to a new position and this seems to be a larger delta than the average
	//heart-rate jitter or control noise.
	//TODO: this line might actually be wrong. I just redid the math, and it looks right but...
	return currentRead;
}

//Sets Sent if prip
uint64_t FCabling::GamepadState(IGameInputReading* reading)
{
	// If no device has been assigned to g_gamepad yet, set it
	// to the first device we receive input from. (This must be
	// the one the player is using because it's generating input.)

	// Retrieve the fixed-format gamepad state from the reading.
	GameInputGamepadState state;
	reading->GetGamepadState(&state);
	return FromGamePadState(state);
}

//this is based directly on the gameinput sample code.
uint32 FCabling::Run()
{
	IGameInput* g_gameInput = nullptr;
	HRESULT gameInputSpunUp = GameInputCreate(&g_gameInput);
	IGameInputDevice* g_gamepad = nullptr;
	IGameInputReading* reading;
	constexpr static float LowGainMouse[5] = {0, 0.33, 0.44, 0.55, .75};
	constexpr static float MidGainMouse[9] = {0, 0.25, 0.35, 0.45, .55, .6, .65, 0.70, .8};
	
	constexpr static float HighGainMouse[12] = {0, 0.18, 0.22, 0.26, 0.30, 0.345, 0.44, .54, .64, .72, 0.72, .8};


	GameInputKeyState states[16] = {{0, 0, 0, false}}; //the first 0,0 indicates the end of valid data.
	bool Sent = false;
	//odd behavior occurs if the compiler is allowed to optimize this all the way down
	//when the queues are never correctly set. this can make it impossible to debug, so this var is flagged volatile.
	//TODO remove before launch.
	volatile int TickCounter = 0;
	uint64_t PriorReadingKeyboard = 0;
	RingForGamepadKeybinds = FlickBuffer();
	uint64_t PriorReadingGamepad = 0;

	// Not quantized, these are raw mouse positions.
	float PrevMouseX = 0;
	float PrevMouseY = 0;

	//Hi! Jake here! Reminding you that this will CYCLE
	//That's known. Isn't that fun? :) Don't reorder these, by the way.
	uint32_t lastPollTime = NarrowClock::getSlicedMicrosecondNow();
	uint32_t lsbTime = NarrowClock::getSlicedMicrosecondNow();
	constexpr uint32_t sampleHertz = Cabling::CablingSampleHertz;
	constexpr uint32_t sendHertz = Cabling::BristleconeSendHertz;
	constexpr int sendHertzFactor = sampleHertz / sendHertz;
	constexpr int Period = 1000000 / sampleHertz; //swap to microseconds. standardizing.

	GuessedInputCount = 0;

	constexpr auto HalfStep = std::chrono::microseconds(Period / 2);
	const uint64_t BlankGamepad = FromGamePadState(GameInputGamepadState());
	const uint64_t BlankKeyboard = FromKeyboardAndMouseState(16, states, {}, 0, 0);

	timeBeginPeriod(1);

	//We're using the GameInput lib.
	//https://learn.microsoft.com/en-us/gaming/gdk/_content/gc/input/overviews/input-overview
	//https://learn.microsoft.com/en-us/gaming/gdk/_content/gc/input/advanced/input-keyboard-mouse will be fun
	//https://handmade.network/forums/t/8710-using_microsoft_gameinput_api_with_multiple_controllers#29361
	//Looks like PS4/PS5 won't be too bad, just gotta watch out for Fun Device ID changes.
	double adaptX = 1;
	double adaptY = 1;
	while (running)
	{
		if (lastPollTime + Period <= lsbTime)
		{
			lastPollTime = lsbTime;
			//if it's been blown up or if create failed.
			if (!g_gameInput || !SUCCEEDED(gameInputSpunUp))
			{
				gameInputSpunUp = GameInputCreate(&g_gameInput);
			}

			IGameInputDevice* keyboard = nullptr;
			uint64_t KeyboardCurrentRead = BlankKeyboard;
			uint64_t GamepadCurrentRead = BlankGamepad;

			IGameInputDevice* Mouse = nullptr;

			GameInputMouseState mouseState = {};

			// mouse first state is considered first (probably not what you want though)
			float MouseXDelta = 0;
			float MouseYDelta = 0;
			if (g_gameInput && !Sent && SUCCEEDED(g_gameInput->GetCurrentReading(GameInputKindMouse, Mouse, &reading)))
			{
				if (reading->GetMouseState(&mouseState))
				{
					float delX = (float)mouseState.positionX - PrevMouseX;
					float delY = (float)mouseState.positionY - PrevMouseY;
					auto absX = FMath::CeilToInt(abs(delX));
					auto absY = FMath::CeilToInt(abs(delY));
					adaptX = FMath::Max(adaptX, absX);
					adaptY = FMath::Max(adaptY, absY);
					if (adaptX < 5 && adaptY < 5) //|| (absX + absY) < 2)
					{
						MouseXDelta = FMath::Sign(delX) * LowGainMouse[absX]; 
						MouseYDelta = -FMath::Sign(delY) * LowGainMouse[absY];
					}
					else if (adaptX < 9 && adaptY < 9)
					{
						MouseXDelta = FMath::Sign(delX) * MidGainMouse[absX]; 
						MouseYDelta = -FMath::Sign(delY) * MidGainMouse[absY];
					}
					else if (adaptX < 13 && adaptY < 13)
					{
						MouseXDelta = FMath::Sign(delX) * HighGainMouse[absX]; 
						MouseYDelta = -FMath::Sign(delY) * HighGainMouse[absY];
					}
					else if (adaptX < 37 && adaptY < 37)
					{
						MouseXDelta = FMath::Sign(delX) * HighGainMouse[absX/3]; 
						MouseYDelta = -FMath::Sign(delY) * HighGainMouse[absY/4];
					}
					else
					{
						MouseXDelta = FMath::Sign(delX) * FMath::Min(absX/58.0, .9); 
						MouseYDelta = -FMath::Sign(delY) *  FMath::Min(absY/67.0, .8);
					}

					PrevMouseX = mouseState.positionX;
					PrevMouseY = mouseState.positionY;
					adaptX -= 2;
					adaptY -= 2;
				}

				reading->Release();
			}

			//get the keeb...
			if (g_gameInput && !Sent &&
				SUCCEEDED(g_gameInput->GetCurrentReading(GameInputKindKeyboard, keyboard, &reading)))
			{
				//if we don't have a WASD input, we don't send, and we'll check the controller next.
				uint32_t keyCount = reading->GetKeyCount();
				reading->GetKeyState(keyCount, states);
				reading->Release();

				KeyboardCurrentRead = FromKeyboardAndMouseState(keyCount, states, mouseState.buttons, MouseXDelta,
				                                                MouseYDelta);
			}
			// AND get the gamepad... we need both inputs to check which has data.
			if (g_gameInput &&
				SUCCEEDED(g_gameInput->GetCurrentReading(GameInputKindGamepad, g_gamepad, &reading)))
			{
				if (!g_gamepad)
				{
					reading->GetDevice(&g_gamepad);
				}
				GamepadCurrentRead = GamepadState(reading);

				reading->Release();
			}
			else if (g_gamepad != nullptr)
			// if gamepad read failed but a gamepad exists, we're in a failed state.
			{
				g_gamepad->Release(); //release it, we'll reacquire it on the next pass.
				g_gamepad = nullptr;
			}

			Sent = SendNew(Sent, PriorReadingKeyboard, KeyboardCurrentRead);
			Sent = SendNew(Sent, PriorReadingGamepad, GamepadCurrentRead);
			if (GamepadCurrentRead != BlankGamepad)
			{
				Sent = SendIfWindowEdge(Sent, TickCounter, GamepadCurrentRead, sendHertzFactor);
			}
			else if (KeyboardCurrentRead != BlankKeyboard)
			{
				Sent = SendIfWindowEdge(Sent, TickCounter, KeyboardCurrentRead, sendHertzFactor);
			}
			//this check isn't needed, but removing it creates an instant maintenance hazard.
			else if (TickCounter % sendHertzFactor != 0)
			{
				Sent = SendIfWindowEdge(Sent, TickCounter, BlankGamepad, sendHertzFactor);
			}

			++GuessedInputCount;
			//this is performed even if they're null data packets. Godspeed.
			PriorReadingGamepad = GamepadCurrentRead;
			PriorReadingKeyboard = KeyboardCurrentRead;

			if (TickCounter % sampleHertz == 0)
			{
				long long now = std::chrono::steady_clock::now().time_since_epoch().count();
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Cabling hertz cycled: %lld against lsb %lld with last poll as %lld"),
					now, static_cast<long long>(lsbTime), static_cast<long long>(lastPollTime));
			}

			if (TickCounter % sendHertzFactor == 0)
			{
				Sent = false;
			}

			++TickCounter;
		}
		//if this is the case, we've looped round. rather than verifying, we'll just miss one chance to poll.
		//sequence number is still the actual arbiter, so we'll only send every 4 periods, even if we poll
		//one less or one more time.

		//modified to ensure we don't oversleep. we busy cycle if we're less than 1.1 ms out.
		lsbTime = NarrowClock::getSlicedMicrosecondNow();
		if (lsbTime <= (lastPollTime + Period) - (1.1 * (HalfStep).count()))
		{
			std::this_thread::sleep_for(HalfStep);
			lsbTime = NarrowClock::getSlicedMicrosecondNow();
		}
		if (lsbTime < lastPollTime)
		{
			lastPollTime = lsbTime;
		}
	}
	if (g_gamepad)
	{
		g_gamepad->Release();
	}
	if (g_gameInput)
	{
		g_gameInput->Release();
	}

	timeEndPeriod(1);
	GuessedInputCount = 0;
	return 0;
}

void FCabling::Exit()
{
	Cleanup();
}

void FCabling::Stop()
{
	Cleanup();
}

void FCabling::Cleanup()
{
	running = false;
}
