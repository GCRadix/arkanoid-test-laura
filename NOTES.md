# Notes
## Feature: Ball travel Loop
- filepath of Sound added to *Global.h*
- playing of sounds is controlled by *AudioManager*
- *AudioManager::play* is usually called in updating methods of the *Game* class
- state and position of a projectile (ball) is gathered in *updateProjectiles*
- Sound should start as soon as a projectile is detached from the pedal/player

1. Main decision: a new loop voice instead of using the extending PlayingVoice

- i thought about just modifying SPlayingVoice, with a loop flag, but I chose a dedicated LoopVoice to load the Samples to loop at startup and be able to control gain, rate, starting/stopping of loop
- no reading from disk during playback => less unpredictable behaviour
- the game thread passes parameters to the audio thread through atomics.

- the AudioManager starts the loop as soon as there are projectiles moving in UpdateProjectiles
- the LoopsVoice position is getting updated once per frame in UpdateProjectiles as well
- when a Projectile is erased from m_pProjectile indicating the projectile isn't in the game anymore

2. Secondary decision: Since the method MixFile1InPositional in *AudioMixer* already implements positioning I decided to implement helper functions that can be used both in MixFile1InPositional and the new mixing function to set speaker gains according to a given speaker amount and listener direction

3. MixLoop in AudioMixer
- to ramp gain for fading between loops a gainStep is being calculated
- new gain for the loopvoice is being calculated according to a targetGain and the gainStep
- a sample is linearily interpolated between a current sample and the next one based on the rate
- rate changes drive the sound's pitch
- the read position gets updated for the next loop
- if it exceeds the number of samples in the sound, it will be reseted by -numFrames


approximate time to implement all these features: ~2h