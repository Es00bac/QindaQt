# Installed provider bridge/probe — exact operational BLOCK

- Bridge:faa461a71866ab766dfbfbc0bcb9a873a5f119a2a094d46789689803e8e07d68.
- Synthetic probe:a93934cd007350945b7e6227affc0e977bb806c45e553847b58b8ba24011630f.
- Source context:dec748147853ca12859c53dd8a3a3ee40fc17ece.
- Reviewer:qinda_icon_brand_audit; root owns scripts and all actual actions. No runtime/provider/UI/credential/install/build/product actions by reviewer.

BLOCK the current bridge before old provider release, solely for these narrow operational lifetime issues:

1. SIGTERM/SIGINT handlers are installed at bridge90 only after startup/policy/probe. Until then normal owned interruption kills the Python parent by default. bwrap --die-with-parent then drops the new native provider after old GNOME was released. Install stopping state and handlers before Popen/post-launch work, preserving cooperative exact-owned native/launcher teardown. No force-kill of old GNOME.
2. Bridge83 has outer160s timeout, while probe operations can each use100s and failure cleanup can use another100s. Outer timeout kills probe Python before its cleanup/finally and may leave a running secret-tool or controlled probe item. Keep existing per-step limits, with outer600s covering four operations plus cleanup and bounded owner checks. No unchanged retry or new framework required.

Other inspected boundaries match accepted0b1 procedure: both verification/install receipts are prerequisites; old GNOME owner/UID/PID is rechecked and targeted by pidfd/gracefulSIGTERM, only active obsolete GNOME socket selected by exact fragment basename; installed binaries/RO root/private dev/runtime+realstore+task writes only; real HOME/datahome; native4090 direct supervisor with keyring enabled/no-autostart/portal/power/media/global effects; actual new names/socketpeer and native lock observation; no explicitroot overrides that disable residentpolicy.

Probe uses a unique controlled attribute and synthetic bytes only, pins selected native/secrets owner, captures stdout/error privately, performs store/lookup/clear/empty lookup, and emits only operation exit/equality facts. It never retrieves a user's imported secret or emits payload/nonce. Default collection prompt is the actual native helper. The synthetic roundtrip may mutate native collection metadata/ciphertext; prior preservation gates must precede it.

Read-only exact bytes/SHA inspected on laptop. No implementation/source edits/tests executed. Root can repair only handler placement/outerdeadline and route exact successor for immediate recheck. Stop available awaiting that source candidate; do not launch currentfaa.
