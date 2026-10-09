# Receipt-pinned private console gain qualification

Package/image/lifecycle qualification remains source-only. The isolated
Qt-only strict receipt fixture passed 36 checks with no failure, skip or
blacklisting; it compiled only the fixture and three receipt parsers against
Qt Core/Test, with no package, Core or graph admission. Do not invoke the
commands below until the Program Manager grants the package/native lane for
the exact reviewed source. This packet does not enable the production gain
worker, install a plugin, merge a package, start a host service, or inspect a
host graph.

## Artifact and loader contract

`artifact_receipt.py` records exact prepared Portage source, reviewed recipe,
archive, Core/module/daemon/config files and a completed full gpkg round trip.
Normal images require Portage signature verification. Schema2 additionally
pins the explicit diagnosticOnly/upstreamTests status, complete upstream
evidence and final input observation. A receipt is published only after the
extracted gpkg payload matches the entire retained Portage image. Hashes are observations of stable files, not exclusion of a concurrent
writer; the manager must keep the qualification image unchanged.

`run_experiment.py` copies that receipt and pins its SHA256 into the diagnostic
build. The bootstrap environment is explicit before any executable/Core/module.
An inherited `LD_*` or `PIPEWIRE_MODULE_DIR` refuses the driver before exec.
The native guard checks exact files, actual Core symbol/mapping, version
WirePlumber0.5.15/PipeWire1.6.8, module directory and package proof before any
private daemon or Core. Each fixed public loopback load rechecks admission,
then confirms actual `pw_impl_module_info.filename` and the mapped ELF inode.
Loader variables are never changed inside the worker/load helper. A different
loaded image aborts the private fixture; the driver kills only its own fresh
process group, including any remaining private daemon/policy.

This is a diagnostic guard. Production borrowed-Core code remains unwired.
The future desktop dependency must require the separately reviewed
`=media-video/pipewire-1.6.8-r1::qindaqt` cohort and qualify its production guard.

## Portage phases and evidence

Supply the clean exact overlay candidate
`b489a5f8ce215da9fad5f5f10db0adbfa22af2a4` from authoritative master
`b128176c5e2771be7719e9f72cd4a1f270bc4b7b`, current Gentoo tree with the frozen
original1.6.8 recipe, both already cached Manifest distfiles, and the actual
selected compiler's ASan runtime. Choose a fresh lab directory whose ancestors
are accessible to Portage's build user. Invoke this package recipe explicitly
as root in the manager's package lane; it never requests elevation itself.

Installed package CFLAGS/CXXFLAGS/LDFLAGS/USE/CHOST/CBUILD are retained. Current
Portage FEATURES/signing policy is read separately and recorded, without
rewriting configuration or keys. Diagnostic ASan adds instrumentation, debug
symbols and USE=test. Only private ASan modes remove FEATURES sandbox and
usersandbox: their LD_PRELOAD interposition prevented the actual Meson compiler
sanity executable from initializing libasan. Network/pid/ipc sandbox, userpriv
and other retained policy remain; normal test/release modes retain their
sandbox/signing baseline. No ASan link-order suppression or package-build
preload override is introduced. This follows [Gentoo's per-package sanitizer
policy](https://wiki.gentoo.org/wiki/AddressSanitizer#Per-package), applied solely
to the child build environment. The normal test variant adds only USE=test;
the normal release variant keeps the installed USE/flags. Future task MAKEOPTS is exactly8/8; the original24/24 source/cohorts remain
preserved. All
temporary outputs and package files are in the supplied fresh lab. Private XDG,
relative PipeWire names and absent host buses are set for package tests.

Portage's emerge buildpkgonly completion calls clean and deletes image even
with keepwork. The recipe therefore invokes only explicit ebuild
clean/setup/unpack/prepare/configure/compile, test for the test variants, and
install-to-image/package. It never invokes merge, preinst, postinst or a final
clean. No dependency transaction occurs; missing prerequisites fail the gate.
Complete raw logs, command exits, timeout state, baseline and verified payload
manifest remain in each output directory. A .tested marker records phase
completion, not a count or a substitute for inspecting upstream test logs.

### Explicit original failed-suite diagnostic image

Default policy refuses any failed suite. The optional
--allow-failed-upstream-old-asan applies only to unpatched-asan-control, in both
the package recipe and native driver. It never admits a failed patched-ASan or
normal cohort. Receipt schema2 carries diagnosticOnly=true and
upstreamTests=failed; unknown/missing flags, contradictory claims or schema1
refuse at the Python and native boundaries. This is an incompatible private
diagnostic receipt change, not an Audio1 or installed SDK change.

The orchestrator first invokes exactly clean/setup/unpack/prepare/configure/
compile. It freezes the compiled Meson registry before one explicit test
phase. The registry, actual JSON rows/commands, full text totals and JUnit
identities/results must agree; none may be missing, repeated or shortened.
Every row must expose both literal absent-bus addresses below this fresh lab,
and ENV_UNSET must not name either variable. Individual TIMEOUT rows may be
part of a fully returned failed suite. An outer runner timeout, nonstandard
exit, spawn/reap/log/record exception, uncertain group cleanup, failed compile
or changed inputs denies image assembly.

Only after this complete evidence is frozen does the admitted original failed
mode add process-local FEATURES=noauto to exactly install/package. USE=test
and strict test policy remain set. No marker is fabricated; .compiled must
exist and .tested must remain absent before and after assembly. Normal and
green fixed modes retain their automatic dependency and signing/isolation
policy. No test-fail-continue, retry, .tested invention, skipped USE=test or
upstream sanitizer suppression exists.

This transition follows the inspected installed Portage sources:
doebuild.py2369 prevents phase dependency recursion only under noauto (unless
alwaysdep); phase-functions.sh558–565 records .tested only after successful
src_test; ebuild.sh159–163 applies ENV_UNSET. Their observed SHA256s are
6622d19118c4d2dcb27222d886481b28232b2267fd97d8ae8bf313d4daa93c2e,
556df074f19688e0380a3197067d909e226311429c8f690bbb2eb12f6a01c458 and
bdb252055444ab8b0df70ca9fab08a636c51acde07d576e23699856201bf0e66.
Recheck this phase contract before dispatch if the installed engine changes;
the recipe never changes Portage configuration.

ENV_UNSET preserves every configured unset except the two locally constructed
DBus addresses; it never imports either address from the host. The installed
Meson mtest.py (SHA256
491426d65c11b702039ee3bf7671c53ecd24f2b072490c2fca68d44eb26f9046)
logs result.env in JSON and supplies that same runobj.env to its test
subprocess. Thus the gate examines the effective post-Portage test environment,
rather than the parent command's intention. Actual forwarding is still a
future package gate; the existing failed lab omitted the session address and
cannot qualify under this new requirement.

suite-evidence holds exclusive single-link read-only snapshots with stable
descriptor/name/version/SHA observations and bounded duplicate-key-rejecting
Python JSON reads. Registry/JSON/text/JUnit/raw phase logs, exact argv/exits,
reserved-leader cleanup and test-final input observations are pinned into
upstreamEvidence. A separate final publication input record remains in that
same snapshot; the producer, Python bootstrap and native preflight recheck it.
The native guard also reparses actual registry/results/environment/phase
evidence before private Core/children, then rehashes the pinned receipt and
evidence before each load. Native parsing has byte/count bounds, not an
allocation deadline or concurrent-writer exclusion. Root-owned package
snapshots must remain readable by the explicitly authorized diagnostic runner;
the recipes request no escalation or permission changes.

A complete failed upstream suite remains FAILED in every receipt and result.
The diagnostic image has no release, fixed, installed, physical or production
gain permission. The later old native gate must independently observe the
specific module-loopback.c ASan heap-use-after-free, without timeout; these
upstream LeakSanitizer failures are not that control. Normal/patched
qualification never inherits this exception.

The --distdir argument is now a read-only source for already cached inputs,
not the directory passed to Portage. portage_inputs.py admits the two fixed
Manifest rows and their exact SHA256/BLAKE2B/SHA512 bytes through bounded
no-follow regular descriptors. It detects observed same-inode rewrites and
named replacement while copying. Each fresh output holds a root-owned
source-inputs directory (0700, files0400) and separate root:portage distfiles
(0770, files0664). Independent copies prevent Portage's cache permission
normalization from changing the protected snapshot or the caller/global cache.
The strict artifact_receipt.artifact contract is unchanged; the final archive
receipt uses the protected copy. The snapshot is protected from the build
user, not an exclusion of concurrent root writers.

input-admission.json pins protected identities and actual cache bytes.
Both are rechecked in a finally boundary covering ebuild and result recording,
including returned nonzero/timeout and raised spawn/reap/log/record errors, and
before receipt publication. input-final-verification.json retains the final
observation. Secondary verifier/write errors cannot erase the original build
exception; unavailable evidence storage can prevent that file, never permit a
usable cohort receipt.
Different bytes, symlinks/hardlinks or protected identity/version changes refuse
a usable receipt; permitted writable cache modes do not relax image admission.
Partial staging remains evidence and requires a fresh cohort directory.

The first actual unpatched ASan attempt reached configure and failed before
any loopback/runtime control. Its Meson sanity binary compiled successfully but
exited1 with an ASan library-order diagnostic. Its Portage build log SHA256 is
2b4958df54523007a7fafdc240042ef761b033b82b585b313e891dae4ec6dd66;
Meson log SHA256 is
205f2a7b92fdc1cc9f350519b1c2449097372460d63adc0d85eaead97779198f.
The original private cache was normalized from0644 to0664. Those helper repairs later passed31 pure controls and six AST checks. A fresh
original ASan cohort then completed configure/compile and returned a complete
FAILED upstream suite:51 identities,45OK/5LeakSanitizer FAIL/1context TIMEOUT.
That original lab produced no image, receipt or native module control. Its
raw suite and bounded read-only RCA remain preserved; DBus allocation
accounting and fork/wait explanations are source inferences, not proven leak
or hang causes. This amendment permits only a separately reviewed/granted
fresh original diagnostic cohort; it neither resumes nor reruns that lab.
Patched image/native cohorts and the genuine original module
heap-use-after-free control remain unexecuted.

Before any package dispatch, run the focused Python fixtures in the granted
private lane. They use tiny pinned inputs in temporary directories and never
import Portage, compile code, launch audio or alter an installed cache:

```sh
python3 -m unittest discover -s tests/services/audio_service/console_gain_experiment \
  -p 'tst_portage_*.py' -v
```

The earlier exacte704 input19/policy6/orchestration6 controls passed31/31 with
six AST checks. The final amendment verification passed85/85 across eight pure fixtures
(19/7/6/6/24/11/9/3) and21 AST parses. This includes the final-publication
controls and the extracted private folder/environment composition. The recipe
entrypoint delegates that composition through explicit installed flags, policy,
UID/group, repository and cache arguments; it never imports Portage in the
pure helper. These are helper results, not Portage, native or ASan qualification. The Manifest
observe→FIFO replacement case uses the actual helper in a bounded owned child;
no descendant process or unrelated PID is targeted. Both Manifest opens are
nonblocking before held-descriptor rejection. Their private injected fixture pins are not accepted by the
production stage_inputs wrapper, which always uses the reviewed two-row
Manifest/archive pins.

Mandatory future invocations use the same exact source, distinct fresh outputs
and an observed sanitizer runtime; these are templates, not executed evidence:

```sh
python3 portage_recipe.py --overlay "$gain_overlay" --gentoo "$gain_gentoo" \
  --distdir "$gain_distdir" --output "$gain_lab/old-asan" \
  --mode unpatched-asan-control --asan-runtime "$gain_asan_runtime" \
  --allow-failed-upstream-old-asan
python3 portage_recipe.py --overlay "$gain_overlay" --gentoo "$gain_gentoo" \
  --distdir "$gain_distdir" --output "$gain_lab/fixed-asan" \
  --mode patched-asan --asan-runtime "$gain_asan_runtime"
python3 portage_recipe.py --overlay "$gain_overlay" --gentoo "$gain_gentoo" \
  --distdir "$gain_distdir" --output "$gain_lab/fixed-normal-test" \
  --mode patched-normal --upstream-tests
python3 portage_recipe.py --overlay "$gain_overlay" --gentoo "$gain_gentoo" \
  --distdir "$gain_distdir" --output "$gain_lab/fixed-normal-release" \
  --mode patched-normal
```

The normal two variants require full Portage gpkg signature/payload validation.
Neither is an installed-host proof. Keep the original and repaired package
source/plugin/library bytes separate; no manual loopback copy or private loader
implementation is permitted.


### Reserved task resource envelope

The owner superseded the previous24/24 task setting. Future qualification
cohorts use literal -j8 -l8 in the private child environment and result metadata,
with no global MAKEOPTS, FEATURES, compiler, service or host configuration edit.
The exact qinda package command is wrapped by one existing transient systemd
service: Type=exec, CPUQuota=800%, CPUAffinity=0-5 12-17 and a literal
payload taskset0-5,12-17, MemoryMax=12G (12GiB), MemorySwapMax=0, TasksMax=256,
Nice=10 and RuntimeMaxSec=1800. WorkingDirectory is the existing /var/tmp.
The cpuset controller/AllowedCPUs is not assumed or required. The service uses wait/pipe to retain output and
actual unit outcome; it is not a detached job or a new supervision framework.
Root's harmless probe found missing cpuset effective state in the user-service
cgroup. Existing R20 CPU/memory/pids proof does not prove all launchers had the
requested affinity/nice; only Network's affinity was directly observed. Future
qualification must inspect the actual worker/phase affinity and priority rather
than infer them from argv or those earlier units.

Root owns the literal unit-name/fresh-root lease, actual effective properties,
MainPID/start/pidfd and cgroup settlement observations. The service limit is a
stricter outer gate than the retained1800/90 cooperative attempt/evidence
budgets: resource timeout, OOM/task exhaustion, missing/incomplete final
evidence or uncertain cleanup always denies package admission. A CLI exit,
candidate-only disk result or artifact presence cannot replace that proof.
The unit may stop work before cooperative final publication; preserve all
partial failure evidence and separately establish owned settlement.

Only one heavy qinda8/8/12GiB cohort runs at a time; half-core affinity reserves
capacity for the other project. Laptop heavy work is separately capped4/4 and
at most5GiB under its own root-owned grant; this qinda command does not dispatch
it. Actual resource verification passed62 controls (the preserved59 plus
three existing owning folder/environment cases), with no subject child/signal.
Original plans/source, accepted0e terminal semantics, original failed51,
strict old-only diagnostic policy and normal/fixed green release gates remain
unchanged. Resource preparation is source-only and starts no unit or package.

### Cooperative package cancellation and evidence retirement

The optional public run_logged cancellation callback is borrowed, same-thread,
non-raising and non-reentrant; its owner outlives acquisition, polling and
retirement and never waits, reaps or signals the runner's child. Default callers
keep the prior phase timeout/result shape. The prospective package dispatcher
installs only latched signal requests, with no raising timer/handler. The owning
runner checks before launch, after acquisition, in polling and after
cleanup/log hashing. Acquisition is inside its owning try/finally. Before group
signaling it rechecks the exclusive waitable-child reservation; losing that
reservation denies signaling. Cleanup always attempts the held-group kill and
bounded wait, ignores cooperative requests while retiring, preserves the first
exception and attaches secondary kill/wait/log-close errors. A request during
retirement denies the phase after cleanup, preventing the next phase.

portage_recipe.main and build_attempt accept a constructor-visible optional
synchronous runner; the dispatcher supplies the public cancellation callback
through that boundary for every compile/test/image/package-verifier invocation.
No ignored monkeypatch or process-global runner lookup is used. This changes no
old-only qualification, noauto, ENV_UNSET, signing, sanitizer or payload policy.

The prospective workflow allows 1800 seconds for attempted build including its
owned phase cleanup, then at most 90 seconds for final source/world/profile/cache
observations, raw index and result publication, capped at 1890 seconds total.
Every phase checks the monotonic attempt deadline; final loops check a separate
evidence predicate. Cooperative checks deny late completion after a blocking
call returns; they do not promise interruption of kernel-uninterruptible I/O.
Latched requests never asynchronously unwind an acquisition or cleanup path.

The on-disk result snapshot always has success=false and successCandidate only;
it cannot admit a cohort. After the result write returns, the terminal report
records completed duration including final inputs/index/write and refuses
cancelled/late completion. Admission additionally requires the manager's held
Popen/pidfd/start identity, successful owned wait/exit, complete raw report and
actual total process duration including terminal emission/exit. A cancelled or
late terminal emission returns failure. No standalone snapshot, receipt-file
presence, dispatcher disappearance or PID guess substitutes for this settlement.
Incomplete cleanup or evidence denies the gate. Primary exceptions remain
authoritative when final verification/index/write also fails.

The owning injected fixtures replace Popen/waitid/killpg and clocks; direct
request calls model signals without sending an OS signal. They cover
prelaunch/postacquisition/poll/observed-exit/kill/wait/log-close/hash requests,
lost reservations, primary plus cleanup failures, no later phases, and final
pins/index/result-write deadline crossing. These are pure source controls, not
real child cleanup, package, upstream, Core/lifecycle or gain qualification.
The focused actual batch passed 54 controls: 21 runner, 14 publication, eight
build-input and 11 unchanged cohort-policy cases; all subject child/signal
operations were injected. The earlier 49-pass batch remains preserved.
The final terminal-emission boundary also captures print/flush and its
post-emission deadline refusal, retaining the exact earlier primary exception
and its cleanup/evidence attachments. Without an earlier error the first
terminal error remains primary; a failing diagnostic sink stays secondary.
Five additional injected controls exercise these paths without an OS signal,
child, package or audio runtime. They do not extend package admission policy.
The actual focused successor batch passed all 59 controls (the preserved54
plus these five terminal methods), with unchanged source-before/after hashes.
Immutable c10 remains the raising-handler NeedsFix control. The 45db old-only
policy and original failed 51/unadmitted cohort stay preserved.

## Private lifecycle and gain evidence

After package/image source acceptance and explicit native authorization:

```sh
python3 run_experiment.py --build "$gain_lab/old-native" \
  --cohort-receipt "$gain_lab/old-asan/cohort.json" \
  --allow-failed-upstream-old-asan
python3 run_experiment.py --build "$gain_lab/fixed-native" \
  --cohort-receipt "$gain_lab/fixed-asan/cohort.json"
python3 run_experiment.py --build "$gain_lab/fixed-normal-native" \
  --cohort-receipt "$gain_lab/fixed-normal-release/cohort.json"
```

The old control runs only `borrowed_core_lifetime::moduleFirstThenCoreStop`
against the unmodified upstream prepared module, with actual Core and module
ASan dependencies verified. Success of this **negative gate** requires an
actual ASan heap-use-after-free with module-loopback.c provenance. Compile
failure, guard refusal, timeout, Qt assertion or an unrelated crash is not the
old failure control. The failed-upstream flag additionally selects the default-OFF native
QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN definition; it requires the separate
unpatched-control build flag and old mode. Fixed source must pass all eight
selected owning rows:

| Row | Actual future gate |
| --- | --- |
| routing_gain | Independent stages, exact bottom silence, pan and finite headroom |
| send_admission | Producer/client serial, exact object/role/token, replacements and retirement |
| private_environment | Config/socket/notification scrub; module override rejected before startup |
| image_preflight | Actual pinned Core, then private receipt withholding/change/restoration |
| qualification_receipt | Synthetic complete-suite fixed green/old failed admission; explicit old opt-in/default and failed-fixed refusal; per-row private bus/ENV_UNSET, pinned raw hash, count/registry/JSON/text/JUnit consistency and duplicate/incomplete controls; prior flag/phase/input/FIFO negatives. Exact e7a9 passed the Qt-only strict native fixture: 36 passes, zero failures/skips/blacklisting; package/image/Core/graph gates remain separate. |
| core_scope_runtime | Missing gate, foreign/occupied/nested slot, insert/load loss and reconnect |
| loopback_gain_runtime | Initial quiet Props/first PCM, loud restoration control, cubic/headroom PCM and public visibility |
| borrowed_core_lifetime | Owned/borrowed producer, failed-init after hooks, Core-first/module-first,64 churn and override refusal |

Only the lifecycle executable exports a subtractive `pw_stream_new` allocator
fault. Original/patched module code still executes its real init/listener/free
paths. The fault consumes properties and returns ENOMEM, matching frozen
PipeWire1.6.8 stream.c1557–1649; its observed counter must increase. It is not
linked into a production target or the other experiments. Module/Core fixture
listeners unlink in their actual destroy callbacks and outlive the graph.

Every native invocation has a fresh process group and bounded timeout. The
Linux leader remains unreaped (waitid WNOWAIT) through group cleanup so that an
unrelated reused PID cannot receive the cleanup signal. Raw Qt
text/XML, loader/cohort pins, process-group cleanup and every failed command
are retained. Generic CTest selector is
`^qindaqt[.]audio-gain-experiment-`; use the receipt/bootstrap driver for real
execution. Direct ambient CTest is not a qualified invocation.

The declaration/model/production worker, actual public-operation old5b
regressions, SDK stage/header poison/restoration, two-output bus DSP,
fresh-login and physical/game/fullscreen gates remain separate and required.
Neither these sources nor a successful private experiment complete ED09.


The separately observed 20261008T180303Z old diagnostic again failed both-bus
admission: all51 Meson rows retained the private system address but omitted the
session address. Its saved EAPI ENV_UNSET still contained the profile's session
name. ENV_UNSET is incremental, so caller omission cannot negate a profile
entry. The bounded preparation repair supplies explicit negative tokens for
exactly both locally constructed private bus variables; all other configured
unsets remain. A read-only actual Portage config expansion now refuses either
remaining positive name or changed address before any phase. Saved EAPI and
every actual Meson row remain independently checked; the old45OK/5FAIL/1TIMEOUT
lab is preserved and never resumed or admitted by this source repair.
