# Technical Test – Audio Feature for Arkanoid

Nov 25, 2026 · @Lukas

## Overview

This test is based on a past programming test, that was built out by humans without AI, so you might find confusing, obfuscated or buggy code, that is by no means perfect. It is up to your own judgement to mirror style, correct, expand or rewrite sections to achieve your goal.

There are playable binaries for windows (`Arkanoid/bin/x64/Release/Arkanoid.exe)`.

There is also an existing worklog.txt that might help with understanding the last coders thought process.

Add one new audio feature, optionally with new sounds, to the existing project.

This test evaluates approach, decision-making and code quality, not feature count. A partial implementation with clear reasoning is better than several features without it.

## Objective

1. Clone the repository, then build and run the game as described in the README. Resolve any setup issues and document the changes.
2. Analyze the existing code before writing new code. Find out how audio is currently played, which thread it runs on and where game events originate.
3. Implement one new feature from the options below, or a proposal of your own. Integrate it into the existing structure and justify any refactoring.
4. Document approach, decisions and open points in `NOTES.md`.
5. Submit progress and documentation as it makes sense with meaningful commit messages to the repo directly.

## Feature Options

Select one. Each option adds a new sound to the game. Each has a minimal version that fits the time budget and room for further depth.

| Feature | Scope | Focus areas |
| --- | --- | --- |
| Ball travel loop | Add a continuous sound to the ball. Drive pitch and volume from its speed; pan it by position. | Click-free start and stop, parameter smoothing, balance against the existing impacts. |
| Paddle motion | Add a movement sound to the paddle (servo, friction, hover). Drive it from paddle velocity. | Loop handling, attack and release on quick direction changes. |
| Danger state | Add a tension layer (heartbeat, alarm) while the last ball approaches the bottom. Duck or filter the music while it is active. | Mapping game state to audio parameters, ducking envelope, integration with the existing music system. |
| Power-up capsule | Add a falling capsule randomly spawning from blocks with its own sound: a loop while it drops, a pickup sound when caught. | Spawning and stopping voices from gameplay, position tracking, voice cleanup. |
| Procedural sound | Synthesize a new sound at runtime instead of loading a sample, e.g. a laser shot or the shatter of a new brick type. | Synthesis method, CPU cost, sound quality, real-time safety. |

Own features are also welcome. Describe the feature in `NOTES.md` before starting implementation. Keep gameplay additions to the minimum the feature needs.

## Constraints

- **Time budget:** 1–2 hours. Partial results are fine.
- **Language:** C++. Document any new third-party dependency in `NOTES.md`.
- **Real-time safety:** No locks, heap allocations or file I/O on the audio thread.
- **Existing behavior:** Do not break existing gameplay or audio.
- **Assets:** Create, record or synthesize the new sound, or use license-free material. Document the source in `NOTES.md`.

## Version Control

Work as on any professional team project. The commit history is part of the evaluation.

- Committing to `main` is fine, though if you want to use branches, that's welcome.
- Commit in small, logical increments.
- Write commit messages that state what changed and why.
- Do not rewrite existing history.
- Follow the existing code conventions. Document proposed deviations in `NOTES.md`.

## AI Usage

AI tools are permitted.

- Commit all AI sessions, unedited, to `ai-sessions/`: chat exports, agent logs or workspace transcripts.
- Include sessions from every tool used.
- Be prepared to explain your changes on the branch. Knowledge of the surrounding code  is an advantage.

## Deliverables

Push the feature branch containing:

- The implementation
- `NOTES.md` in the repository root, covering:
  - Chosen feature and rationale
  - Approach and key decisions, including rejected alternatives
  - Current state and known issues
  - Next steps given more time
  - Approximate time spent
- `ai-sessions/`, if AI tools were used

Keep `NOTES.md` brief. Bullet points are sufficient.

Deadline: The final commits should be done by Friday, 27 November, 12:00 CET. Including `NOTES.md` and `ai-sessions/`. Pushing changes to the remote while you work on them would be great, so we have more time to look through the increments.

## Evaluation

| Criterion | What we look for |
| --- | --- |
| Approach | How well you found your way through the codebase. |
| Decisions | Clear reasoning behind design choices and trade-offs. |
| Audio quality | Correct, artifact-free output and real-time-safe code. |
| Code quality | Readability and fit with the existing structure. |
| Version control | Commit granularity and message quality. |
| AI usage | Critical review of AI output rather than blind acceptance. |

Feature count is not a criterion.

## Review Session

Duration: 30–45 minutes.

We will look through your delivery before the session and prepare 2-3 questions on specific implementations.

Lastly. This is not a test for a senior position. We don't expect enterprise grade code or any crazy deliveries. We hope that you have some fun with this and can apply your domain knowledge for audio, as well as show your creativity and most importantly have some fun with it.

Direct setup issues and questions about this brief to Lukas by email. Questions are not penalized ;)
