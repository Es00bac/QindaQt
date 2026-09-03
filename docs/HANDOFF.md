# Integration handoff

## Current baseline

Manager integration delta after the public baseline below:

- 2026-09-03T13:35:22-06:00 — Task List T2 repair `1e32f0a` is integrated at `431856a5` after Lauren Williams's terminal ACCEPT `0/0/0/0`; merged-count documentation reconciliation `328f8b5e` closes the only broad-guard failure. System-KWin verification passes 33/33 focused rows in both profiles, static gates, the complete 592-row broad-safe Debug set across the corrected rerun, and 8/8 serialized DesktopVirtual rows including both panel scenarios.
- 2026-09-03T13:35:22-06:00 — Tray S2 `b10692c` is shelved after Rózsa Péter's sole funded review rejected it `0/1/0/0`: a malformed live replacement changes registry presentation to degraded without notifying the controller, and no production acknowledgement path exists. Because both applets did not integrate, the named Task List + tray hosting lane was not opened.
- 2026-09-03T13:35:22-06:00 — Bluetooth pairing `7025a1c` is shelved after Kathrin Bringmann's final funded recheck rejected it `0/0/1/0`: duplicate window-context Escape shortcuts in the real Settings host are ambiguous, so an active prompt receives no cancel reply. No further repair lane is funded.
- 2026-09-03T13:03:46-06:00 — QQ-004.02's installed panel-visibility proof is restored on the host system KWin 6.6.6 by accepted repair `1d86b13` at merge `b5c8d87`; manager verification passes focused 125/125 Debug and Release, broad Debug 586/586, and all 8 serialized DesktopVirtual rows including both 1080p and WUXGA panel scenarios, with no live compositor survivor.
- 2026-09-03T12:57:47-06:00 — Program management handed from Claude to Codex; the full state, live lanes, recipes, and the install plan are in `ops/team/messages/manager-results-first/1788461867-claude-program-manager-handoff-to-codex.md`.
- 2026-09-03T12:47:31-06:00 — Session actions integrated at `70763c7`: a user can now lock, log out, suspend, restart, and shut down from the shell; the Wi-Fi secret agent starts with the session.
- 2026-09-03T12:30:19-06:00 — Clipboard applet hosted in the panel (`82e256d`); Wi-Fi join of visible networks (`c269830`). Panel dispatcher count is now eight.
- 2026-09-03T11:21:44-06:00 — Menu export (`a8c171c`), stage closure guard (`0a59236`), secret agent (`287ba6d`), and Clipboard Settings (`22d9f23`) integrated; whole suite green on the system KWin 6.6.6 roots (`sys-dev`/`sys-release`).
- 2026-09-03T10:23:54-06:00 — The KWin plugin ABI pin is now the host's system KWin 6.6.6 (`a5c1c20`, manifest v6.6.6); build with `/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake` (system Qt 6.11.1, KF6 6.27, KWin/KDecoration3/PlasmaActivities/KWayland/LayerShellQt 6.6.6; Weston still from the private prefix, confined to the parent compositor's environment). Verified on 6.6.6: compositor 33/33, nested compositor rows 16/16, desktop boot, package contract. Open: both panel-visibility nested rows fail on 6.6.6 (window-overlap-hidden phase) — repair lane open. A `wayland-sessions/qindaqt.desktop` entry is now installed for display managers.
- 2026-09-03T09:48:12-06:00 — Tray S1 transports integrated at `e3eacdd`; new-lane intake paused, in-flight candidates get one funded recheck each.
- 2026-09-03T09:40:00-06:00 — Task list T1 integrated at `90fe400`.
- 2026-09-03T09:33:11-06:00 — Main is nested-qualified again: boot.1080p and both panel-visibility rows green on `ff08ea2` after the staging and capture reconciliations; Clipboard applet C1 and Font F1 integrated.
- 2026-09-03T09:25:01-06:00 — Clipboard applet C1 (`2abc448`), Font F1 (`045f1a1`), and the panel-visibility interaction repair (`ff08ea2`) merged and under batch verification; the DesktopVirtual route staging moved to `tests/session/DesktopSessionRouteStaging.cmake` (`fb2b280`) to restore the source-shape gate.
- 2026-09-03T07:15:39-06:00 — Power Settings route (`a83f34e`) and Display Color C1 (`f6b14b2`, ADR-0066) integrated; any test that constructs the Settings Center's Main.qml in-process must link the static PowerBackend module (`3afa972`).
- 2026-09-03T06:58:16-06:00 — Nested desktop status on main: after the Global Menu and Settings-route merges the private desktop stage lacked libqindaqt_global_menu_qml.so and the Audio/Bluetooth/Power route modules (fixed at `e51372a`, `197f104`; `desktop.virtual.boot.1080p` green again); the two `desktop.virtual.panel-visibility.*` rows still fail on main because the hidden left panel stays mapped at zero size with the Global Menu hosted — an interaction repair lane is open. Treat main's nested panel proof as red until that lands.
- 2026-09-03T06:33:14-06:00 — Text Editor S2 integrated at `28bcd37` (ADR-0065); ADR numbering is assigned at integration when lanes collide.
- 2026-09-03T06:07:45-06:00 — Terminal S2 (search, links) integrated at `9e3422e`.
- 2026-09-03T05:37:29-06:00 — Global Menu G2 (`7de332f`) and File Manager S1 (`0b97954`) integrated; reconciliations `a1bac21` and `4609beb`; the dispatcher now hosts seven built-in applets and every dispatcher-level QML row needs a stub under `tests/shell/qml/imports/QindaQt/Shell/<Module>/` for each module `BuiltinAppletContent.qml` imports.
- 2026-09-03T04:58:12-06:00 — Bluetooth Settings route integrated at `500a33e` (merge `111dedb`); Settings Center order is now appearance, display, network, customize, audio, bluetooth.
- 2026-09-03T04:42:48-06:00 — Controls visual fixtures pinned at `5cf24a2`; the broad safe suite now runs the visual rows again (465 rows).
- 2026-09-03T04:32:48-06:00 — Audio Settings route integrated at `fac8d6a` (merge `f6d47db`); Bluetooth Settings route accepted and merging; Global Menu G2 `f7a49c5`, panel-visibility proof `a52561f`, File Manager S1 `61283bf`, and the controls-fonts repair `b7b5208` are in exact review; Text Editor S2 and Terminal S2 lanes started; GLM is limited until 20:25 and Kimi has recovered.
- This integration merges exact accepted compositor identity descendant
  `8505bdbd09961863538616efd3dd8e7ef834a126` at `58496e8` (ADR renumbered to 0063) and exact accepted
  launcher hosting candidate `7ecdb36f53c60ea03bea40197f17c9cc6bf9b9e1` at `6f5e213`. The authenticated
  boundary now publishes active-window identity facts to the bound shell owner with an exported, validated
  change signal; the launcher applet renders in the production panels with a closed `LauncherAppletRuntime`
  component. Fresh merged-tree Debug and Release each pass compositor 36/36, window-actions/identity and
  client rows, launcher 17/17, and the integrity/runtime/closure/installed rows under host-unset isolation;
  the sixteen nested KWin rows pass serially; the broad safe Debug suite passes 430/430; 127-document validation, strict MkDocs, source shape,
  diff, JSON, and Team Board 16/16 pass. QQ-004.07 advances WIRED → EXECUTABLE.

- This integration merges exact accepted Terminal S1 descendant `00f2db99ce13df2f426abd93e277de3d33411041`
  at manager merge `00b4f45`. Sessions are bounded, each owns its PTY/child, and shutdown completes only
  when the captured process group is verifiably empty; profiles stay within the existing launch policy;
  Settings1 persistence presents asynchronous apply outcomes accessibly and never replays; argv is
  preserved exactly. Dina St Johnston (OpenAI Codex) accepted the second repair at `0/0/0/0`. Fresh
  merged-tree Debug and Release each pass the terminal selector 15/15 under host-unset isolation; the broad safe Debug suite passes 427/427;
  126-document validation, strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass.

- This integration merges exact accepted Launcher L1 descendant `26f366a4a2ab14fc341407f015d21685b8c8415f`
  at manager merge `71900bd`. The scanner, persistence, execution, and compiled applet adapters compose the
  pure L0 model behind injected seams; every adapter test now runs Core-only or offscreen with host display
  and bus variables unset after a reviewer proved the earlier rows attached to the host desktop. Kay
  McNulty (OpenAI Codex) accepted the third repair at `0/0/0/0`. The candidate's ADR is renumbered to
  ADR-0062. Fresh merged-tree Debug and Release each pass launcher 15/15 and applet integrity rows under
  isolation; the broad safe Debug suite passes 421/421; 126-document validation, strict MkDocs, source shape, diff, JSON, and Team Board
  16/16 pass. Production panel hosting of the launcher applet remains a composition lane.

- This integration merges exact accepted Customize canvas descendant `2500a3d71343f238ce30fd0d98f5cc2aa9a95809`
  at manager merge `ce5541e`. The `customize` route composes only the public customization-editor,
  shell-customization, profiles, and Settings1 boundaries into a WYSIWYG panel/applet canvas with
  pointer/keyboard parity, atomic persistence, and discard confirmation on every departure path,
  including window close across the wide/compact host switch; its boundary scan is allow-list-only. Adele
  Goldstine (OpenAI Codex) accepted the third repair at `0/0/0/0`. Fresh merged-tree Debug and Release each
  pass customize 17/17 (route plus domain rows) and Settings Center 9/9; the broad safe Debug suite passes 412/412; 125-document validation, strict
  MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. QQ-004.08 advances WIRED → EXECUTABLE.

- This integration merges exact accepted compositor window-actions descendant
  `3690a056e667135d486da4fa60d7996882a4560a` at manager merge `135fe65`. A new authenticated
  `org.qindaqt.CompositorShell1` boundary admits activate/minimize/unminimize/close/raise only for the
  D-Bus caller whose credentials match the bound panel-owning Wayland client, authenticates before any
  parsing, bounds every entry field, replies constant-size to unauthenticated or unbound callers, fences
  by generation, rate-limits, routes Hybrid members through container policy, and ships an exact-owner
  shell client; the unauthenticated `Compositor1` mutators stay `control-disabled`. Margaret Rock (OpenAI
  Codex) accepted the repair at `0/0/0/0`. The candidate's ADR is renumbered to ADR-0061. Fresh merged-tree
  Debug and Release each pass compositor 35/35 non-nested rows and the window-action rows 4/4 including the
  nested live row; the sixteen `compositor.kwin-` rows pass 16/16 serially; the broad safe Debug suite passes 406/406; 124-document validation, strict MkDocs, source shape, diff,
  JSON, and Team Board 16/16 pass.

- This integration merges exact accepted Power PB-2 descendant `6cef8b582aeb33522829d6ae838269f31aad7611`
  at manager merge `4ae7f89`. The resident Power1 service now composes production UPower, logind,
  power-profiles, and backlight adapters over injected buses and roots, defaulting to production with an
  explicit deterministic mode; UPower line-power/PowerSupply semantics, dispatch-time logind
  re-authorization, and generation-fenced pending maps were each forced by an exact rejection. Ida Holz
  (OpenAI Codex) accepted the final descendant at `0/0/0/0`. The candidate's ADR is renumbered to ADR-0060.
  Fresh merged-tree Debug and Release each pass the power selector 26/26 with the host system bus
  unreachable; the broad safe Debug suite passes 403/403; 123-document validation, strict MkDocs, source shape, diff, JSON, and Team Board
  16/16 pass.

- This integration merges exact accepted Audio applet descendant `14f3e670d66988ec87648f195b91e26c56a45fbf`
  at manager merge `780981c`. The applet composes only the public AudioClient with separate read/control
  grants and audited routing; the merge also carries the staging-closure repairs that ship the shell's
  Controls/Tokens runtime dependency in every shell-carrying install component (Power, Bluetooth, Audio,
  and the default component) with a new `qindaqt.shell-runtime-component-closure` row, after the first
  manager merge attempt broke two installed-package rows and was reset. Fresh merged-tree Debug and
  Release each pass the three applet selectors 22/22 and the integrity/runtime/closure rows 7/7; the broad safe Debug suite passes 396/396;
  122-document validation, strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. QQ-004.12
  advances WIRED → EXECUTABLE.

- This integration merges exact accepted Portal P1 candidate `c33b4908f99cb1dfac04287383441d028ab8f25b`
  at manager merge `2c514ac`. The installed `xdg-desktop-portal` 1.20.4 frontend, started on a private bus
  with a staged portal directory, selects the QindaQt Settings backend only under
  `XDG_CURRENT_DESKTOP=qindaqt`, exposes the QindaQt color-scheme/accent/contrast projection and its
  `SettingChanged` propagation, and a Qt offscreen probe under the `xdgdesktopportal` platform theme
  follows it live; non-Settings families route through an explicit fallback table and never resolve to
  QindaQt. Gertrude Blanch (OpenAI Codex) accepted the exact candidate at `0/0/0/0`. The candidate's ADR is
  renumbered to ADR-0059. Fresh merged-tree Debug and Release each pass the portal selector 9/9;
  the broad safe Debug suite passes 391/391; 122-document validation, strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass.

- This integration merges exact accepted Clipboard C1 service repair descendant
  `63e884cfa2216d7dc492407e30c7ce28b8512ac0` at manager merge `f34f81a`. The resident host captures only
  after an explicit user-override `services.clipboardHistory` opt-in (both shipped schemas now default to
  `false`), pauses capture and withdraws readable truth unless the authenticated lock state is Unlocked,
  bounds MIME/byte/offer limits, evicts remembered requests with exactly-once semantics preserved, and
  ships D-Bus activation and a user unit. Evelyn Berezin (Kimi K3-256k) rejected the ancestor at `0/1/1/3`;
  Ruth Lichterman (OpenAI Codex) accepted the repair at `0/0/0/0`. The candidate's ADR is renumbered from
  the colliding 0056 to ADR-0058. Fresh merged-tree Debug and Release each pass clipboard 14/14 and the
  Settings, notification-quieting, and appearance rows 29/29; the broad safe Debug suite passes 389/389; 121-document validation, strict
  MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. QQ-005.06 advances WIRED → EXECUTABLE.

- This integration merges exact independently accepted BlueZ adapter candidate
  `f44919a52f67515779f887b8d54a9bb2a57b3c4b` at manager merge `d43463c`. The adapter implements the
  accepted AdapterBackend port over an injected direct-QtDBus `org.bluez` connection (ObjectManager,
  Adapter1, Device1, PropertiesChanged), calls only Properties.Set(Powered), StartDiscovery,
  StopDiscovery, Connect, and Disconnect, retires truth on BlueZ owner loss, bounds hostile values, and is
  selected by the composition root in production with an explicit deterministic escape hatch. Betty
  Holberton (Kimi K3) accepted the exact candidate at `0/0/0/2`; both P3s (an observed-property
  overstatement and a wording precision) are corrected here, and the candidate's ADR is renumbered from
  the colliding 0056 to ADR-0057. Fresh merged-tree Debug and Release each pass the complete 15/15
  Bluetooth service selector including the whole-repository staged-install row; the broad safe Debug
  suite passes 379/379; 119-document validation, strict MkDocs, source shape, diff, JSON, and Team
  Board 16/16 pass. Physical radios, pairing UX, Settings UI, and hardware qualification remain later.

- This integration merges exact independently accepted Bluetooth applet B1 repair descendant
  `882cc0cdbb31ee9d619c625a2856aee90c7a49b0` at manager merge `34a79c2`. The regex-based positive
  controller-surface gate that four reviewers had split over is replaced by a compiled QMetaObject
  surface test comparing the complete ordered property, method, and enumerator surface and the
  QML-visible names, with token-paste, public-slot, and enum negative controls; the textual
  composition-chain contracts (stock-profile placement, QML delegate wiring, shell composition
  tokens) and dependency-policy poisons are retained. Cecilia Payne (Kimi K2.7) and Chien-Shiung Wu
  (Kimi K3-256k) each accepted the descendant at `0/0/0/0`. Fresh merged-tree Debug and Release each
  pass Bluetooth 8/8 and adjacent 6/6; direct boundary gates pass 7+6 and 5+4; 118-document validation,
  strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. The broad safe Debug suite on the merged tree passes 373/373 after a clean incremental build (nested-compositor rows and the 25 host-font-drifted controls visual rows excluded as recorded).
  QQ-004.14 advances from ABSENT to EXECUTABLE; the BlueZ adapter, pairing UX, nested interaction,
  and hardware remain later outcomes.

- This integration merges exact independently accepted Global Menu G1 candidate
  `7c27ee5b1b50746e59f70360d89b0e959328dd47` at manager merge `729bebd`. The registrar owns
  `com.canonical.AppMenu.Registrar` on an injected session-bus connection, keys registrations to the
  caller's exact unique name, and retires them on owner loss; the dbusmenu client decodes `GetLayout`
  replies with bounded depth, item counts, and string lengths, treats property-update signals as
  invalidations followed by a complete revisioned reread, and rejects replayed revisions; the transport
  coordinator binds both to the G0 proof-bound lineage. ADR-0056 records the standard-protocol adoption.
  Elizabeth Feinler (Kimi K3) accepted the exact candidate at `0/0/0/1`; the sole P3 (harness prose claiming
  a lower-revision row that no test drives) is corrected in this integration. Fresh merged-tree Debug and
  Release each pass the complete 16/16 global-menu selector; 117-document validation, strict MkDocs,
  source shape, diff, JSON, and Team Board 16/16 pass. The broad safe Debug suite on the merged tree (all 404 registered rows minus the serialized nested-compositor rows and the 25 controls visual rows that drift only by host font rendering) passes 365/365 after a full 2,715-action incremental build.
  Shell composition, applet wiring, submenu popups, and installed-session proof remain later lanes.

- This integration merges exact independently accepted S3 readiness candidate
  `4d4b3dc395d80135a8f66f5ffcb2395cf1874c60`. The installed private desktop
  executes WUXGA, 1440p at 125%, 1080p at 150%, light/dusk/dark themes, and
  dual outputs while booting the production compositor, shell, resident
  services, Settings, and Text Editor. Its readiness contract joins the exact
  active GlobalAccel component/action, stable unique shell owner and PID,
  private closed/hidden center before one Meta+N batch, open/visible center
  with increased counter after it, and one mapped/committed compositor surface
  on the desired/actual output. Mina Shah's external Claude source/archive
  review accepted the immutable candidate at P0/P1/P2/P3 `0/0/0/2`; Lise
  Meitner's independent fresh static+dynamic review accepted it at `0/0/0/0`
  after building 882/882 actions and passing focused 5/5, formerly failing
  1080p@150% plus package 2/2, and package plus the four-row matrix 5/5. Four
  fresh archives prove canonical activation/shell/surface 4/4, false host
  reachability 12/12, bounded PSS, nontrivial captures, empty teardown, and
  exact dual `[WL-1, WL-0]` authority with interaction and capture on WL-1.
  Fresh merged-tree replay configures successfully, builds 882/882, passes
  focused 5/5, passes the formerly failing 1080p@150% plus package 2/2 in
  8.18 seconds, and passes one unretried package-plus-four-row matrix 5/5 in
  33.94 seconds. Manager run IDs are `666752f4`, `6c6f251d`, `73d82c54`, and
  `93ab1392`; all retain containment 12/12, PSS below the 1,024 MiB ceiling,
  byte-authenticated visually coherent captures, empty teardown, and exact
  WL-1 dual interaction/capture. Final process inspection is empty.
  QQ-004.09 and QQ-006.09 advance from WIRED to EXECUTABLE. Complete
  screen-reader/keyboard coverage, heterogeneous mixed scaling, portrait and
  hotplug/lid behavior, physical input/display, GPU/DRM, and perceptual
  baselines remain later qualification.

- This integration merges exact independently accepted Network Settings N2
  repair `6f5d0ba9915851195a4776b3a1e2f224c369a958`. The installed `network`
  route composes only the public Network1 client into secret-free device,
  access-point, and saved-profile presentation; exact owner, epoch, revision,
  and operation fences withdraw stale truth and make displayed action
  availability identical to request admission. Literal hardware addresses,
  credentials, text-entry surfaces, radio mutation, and private service
  implementation remain outside the route. Barbara Liskov's external Claude
  exact rereview accepted the repaired descendant with P0/P1/P2/P3
  `0/0/0/3` after directly closing all four former P2 findings. Fresh merged-
  tree Debug and Release each build **2,036/2,036** and pass the mutation 5/5,
  affected 14/14, public package/policy 5/5, Network 25/25, and Settings 9/9
  selectors. Documentation validates 116 pages; strict MkDocs, source shape,
  JSON, diff, conflict, direct boundary, and poison gates pass. QQ-006.05
  remains WIRED because other platform-service Settings routes and whole-
  desktop qualification remain.

- This integration merges exact independently accepted Portal P0 repair
  `9f59a77aee9cbfd2f4f541b136ecd611e0cda798`. The resident backend owns only
  the standard `org.freedesktop.impl.portal.Settings` endpoint, projects
  complete exact-owner Settings1 appearance truth through QST-1 into the
  standard color-scheme, accent-color, and contrast values, and withdraws
  readable truth on owner, epoch, bus, or projection loss. Source and staged
  package metadata are exact Settings-only singletons; OpenURI, installed
  xdg-desktop-portal 1.20.4 Background, duplicate Settings, extra families,
  and private headers fail closed. Frances Allen's exact rereview accepted the
  repaired descendant with P0/P1/P2/P3 `0/0/0/0`; fresh Debug and Release
  each built 97/97 and passed the contained seven-row selector. The manager
  merge resolved only additive ADR/navigation conflicts and independently
  repeats strict Debug and Release **97/97 plus 7/7** each, 114-document
  validation, strict MkDocs, the 1,715-file source-shape gate, JSON, diff, and
  conflict checks. QQ-005.09 advances from ABSENT to EXECUTABLE. Host portal
  selection, toolkit reaction, every non-Settings portal family, and physical
  distribution qualification remain explicitly outside P0.

- This integration merges exact independently accepted notification live-output
  repair `89557a0a090b6b910621463b4ac97a6d1d054469`. The production shell now
  binds the exact current `org.qindaqt.Compositor` owner, consumes the ordered
  public `Compositor1.Outputs()` projection, and joins its generation and exact
  output-ID set to accepted shell-visibility and Qt inventory truth before
  resolving notification popup/center surfaces. Owner loss, invalidation,
  malformed replies, replacement, generation mismatch, and missing exact Qt
  screens fail closed; stale `QGuiApplication::primaryScreen()` no longer
  selects notification output. Charles Babbage's immutable review accepted the
  candidate with P0/P1/P2/P3 `0/0/0/1`: Debug built 297/297 plus 161/161
  adjacent actions, passed 20/20 and repeated both focused rows 25 times;
  Release built 458/458 and passed 20/20. The sole documentation-precision P3
  is corrected in this manager integration. After an initial Debug attempt
  stopped only because the `/tmp` filesystem filled, exact build-system clean
  targets reclaimed reproducible old outputs and the fresh merged tree passed
  Debug and Release **458/458 plus 20/20** each. QQ-004.09 remains WIRED until
  the preserved S3 worktree proves the real WL-0 to WL-1 layer-surface transfer
  and completes the wider whole-shell matrix.

- This integration merges exact independently accepted Display D6 repair
  descendant `9a7872aec60a5e0f8286b3d5af7fa21209e8fd65`. The packaged Display1
  process now resolves the injected durable journal before mutation authority,
  authenticates its compositor Wayland peer and lock/logind safety inputs,
  and composes the public D4 writer through the resident D2 service. Typed
  observation disposition preserves complete live truth and every Staged-or-
  later transaction across benign same-owner rejection while still publishing
  unavailability after a failed replacement-owner establishment. Mary
  Jackson's same-reviewer rereview accepted the exact descendant with P0/P1
  `0/0`; Debug and Release each passed 19/19 hostile service assertions, 7/7
  D6 package/boundary rows, and 40/40 adjacent D0-D6/session-lock rows. The
  prior in-tree poison and WaylandClient configure defects are closed. Fresh
  merged-tree Debug and Release targeted builds complete 197/197 and 228/228
  actions respectively; each passes the same 40/40 adjacent, 7/7 focused, and
  direct 19/19 hostile gates. The review's unrelated
  customization Release P2 is already repaired by integrated `fa22af50`.
  Nested compositor convergence, mixed/physical output behavior, resources,
  suspend/hotplug, and hardware qualification remain later work.

- This integration merges exact independently accepted customization-editor
  Release-portability candidate `fa22af5028e8dd4bfd9c0951cf742ea74d4b914f`.
  The panel-step helper now constructs the complete `DropTarget` value and
  copies it into the outer optional, removing GCC 15.3's optimization-only
  inactive-storage diagnostic without suppressing warnings or changing the
  panel, zone, or null-anchor contract. Grace Hopper's exact review reproduced
  the parent failure at action 59/85 and found P0/P1/P2/P3 `0/0/0/0`; strict
  Debug and Release each built 85/85 and passed the complete 6/6
  customization-editor selector plus the repaired direct row 3/3. Fresh
  merged-tree verification repeats those affected gates. This portability
  repair does not advance QQ-004.08 beyond WIRED; the Settings canvas,
  provisional live-shell binding, reveal UI, rendered matrix, and installed
  session behavior remain later work.

- This integration merges exact independently accepted Network N1 repair
  descendant `aebc4fd3d887f09ae28149f9c016a08f28c86a92`. The resident Network1
  service now composes the N0 model/client with a public Qt transport and a
  libnm-confined NetworkManager adapter. Exact upstream-owner notifications
  retire old generations beneath the fact-refresh interval; admitted backend
  work is queued until the delayed bus reply is owned; definite scan failure
  removes provisional freshness while uncertain cancellation remains
  conservative. Katherine Johnson's exact rereview found P0/P1/P2/P3
  `0/0/0/0`, passed strict Debug and Release 21/21 each, repeated four
  mutation-sensitive rows ten times each for 40/40, and passed the positive
  boundary, six-poison negative, and installed lifecycle selector 3/3. Fresh
  merged-tree Debug and Release each build 111/111 focused actions and pass
  the complete 21/21 Network selector with host display and bus variables
  removed. Physical radios, UI, persistence, external secret-agent/credential
  interaction, distribution policy, and hardware qualification remain later
  gates.

- This integration merges exact independently accepted Power P2 descendant
  `9e4b7a60f2bcc9c9229418a47c2c1801a343a876`. The production panel applet
  composes only the public PB-1 client through a shell-private controller,
  requests independent `power.read` and `power.control` grants, clears stale
  truth and pending operations on exact-owner replacement, exposes compiled
  keyboard-accessible QML, and is discovered through the audited manifest,
  registry, host, profile, and installed-package seams. Ada Lovelace's exact
  descendant rereview found P0/P1/P2/P3 `0/0/0/0`; Debug and Release each
  passed the six applet-integrity rows, eight Power rows, and 11 direct manifest
  cases. Fresh merged-tree Debug and Release each build 327/327 focused
  actions and pass the combined 14/14 selector plus 11/11 direct manifest
  cases. QQ-004.13 advances from WIRED to EXECUTABLE. PB-1 still reports
  honest `upstream-not-integrated` truth; live providers, successful host
  mutations, nested interaction, and physical hardware remain later gates.

- The manager merge `7277771a63747bbcec957465e5f0b676e69168d0`
  integrates exact accepted Display Settings descendant `2f429c11`. The live
  `display` route composes the public D3 client/coordinator into bounded
  snapshot, draft, topology-validation, preview, confirm, and revert behavior;
  preserves authoritative coordinate truth across output replacement and
  same-output refresh; and exposes keyboard-accessible output selection and
  integer coordinate commits through the installed Settings application.
  Katherine's terminal rereview found P0/P1/P2/P3 `0/0/0/0` after the sole
  documentation-integrity defect was repaired. Exact Debug and Release review
  each built 328/328 focused actions and passed 12/12 rows, the compiled page
  passed 10/10, the independent interaction probe passed 7/7, and four hostile
  mutations were killed. Fresh merged-tree verification builds 328/328 and
  passes the same combined 12/12 selector. Display resident composition is now
  integrated by D6; nested preview/confirm/revert convergence remains S3 work; physical
  displays and assistive-technology integration remain release gates.

- The manager merge of exact accepted candidate `acd0168` integrates Display
  D5's crash-safe filesystem journal behind the D4 `JournalStore` boundary.
  The injected effective-user-owned state root, fixed names, canonical bounded
  codec, mode-0600 exclusive temporary file, file sync, atomic replacement,
  and directory barrier preserve one complete recovery pre-image without
  hidden HOME/XDG authority. A typed `DurabilityUncertain` outcome prevents a
  visible rename or unlink from authorizing forward apply before the directory
  barrier is proven; even an already-absent clear retries that barrier and the
  D1 machine remains cleanup-only `Stuck` with zero apply requests until a
  concrete durable clear. Galileo's terminal exact rereview found P0/P1/P2/P3
  `0/0/0/0` and passed strict Debug and Release journal/writer/transaction
  12/12, direct lifecycle 4/4, adjacent service/client 10/10, package poison,
  docs, strict MkDocs, shape, lineage, provenance, and residue gates. Manager
  replay builds all 130 focused actions and passes the 12/12 and adjacent 5/5
  selectors serially. Resident startup recovery/writer composition,
  authenticated lock/logind safety, nested convergence, mixed
  outputs, resources, and physical hardware remain D6+.

- `26bb7f5` supplies the exact independently accepted private interactive
  desktop S2 replay. Astra's immutable Gemini review found P0/P1/P2/P3
  `0/0/0/0`, completed a fresh 2,338-action build, passed 73/73 desktop-session
  units and both private boot/interaction rows, and preserved the candidate
  byte-clean. The combined D4+S2 manager tree completes 2,201/2,201 build
  actions and passes package, boot, and interactive 3/3 plus 73/73 units. Its
  private run `831b6c817364cd4765468fa3194f0d96` observes zero active centers
  before exact `Meta+N`, then a mapped 440x640 center; captures a 1920x1080
  parent frame with 77 full-frame and 48 bound-region colors; measures 167,633
  KiB across all eight QindaQt roles under the 1,048,576 KiB ceiling; and
  records eleven authenticated terminal phases with zero survivors. This
  advances QQ-006.09 from MODELLED to WIRED without claiming the wider DPI,
  theme, multi-output, screen-reader, GPU, or physical-device matrix.

- `d7691ac` integrates the exact independently accepted Display D4 public
  QtWayland output-management writer. Galileo's immutable review found
  P0/P1/P2/P3 `0/0/0/0`, verified both lifecycle repairs and both decomposition/
  mutation-coverage repairs, and passed Release D4 5/5, Debug D0-D4 26/26,
  installed package poison, exact protocol hashes, docs, strict MkDocs, shape,
  provenance, cleanliness, and zero residue. Fresh manager-tree Debug
  verification built all 23 executable Display targets and passed the complete
  D0-D4 selector 26/26. The packaged resident remains deliberately fail-closed
  until authenticated lock/logind safety, writer/journal resident composition,
  and contained nested convergence land.

- `c819db8` integrates the exact independently accepted Display D3 typed
  asynchronous client and D2 transaction-summary projection replay. Astra's
  immutable Gemini review found P0/P1/P2/P3 `0/0/0/0`; the replay preserves
  all 20 D3 leaf blobs and seven D2 source/test blobs while retaining every
  current-manager shared-registry entry. Fresh manager-tree strict Debug and
  Release builds complete 81/81 targeted actions and pass the exact seven-row
  D2/D3 selector in each profile. D4 now supplies the separately integrated
  compositor writer, D5 supplies the durable journal, and the Display Settings
  route is now integrated; resident composition, nested convergence, hardware,
  and resource proof remain.

- `0c9f4b0` integrates the exact independently accepted Settings Center S1
  repair over typed navigation commit `80a91f8`. Noether's immutable rereview
  found P0/P1/P2/P3 `0/0/0/0`; Debug and Release passed 9/9, the direct page
  binary passed 6/6 under `QT_FATAL_WARNINGS=1`, the external responsive focus
  harness passed 5/5, and package-isolation poison, docs, source shape, strict
  MkDocs, provenance, and cleanliness passed. Fresh manager-tree focused
  build and the exact 9-row selector plus direct 6/6 pass. QQ-006.04 advances
  WIRED to EXECUTABLE; most platform pages, drag-from-configuration editing,
  cross-app visual matrices, and live assistive-technology proof remain.

- `2ae29f3` integrates the exact independently accepted Display Color C0
  series. The GLM repair defeated all eight hostile review reproductions;
  independent Gemini Pro review found P0/P1/P2/P3 `0/0/0/0`, completed strict
  Debug and Release builds, passed 6/6 registered rows and 46/46 direct cases,
  and preserved the candidate tree exactly. The fresh manager tree builds all
  1,597/1,597 actions, passes 6/6 registered rows and 46/46 direct cases, and
  passes documentation, source-shape, strict MkDocs, and diff gates. QQ-005.07 advances
  ABSENT to EXECUTABLE. C0 remains a pure injected model: live ICC discovery
  and import, persistent assignment, compositor application, Settings UI,
  nested HDR/WCG proof, and physical hardware qualification remain later.

- `ea4d986` replays the exact independently accepted Network N0 series onto
  the Terminal and Bluetooth manager tree with additive shared registries.
  Independent exact review found P0/P1/P2/P3 `0/0/0/0`, proved all 49 Network
  leaf blobs byte-identical and all seven shared paths additions-only, and
  passed Debug/Release 13/13, direct 118/118, eight mutation checks, package,
  poison, docs, and provenance gates. Fresh manager evidence passes 64/64
  focused build actions, 13/13 registered rows, source shape over 1,477 files,
  99-page docs, and strict MkDocs. QQ-005.04 advances ABSENT to EXECUTABLE;
  resident service, NetworkManager/secret transport, persistence, UI, radio
  mutation, and hardware qualification remain N1+.

- `c08b32e` replays the exact independently accepted Bluetooth B0 series onto
  the Terminal milestone with additive Terminal/Bluetooth ADR, module, source,
  test, and documentation registries. Independent exact replay review found
  P0/P1/P2/P3 `0/0/0/0`, proved 54/54 Bluetooth blobs byte-identical, and
  passed 9/9 rows, 70/70 direct assertions, the staged package and its poison
  negative. Fresh manager evidence passes all eight source/private-bus rows,
  source shape over 1,431 files, 96-page docs, strict MkDocs, and Team Board
  17/17. QQ-005.05 advances ABSENT to EXECUTABLE; production BlueZ, hardware,
  pairing UX, Bluetooth audio, suspend/hotplug, resource, and UI remain later.

- `4f99a7f` replays the exact independently accepted Terminal S0 series and
  its relocatable-qtermwidget package repair onto the current manager tree.
  Independent exact review found P0/P1/P2/P3 `0/0/0/0`; all 20 production
  Terminal blobs match the private-Weston-qualified candidate. Fresh manager
  evidence passes 63/63 build actions, 9/9 registered rows, 7/7 appearance
  cases, 4/4 real-adapter cases, source shape over 1,383 files, 93-page docs,
  and strict MkDocs. QQ-006.08 advances from ABSENT to EXECUTABLE; S0 does not
  claim tabs/profiles, settings persistence, AppShell/global-menu integration,
  a nested screenshot matrix, or whole-application assistive-technology proof.

- `d0e0809` replays the exact independently accepted Text Editor AppShell
  migration `75f786e9`. The manager tree builds the seven focused editor
  targets in 139/139 actions and passes the Text Editor selector 10/10 plus the
  rebuilt AppShell/File Manager/Appearance adjacent selector 17/17. Source
  shape checks 1,354 files, documentation validates 90 pages, strict MkDocs
  and all 16 Team Board tests pass, and only the manager-owned provider-status
  record remains operationally modified. The later localization/global-menu
  authority is a nonblocking P3; no portal or live desktop claim is added.

- Branch: public `main`
- Functional boundary: public milestone `ab36cd8` plus exact accepted
  Appearance Settings S0 candidate `d71fac4` and the privately qualified
  1920x1080 whole-desktop boot boundary in this integration change
- Outcome: qualified QST-1 and Controls, bounded Audio1, Display D0/D1/D2,
  live Notifications and Appearance settings routes, executable native Text
  Editor S1 and local File Manager S0, and executable shared
  QindaQt.AppShell 1.0 contracts, plus a production-built private whole-desktop
  boot with exact topology, a 1024 MiB PSS ceiling, and bounded teardown
- State: independently accepted, manager-qualified, and published with a
  documentation-only project-identity descendant

The baseline combines generic persistent Settings1 and the first-class
Notifications route with QST-1's pure semantic token derivation, accessibility
overrides, read-only QML adapter, and installed consumer packages. The
Settings1 resident exits on permanent session-bus loss; a new daemon activates
a new process and lineage rather than reconnecting stale repository state.
QST-1 owns semantic policy without importing a general application framework
or widening the theme schema. Audio1 adds a versioned, asynchronous Qt
boundary over a resident service whose production WirePlumber and GLib handles
remain confined to one private worker thread. Run generations, owner/epoch/
revision lineage, and atomic validation prevent stale or malformed backend
state from reaching future shell and Settings consumers. Display D0/D1 adds a
revisioned compositor inventory plus bounded protocol, identity, topology, and
reversible transaction state. Text Editor S1 adds the first native application:
one local UTF-8 document with optimistic conflict checks and atomic persistence.
The installed Notification Live path qualifies the shell shortcut,
keyboard/focus behavior, Settings1 persistence and replacement, Do Not Disturb,
critical bypass, shell restart, authenticated private lock privacy, and bounded
teardown across the required nested resolution and scale matrix.

Integrated evidence:

- The combined production graph built 612/612 targets and the dependency-light
  integrated suite passed 189/189. The private `desktop.virtual.boot.1080p`
  row then passed with one `Virtual-0` 1920x1080@1 output, exact compositor,
  Settings1, Audio1, and Notifications owners, mapped Settings and Text Editor
  windows, the supervised shell/session process topology, zero teardown
  survivors, and 88,688 KiB resident PSS against the 1,048,576 KiB ceiling.
  Cold-boot polling now keeps each probe inside its fixed one-second lifetime:
  service gaps produce retryable complete snapshots under the outer 15-second
  budget instead of allowing an inner wait to self-timeout before evidence.
- The exact Appearance Settings repair `d71fac4` passed independent rereview
  with P0/P1/P2/P3 `0/0/0/1`. Its six-target warning-clean build and direct
  suites passed 7/7 values, 8/8 preview, 11/11 plus 6/6 adversarial model,
  9/9 page, and 10/10 migration checks; registered selectors passed 4/4
  Appearance, 5/5 Settings application/package, and 1/1 migration rows. The
  manager's combined tree repeated those registered rows, retained both
  Notifications and Appearance installed routes, and made `DesktopVirtual`
  stage the Appearance, Tokens, and Controls transitive runtime instead of
  publishing an incomplete Settings package. The remaining P3 is later live
  assistive-technology/nested visual qualification; no host desktop or input
  was contacted.
- The exact File Manager runtime/package repair `3fd3842` passed independent
  rereview with P0/P1/P2/P3 `0/0/0/1`: fresh strict serial build 138/138,
  focused File Manager selector 8/8, hostile parent failure on all three
  repaired seams, real staged `Loading` to `Ready`, bounded timeout/error and
  nested-loop lifetime probes, exact relative RUNPATH, confined QML/library
  inventory, strict docs, source shape, and clean provenance. The manager's
  combined tree separately builds the five focused targets and passes the same
  8/8 selector plus all 53/53 File Manager, QST/Controls, AppShell, and
  Power/Brightness rows after a complete installable-tree build. Strict
  72-page documentation and the 1,098-file source-shape gate also pass. The
  remaining P3 is a
  direct repository-owned timeout/Error unit row; the installed runtime row is
  already non-vacuous and the authority remains read-only/local.
- The exact repaired contained-virtual-desktop candidate `d08747d` passed an
  independent exact-commit rereview with P0/P1/P2/P3 `0/0/0/0`. On the
  combined tree, all 62/62 focused Python units pass, 14 harness sources
  compile in memory, source shape checks 1,069 files, documentation validates
  68 pages, strict MkDocs passes, and the staged diff is whitespace-clean.
  This integrates the authenticated sandbox, package contract, topology,
  resource, evidence, and teardown boundary only. No compiler, private desktop
  boot, screenshot, input, or host-session action ran in this merge.
- The exact PB-0 candidate `3078386` passed independent GLM rereview with no
  P0/P1/P2 finding. The combined tree built all five focused test targets,
  passed 6/6 Power/Brightness CTest rows and 54/54 direct QtTest cases, and
  retained 5/5 Display1 and 5/5 AppShell regressions after their binaries were
  built. Documentation navigation, strict MkDocs, source shape, and whitespace
  also pass. PB-0 remains a pure WIRED boundary, not a resident service or UI.
- The exact PB-1 collision-recovery descendant
  `a8a57a9856666c6293fac6872c27c0be9928d8c4` passed Noether the 5th's
  immutable rereview with P0/P1/P2/P3 `0/0/0/0`. The manager merged it onto
  the D4+S2 tree without conflict, completed 139 incremental build actions,
  and passed the exact client/service/package/private-lifecycle selector 8/8.
  Reviewer probes prove valid profile/session siblings recover both when a
  colliding battery identity is replaced and when battery facts become
  unavailable, while intrinsically malformed profiles never resurrect. PB-1
  is an EXECUTABLE resident injected/unavailable boundary; production UPower,
  logind, profile and brightness adapters, policy persistence, Settings/shell
  UI, physical hardware and suspend/hotplug proof remain later work.
- The exact repaired AppShell candidate `5c914a6` passed independent GLM
  rereview with no blocking finding. The combined tree then built the five
  AppShell targets serially and passed 5/5 action-registry, coordinator,
  offscreen accessibility/close-consent, source-policy, and clean installed-
  consumer rows, plus documentation navigation, strict MkDocs, source shape,
  and whitespace checks.
- The immutable Notification Live candidate passed an independent five-profile
  private nested matrix and ten repeated 1080p lifecycles. The conflict-resolved
  manager commit then passed an independent exact-tree integration review, a
  fresh 1,299-action combined Debug build, 11/11 exact focused regressions, and
  a fresh installed private 1080p smoke. No matching private process or recent
  fixture root remained afterward.
- The exact Text Editor candidate passed independent review, then built in the
  integrated Debug tree and passed all 8/8 focused document, store, controller,
  large-document, offscreen window, desktop metadata, CLI, and installed-theme
  tests. Its accepted candidate also passed Release/package proof and measured
  266 ms startup with 19,511 KiB median PSS.
- The accepted Audio candidate and the exact integrated functional tree both
  received different-worker review with P1/P2/P3 `0/0/0`.
- Fresh strict-warning Debug and Release builds passed 749/749 steps each.
  The focused Audio selector passed 7/7 and the complete QindaQt registry
  passed 108/108 in both configurations.
- Debug and Release activation/runtime/reset lifecycle stress passed all three
  tests for ten repetitions each: 30 executions per configuration, 60 total.
- A fresh ASan+UBSan build passed 59/59 focused steps and all 7/7 Audio tests
  with leak detection and halt-on-error enabled, including the 250-cycle
  worker teardown and deterministic reset-source barriers.
- A fresh testing-disabled production/package build passed 485/485 steps and
  all four QML-lint targets. Its 186-file staged install contains the exact
  Audio executable, public libraries/headers, D-Bus descriptor and XML, and
  hardened systemd user unit with staged executable resolution.
- The exact installed Audio descriptor completed 10/10 private-D-Bus daemon-
  loss/replacement cycles: 20/20 staged service activations and exact PID exits,
  10/10 distinct owner/PID/epoch replacements, zero surviving staged services,
  and zero fixture roots.
- Documentation link/navigation validation, source-shape audit, strict MkDocs,
  whitespace, and post-test process cleanup passed on the integrated tree.
- No active desktop, user session bus, global input, host audio graph/device,
  physical display, or physical screen lock was touched by this evidence.

## Next outcome

Extend the accepted private interactive desktop S2 evidence described in
[Task list](TASK_LIST.md) across WUXGA, 1440p, representative 125%/150% scales,
light/dusk/dark themes, and a real multi-output arrangement. Preserve the exact
private-seat input, machine-bound screenshot region, all-eight-role 1,024 MiB
ceiling, and authenticated zero-survivor teardown without connecting to the
host pointer, display, session bus, or user configuration.

The reusable `QindaQt.Controls 1.0` component set is now integrated after exact
independent Debug/Release, visual, accessibility-event, package, source-policy,
and PSS qualification. The revisioned compositor output inventory and contained
virtual-output development seam are also integrated after exact review and
focused integrated-tree verification. The pure Display1 protocol, identity,
topology, and reversible transaction model are now integrated after the
same-revision lineage defect was reproduced, repaired, and exactly rereviewed.
The resident Display1 service and exact-owner compositor inventory adapter are
now integrated at `a5528f8` after the A/B/A epoch-reuse defect was repaired and
two independent exact reviews passed. A fresh combined-tree Debug build passed
68/68 focused build steps and all five Display1 service tests, including both
serial private-D-Bus lifecycle rows, with no surviving service or fixture.
Display1 now exposes the fail-closed D3 typed asynchronous client and
server-projected reversible transaction coordinator; D4, D5, and the Display
Settings route are integrated separately. Production output mutation remains
unavailable until resident writer/journal composition and contained nested
preview/confirm/revert convergence proof land.
Power/Brightness PB-1 is integrated as an EXECUTABLE resident service/client,
package and private lifecycle boundary over the PB-0 protocol/aggregation/
brightness foundation. PB-2 production upstream adapters and policy remain
behind the routed session-lane activation contract. A source-only handoff or a
live worker process is not completion.
