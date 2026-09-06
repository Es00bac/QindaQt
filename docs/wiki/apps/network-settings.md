# QindaQt Settings — Network route

`qindaqt-settings --page network` is the first-party, secret-free Network
settings surface. It is a modular Settings Center route composed exclusively
through the public Network1 client boundary. The route observes bounded
network truth and offers only actions that the current authoritative snapshot
admits. [ADR-0055](../adr/0055-compose-network-settings-through-network1.md)
records this composition choice.

## Truth shown by the route

The route presents the current public snapshot in four groups:

| Group | Public truth | Interaction |
| --- | --- | --- |
| Radio state | Wi-Fi and WWAN software/hardware state | Read-only |
| Devices | Bounded interface, type, state, active known-network id, and capabilities | Disconnect when that active device is currently admitted |
| Saved networks | Derived known-network id, presentation name, security, and capabilities | Connect the existing stored profile when currently admitted |
| Wi-Fi access points | Presentation-safe SSID, signal, security, saved-network relation, scan freshness, and secret-agent presence truth | Connect a supported visible unsaved network when currently admitted |

Connectivity and scan state remain visible alongside the inventory. The route
model retains the exact owner, epoch, and revision for lineage gating and
focused diagnostics; the ordinary page does not render the broker's technical
owner identifier. Empty states distinguish no observed values from loading,
service unavailability, retained stale truth, and a failed operation. The
route never exposes NetworkManager object paths, hardware addresses, private
service values, or unbounded backend diagnostics.

## Owner, freshness, and operation lifecycle

One process-lifetime route model projects one public `NetworkClient`:

- Loading has no authoritative inventory and offers only reload.
- Ready inventory is tied to the exact public owner, epoch, and revision.
- A validated degraded snapshot may retain inventory, labels it stale, and
  makes all mutation controls read-only.
- Public-owner loss clears inventory immediately. Replacement truth appears
  only after the new exact owner publishes an accepted snapshot; a late reply
  from a retired owner cannot repopulate or complete the route.
- A duplicate snapshot does not disturb current truth. Retired epochs remain
  fenced across A→B→A owner sequences.

Reload asks the public client for authoritative truth. Scan, saved-network
connect, visible-network connect, and disconnect carry only public typed
identifiers and the initiating lineage. The first-use action carries an opaque
access-point id derived from public device/BSSID truth; it carries no SSID or
credential.
The route relies on public intent admission and disables an action when the
snapshot lacks the corresponding capability, the target is not eligible,
truth is stale, another operation is pending, or an authoritative snapshot
refresh is scheduled or in flight. Both displayed availability and dispatch
consume the same projected row predicate. The visible-network invokable
revalidates that row and returns a typed rejection without calling Network1
when route-owned secret-agent presence or any public admission fact disables
Connect. A successful reply triggers a refresh and does not optimistically
edit any list. Timeout, owner replacement, or another uncertain result stays
visible and is never automatically replayed.

## Credential and authority boundary

The route has no password, passphrase, certificate, private-key, secret-agent,
profile-editor, or radio-mutation API. Connecting a saved network activates its
existing profile. Connecting a visible unsaved Open, WPA2 Personal, or WPA3
Personal network asks Network1 to create and activate a minimal profile. Hidden,
WEP, and enterprise first-use connections remain unsupported.

NetworkManager may separately consult `qindaqt-network-secret-agent`; Network1
and this route never receive that exchange. The route observes only ownership
of the agent's presence-only session-bus name. Beside each unsaved network it
states either “No password is required”, “A password prompt will appear”, or
“No secret agent running — secured networks cannot prompt”. A secured Connect
action is disabled when the agent is absent; Open connection remains available
when all public Network1 admission checks pass.

Neither QML nor the route model imports private Network service headers, libnm,
or NetworkManager. One private presence observer uses Qt D-Bus name-owner
truth; it has no object path or callable interface and never receives a
credential. See [Network service architecture](../architecture/network-service.md)
and [Network secret agent](../architecture/network-secret-agent.md).

## Responsive interaction and accessibility

The page uses only QST-1 semantic roles and QindaQt.Controls. At compact sizes,
the same ordered content remains vertically scrollable. Page Up/Page Down and
Ctrl+Home/Ctrl+End move through it, and changing keyboard focus reveals the
focused control. The page declares its first eligible action—Scan or Reload—as
the Settings host entry target and keeps forward and reverse Tab navigation
within the route. Closing Settings remains a single window-level action; the
route does not duplicate it with a page button.

Inventory cards expose accessible names, descriptions, roles, current state,
and disabled state. Scan, reload, connect, and disconnect have explicit
accessible action names. Stale, unavailable, pending, and error notices use
truthful visible text rather than color alone.

## Composition and package boundary

The closed Settings route registry maps only the canonical `network` id to the
compiled Network component. Unknown and path-like startup values exit before
the Network transport or model is constructed. `qindaqt-settings` owns one
public Qt Network transport, client, and route model on the session bus for its
process lifetime; only the QObject route projection crosses into QML.

The `SettingsAppearanceRuntime` install component includes the Network QML
module, linked public Network client/transport modules, and the executable's
relative runtime paths. The relocated-package fixture withholds the installed
Network module while the developer tree remains present and requires startup
to fail, then restores it and proves all registered routes from only the staged
prefix.

## Verification and stopping point

Focused selection:

```sh
ctest --test-dir build/dev --output-on-failure --no-tests=error --parallel 1 \
  -R '^qindaqt\.(network-(settings-model|settings-agent-gate|settings-model-adversarial|page|settings-boundary|settings-boundary-poison)|settings-(route-registry|navigation-controller|navigation-page))$'
```

- the model row proves bounded projection, exact lineage, public capability and
  secret-agent-presence admission, one-shot scan/saved-connect/visible-connect/
  disconnect dispatch, and secret-free errors;
- the agent-gate row proves a secured unsaved network with absent-agent
  `connectAvailable=false` cannot dispatch through the invokable and returns
  the typed `secret-agent-unavailable` rejection;
- the adversarial row proves malformed/stale handling, owner loss and A→B→A
  replacement, ignored late replies, mismatched operation lineage, redaction,
  and absence of credential/radio mutation APIs;
- the page row runs under fatal QML warnings and proves accessible controls,
  visible-network prompt truth/action wiring, stale/owner-loss fail-closed
  behavior, compact focus reveal, and keyboard cycling; and
- boundary and poison rows reject private service headers, Qt D-Bus outside
  the exact presence observer/model seam, callable D-Bus from that observer, a
  radio invokable, credential text input, or a private service dependency.

The same selector runs in strict Debug and Release builds. Settings Center's
route and installed-package rows additionally prove canonical startup,
complete relocated construction, withheld-module failure, and hostile route
rejection. Tests use fake public transports or absent private buses and never
touch host networking.

This route still does not claim credential payload handling, arbitrary profile
creation/editing, software-radio mutation, persistence control, a shell applet,
physical network/radio qualification, or session-runtime integration. Its only
profile-creation request is the fixed supported visible-network intent. The
separate process owns the bounded credential-entry claim.

## Recovery presentation

Fresh limited snapshots no longer say the connection information is stale.
The route reports stale information only after the client loses currentness.
Normal AP truncation does not disable connection controls. The password-prompt
notice is shown only when prompts are unavailable, and connection instructions
use user-facing language. Model fixtures inject a disconnected presence bus so
a running host credential agent cannot change their expected admission results.
