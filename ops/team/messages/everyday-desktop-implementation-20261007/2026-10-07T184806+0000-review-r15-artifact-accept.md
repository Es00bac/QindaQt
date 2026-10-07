# Exact signed r15 artifact review — ACCEPT

- Candidate: a6bff0aae6693664c2cdaa8441a623ff06039863d85f47ae04378c3abf538c47
- Candidate type: signed binary artifact SHA256, not a Git/runtime source pin
- Artifact: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-platform-20261007/.cache/ed-platform-binpkgs/gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20261002-r15-1.gpkg.tar
- Bytes: 60866560
- Runtime source freeze: bef80e861680f6d7bd53a556c39e13d97ff9e742
- Accepted recipe: e9f04b8c736d1ff9c5f42d0ba42fcd4710ea665d
- Accepted desktop mirror: e6f8787b3ba5d5ffbe2320eb4cbe9f646fc2b337
- Authored artifact handoff coordination commit: d019bbf0cc9c3c5cc5d947002939ae0d523730dc
- Verdict: ACCEPT
- Reviewer: Everyday Review Sol
- Time: 2026-10-07T18:48:06+00:00

Independent artifact gates in own ignored build/evidence and safely extracted image:

- Exact byte count/SHA256 match before review and unchanged after probes. Native Portage gpkg explicitly request_signature=True/verify_signature=True with ignore-signature removed and required-signature set in cloned process settings: full-container checksum/signature verification passes, signature_exist=True. Own gpg signed Manifest verification exits0 and VALIDSIG matches F7DA5CF3CF5B561FB219E4BBE015A86983EBD777; no auto key retrieval.
- Signed metadata embedded ebuild byte-identical to exact accepted overlay recipe, with approved bef80e861 runtime pin, r6 dependency, lock-PAM>=1 floor and NativePowerExclusive=OFF. Signed RDEPEND/PDEPEND contain required native dependencies and no forbidden Plasma runtime atoms. Prior independent exact recipe two-root599-package runtime closure remains preserved at b19c6d0a1; this artifact review does not claim a new installed resolver transaction.
- Own Portage safe extraction after full verification:2292files/links. All six Network files at generated qmldir qml/ paths are byte-identical to frozen source; native PAM worker and source-defined Power1 --upstream=production unit present without exclusive flags. Root-managed PAM configuration remains an external package prerequisite, not duplicated by desktop.
- `tools/check-release-contract --install-root build/r15-artifact-review-image/usr`: exit0. Plugin readelf/strings verifies fork libqindaqt-kwin.so.0 dependency, no stock libkwin dependency, and sole factory IID org.qindaqt.kwin.PluginFactoryInterface6.6.6.1.
- Actual verified package-image Network copied into own isolated import root; package Tokens/Controls supplied from own extracted artifact, system QindaQt excluded from public Qt dependency root. Reviewer reuses its already independently compiled standalone Qt probe (source identical to runtime freeze), with no new compiler work. Final private HOME/all XDG/offscreen software/disk-cache-disabled/both-buses-blocked probe:11/11 expected exits, including six compiled types, whole-module withholding exit3, six disk types, each-file withholding exit1, and restored disk/compiled Ready6/failures0. Earlier probe was repeated with stronger HOME/XDG isolation; final retained receipt is r15-artifact-network-own-probe-private-home.json. Original signed bytes/image remain untouched.
- Exact authored d019bbf0 handoff was read and agrees with identity/source/recipe/build-only limits. `git diff --check`: exit0. Reviewer performed no package compilation, installation, publication, live service/radio/credentials/session/device action or delivery-metadata update.

Caveats: binary integrity, coherent signed source/recipe metadata, focused image/package behavior and exact plugin ABI are accepted. Author full-source Portage build receipts remain distinct from this independent artifact review; reviewer did not repeat the broad build. Current desktop adoption, exact installed ownership/closure, fresh login, physical owner-entered unlock, battery/AC transitions and full native power parity remain separate manager gates. This verdict does not authorize publication or installation beyond existing user/manager scope.

Requested next action: manager preserves this exact immutable binary and takes its separately authorized delivery/adoption step only after remaining deployment prerequisites. The two requested reviews are complete: pure protocol ACCEPT a2e8631d4 and this artifact verdict. Reviewer is available with no resource lease and claims no extra task.
