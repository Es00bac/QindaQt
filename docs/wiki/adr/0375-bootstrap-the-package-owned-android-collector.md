# ADR-0375: Bootstrap the package-owned Android collector

- **Status:** Accepted
- **Date:** 2026-10-09
- **Owners:** Android Runtime and Android Applications
- **Supersedes:** Only the permanent null-production-port consequence of ADR-0369/0371/0372; guest/window qualification gates remain
- **Superseded by:** None

## Context

The private transport and independently held renderer boundaries exist, but the
manager still needs a concrete package executable and actual supervised startup.
A generic adapter does not connect the product. Qualified native transport must
remain distinct from missing Android image/package/task/surface evidence.

## Decision

Install `qindaqt-android-collector` at the fixed `/usr/libexec/qindaqt` path and
its exact conservative recipe at `/usr/share/qindaqt/android-runtime/collector.json`.
NativeRuntimeBootstrap verifies every fixed path component with root ownership,
non-writable group/other permissions and no symlinks. The helper/session code is
native ELF with execute permission and no set-id bits. Held file identities are
rechecked, including replacement and timestamps; recipe bytes must match the
compiled version1 `waydroid-stock`, `guestQualified:false` policy exactly.

Admission binds the actual session bus daemon ID and current Session1 unique
owner/PID/UID to the held `/usr/bin/qindaqt-session` executable and pidfd. A bus
name alone does not admit the session. Context checks repeat on every port
getter/request and periodic owning-thread loss notification. No Session1 or
existing supervisor implementation is changed.

The owning manager creates an unnamed private seqpacket pair and cryptographic
32-byte key, then starts only the exact pinned helper executable through its
inherited object descriptor. A sealed bounded memfd carries private key, parent
PID, runtime attachment incarnation and recipe digest. Fixed inherited slots
are internal, never public argv/environment credentials. Child environment is
closed, stdout/stderr are null, and race-closed parent-death signal plus supplied
parent pidfd/executable bind helper lifetime. The receiver holds the actual
started child's pidfd and exact code object; only a valid kernel-credential/key
packet admits the port before manager injection. Startup is bounded and no
restart or mutation replay loop exists. Closing the owner retires only its
explicitly started native child.

The helper publishes Unavailable with no registrations/apps. Its minted attachment
identifies this native collector instance, not a qualified Android guest. Refresh
returns the same copied observation and deduplicates unchanged snapshots. No
Waydroid command/Binder call, image initialization, app dispatch or ambient guest
activation occurs. No renderer is admitted/held while renderer evidence is absent;
ADR-0374's independent renderer lifetime remains the next qualified composition.
Native helper readiness never means guest Ready, Running or a mapped window.

The product-owned `src/apps/android_manager/component` CMake root configures only
launcher/catalog, AndroidRuntime, manager/QindaTK and Qt. `AndroidApplications`
installs exactly manager, collector, desktop entry and recipe. No desktop/KWin
build or tests harness is used for packaging. The ordinary source entry retains
NoDisplay. Only the focused Portage component may set
`QINDAQT_ANDROID_SETUP_MENU_ENABLED=ON` while RDEPENDing on
`app-portage/qindaportage`, exposing truthful native setup/status with fixed
Runtime/Images selection buttons. Guest apps remain unavailable/no rows.

## Consequences

Actual built-helper private-descriptor tests establish a live kernel-authenticated
connection, unchanged Unavailable refresh and context-loss fencing. They separately
refuse writable staging, verify exact recipe bytes, decline missing production
context and reject a public argument mode. They are inert native helper tests, not installed session/Android
app evidence. The component is staged under ignored DESTDIR; no system install
or current-session helper activation occurs at this source gate.

Signed Portage build/adoption and actual setup menu remain package/installed gates.
Verified guest image/materialization and an authenticated package/task/surface
collector remain required before app catalog rows, renderer association, green
ordinary app-window identity or mixed native/Android QindaQt containers. The
[Android Applications page](../apps/android-applications.md) records this boundary.

## Revisit when

Qualified guest facts and renderer bootstrap exist, or the package/session/private
record contract changes. Extend the native collector deliberately; never promote
setup/helper acknowledgement or title/app_id matching into app authority.
