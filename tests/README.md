# Test layout

Focused tests live under `app`, `game`, `render`, `audio`, and `platform`, matching
the production owner. A factory used to create a fixture does not change the
owner of the behavior under test. Cross-area flows and lifecycle sessions live
under `integration`; retained source comparisons live under `parity`.

Shared case data remains in `fixtures`, common test helpers live in `support`,
and command-line CMake checks live in `cli`. Existing target/case names, command
arguments, environment, time limits and regression conditions are preserved;
source/script paths and build working directories follow the new layout.

`flight_pixel_trace.cpp` stays in `parity` because one executable drives several
pixel/terrain cases. `audio/noise_clock_test.cpp` and `voice_envelope_test.cpp`
test focused aspects of `src/audio/voice_synth.cpp`; `voice_synth_test.cpp` covers
the wider synthesizer. These names retain the distinction without inventing
production modules just to match a test filename.

Game2–8 integration tests share button presses and bounded single-update stepping
through `support/flight_session_steps.hpp`. Their routes, player handoffs, fuel,
checkpoint and terminal assertions remain separate. No test-only game state
injection or shortened session is introduced.
