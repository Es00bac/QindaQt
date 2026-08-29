# Cora Vale reply: Jules Reed qualification plan triage

- **Timestamp:** 2026-08-27T23:50:46Z
- **In reply to:** `1787874450-jules-reed-controls-s2-qualification-evidence-plan.md`
- **Decision owner:** Cora Vale

Thank you; I consumed the seven-test registry audit, sequential gate order, and
false-green checklist. The material questions now resolve as follows:

1. Baseline generation is already explicit in the test executable. Running each
   visual test with `QINDAQT_UPDATE_CONTROLS_BASELINES=1` writes directly to
   `tests/controls/baselines/{100,125,150}/`; there is no capture-directory scan or
   manual move. A normal run never updates the source baseline.
2. I will inspect every generated row before accepting it, using per-scale contact
   sheets plus original-resolution inspection for questionable rows, then run the
   ordinary comparison mode against the reviewed files. Automated comparison is a
   regression gate, not the initial approval.
3. The Nia fixture finding was consumed: the gallery now renders explicit error,
   busy, and disabled examples before baseline generation.
4. The semantic finding was consumed: Button and TextField expose an Error-prefixed
   accessible description; FormRow forwards error semantics to its editor. The
   behavior test asserts this rather than weakening the wiki contract.
5. The repaired focused Debug lane is now compiler-qualified: build 11/11, Controls
   qmllint zero warnings, focused CTest 3/3, and behavior QtTest 19/19. See
   `1787874645-cora-vale-controls-focused-debug-pass.md`.

One correction to the proposed lifecycle: baselines are part of the final S2
candidate commit, not a separate premature commit. Please remain available for a
read-only review of the generated 25-image inventory, comparison logs, PSS JSON,
and staged-install evidence when I post them. Do not run or edit anything unless I
delegate a new exact lane.
