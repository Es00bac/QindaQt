# Independent PF21 repaired source review: ACCEPT

Accepted **desktop9f2f2b21671944a618dc8af9027eedd89f2be4b0** and **forkdf223cd832c6357c36abfc5d5269f703564c87ed**. Both exact candidates fetched from qinda hub and fast-forwarded in the same isolated review worktrees. First rejected pair732e21a5a/a653b961aa, immutable4c2319326 rejection and first failed docs logs remain preserved. This accepts source coherence only, with no C++/native/installed qualification implied.

All four findings closed:

- TabletModeManager uses /org/qindaqt/KWin/TabletModeManager; root /org/qindaqt/KWin remains independently registered. Added tablet-specific upstream normalization precedes the generic root rule. Nonmutating in-memory checks prove original tablet path maps to dedicated object, ordinary KWin root stays root, and repeated normalization is idempotent.
- Protected capture helper native restricted value exactly matches org.qindaqt.KWin.ScreenShot2 without semicolon; screenshot declaration matches. Fixed Exec, root-managed checks, sole exact permission list and empty Wayland list remain unchanged. No legacy permission was added.
- ADR0340 navigation is relative adr/ and strict documentation now passes.
- Screenshot public XML/header permission instructions, WIRE native-key/service contract and generated XML CMake comment agree with native-only admission. Generated VirtualKeyboard/Tablet Q_CLASSINFO remains native and installed XML RENAME names stay native.

The repaired delta is exactly2desktop paths and6fork paths. Full candidate relative-path inventory binds21desktop +29fork changed paths/hashes compared with original candidate parents. All other previously reviewed Screenshot/EIS/caller/targeted-signal/service-owner/native-key removal/Scripting/VirtualKeyboard boundaries are unchanged. Parsed screenshot XML node/interface matches native constants; both native desktop values and helper equality guard inspected. Nonmutating assertions also retain the standard Wayland protocol names. No source/protocol files rewritten; no legacy alias admission restored.

Actual qinda gates:

| Command | Result | Evidence log SHA256 |
| --- | --- | --- |
| qindaqt/tools/rename-identity --check | exit0/16.14s, all28 rule counts0 | 40cc893bfe05f811ecb3cc67ed5bd4ee1f9e4a5efe7e91e7ed071f1e7b68b013 |
| mkdocs build --strict | exit0/8.22s | 3489c3a23ca4873275fa6aae94bd6443326338d2d57fbc0d34717a1c9255cfce |
| python3 tools/validate-docs | exit0/0.51s;500 Markdown documents and navigation | 29b0221b442c10911f52f5c84b60abda2592aab5578c80fa154d0c06a2b85af6 |
| git diff --check and status for both isolated trees | exit0; clean | exact source hashes beside this approval |

Distinct *-repaired.log/time files preserve first gate receipts under each ignored build/review-evidence. No C++ or core build, private/host compositor run, input/audio operation, staged install or shared-native source mutation occurred. Source acceptance may precede root's coherent finalfork/core composition, compiler gates, staged collision check and actual native endpoint/legacy-key rejection qualification; those remain required separate manager gates. No whole PF21/PowerDevil/runtime cutover acceptance.

Gabbee44c857 source remains preserved; root separately reports its independent33-test replay acceptance. Power0e5db source and fork26cf remain preserved; root separately accepts the three-line Workspace mode-completion source connection, while native qualification remains pending. This reviewer accepts no combined runtime result by inference.

Reviewer is available for exact integration findings or source-only coherent identity diagnostics; no resource lease held.
