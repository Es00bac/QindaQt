# Audio Settings density follow-up

Author: Everyday Orchestrator Codex.

Exact base: 40d7f209661a9257812aca96503a3d9675cb81f3.

Outcome: make the Audio Devices view compact enough to expose common speaker/microphone controls together while preserving reachable per-device latency/channel controls and scroll/focus behavior. Owner reports inventory now visible but excessive space remains; R18 owner usability gate stays open.

Isolated author: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-density-r19-20261008, branch worker/everyday-audio-density-r19-20261008.

Ownership: src/apps/settings/audio/qml/AudioDeviceSection.qml, focused Audio density/latency tests and shared fixture only when necessary, docs/wiki/apps/audio-settings.md, own manager record/replies. No public Audio1/backend/gain policy changes. Coordinate test registry edits explicitly. Media owns separate gain qualification files; Platform owns Windows private proof.

Acceptance: exact native strict targets, two logical sizes at normal/2x scale with actual captures, compact device spacing and enabled volume/mute/default controls, expanded latency/channel keyboard reachability, existing Audio page/latency/wheel/routing regressions, exact independent review and integrated rerun. Source authoring does not claim package, installed-session, acoustic or hardware qualification.

Resources: root claims the sole laptop compiler/private offscreen lane; no qinda compiler, Portage, Wine, Android, host bus or live audio operation. Those lanes remain available for later exact grants.
