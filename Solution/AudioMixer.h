#pragma once
#include "AudioBuffer.h"
#include "Global.h"
#include "LoopVoice.h"
#include <sndfile.h>

namespace Arkanoid::Audio
{
struct SAudioBuffer;

constexpr int g_maxPositionalChannels = 6;

// Per-channel gains for a positioned mono source, in output channel order (FL, FR, RL, RR, SL, SR).
struct SSpeakerGains final
{
	float gains[g_maxPositionalChannels]{ 0.0f };
	int   numChannels = 0; // Channels to write: 2, 4 or 6. 0 means the output can't be positioned (mono).
};

class CAudioMixer
{
public:
	CAudioMixer() = delete;

	explicit CAudioMixer(double const sampleRate_, unsigned int const bufferLength_, int const numChannels_)
		: m_pBufferData(malloc(sizeof(float)* bufferLength_* numChannels_))
		, m_pMixBuffer(new SAudioBuffer(sampleRate_, bufferLength_, numChannels_, m_pBufferData))
	{}

	~CAudioMixer()
	{
		delete m_pMixBuffer;
		free(m_pBufferData);
	}

	sf_count_t MixFileNInN(SAudioBuffer const* const pOutBuffer, SNDFILE* const pFile, int const numFileChannels, float const volume = 1.0f, bool const loop = false, bool const fade = false, bool const fadeIn = false);
	sf_count_t MixFile1InNPositional(SAudioBuffer const* const pOutBuffer, SNDFILE* const pFile, int const numFileChannels, Vec2D const relativePosition, float const attenuationDistance = 0.0f);

	// Real-time safe: reads only the voice's preloaded samples, no file I/O or allocation.
	// relativePosition is in game pixels, x relative to the paddle centre (as passed to StartLoop).
	void MixLoop(SAudioBuffer const* const pOutBuffer, SLoopVoice& voice, float const targetGain, float const targetRate, Vec2D const relativePosition, bool const snapToTarget);

	// Positioning, shared by every positioned voice. Both are pure functions and real-time safe.
	// Turns a paddle-relative game position into a unit listener direction and a distance factor.
	static void          ComputeListenerDirection(Vec2D const relativePosition, Vec2D& outDirection, float& outDistance);
	// Speaker gains (panning and distance attenuation) for a mono source in that direction.
	static SSpeakerGains ComputeSpeakerGains(Vec2D const direction, float const distanceFactor, int const numBufferChannels);

	void*         m_pBufferData;
	SAudioBuffer* m_pMixBuffer;
};
} // Arkanoid::Audio
