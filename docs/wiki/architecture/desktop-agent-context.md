# Scoped desktop-agent context

This is the **proposed ED-17 contract**, governed by
[ADR-0353](../adr/0353-scoped-desktop-agent-context.md).
It specifies the missing join between existing providers, a selected task and
desktop privacy; it is not a production service or universal semantic access.

## Actual source evidence

Read-only inspection used QindaTK
`393c1ce5c075b3a4dfc90b7b0b58ae80ab3fd7bb`, QindaOffice
`f1f3492b38e88c04ac2e2aee724c89874fdd67ea` and desktop ED-20 branch base
`f601445c4`. Toolkit checkout matches its hub; its unrelated .obsidian
directory was preserved. No toolkit/Office source or live grant was changed.

| Existing owner/source | Actual contract | ED-17 consequence |
| --- | --- | --- |
| QindaTK src/agent/agent_provider.* | Borrowed same-thread provider, lifetime UUID; generation plus state hash revision; state/actions/execute copies | Keep provider IDs/revisions opaque and reuse semantic engines |
| QindaTK AgentHost | App-controlled Off/Read/Edit; no remote grant; permission before cached receipt; 1024 non-evicted receipts and 256 metadata events | Reuse checks but add explicit principal/scope/privacy admission for scoped mode |
| QindaTK AgentServer | User-only Unix socket; private directory; 32 connections; 256 KiB input and 8 MiB output/backlog; synchronous host dispatch | Same UID is cooperative access, not one agent's authority |
| QindaTK tools/agent/qinda-agent | CLI/MCP, scans local sockets, routes granted document UUID, reconnects per request | Scoped adapter needs pinned channel/peer; no v1 discovery fallback |
| Office office_quick agent_runtime/document_host_agent | Per-document menu grants; CLI --agent-read/edit also grants future windows in that process | Do not reuse process-wide launch flags for selected-task scope |
| Calc/Note providers and docs/agent-interface.md | Real engine validation, current revision, native undo; Note page/selection/draft and bounded transcripts | Semantic content is broad; selected-only exposure requires an explicit provider projection |
| NativeLockStateMonitor | Authenticated current attachment/owner; unknown denies content; admission rechecked by getter | Inject public privacy gate; GUI focus or a bool sent by an agent is insufficient |
| WindowManagement1 | Voice-owner-specific admission and expiring captured subject | Agents need separately reviewed admission, not borrowed Voice1 authority |
| Native portals | Family-specific consent/session lifetime; frontend remembers restore tokens | Semantic grants never substitute for capture/input/file permission |

The existing toolkit protocol does not identify individual agents, and Office
explicitly describes broad local grants. That is an intentional existing
boundary, not evidence it already implements ED-17. Existing native contracts
are [window commands](window-management-commands.md),
[native lock](native-session-lock.md),
[portal foundation](portal-foundation.md) and
[remote input](portal-remote-input.md).

## Ownership and dependency direction

Proposed QindaTK scoped policy is transport-neutral Qt Core below AgentHost.
A small admission port supplies task principal, current grant and privacy
generation; provider code/UI owns grant creation and revocation. A scoped
transport adapter supplies immutable kernel-admitted caller context to a
separate host entry point; it never trusts principal fields from JSON.
Providers/ports remain borrowed and same-thread, destroyed after pending work
is retired; the host uses lifetime-safe references before callbacks.

Desktop composition owns task enrollment, private channels, public native
session attachment/lock observation and a read-only aggregate of grants.
It never includes Office/File Manager/terminal implementation headers.
Applications implement their own resource selectors and explicit projection
over existing engines. CLI and MCP are adapters over the same scoped client,
not alternative authorization paths. Public interfaces are added by their
owners; no copied host or second universal agent server.

The proposed task bridge is launched/enrolled by a human-authorized local
workflow and receives a private inherited channel. A coordinator binds that
channel to an immutable task principal and exact process lifetime, not its
display name, model claim, PID string or executable name. Agent replacement
creates a new principal. Provider registration also binds an authenticated
live application channel to an instance identity. A same-UID socket pathname
or claimed UUID alone cannot establish this binding. Reconnect requires a
fresh peer/lifetime join; no cached path is authority.

The application UI selects the enrolled task from the trusted local
coordinator and confirms its resource/scope. Agent RPC may request that a
consent UI be offered but cannot accept it or prepopulate hidden authority.
Provider-issued grants are returned only over the admitted task channel.
The coordinator is an intentional deputy for those exact grants; the host
checks its delegated principal and current provider grant on every call.
Tokens never appear in argv, environment, logs or model-visible content.

## Typed records and limits

New wire negotiation advertises a separate scoped capability/version; do not
change qindatk-agent/1 semantics. V1 and scoped requests have distinct host
dispatch paths. Unsupported peers return ScopedAccessUnavailable. Existing
cooperative local grants continue to mean what their UI says; enabling scoped
sharing does not silently revoke or narrow them. The access UI explicitly
reports if broad local access is also enabled. Scoped grants are never
accepted on the broad local endpoint.

| Record | Fields and invariants |
| --- | --- |
| Principal | Session incarnation + task UUID + transport incarnation, supplied by admitted channel; no model/provider subscription claims |
| ProviderBinding | Authenticated app process incarnation + SDK provider lifetime UUID; app title/desktop ID/window PID are presentation only |
| ResourceRef | Provider-owned immutable selector plus selector generation; optional exact window incarnation for user orientation only |
| Grant | Provider-issued ID, non-wrapping epoch, exact Principal/ProviderBinding/ResourceRef, scope/action allowlist, monotonic deadline and privacy epoch |
| Context | Context UUID, exact binding/grant/resource, opaque provider revision, immutable bounded snapshot and omission metadata |
| Proposal | ID, original grant/privacy epochs, context revision, exact action and canonical arguments, provider-computed digest, monotonic expiry at most 120 seconds; no side effects |
| Commit | Request ID (1–128 safe characters), proposal identity/digest, expected opaque revision and current grant; no arbitrary RPC or command |
| Receipt | Request/binding, Accepted/Completed/Partial/Failed/Cancelled/OutcomeUnknown, changed targets, before/after revision, optional owner undo reference |
| Event cursor | Provider incarnation + decimal sequence; refresh-required on gap; no global sequence inferred |

Bounds: at most 32 enrolled tasks, 64 grants per task and 256 live grants per
coordinator; 16 provider resources per context; 256 KiB encoded input per
request, 1 MiB per resource snapshot and 4 MiB aggregate context; 8 MiB maximum
queued output per peer. Strings are valid UTF-8 with field limits (labels
512 bytes, resource selectors 4096, revisions 256, action IDs 128). Typed
schemas reject unknown fields, enum values, duplicate IDs, nesting beyond 16,
NaN and invalid integer ranges before allocation or authority lookup.
Truncated context declares omitted fields/items; it never silently shortens a
target selector or revision. Provider limits may be smaller.

All mutable object fields are copies. No QObjects, file descriptors, credentials
or private paths are serialized to a model. Resource descriptors referring to
files are owner-issued handles, not path authority. File Manager validates the
exact selected item and generation at read/open/commit; it uses stable opened
objects when needed, and refuses replacement instead of following a new symlink
or renamed target. Selection metadata permission does not include file bytes.

Grants are volatile, no longer than the task lifetime and at most eight hours
without explicit renewal; renewal replaces the epoch and cannot revive revoked
contexts/proposals. Default grants last only the selected task/document session.
No background/persistent grants in this version. Opaque revisions keep the
SDK's generation/hash contract: focus/view/selection change may invalidate a
snapshot; do not reduce it to mtime, integer comparison or content hash alone.

## Scope and privacy

Scopes are separate: WindowMetadata, SelectedMetadata, DocumentRead and
SemanticEdit. Read can produce inert proposals; edit is an explicit
application action allowlist and resource boundary. Capture and RemoteInput
are references to independently authorized standard portal sessions, not
semantic scope bits that can mint portal permission. Terminal selection/output
is a separate explicit terminal-owned provider; cwd or visible window title
does not authorize shell history or process-environment harvesting.

The first journey grants a provider-owned snapshot. Calc's current whole
workbook-limited snapshot and Note's page/metadata/transcript projection must
be described accurately in consent; never call them selection-only until the
provider implements that narrower projection. File Manager selected items and
Text Editor selected/current document require their own providers. Passwords,
keyrings, authentication surfaces and secret clipboard data are excluded;
a provider must omit them before constructing output, not rely on downstream
redaction. No screenshot, accessibility dump, clipboard scrape or keystroke
fallback is implicit.

An explicit document grant is independent of focus and can survive minimizing
the window while the document stays open. It cannot follow focus to another
document. Selection-scoped context freezes the selected IDs/generation; a new
selection invalidates that context and any proposed change. An application
switching to a different page/sheet must expose its current revision and
reject older proposals.

Only an authenticated current NativeLockStateMonitor Unlocked observation may
admit content. The desktop supplies the SDK a constructor-visible privacy
port tied to ordinary attachment and current lock epoch. Initial Unknown,
lock, lost attachment or owner replacement first invalidate grants/contexts/
proposals and pending responses, then clear retained payloads and disconnect
task content channels. A new Unlocked observation requires fresh human grant;
it cannot restore prior authority. Coarse unavailable status discloses no
document existence/title.

Admission is rechecked before state()/actions(), before mutation, before
serializing a reply/event/receipt and immediately before queued output is
released. Owner code must avoid nested event loops inside synchronous
execute. If authority changes during a reentrant provider call, its returned
payload is suppressed; report generic OutcomeUnknown if work may have
occurred. No automatic undo after revoke: it could overwrite human work.
Bytes delivered before revoke cannot be recalled; the UI must not promise
that model-side copies are erased. Future streaming requires separately
qualified per-chunk gating.

## Propose, commit, replay and results

1. Read exact granted resources at a current revision. Context data, website
   text, file contents and tool results are untrusted data, never authority.
2. Ask the owner to validate a typed proposal against an advertised action
   schema, target selector and current revision; no provider execute yet.
   The provider canonicalizes arguments and binds the proposal digest to every
   identity/revision field. Store at most 64 pending proposals per task.
3. Commit only inside a currently granted edit allowlist or a provider-created
   single-operation human approval bound to that digest. A changed proposal
   requires a new validation/approval; standing edit grants avoid redundant
   approval for each permitted action.
4. Immediately recheck principal, provider lifetime, grant epoch, deadline,
   privacy, selector, revision, action enablement and schema in the owner's
   thread. Execute through existing application engines, never a generic
   desktop menu action.
5. Return the owner receipt. Accepted means admitted, not finished. Partial
   names actual changed targets and failures; Cancelled requires proved
   no further work. Disconnection/timeout after dispatch is OutcomeUnknown.
   No inferred success from process exit, proposed text or missing window.

Deduplication key is provider incarnation + task principal + request ID, not
grant epoch or socket pathname. The ledger binds original resource, action,
canonical arguments, revision and proposal digest; grant renewal cannot erase
an execution tombstone. Reserve the request ID before invoking provider code
to prevent reentrant duplicate execution. Conflicting reuse is rejected.
An unexecuted proposal from a replaced grant/privacy epoch is stale even after
regrant. Only an already-recorded request can return its authorized historical
receipt across grant changes. An identical pending retry says Pending; identical completed replay requires
current matching scope and returns the historical receipt plus current
revision, with no execute. A read-only downgrade cannot replay edit result
payloads; a separate consented audit view may later expose redacted metadata.

At 1024 request records per provider host, refuse new commits rather than evict
executed IDs. Revocation suppresses receipt disclosure but does not erase this
bounded ledger. Provider restart ends the ledger/UUID; an uncertain old
operation may be inspected/reconciled by the user but never replayed into the
new provider automatically. Cross-app contexts contain independent revisions;
multi-app commits are a sequence of explicitly separate actions with individual
receipts, not an atomic transaction.

Undo uses the existing application-owned reference and a fresh permitted
request. Newer human changes may invalidate undo; the coordinator cannot
force it. Save, export, send, publish, app launch, file mutation and terminal
execution are excluded from generic SemanticEdit. Add them only as separate
reviewed owner actions with explicit scope/consent contracts.

## Production slice routing and acceptance

This is executable dispatch guidance, not assigned ownership outside the
manager's queue. Each implementer receives fresh hub bases and isolated
worktrees; do not modify the current shared toolkit/Office checkouts.

| Slice / owner | Exact source boundary | Deliverable and acceptance |
| --- | --- | --- |
| ED17a / QindaTK | new src/agent scoped values/policy beside agent_host; focused tests/cpp fixtures; docs/agent-interface | Add principal/resource/grant/privacy ports and separate scoped host entry; retain provider APIs and v1 behavior. Test wrong caller, revoke-before-replay, generation/revision ABA, no-lookup-before-admission, reentrant duplicate and non-evicting capacity. No desktop dependency. |
| ED17b / desktop Platform | new services/desktop_agent_context public values/client/composition; public session_lock_state and compositor_attachment consumers only | Enroll one task and one provider over private authenticated lifetime channels; projected grants, lock gate and no v1 fallback. Private-bus/socket replacement, wrong-peer, stale-unlocked reply, timeout and pending-output purge tests. |
| ED17c / Office owner | office_quick agent_runtime/document_host_agent plus Calc provider public adapter | One visible task/document Read grant; existing provider-issued snapshot and undo engine unchanged; no broad initialAccess toggle. UI grant → read → revoke → denied, provider restart and real private native lock gate. |
| ED18a / File Manager owner | new provider module through selected-items/public file boundary, not shell private model reach | Exact selected IDs/metadata first; separate file content scope, hostile link/replacement checks and bounded snapshots. |
| ED18b / Text Editor owner | editor-owned provider over document controller/edit/undo | Read-only document first; then bounded semantic edit/proposal/receipt with Unicode, unsaved data, stale revision and named undo evidence. |
| ED17d / QindaTK CLI + desktop Shell | qinda-agent additive scoped CLI/MCP mode; purpose-specific access controller/QML | Same admitted principal and typed routes in CLI/MCP; visible selected resources, deadlines and revoke result. No endpoint-directory discovery in scoped mode and no read before grant. |
| Later commit packet / provider owners | scoped host ledger and existing engine actions | Exact proposal commit, mid-call revocation, historical receipts, reconnect/unknown outcome, edit downgrade, denied export and native undo. |

The first integrated acceptance is a disposable Calc document plus one
enrolled local task: grant Read in owning app UI, read only that resource,
change focus without retargeting, revoke and deny read/events/receipt,
replace provider/transport and reject old context, then lock through a private
native harness and prove pending payload cannot emerge after unlock without a
new grant. This is not a physical-user lock trial. Source/private acceptance,
Portage package delivery and installed graphical/keyboard/MCP journey are
separate evidence rows. ED-17 remains open until production checks pass.

The pure negative model is
`python3 tests/design/desktop_agent_context_contract.py`.
It assumes authenticated fixture identities/privacy facts and exercises policy
ordering; it does not authenticate OS peers, enforce a real application grant,
drive an LLM or prove a usable graphical workflow. Documentation gates are
`python3 tools/validate-docs` and `mkdocs build --strict`.
