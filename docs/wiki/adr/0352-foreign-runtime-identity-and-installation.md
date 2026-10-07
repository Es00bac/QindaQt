# ADR-0352: Separate foreign application identity, runtime authority and package ownership

- **Status:** Proposed
- **Date:** 2026-10-07
- **Owners:** Platform, ApplicationCatalog, QindaLutris and Shell
- **Supersedes:** None; proposed ED-20 installation policy narrows ADR-0275 for future managed application flows
- **Superseded by:** None

## Context

The [foreign application plan](../development/foreign-app-integration-plan.md)
requires independently usable Android and Windows applications, green/blue
origin identification and one ordinary Applications catalog. Existing desktop
IDs, StartupWMClass and executable-name recovery are presentation hints
([ADR-0303](0303-resolve-window-identity-through-application-catalog.md)),
not authenticated app identities. Waydroid may multiplex guest apps through a
host renderer; Wine services may be shared by launches using the same prefix.
Neither a host PID nor a per-launch process scope establishes independent
application ownership in those cases.

[ADR-0275](0275-qindalutris-installs-games-and-manages-pinned-proton.md) permits
direct downloads and private Proton installations. The owner's current
Portage-only policy requires a different installation path for the new
managed experience; copying that downloader into Applications would violate
that policy. Existing private state must be preserved.

The [owning architecture page](../architecture/foreign-applications.md)
records typed contracts, actual feasibility evidence and unresolved gates.
This ADR is proposed: no Android image, app-window proof or production
adapter has been delivered by this decision.

## Decision

1. **One catalog, distinct facts.** Extend the neutral ApplicationCatalog
   boundary with opaque registered app identity, execution origin and recipe
   revision. Launcher, File Manager Applications, tasks and chrome project it.
   Native XDG shadowing/Hidden behavior remains. A user override can change
   presentation/ordinary launch but cannot inherit authenticated origin or a
   managed launch binding just by copying an ID. A managed row's executable
   action resolves through the adapter against its exact registration revision;
   it never executes an arbitrary desktop key as a managed action.

2. **A registration is not a window.** A package-authenticated recipe plus an
   adapter-confirmed per-user materialization creates a registered entry.
   A current compositor window association additionally requires live transport
   evidence tied to the session, adapter generation, runtime incarnation,
   launch and window incarnation. Every origin observation rechecks the live
   launch relationship; lifecycle retirement withdraws dependent associations
   before a fresh launch can be admitted. Registration IDs, desktop keys, titles,
   WM_CLASS, argv, environment variables and guessed process ancestry are
   insufficient. Failed or unavailable evidence yields unknown association,
   never a green/blue authenticated app assignment.

3. **Keep process authority in its owner.** Platform owns an asynchronous
   foreign-runtime broker and platform-specific adapters; shell owns normal
   window/container behavior. The broker is bound to the supervisor's current
   session. A supervisor-created private descriptor authenticates each
   adapter instance, and the compositor receives broker assertions only on
   its admitted channel. Neither a well-known bus name nor same UID alone
   authenticates a producer. A generation value is a freshness label, not a
   transferable credential. Android needs a runtime-owned task/surface mapping;
   Windows needs compositor-observed client evidence joined to an exclusively
   owned prefix/process lifetime. Stock hints remain hints until this mapping
   is qualified.

4. **Isolate lifecycle before exposing force stop.** Default Windows managed
   registrations use a private prefix per app. Launch instances may share that
   app's prefix; a verified prefix scope is owned by the app, not by its first
   launch. Shared/adopted prefixes permit close-window actions, but app force
   stop is unavailable unless an independently qualified app-specific target
   exists. Never issue generic wineserver kill, runtime stop or scope stop
   which can kill another app. Android stop targets the registered package and
   guest user inside the exact current runtime; shared-UID or otherwise
   inseparable applications do not advertise independent force stop.

5. **Portage owns software; users own data.** Portage owns runtimes, helpers,
   image versions, APK/MSI/EXE payloads, recipe manifests, units and system
   configuration. QindaPortage owns privileged package application. A
   versioned installed recipe authorizes a deterministic, unprivileged,
   per-user materialization into guest storage, with a verified receipt.
   Guest files are tracked by that recipe/receipt, not misrepresented as
   individual Portage CONTENTS entries. No ebuild executes into an active
   user's guest or home. Updates are explicit package revision plus
   materialization transactions; no store/self-updater downloads executable
   software. Remove registration, remove packaged software and delete private
   data are separate actions.

6. **Reuse QindaLutris without importing its game model.** Extract the bounded
   Wine/umu planner, exact runner pin/environment policy and process supervisor
   into a public non-game runner boundary. Existing QindaLutris consumes the
   same implementation. The foreign adapter never links its Game/Title store,
   UI, downloader or private headers. New managed flows accept only
   Portage-owned runners and recipes. Existing untracked registrations are
   reported as unmanaged and preserved; adoption requires a matching packaged
   artifact and explicit recipe, not a new receipt pasted over arbitrary
   private bytes. Disable or redirect direct-install/update affordances in the
   affected managed composition before claiming Portage-only delivery.
   Migration of the existing game UI is a separate coordinated owner change;
   this proposal does not silently delete games or change live runner state.

## Consequences

Origin describes execution platform, not a permission grant or application
trust rating. Existing window commands, portal consent and QindaTK semantic
grants remain their own authorities. One app can have several independent
windows and child prompts; one container can contain several origins.

The first support set is deliberately small: package-tracked Wine notepad and
wordpad as independent non-game fixtures, then specifically packaged Android
apps whose ABI, signing identity and task/surface mapping are verified. Proton
is offered only for recipes qualified against its exact build. There is no
claim for arbitrary APKs, MSI installers, stores, DRM or driver-dependent apps.

Implementation requires distinct pure values/policy, codec, transport,
registration persistence, materialization and presentation collaborators.
Strict snapshots publish atomically; timeout is not successful cancellation;
owner loss fences all outstanding operations. The executable design model
tests these policy edges but cannot prove the OS evidence producer.

Android proof currently requires Portage runtime/image/app provisioning.
Windows proof requires a private nested-display lease. Both two-app window
proofs and independent critical review remain ED-20 acceptance gates.
Review can accept the contract for staged implementation while leaving those
feasibility rows open; it cannot mark ED-20 or ED-21–24 complete.

## Revisit when

A tested stock runtime provides authenticated per-app surface mapping; a
shared-prefix application can demonstrate safe independent stop; an app
requires self-updating executable content; or new privilege/data-sharing
needs exceed the recipe materializer and existing portals.
