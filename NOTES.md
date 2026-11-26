# Notes
## Feature: Ball travel Loop
- filepath of Sound added to *Global.h*
- playing of sounds is controlled by *AudioManager*
- *AudioManager::play* is usually called in updating methods of the *Game* class
- state and position of a projectile (ball) is gathered in *updateProjectiles*
- Sound should start as soon as a projectile is detached from the pedal/player