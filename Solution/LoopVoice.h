#pragma once
#include <atomic>
#include <sndfile.h>

#include "Global.h"

namespace Arkanoid::Audio
{
    enum class ELoopState { Free, Playing, Stopping };

    struct SLoopVoice
    {
       /* explicit SLoopVoice(unsigned char const numFileChannels_, unsigned short const fileSampleRate_, bool const positioned_ = false, Vec2D const& position_ = Vec2D(0.0f, 0.0f))
            : numChannels(numFileChannels_)
            , sampleRate(fileSampleRate_)
            , positioned(positioned_)
            , position(position_.x, position_.y)
            , isValid(true)
        {}*/
    
        float const* pSamples = nullptr;
        sf_count_t           numFrames = 0;
        int                  sampleRate = 0;
       // unsigned char const  numChannels;

        Arkanoid::AtomicVec2D   position;
        bool                    positioned;
        bool                    isValid;

        std::atomic<ELoopState> state{ ELoopState::Free };
        std::atomic<float>      targetGain{ 0.0f };
        std::atomic<float>      targetRate{ 1.0f };

        ELoopState lastSeenState = ELoopState::Free;
        double     readPos = 0.0;
        float      fade = 0.0f;
        float      gain = 0.0f;
        float      rate = 1.0f;
    };
}