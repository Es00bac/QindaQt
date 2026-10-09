# ADR-0371: Delegate Android package setup to QindaPortage

- **Status:** Accepted
- **Date:** 2026-10-08
- **Owners:** Android Applications and QindaPortage
- **Supersedes:** None; narrows the current release discoverability of ADR-0369
- **Superseded by:** None

## Context

The Android Applications UI originally injected a null runtime port while its
native desktop entry remained visible. A package must not advertise a management
window with no actionable setup/recovery. Existing QindaPortage owns package-plan
preview/approval/helper execution and already accepts native `--page packages
--select <category/name>` options. It does not provide an admitted Android
registration/materialization service on the desktop main branch.

Read-only source inspection also showed that stock Waydroid app listing changes
container freeze state, and Binder launch acknowledgement proves no Running
process/window. Direct CLI polling or a success-exit shortcut cannot fill that
missing authority.

## Decision

Keep AndroidApplications.desktop `NoDisplay=true` until an admitted actionable
production composition is qualified for menu discovery. Preserve the manual
binary for source/UI evidence. No user data or installed guest state changes.

Add a borrowed GUI-thread QObject `SetupPort` alongside the separate runtime
port. It accepts only closed Runtime or Images targets. The pure owner CLI mapper
produces `--page packages --select app-containers/waydroid` or
`--page packages --select app-containers/waydroid-vanilla-images`. Unknown targets
refuse. There is no arbitrary atom/URL/Exec, --ask, privileged apply or guest
initialization call.

The production native adapter uses only the fixed protected root-owned regular
`/usr/bin/qindaportage` beneath protected fixed directories. It supplies a bounded
native session/locale/display environment without loader or QindaPortage model
overrides. Native planning dispatch is confined to this adapter target; the
controller and QML borrow only the public SetupPort. QindaPortage retains plan
preview, user approval, helper validation and all installation authority.

Accepted dispatch records setup handoff feedback; it does not alter RuntimeState,
app registration, Running, materialization or window count. Unavailable/refused
or destroyed setup owners fail closed. No automatic retry occurs. Existing
asynchronous guest launch is unchanged and remains unavailable without its
separately admitted runtime port.

## Consequences

This supplies actionable package selection in the manually opened management
window while preserving the release menu guard. It does not install Android,
validate/materialize an APK, activate a runtime or publish a registered app.
Current image recipes/producers still require their own accepted packaging and
runtime authority. Guest app association, mixed containers and Audio1 remain
open. The owning [Android Applications page](../apps/android-applications.md)
records exact stock source findings and remaining gates.

Short controller/UI/compiled smoke uses injected setup ports and never starts
QindaPortage, Waydroid, a root helper or a VM. The native dispatcher follows the
existing CLI contract; its actual graphical package-plan journey remains an
installed release/user-action gate.

## Revisit when

An admitted materialization/runtime producer can compose the complete setup
flow, QindaPortage changes its native page/selection contract, or the protected
software-manager path must support a qualified platform alternative.
