#include "AudioMixer.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <xutility>
#include <algorithm>

namespace Arkanoid::Audio
{
static_assert(sizeof(SLoopVoice::channelGains) / sizeof(float) == g_maxPositionalChannels, "SLoopVoice::channelGains must hold g_maxPositionalChannels gains");

//////////////////////////////////////////////////////////////////////////////////
sf_count_t CAudioMixer::MixFileNInN(SAudioBuffer const* const pOutBuffer, SNDFILE* const pFile, int const numFileChannels, float const volume, bool const loop, bool const fade, bool const fadeIn)
{
	std::fill(static_cast<float*>(m_pMixBuffer->pData), static_cast<float*>(m_pMixBuffer->pData) + (m_pMixBuffer->bufferLength * m_pMixBuffer->numChannels), 0.0f);

	int const numBufferChannels = pOutBuffer->numChannels;
	int const channelsToWrite =
		(pOutBuffer->numChannels < numFileChannels)
		? pOutBuffer->numChannels
		: numFileChannels;

	auto const requestedSamples = static_cast<sf_count_t>(pOutBuffer->bufferLength);
	auto pStreamBuffer = static_cast<float*>(pOutBuffer->pData);
	auto const pMixBuffer = static_cast<float*>(m_pMixBuffer->pData);

	sf_count_t numFramesRead = 0;
	numFramesRead = sf_readf_float(pFile, pMixBuffer, requestedSamples);

	if(loop)
	{
		while (numFramesRead != requestedSamples)
		{
			sf_seek(pFile, 0, SF_SEEK_SET);
			numFramesRead += sf_readf_float(pFile, (pMixBuffer + numFramesRead), (requestedSamples - numFramesRead));
		}
	}

	if (fade)
	{
		float const fadeFactor = 1.0f / static_cast<float>(requestedSamples);

		if (fadeIn)
		{
			for (sf_count_t i = 0; i < numFramesRead; ++i)
			{
				for (int j = 0; j < channelsToWrite; ++j)
				{
					*pStreamBuffer++ += (pMixBuffer[i * channelsToWrite + j] * fadeFactor * i) * volume;
				}

				pStreamBuffer += (numBufferChannels - channelsToWrite);
			}
		}
		else
		{
			for (sf_count_t i = 0; i < numFramesRead; ++i)
			{
				for (int j = 0; j < channelsToWrite; ++j)
				{
					*pStreamBuffer++ += (pMixBuffer[i * channelsToWrite + j] * fadeFactor * (requestedSamples - i)) * volume;
				}

				pStreamBuffer += (numBufferChannels - channelsToWrite);
			}
		}
	}
	else
	{
		for (sf_count_t i = 0; i < numFramesRead; ++i)
		{
			for (int j = 0; j < channelsToWrite; ++j)
			{
				*pStreamBuffer++ += pMixBuffer[i * channelsToWrite + j] * volume;
			}

			pStreamBuffer += (numBufferChannels - channelsToWrite);
		}
	}

	return numFramesRead;
}

//////////////////////////////////////////////////////////////////////////////////
sf_count_t CAudioMixer::MixFile1InNPositional(SAudioBuffer const* const pOutBuffer, SNDFILE* const pFile, int const numFileChannels, Vec2D const relativeRotation, float const distanceFactor)
{
	std::fill(static_cast<float*>(m_pMixBuffer->pData), static_cast<float*>(m_pMixBuffer->pData) + ((m_pMixBuffer->bufferLength) * m_pMixBuffer->numChannels), 0.0f);

	SSpeakerGains const speakerGains = ComputeSpeakerGains(relativeRotation, distanceFactor, pOutBuffer->numChannels);

	// We expect at least 2 channels to mix positional audio
	if (speakerGains.numChannels == 0)
	{
		return 0;
	}

	int const numBufferChannels = pOutBuffer->numChannels;
	auto const requestedSamples = static_cast<sf_count_t>(pOutBuffer->bufferLength);
	auto pStreamBuffer = static_cast<float*>(pOutBuffer->pData);
	auto const pMixBuffer = static_cast<float*>(m_pMixBuffer->pData);

	sf_count_t const numFramesRead = sf_readf_float(pFile, pMixBuffer, requestedSamples);

	for (sf_count_t i = 0; i < numFramesRead; ++i)
	{
		float const sample = pMixBuffer[i * numFileChannels];

		for (int j = 0; j < speakerGains.numChannels; ++j)
		{
			*pStreamBuffer++ += sample * speakerGains.gains[j];
		}

		pStreamBuffer += (numBufferChannels - speakerGains.numChannels);
	}

	return numFramesRead;
}

//////////////////////////////////////////////////////////////////////////////////
void CAudioMixer::MixLoop(SAudioBuffer const* const pOutBuffer, SLoopVoice& voice, float const targetGain, float const targetRate, Vec2D const relativePosition, bool const snapToTarget)
{
	if (!voice.isValid || (voice.pSamples == nullptr) || (voice.numFrames < 2) || (voice.sampleRate <= 0))
	{
		return;
	}

	Vec2D direction(0.0f, 1.0f);
	float distance = 0.0f;
	ComputeListenerDirection(relativePosition, direction, distance);
	SSpeakerGains const speakerGains = ComputeSpeakerGains(direction, distance, pOutBuffer->numChannels);

	if (speakerGains.numChannels == 0)
	{
		voice.gain = 0.0f; // Nothing can be heard on a mono output; let a stopping voice be freed.
		return;
	}

	auto const outputSampleRate = static_cast<float>(pOutBuffer->sampleRate);
	float const gainStep = 1.0f / (g_loopFadeSeconds * outputSampleRate);      // Linear fade, so a fade-out reaches exactly 0.
	float const smoothing = 1.0f - expf(-1.0f / (0.01f * outputSampleRate));  // ~10 ms one-pole for rate and panning.

	if (snapToTarget)
	{
		voice.rate = targetRate;

		for (int j = 0; j < g_maxPositionalChannels; ++j)
		{
			voice.channelGains[j] = speakerGains.gains[j];
		}
	}

	// The file's sample rate can differ from the device's.
	double const sampleRateRatio = static_cast<double>(voice.sampleRate) / pOutBuffer->sampleRate;
	auto const numFrames = static_cast<double>(voice.numFrames);
	int const numBufferChannels = pOutBuffer->numChannels;
	auto pStreamBuffer = static_cast<float*>(pOutBuffer->pData);

	for (unsigned int i = 0; i < pOutBuffer->bufferLength; ++i)
	{
		if (voice.gain < targetGain)
		{
			voice.gain = std::min(voice.gain + gainStep, targetGain);
		}
		else if (voice.gain > targetGain)
		{
			voice.gain = std::max(voice.gain - gainStep, targetGain);
		}

		voice.rate += (targetRate - voice.rate) * smoothing;

		// Linear interpolation between neighbouring frames; after the last frame it wraps to the first.
		auto const index0 = static_cast<sf_count_t>(voice.readPos);
		sf_count_t const index1 = ((index0 + 1) < voice.numFrames) ? (index0 + 1) : 0;
		float const fraction = static_cast<float>(voice.readPos - static_cast<double>(index0));
		float const sample = (voice.pSamples[index0] + ((voice.pSamples[index1] - voice.pSamples[index0]) * fraction)) * voice.gain;

		for (int j = 0; j < speakerGains.numChannels; ++j)
		{
			voice.channelGains[j] += (speakerGains.gains[j] - voice.channelGains[j]) * smoothing;
			*pStreamBuffer++ += sample * voice.channelGains[j];
		}

		pStreamBuffer += (numBufferChannels - speakerGains.numChannels);

		voice.readPos += static_cast<double>(voice.rate) * sampleRateRatio;

		while (voice.readPos >= numFrames)
		{
			voice.readPos -= numFrames;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////
void CAudioMixer::ComputeListenerDirection(Vec2D const relativePosition, Vec2D& outDirection, float& outDistance)
{
	float const horizontalCenter = ((static_cast<float>(Arkanoid::Game::g_borderRight) - static_cast<float>(Arkanoid::Game::g_borderLeft)) * 0.5f);
	float const posX = relativePosition.x / horizontalCenter;
	float const posY = (static_cast<float>(Arkanoid::Game::g_borderBottom) - relativePosition.y) / static_cast<float>(Arkanoid::Game::g_borderBottom);

	float const distance = sqrtf(posX * posX + posY * posY);

	// A source right on the listener has no direction: treat it as straight ahead and unattenuated instead of dividing by zero.
	if (distance <= 0.0001f)
	{
		outDirection = Vec2D(0.0f, 1.0f);
		outDistance = 0.0f;
		return;
	}

	float const scale = 1.0f / distance;
	outDirection = Vec2D(-posX * scale, posY * scale);
	outDistance = distance;
}

//////////////////////////////////////////////////////////////////////////////////
SSpeakerGains CAudioMixer::ComputeSpeakerGains(Vec2D const direction, float const distanceFactor, int const numBufferChannels)
{
	SSpeakerGains speakerGains;

	// We expect at least 2 channels to mix positional audio
	if (numBufferChannels < 2)
	{
		return speakerGains;
	}

	// TODO: there should be a global for this.
	float volumeAttenuation = 1.0f;
	if (distanceFactor != 0.0f)
	{
		volumeAttenuation = 1.0f - (distanceFactor * 0.6f);

		if (volumeAttenuation < 0.0f) volumeAttenuation = 0.0f;
		if (volumeAttenuation > 1.0f) volumeAttenuation = 1.0f;
	}

	float const directionLength = sqrtf(direction.x * direction.x + direction.y * direction.y);
	float const listenerCosine =
		(directionLength > 0.0f)
		? std::clamp(direction.y / directionLength, -1.0f, 1.0f) // clamp: rounding must not push acosf out of its domain
		: 1.0f;

	// Differentiate between Stereo(up to 2.1) and surround setups
	if (numBufferChannels <= 3)
	{
		float volumeRight = 1.0f;
		float volumeLeft = 1.0f;

		if (direction.x < 0.0f)
		{
			float directionFactor = fabsf(listenerCosine);
			directionFactor = (powf(g_euler, (2.0f * (directionFactor - 1.0f))) * directionFactor);
			volumeRight = 1.0f - (directionFactor * 0.3f);
			volumeLeft = 0.1f + (directionFactor * 0.6f);
		}
		else
		{
			float directionFactor = fabsf(listenerCosine);
			directionFactor = (powf(g_euler, (2.0f * (directionFactor - 1.0f))) * directionFactor);
			volumeLeft = 1.0f - (directionFactor * 0.3f);
			volumeRight = 0.1f + (directionFactor * 0.6f);
		}

		speakerGains.gains[0] = volumeLeft * volumeAttenuation;
		speakerGains.gains[1] = volumeRight * volumeAttenuation;
		speakerGains.numChannels = 2;

		return speakerGains;
	}

	// panning over 3 Speakers with 1 primary and 2 secondary channels
	// Assuming the most common setups for now without center: Quad and 6 Speaker surround
	float listenerAngle = (acosf(listenerCosine) * (180.0f / static_cast<float>(M_PI)));

	listenerAngle =
		(direction.x > 0.0f)
		? listenerAngle
		: 360.0f - listenerAngle;

	int const channelsToWrite = (numBufferChannels > 7) ? 6 : 4;

	float quadrantProxy = 0.0f;
	float quadrantFine = listenerAngle / (360.0f / channelsToWrite);
	quadrantFine = modff(quadrantFine, &quadrantProxy);
	int const quadrant = static_cast<int>(quadrantProxy);
	float const quadrantFinePrimary = (quadrantFine < 0.5f) ? quadrantFine * 2.0f : 1.0f - (quadrantFine - 0.5f) * 1.0f;

	float const volumeSecondaryLeft = 0.7f * quadrantFine;
	float const volumePrimary = 0.7f + (0.25f * quadrantFinePrimary);
	float const volumeSecondaryRight = 0.7f * (1.0f - quadrantFine);

	float volumeFrontLeft = 0.0f;
	float volumeFrontRight = 0.0f;
	float volumeRearLeft = 0.0f;
	float volumeRearRight = 0.0f;
	float volumeSideLeft = 0.0f;
	float volumeSideRight = 0.0f;

	switch (quadrant)
	{
	case 0:
		{
			if (channelsToWrite == 4)
			{
				volumeFrontLeft = volumePrimary;
				volumeFrontRight = volumeSecondaryRight;
				volumeRearLeft = volumeSecondaryLeft;
				volumeRearRight = 0.0f;
			}
			else
			{
				volumeFrontLeft = volumePrimary;
				volumeFrontRight = volumeSecondaryRight;
				volumeRearLeft = 0.0f;
				volumeRearRight = 0.0f;
				volumeSideLeft = volumeSecondaryLeft;
				volumeSideRight = 0.0f;
			}

			break;
		}
	case 1:
		{
			if (channelsToWrite == 4)
			{
				volumeFrontLeft = volumeSecondaryRight;
				volumeFrontRight = 0.0f;
				volumeRearLeft = volumePrimary;
				volumeRearRight = volumeSecondaryLeft;
			}
			else
			{
				volumeFrontLeft = volumeSecondaryRight;
				volumeFrontRight = 0.0f;
				volumeRearLeft = volumeSecondaryLeft;
				volumeRearRight = 0.0f;
				volumeSideLeft = volumePrimary;
				volumeSideRight = 0.0f;
			}

			break;
		}
	case 2:
		{
			if (channelsToWrite == 4)
			{
				volumeFrontLeft = 0.0f;
				volumeFrontRight = volumeSecondaryLeft;
				volumeRearLeft = volumeSecondaryRight;
				volumeRearRight = volumePrimary;
			}
			else
			{
				volumeFrontLeft = 0.0f;
				volumeFrontRight = 0.0f;
				volumeRearLeft = volumePrimary;
				volumeRearRight = volumeSecondaryLeft;
				volumeSideLeft = volumeSecondaryRight;
				volumeSideRight = 0.0f;
			}

			break;
		}
	case 3:
		{
			if (channelsToWrite == 4)
			{
				volumeFrontLeft = volumeSecondaryLeft;
				volumeFrontRight = volumePrimary;
				volumeRearLeft = 0.0f;
				volumeRearRight = volumeSecondaryRight;
			}
			else
			{
				volumeFrontLeft = 0.0f;
				volumeFrontRight = 0.0f;
				volumeRearLeft = volumeSecondaryRight;
				volumeRearRight = volumePrimary;
				volumeSideLeft = 0.0f;
				volumeSideRight = volumeSecondaryLeft;
			}

			break;
		}
	case 4:
		{
			volumeFrontLeft = 0.0f;
			volumeFrontRight = volumeSecondaryLeft;
			volumeRearLeft = 0.0f;
			volumeRearRight = volumeSecondaryRight;
			volumeSideLeft = 0.0f;
			volumeSideRight = volumePrimary;

			break;
		}
	case 5:
		{
			volumeFrontLeft = volumeSecondaryLeft;
			volumeFrontRight = volumePrimary;
			volumeRearLeft = 0.0f;
			volumeRearRight = 0.0f;
			volumeSideLeft = 0.0f;
			volumeSideRight = volumeSecondaryRight;

			break;
		}
	default:
		{
			break;
		}
	}

	speakerGains.gains[0] = volumeFrontLeft * volumeAttenuation;
	speakerGains.gains[1] = volumeFrontRight * volumeAttenuation;
	speakerGains.gains[2] = volumeRearLeft * volumeAttenuation;
	speakerGains.gains[3] = volumeRearRight * volumeAttenuation;
	speakerGains.gains[4] = volumeSideLeft * volumeAttenuation;
	speakerGains.gains[5] = volumeSideRight * volumeAttenuation;
	speakerGains.numChannels = channelsToWrite;

	return speakerGains;
}
} // Arkanoid::Audio
