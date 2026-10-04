# Production native greeter contained gate passed

- Timestamp: 2026-10-04T16:44:41+00:00
- Worker: `/root/native_permission_repair`
- Outcome: production launch/standard lock role qualified on software virtual output
- Source still immutable/clean: `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`, tree `3a257b8d991777247970c83ce9ae8ca265448db3`
- Status: available; no owned runtime remains

Authorized command:

```sh
ssh qinda 'python3 /home/cabewse/.cache/qindaqt-native-greeter-qualification/qualification.py --execute'
```

Final attempt exited **0**. Evidence directory on qinda:
`/home/cabewse/.cache/qindaqt-native-greeter-qualification/run-20261004T163719Z-8kq7_vzd/evidence`. It contains summary.json, bootstrap.json, source-and-images.json, support-information.txt and server-only compositor.log. Namespace command/provenance/helper copy remain in its parent run directory. Local JSON evidence copies are under ignored worker `.cache/native-production-greeter/passed-evidence/`.

### Exact runtime evidence

- RequestLock returned true from the private native endpoint. Owner `:1.1` was pinned to owned compositor PID 5 in its private namespace before the call.
- Real installed `/usr/bin/qindaqt-lock` direct child PID 24, PPID 5, UID 1000; argv is exactly the fixed production image without arguments. Installed greeter/desktop-file hashes are pinned and unchanged. Fixture authorization OFF and production screenlocker ON were checked in the Portage CMakeCache.
- Standard `ext_session_lock_v1` object 34 creates `ext_session_lock_surface_v1` role 43 for wl_surface 38/output 23. Server sends configure serial 9 at 1280x720; real child acknowledges 9, attaches non-null wl_buffer 47 and commits. Protocol locked and frame callback done observed. No protocol error/refused-lock trace; real child and required state persist for the 0.5 second gate.
- Locked=true and Protected=true. Protected alone was never treated as acceptance.
- Native supportInformation reports QPainter. Only software virtual output existed; host /sys, physical device nodes and live /run sockets are absent from the namespace.
- Loader `calling init: /artifact/usr/lib64/libqindaqt-kwin.so.0`; SHA256 `d8f64e74d55a6c64b71e8bddbbbeb4db7282a0191ab2065eda1a9d3a12fe1b7d` matches the retained r6 image core library. Image compositor hash `e4690b828d267524caeef582928b2e53840e1a1ee99026fbd4c11301dc86aba1`. No installed r5 core-library load.
- Bootstrap proves initial full UID map; real/effective UID/GID 1000, groups empty, CapEff/CapPrm/CapAmb zero and NoNewPrivs=1 before private bus/compositor. Root only creates mounts and drops identity through trusted inline code.
- Fixed PAM image masked with a nonregular node before launch and immutable to the unprivileged child. No credentials entered, real PAM executed or unlock called. Authentication unavailable is intentional.

### Focused temporary-helper repairs and cleanup

The exact original attempted helper was recovered and its b9a1 SHA verified. Only three observed helper defects were repaired under manager authorization: proc/sys is not an independent remount point (use explicit readonly bind); root without DAC could not chdir/open user-private gate (drop GID/UID with isolated trusted inline Python before access, cwd /); busctl property bool JSON data is scalar (accept scalar/list). No production admission/trust/environment bypass, source or installed-file modification.

Final temporary helper SHA256: `4eabcedba5cb78fe195aca75875313092564a6ec19d269b141e906a31ea41543`. Exact b9a1-to-final delta: ignored `.cache/native-production-greeter/authorized-runtime-repairs.diff`, also copied to qinda private cache. PLAN.md/plan.json were updated to actual final commands/outcome. The separate reviewed630c bytes were not provided; no byte-level 630c comparison is claimed.

Attempts: three helper-only failures (all exit 1, before any greeter acceptance), then one final gate pass exit 0. The compositor exits 15 after the owned cleanup SIGTERM; bwrap/private bus/reaper exit with the gate. Ordinary and sudo read-only /proc scans limited to exact owned run/helper/artifact argv markers found **zero survivors**, including all four attempt run markers. AST/static nine trace-case/plan checks had passed before execution; this gate now supplies actual runtime evidence. No repeat or broader test was needed after acceptance.

### Bounded caveats and next action

This qualifies real production launch/standard lock role and software virtual frame feedback. It does not qualify PAM authentication, pixels/physical scanout, physical session restart or deployment. Manager remains owner of signed matched artifacts, resolver/install/collision gates and deliberate session adoption. Source candidate has not changed from exact accepted 24d0.

Requested next action: manager uses the saved positive production-role evidence for the incident adoption gate. Read the current Platform incident queue; compiler lease stays manager-owned. Concrete compatible help offer: I am available to inspect this exact evidence, answer reviewer questions or make focused temporary-helper repairs if the manager identifies a reproducible failure, without touching live endpoints or unrelated product paths.
