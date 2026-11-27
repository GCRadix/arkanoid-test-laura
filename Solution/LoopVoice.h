#pragma once
#include <atomic>
#include <sndfile.h>

#include "Global.h"

namespace Arkanoid::Audio
{
enum class ELoopState
{
	Free,     // Not playing; the audio thread skips it.
	Playing,  // Set by the game thread (StartLoop).
	Stopping, // Set by the game thread (StopLoop); the audio thread fades out, then sets Free.
};

// A looping voice that plays from preloaded mono sample data. It is never created or destroyed during play.
// The game thread writes only the atomics; every other field after the sample data belongs to the audio thread.
struct SLoopVoice final
{
	// Sample data, written once before the stream starts (CAudioManager::LoadLoopSample).
	float const* pSamples = nullptr;
	sf_count_t   numFrames = 0;
	int          sampleRate = 0;
	bool         isValid = false;

	// Game thread -> audio thread.
	Arkanoid::AtomicVec2D   position;
	std::atomic<ELoopState> state{ ELoopState::Free };
	std::atomic<float>      targetGain{ 0.0f };
	std::atomic<float>      targetRate{ 1.0f };

	// Audio thread only.
	ELoopState lastSeenState = ELoopState::Free;
	double     readPos = 0.0;
	float      gain = 0.0f;
	float      rate = 1.0f;
	float      channelGains[6]{ 0.0f }; // Smoothed speaker gains (FL, FR, RL, RR, SL, SR); size matches g_maxPositionalChannels.
};
} // Arkanoid::Audio
