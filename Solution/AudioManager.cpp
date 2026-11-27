#include "AudioManager.h"
#include "BiquadFilter.h"
#include "Global.h"
#include "RenderedObject.h"
#include <complex>

namespace Arkanoid::Audio
{
//////////////////////////////////////////////////////////////////////////////////
static int StreamCallback(
	void const* pInputBuffer,
	void* pOutputBuffer,
	long unsigned framesPerBuffer,
	PaStreamCallbackTimeInfo const* pTimeInfo,
	PaStreamCallbackFlags statusFlags,
	void* pUserData)
{
	auto const pAudioManager = static_cast<CAudioManager*>(pUserData);

	if (pOutputBuffer != nullptr)
	{
		pAudioManager->RenderAudio(pOutputBuffer);

		if (pAudioManager->ShouldFilter())
		{
			pAudioManager->FilterBuffer(pOutputBuffer);
		}

		return PaStreamCallbackResult::paContinue;
	}

	return PaStreamCallbackResult::paAbort;
}

//////////////////////////////////////////////////////////////////////////////////
CAudioManager::CAudioManager()
{ //TODO: This should all live in a general initialize
	m_numChannels = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice())->maxOutputChannels;
	m_sampleRate = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice())->defaultSampleRate;
	m_bufferLength = 2048;
	m_pOutputBuffer = new Arkanoid::Audio::SAudioBuffer(m_sampleRate, m_bufferLength, m_numChannels, nullptr);
	m_pMixer = new Arkanoid::Audio::CAudioMixer(m_sampleRate, m_bufferLength, m_numChannels);

	int index = 0;

	for (auto& pFilter : m_pFilters)
	{
		pFilter = new CBiquadFilter(EBiquadType::Lowpass, static_cast<float>(m_sampleRate));

		pFilter->ComputeCoefficients(20000, 1.0f, 0.0f);
	}

	// TODO: Making this react to loading fails has a lot of possible branches or means no music at all, so this is just not allowed to fail for now, or else we crash later on while playing. 
	for (auto pFile : asset_audio_musicTracks)
	{
		SF_INFO sfInfo{ 0 };
		m_pMusicFiles[index] = sf_open(pFile, SFM_READ, &sfInfo);
		++index;
	}
	
	m_pMusicVoice = new SPlayingVoice(
		m_pMusicFiles[0],
		2,
		44100,
		false,
		Vec2D(0.0f, 0.0f));
	
	bool loopSampleLoaded = LoadLoopSample(asset_audio_ballTravel01);
	
	Pa_OpenDefaultStream(
		&m_pStream,
		0,
		m_numChannels,
		paFloat32,
		m_sampleRate, // System sampleRate
		m_bufferLength,
		StreamCallback,
		this);

	Pa_StartStream(m_pStream);
}

//////////////////////////////////////////////////////////////////////////////////
CAudioManager::~CAudioManager()
{
	Pa_StopStream(m_pStream); //wait for stream to have stopped so we can cleanly dealloc all used memory
	Pa_CloseStream(m_pStream);

	m_pPlayingVoices.clear();

	for (auto& pFilter : m_pFilters)
	{
		delete pFilter;
	}

	delete m_pOutputBuffer;
	delete m_pMixer;
}

//////////////////////////////////////////////////////////////////////////////////
SPlayingVoice* CAudioManager::Play(char const* filePath, bool const positioned/* = false*/, Vec2D position/* = Vec2D(0.0f, 0.0f)*/)
{
	SF_INFO sfInfo{ 0 };
	SNDFILE* const pFile = sf_open(filePath, SFM_READ, &sfInfo);
	
	if (pFile != nullptr)
	{
		SPlayingVoice* pPlayingVoice =
			new SPlayingVoice(
				pFile,
				static_cast<unsigned char const>(sfInfo.channels),
				static_cast<unsigned short const>(sfInfo.samplerate),
				positioned,
				position);

		m_pPlayingVoices.push_back(std::shared_ptr<SPlayingVoice>(pPlayingVoice));

		return pPlayingVoice;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////
bool CAudioManager::ShouldFilter()
{
	return (m_filterAmount != 0.0f);
}

//////////////////////////////////////////////////////////////////////////////////
void CAudioManager::FilterBuffer(void* pOutputBuffer)
{
	float const filterAmount = m_filterAmount.load();
	m_filterSwitch = !m_filterSwitch;

	CBiquadFilter* pFilterDominantL = nullptr;
	CBiquadFilter* pFilterDominantR = nullptr;
	CBiquadFilter* pFilterResidualL = nullptr;
	CBiquadFilter* pFilterResidualR = nullptr;

	if (m_filterSwitch)
	{
		pFilterDominantL = m_pFilters[0];
		pFilterDominantR = m_pFilters[1];

		pFilterResidualL = m_pFilters[2];
		pFilterResidualR = m_pFilters[3];
	}
	else
	{
		pFilterDominantL = m_pFilters[2];
		pFilterDominantR = m_pFilters[3];

		pFilterResidualL = m_pFilters[0];
		pFilterResidualR = m_pFilters[1];
	}

	float* pBuffer = static_cast<float*>(pOutputBuffer);

	// We are only filtering two channels L/R, even though the rest of the audio system works with surround theoretically.
	if (m_oldFilterAmount != m_filterAmount) // fade
	{
		pFilterDominantL->ComputeCoefficients(20000 - static_cast<int>(19500 * m_filterAmount), 1.0f, 0.0f);
		pFilterDominantR->ComputeCoefficients(20000 - static_cast<int>(19500 * m_filterAmount), 1.0f, 0.0f);

		float const fadeFactor = 1.0f / static_cast<float>(m_bufferLength);

		for (int i = 0; i < m_bufferLength; ++i)
		{
			*(pBuffer)++ = ((i * fadeFactor) * pFilterDominantL->ProcessSample(*pBuffer)) + ((1.0f - (i * fadeFactor)) * pFilterResidualL->ProcessSample(*pBuffer));
			*(pBuffer)++ = ((i * fadeFactor) * pFilterDominantR->ProcessSample(*pBuffer)) + ((1.0f - (i * fadeFactor)) * pFilterResidualR->ProcessSample(*pBuffer));

			pBuffer += (m_numChannels - 2);
		}
	}
	else
	{
		for (int i = 0; i < m_bufferLength; ++i)
		{
			*(pBuffer)++ = pFilterDominantL->ProcessSample(*pBuffer);
			*(pBuffer)++ = pFilterDominantR->ProcessSample(*pBuffer);

			pBuffer += (m_numChannels - 2);
		}
	}

	m_oldFilterAmount = m_filterAmount;
}

//////////////////////////////////////////////////////////////////////////////////
void CAudioManager::SetFilterAmount(float const amount)
{
	m_filterAmount.store(amount);
}

//////////////////////////////////////////////////////////////////////////////////
void CAudioManager::RenderAudio(void* pOutputBuffer)
{
	std::fill(static_cast<float*>(pOutputBuffer), static_cast<float*>(pOutputBuffer) + m_bufferLength * m_numChannels, 0.0f);

	// Play music
	{
		m_pOutputBuffer->pData = pOutputBuffer;

		if (m_playing != m_lastPlayed)
		{
			m_lastPlayed = m_playing;

			sf_seek(m_pMusicFiles[static_cast<int>(m_playing)], 0, SF_SEEK_SET);

			m_pMixer->MixFileNInN(m_pOutputBuffer, m_pMusicVoice->pFile, m_pMusicVoice->numChannels, g_musicVolume, true, true, false);                      // A flock of wild booleans appeared
			m_pMixer->MixFileNInN(m_pOutputBuffer, m_pMusicFiles[static_cast<int>(m_playing)], m_pMusicVoice->numChannels, g_musicVolume, true, true, true); // This feature is a bit hacky, but will stay as is.

			m_pMusicVoice->pFile = m_pMusicFiles[static_cast<int>(m_playing)];
		}
		else
		{
			m_pMixer->MixFileNInN(m_pOutputBuffer, m_pMusicVoice->pFile, m_pMusicVoice->numChannels, g_musicVolume, true);
		}
	}

	if (!m_pPlayingVoices.empty())
	{
		m_pOutputBuffer->pData = pOutputBuffer;

		for (int i = static_cast<int>(m_pPlayingVoices.size() - 1); i >= 0; --i)
		{
			auto pPlayingVoice = m_pPlayingVoices[i];

			if ((pPlayingVoice != nullptr) && (pPlayingVoice->isValid))
			{
				if (pPlayingVoice->positioned)
				{
					float const horizontalCenter = ((static_cast<float>(Arkanoid::Game::g_borderRight) - static_cast<float>(Arkanoid::Game::g_borderLeft)) * 0.5f);
					float const posX = (pPlayingVoice->position.x) / horizontalCenter;
					float const posY = (static_cast<float>(Arkanoid::Game::g_borderBottom) - pPlayingVoice->position.y) / static_cast<float>(Arkanoid::Game::g_borderBottom);

					float const distance = std::sqrtf(posX * posX + posY * posY);
					float const scale = 1.0f / distance;
					Vec2D pos;
					pos.x = -posX * scale;
					pos.y = posY * scale;
					
					if (m_pMixer->MixFile1InNPositional(m_pOutputBuffer, pPlayingVoice->pFile, pPlayingVoice->numChannels, pos, distance) != m_bufferLength)
					{
						m_pPlayingVoices.erase(m_pPlayingVoices.begin() + i);
					}
				}
				else
				{
					if (m_pMixer->MixFileNInN(m_pOutputBuffer, pPlayingVoice->pFile, pPlayingVoice->numChannels) != m_bufferLength)
					{
						m_pPlayingVoices.erase(m_pPlayingVoices.begin() + i);
					}
				}
			}
			else
			{
				m_pPlayingVoices.erase(m_pPlayingVoices.begin() + i);
			}
		}
	}

	ELoopState const state = m_loopVoice.state.load();

	if (state != ELoopState::Free)
	{
		if ((state == ELoopState::Playing) && (m_loopVoice.lastSeenState == ELoopState::Free))
		{
			m_loopVoice.readPos = 0.0;
			m_loopVoice.gain = 0.0f;
		}

		float const targetGain = (state == ELoopState::Stopping) ? 0.0f : m_loopVoice.targetGain.load();

		// ... mix the buffer, ramping m_loopVoice.gain towards targetGain per sample ...

		if ((state == ELoopState::Stopping) && (m_loopVoice.gain <= 0.0001f))
		{
			ELoopState expected = ELoopState::Stopping;
			m_loopVoice.state.compare_exchange_strong(expected, ELoopState::Free);
		}
	}

	m_loopVoice.lastSeenState = m_loopVoice.state.load();
}

//////////////////////////////////////////////////////////////////////////////////
bool CAudioManager::UpdatePosition(SPlayingVoice* pPlayingVoice_, Vec2D const& pos)
{
	bool success = false;

	for (std::shared_ptr<SPlayingVoice> const& pPlayingVoice : m_pPlayingVoices)
	{
		if (&*pPlayingVoice == pPlayingVoice_ && pPlayingVoice->isValid)
		{
			pPlayingVoice->position.x = pos.x;
			pPlayingVoice->position.y = pos.y;
			success = true;
		}
	}

	return success;
}

//////////////////////////////////////////////////////////////////////////////////
void CAudioManager::SetMusic(EMusic const music)
{
	m_playing = music;
}

bool CAudioManager::StartLoop(Vec2D const& position, float const gain,  float const rate)
{
	//m_loopVoice.positioned = positioned;
	m_loopVoice.state.store(ELoopState::Playing);
	m_loopVoice.lastSeenState = ELoopState::Playing;
	
	if (!m_loopVoice.isValid || (m_loopVoice.pSamples == nullptr))
    {
            return false; // Sample failed to load: no loop, but gameplay continues.
    }

    m_loopVoice.position.x.store(position.x);
    m_loopVoice.position.y.store(position.y);
    m_loopVoice.targetGain.store(gain);
    m_loopVoice.targetRate.store(rate);
	m_loopVoice.state.store(ELoopState::Playing);
	
	return true;
}

void CAudioManager::StopLoop()
{
	ELoopState expected = ELoopState::Playing;
	m_loopVoice.state.compare_exchange_strong(expected, ELoopState::Stopping);
}

bool CAudioManager::LoadLoopSample(char const* filePath)
{
	SF_INFO sfInfo{ 0 };
	SNDFILE* const pFile = sf_open(filePath, SFM_READ, &sfInfo);
	
	if (pFile == nullptr)
	{
		return false;
	}

	sf_count_t const numFrames = sfInfo.frames;
	int const numFileChannels = sfInfo.channels;

	if ((numFrames <= 0) || (numFileChannels <= 0))
	{
		sf_close(pFile);
		return false;
	}
	
	// Read the whole file interleaved, then downmix to mono so the loop can be panned like the other positioned voices.
	std::vector<float> interleaved(static_cast<size_t>(numFrames * numFileChannels));
	sf_count_t const framesRead = sf_readf_float(pFile, interleaved.data(), numFrames);
	sf_close(pFile);

	if (framesRead <= 0)
	{
		return false;
	}

	m_loopSampleData.assign(static_cast<size_t>(framesRead), 0.0f);
	float const channelScale = 1.0f / static_cast<float>(numFileChannels);

	for (sf_count_t frame = 0; frame < framesRead; ++frame)
	{
		float sum = 0.0f;

		for (int channel = 0; channel < numFileChannels; ++channel)
		{
			sum += interleaved[static_cast<size_t>(frame * numFileChannels + channel)];
		}

		m_loopSampleData[static_cast<size_t>(frame)] = sum * channelScale;
	}

	m_loopVoice.pSamples   = m_loopSampleData.data();
	m_loopVoice.numFrames  = framesRead;
	m_loopVoice.sampleRate = sfInfo.samplerate;
	m_loopVoice.positioned = true;
	m_loopVoice.isValid    = true;
	m_loopVoice.state.store(ELoopState::Free);

	return true;
}
} //Arkanoid::Audio
