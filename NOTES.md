# Notes
## Feature: Ball travel Loop
- filepath of Sound added to *Global.h*
- playing of sounds is controlled by *AudioManager*
- *AudioManager::play* is usually called in updating methods of the *Game* class
- state and position of a projectile (ball) is gathered in *updateProjectiles*
- Sound should start as soon as a projectile is detached from the pedal/player

1. Main decision: a new in-memory loop voice

Rejected: add a loop flag to the existing SPlayingVoice. It would take about 10 lines, since MixFileNInN already loops by calling sf_seek. But a loop voice plays for the whole round, so it would keep every existing problem the whole time:
- It reads from disk inside the audio callback.
- It is added to m_pPlayingVoices with no lock while the audio thread removes items from it.
- There's no way to stop it, and a stale pointer is left behind when it ends.
- There's no gain or rate to change.

Chosen: a separate SLoopVoice type.
- CAudioManager keeps a fixed array of them, so no voices are created or destroyed during play.
- They read from sample data loaded into memory at startup.
- The game thread passes parameters to the audio thread through atomics.