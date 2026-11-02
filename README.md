# SkizoDroid Live V0.1

First working prototype of the future Tricky Live core, built around the SkizoDroid live use case.

## Current prototype

- JUCE + CMake
- standalone desktop app
- 4 audio-loop tracks: Drum / Bass / Synth / Noise
- WAV/AIFF loading
- BPM control
- Play / Stop
- 4 scene buttons
- scene changes are queued to the next bar while playing
- all loaded loops restart together when a scene starts

## Next milestone: V0.2

- each scene stores its own loop for every track
- 4x4 or 8x8 Pattern Grid
- loop/hold scene state
- Continue / Next Scene
- per-track mute, solo and volume
- seamless looping
- proper sample-accurate quantized transport clock
- project save/load

## Architecture direction

The Live engine should not hard-code "audio loop" as the only source. A track source will later be able to host:

- Audio Loop
- Drum Mutator
- Bass Mutator
- Drone
- Sampler
- Synth
- External MIDI

This prototype is intentionally small, but it is the beginning of the reusable Tricky Live core rather than a throwaway SkizoDroid-only app.
