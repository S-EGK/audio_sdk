# Architecture Notes

## Primary Decisions

- `PipeWire` is the primary real-time backend for v1.
- The public SDK surface stays backend-neutral.
- `C++` owns real-time work; Python bindings remain control-plane focused.
- Applications provide JSON config paths rather than relying on global daemon state.

## Layering

The repository should evolve in four layers:

1. `Public API`
   - `AudioManager`, device descriptors, event types, and config types.
2. `Core orchestration`
   - policy selection, fallback routing, recording lifecycle, stream ownership, logging.
3. `Backend adapters`
   - PipeWire first, with room for future PulseAudio, ALSA, or JACK integration where it makes sense.
4. `Bindings and integrations`
   - Python first, DDS / CycloneDDS and ROS2-oriented integration later.

## Config Recommendation

Use two config files:

- a checked-in app default, such as `config/audio_sdk.json`
- an optional writable state overlay outside source control

The SDK should merge them in order and expose the resolved configuration to the caller.

## Real-Time Constraint

Strict synchronization is a product requirement, not an optimization. That means:

- no Python in the audio callback path
- no blocking file I/O in stream callbacks
- no hidden backend-specific timing assumptions in the public API

## First End-to-End Scenario

The first fully implemented scenario should be:

1. load a JSON config
2. select mic and speaker using explicit IDs or policy hints
3. start recording to WAV
4. stop recording cleanly
5. play the saved recording through the selected speaker
6. delete the recording on request

