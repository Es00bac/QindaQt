Audio applet A1 P1 compile defect fixes handoff (Rune Mercer)

Candidate: exact commit `aea8a9e44cafacaaa4580bd1265c66cdf5cb73e1`
("Fix Audio applet test C++ compilation defects under strict warnings") on branch
`worker/audio-applet-a1-repair-rune` in worktree
`/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-a1-repair-rune`. Tree
`cd7d9342e4b8fa835a6ef34264b9a313e9eb8cf4`, parent
`262a8493fe5f15991675b6a0f5ef575d4854d19b` (prior CMake path repair).
Working tree clean at handoff.

Changed-path manifest (two P1 compile defect repairs only):

1. `tests/shell/audio_applet/tst_audio_applet_controller.cpp` (line 145-146) —
   Added explicit constructor to FakeTransport class: `explicit FakeTransport(QObject *parent = nullptr) : AudioTransport(parent) {}`.
   Resolves "no matching function for call to FakeTransport(AudioAppletControllerTests*)"
   error when m_transport instantiation was attempted at line 227. QObject parent
   lifetime ownership is now correctly established through base class.

2. `tests/shell/audio_applet/tst_audio_applet_model.cpp` (lines 81, 92, 103) —
   Sequenced `++serial` increment operations deterministically BEFORE each
   makeDevice()/makeStream() call instead of within argument list. Resolves
   "-Werror=sequence-point" violations under strict compilation warnings. Test
   logic unchanged; stable ID and display-text generation sequences preserved.

Verification evidence:

- `python3 tools/check-source-shape --root . --warnings-as-errors`: ✓ Passed
  (1013 source files checked; controller test 485 lines, within threshold)
- `python3 tools/validate-docs --root .`: ✓ Passed (64 Markdown documents)
- `git diff --check`: ✓ Passed (no whitespace/tab violations)
- Working tree clean (all generated .omc/ and build outputs removed)

Not run, deliberately: full C++ compilation build (parent themes module has
unrelated pre-existing CMake configuration issue) or runtime test execution
(audio_applet subdirectories not yet wired via add_subdirectory() integration,
pending manager's integration step per original Elias Frost handoff).

Bounded caveats: These two P1 defects will block test compilation when
audio_applet CMake seams are integrated. Fixes are targeted narrowly to
two exact defects: FakeTransport constructor ownership and sequence-point
violations. No functional changes to test behavior, assertions, or fixture
logic. Gates confirm source shape, documentation, and whitespace integrity.

Requested next action: Astra Quill verifies this exact descendant commit
resolves both P1 compile defects from the rereview verdict (1787928500).
Manager proceeds with add_subdirectory() integration and full test build/run.

— Rune Mercer, 2026-08-28T12:45:00Z
