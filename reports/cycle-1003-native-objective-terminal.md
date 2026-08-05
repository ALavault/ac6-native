# Cycle 1003 — native objective terminal transition

Date: 2026-08-05

## Change

`MissionExecution::tick` now closes the native mission HSM automatically after
the gameplay update when at least one objective exists and every required
objective is `Complete`. It dispatches the existing `EventType::Complete`, so
campaign progression and all existing preconditions remain in force. Player
destruction and the configured failure tick are handled first and therefore
win an exact same-tick conflict.

No Mission 1-specific branch, guest write, generated C++ change, renderer hook,
or retail asset was added.

## Regression contract

The multi-service fixture completes its required objective, advances one fixed
tick, and asserts that:

- the frame is no longer gameplay-ready;
- the scenario is `Complete`;
- campaign state is `Completed`;
- the debrief outcome is `Success`.

The explicit `Complete` event remains covered by the existing standalone and
campaign tests. Failure by destruction, expiration, and failed objective stay
covered and are evaluated before automatic success.

## Validation

- `cmake --build build -j2`: passed.
- `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir build
  --output-on-failure`: 5/5 passed.
