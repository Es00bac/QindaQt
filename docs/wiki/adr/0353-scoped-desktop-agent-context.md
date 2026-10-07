# ADR-0353: Scope desktop agent context through provider-owned grants

- **Status:** Proposed
- **Date:** 2026-10-07
- **Owners:** QindaTK agent SDK, application providers and desktop session composition
- **Supersedes:** None
- **Superseded by:** None

## Context

The [everyday desktop plan](../development/everyday-desktop-plan.md) asks an
agent to work with chosen files and a document, with visible access and revoke.
QindaTK already supplies semantic providers, revision checking, local CLI/MCP
and application receipts; Calc and Note already implement real document
actions and undo. Its v1 grants explicitly cover all cooperative clients
running as the same OS user. They do not identify one agent or task and are
not session-lock-aware. A desktop wrapper cannot turn that underlying broad
endpoint into a per-agent security boundary by hiding extra documents in UI.

Window commands, content grants, screen capture and remote input have
different authorities. The compositor's WindowManagement1 currently admits
the current Voice1 provider only; a desktop agent cannot reuse its bus name
or pass itself off as the voice process. Portal grants do not authorize
semantic document edits. Application origin and desktop-entry identity do
not grant any of these capabilities.

The [owning contract](../architecture/desktop-agent-context.md) records the
actual SDK seams, proposed typed protocol, limits, failure ordering and exact
first implementation packets. This ADR authorizes no completion claim for
production scoped access merely from its executable design model.

## Decision

1. Reuse QindaTK AgentProvider state/actions/execute and application engines.
   Add an explicit opt-in scoped contract to its public agent boundary, with
   caller admission, grant epoch and authenticated session privacy supplied
   through small borrowed ports. Preserve v1's documented cooperative mode
   and existing grants. Never translate a narrow new grant into v1 Read/Edit.
   The scoped path is unavailable on an unextended provider.

2. Applications remain the grant and semantic mutation authority. A desktop
   coordinator composes a chosen task, projects current grant metadata and
   routes requests over authenticated, task-bound channels. It cannot read
   arbitrary files, mint a provider grant or widen action sets. Application
   UI authorizes a task/provider/resource/scope/lifetime combination; no
   agent RPC, document content, focus change or desktop title can authorize it.
   Revocation uses the owning application; the coordinator holds only a
   projection, not a conflicting grant database.

3. Bind each request to the session incarnation, agent-task transport
   principal, admitted provider process incarnation, provider lifetime UUID,
   grant ID/epoch and resource selector. Keep QindaTK's opaque revision
   unchanged. Content reads and mutations require exact current scope,
   revision and privacy. Unknown identities and ambiguous endpoints fail
   closed; never fall back to scanning v1 sockets or whichever app is focused.

4. Separate observation, content reading, proposing an action, committing an
   allowed semantic edit, capture and remote input. A proposal is inert typed
   data. The provider retains an immutable issued record with its original
   canonical arguments, binding and deadline; a client-computed digest is not
   proof of issuance. Commit resolves and consumes that owner record, carries
   its exact digest and revision, and remains inside an
   application-issued edit scope or exact one-operation approval. Edit never
   implies save/export/send/publish, shell execution, credential access or
   another document. Application validation, undo and partial-result semantics
   remain authoritative. No implicit screenshot/keystroke fallback.

5. Revoke on task/provider/session replacement, selected resource closure,
   expiry, user revoke and native lock/unknown authority. Use public ordinary
   compositor attachment and NativeLockStateMonitor through desktop-owned
   ports; the generic SDK gains no compositor-private dependency. Admission is
   checked before lookup, execution and disclosure, including cached receipts
   and event polling. Unlock cannot revive old grants. Already executed work
   is not falsely reported cancelled and already delivered data cannot be
   recalled.

6. Preserve idempotency across grant changes within one provider/task
   incarnation. Ledger keys do not contain grant epoch; revocation does not
   erase executed-request tombstones. Replay requires current authorization,
   identical target/action/arguments/revision and the same request ID; it
   returns a historical receipt, not a new mutation. At the bounded ledger
   limit refuse new commits rather than evicting and reexecuting. Restart
   destroys the old authority: an uncertain request cannot automatically
   replay into a new provider.

## Consequences

The first production outcome is one task reading one explicitly chosen
provider snapshot, with lock/revoke/replacement proof and visible owner
controls. File Manager selections and Text Editor providers follow through
their own public modules, alongside existing Calc/Note providers. Cross-app
editing is a later bounded extension; there is no multi-document atomicity
promise.

QindaTK supplies reusable policy/transport hooks; the desktop owns task/session
composition; applications own grants/resource selection/semantics. Native
portal permission persistence remains frontend-owned and active capture/input
sessions are stopped through their existing owners. The desktop access page
may present these together but cannot treat deleting a remembered portal
permission as ending a live semantic grant or stream.

The threat model covers malformed peers, stale transport, misrouting, replay,
prompt injection and scope widening by cooperative or hostile agent clients.
It does not claim isolation from a root attacker or arbitrary same-UID
process able to debug the coordinator. Optional stronger sandbox isolation
requires a separate threat model and evidence.

## Revisit when

A provider needs asynchronous mutations, durable deduplication across crashes,
a cross-document transaction, remote agents without a local task bridge,
persistent/background grants, or authority to send/export/publish data.
