# QmLive Match Live Implementation Plan

**Status:** active

**Goal:** Build QmLive match features around one Match event model, while keeping ordinary DDNet demo/ranking compatibility intact. The first implementation phase is the pure-client replay/ranking/filter foundation; QmLiveServer authority and admin workflows are intentionally split into later phases.

## Compatibility Boundary

- Pure client phase must not modify `src/game/server/**`, `src/engine/server/**`, protocol definitions, UUIDs, generated protocol files, or server config.
- Ordinary DDNet servers must support full match demo recording, standard demo playback, optional `.qmlive.json` sidecar, local finish cards, and demo playback reconstruction from standard snapshots/messages.
- All QmLive-only client code is compiled behind `CONF_QM_LIVE_CLIENT`; ordinary QmClient/DDNet builds must not show live match UI, write sidecars, or apply team filters.
- QmLiveServer-only authority, referee, admin assignment, countdown, race lock, delayed-start vote, and official match timing belong to later server phases.

## Phase 1: Pure Client Match Replay Foundation

**Files:**

- Modify: `src/game/client/live/live_finish_ranking.*`
  - Own standard `SV_RACEFINISH` processing, ClientId-to-DDRace-team attribution, pending resolution, duplicate suppression, finish sorting, card queue, and sidecar finish serialization helpers.
- Modify: `src/game/client/live/live_replay_sidecar.*`
  - Own optional sidecar schema version 1, demo/map matching, team/finish event validation, JSON parsing/building, and atomic JSON write.
- Modify: `src/game/client/live/live_match_replay.*`
  - Reuse standard DDNet demo recorder for full match `.demo` files under `demos/qm_live/matches/`; sidecar remains optional and non-authoritative.
- Modify: `src/game/client/live/live_team_render_filter.*`
  - Centralize team filter decisions for client-owned render/message/sound paths.
- Modify: `src/game/client/gameclient.*`
  - Wire QmLive presentation mode, sidecar-backed demo playback, finish card HUD, team filter config, transient reset on seek/filter switch, and live overlay reuse.
- Modify: `src/engine/demo.h`, `src/engine/shared/demo.h`
  - Expose current demo player filename for sidecar lookup only; do not change demo format.
- Modify: `src/game/client/components/{chat,hud,menus_ingame,skins}.cpp`
  - Keep QmLive presentation mode isolated from ordinary HUD/chat/skin paths.
- Modify: `src/test/qm_live_client_test.cpp`
  - Cover finish ranking, sidecar validation, team filter, and source-level contracts that protect DDNet-compatible recording.

**Done in this phase:**

- Standard finish messages drive a local ranking state and short card queue.
- Sidecar records map/demo identity, tick range, team changes, and finish events.
- Demo playback can load a matching sidecar and rebuild ranking/team context across seek.
- Team filter has a single decision object and strict unknown-player-event policy.
- Full match recording does not require QmLive observer acceptance; ordinary online DDNet connections remain eligible.

**Known gaps in this phase:**

- Demo recorder still writes directly through the existing manual recorder slot. A dedicated QmLive recorder lifecycle and temp-file-to-final atomic rename need a later recorder change.
- Rendering/audio filter coverage is partial. Owner-known paths are filtered; standard DDNet events/sounds without reliable owner must remain strict-suppressed where wired, and more call sites still need audit.
- UI is console/overlay level, not the final polished Esc/live menu workflow.
- Axiom/DDNet entry-mode choice is not implemented in this phase.

## Phase 2: Entry Mode And Client UI

**Files to modify:**

- `src/game/client/components/qmclient/axiom_auto_login.*`
  - Add an explicit QmLive connection-mode gate so DDNet mode cannot trigger Axiom auto-login or Axiom-only commands.
- `src/game/client/components/menus*.cpp`, QmClient UI helpers
  - Add pre-enter selection, current mode display, per-address remembered choice, manual override, match recording controls, team filter controls, and finish-card settings.
- Config files under `src/engine/shared/config_variables_qmclient*.h`
  - Add only `qm_`/`Qm` prefixed settings.

## Phase 3: QmLiveServer Match Authority

**Files to add/modify:**

- Add: `src/game/server/qmlive/match_state.*`
  - Own `Idle`, `Preparing`, `Ready`, `DelayedStart`, `Countdown`, `Running`, `Finished`, `Reset` state transitions.
- Add: `src/game/server/qmlive/match_assignments.*`
  - Own admin team-assignment state: Pending, Applied, Rejected, Superseded, Expired.
- Modify: `src/game/server/teams.*`, `src/game/server/gamecontext.*`
  - Add minimal hooks for safe pending assignment application, race lock checks, countdown release, and official finish timing without bypassing DDRace team safety.
- Modify: existing rcon/admin auth paths only
  - Gate all authority through existing admin/rcon permission; no client-self-asserted admin flag.

## Phase 4: Admin Panel, Referee, Timeline

**Files to add/modify:**

- Add client live admin panel under QmLive-only client UI files.
  - Search, multi-select, drag/drop team assignment, pending state display, cancel/replace pending assignment, referee actions, undo, recent audit log.
- Add shared event structs/helpers for match event timeline.
  - Match start/countdown/team finish/ranking/penalty/warning/DNF/DNS/DSQ/reset/bookmark.
- Extend sidecar schema only with backward-compatible optional arrays.
  - Keep versioning and safe ignore behavior for missing or mismatched data.

## Verification Plan

- Phase 1 code: run focused `QmLive*` C++ tests, full `run_cxx_tests` when feasible, and `python qmclient_scripts/gate/check_gate.py --mode quick`.
- Docs changed: run `python qmclient_scripts/gate/check_docs.py`.
- Later server phases: add unit tests for pending assignment transitions, race lock, fixed-denominator delayed-start vote, GO same-tick release/timing, referee undo scoring, plus manual QmLiveServer validation.
