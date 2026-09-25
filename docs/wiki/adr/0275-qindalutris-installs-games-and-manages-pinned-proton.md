# ADR-0275: QindaLutris installs Windows games and manages pinned Proton builds

- **Status:** Proposed
- **Date:** 2026-09-25
- **Owners:** Bundled applications
- **Supersedes:** ADR-0231, in part — its "never installs, never downloads"
  boundary and its plain `wine` / `proton run` launch path for Proton titles.
  Discovery, de-duplication, the app-local store posture and the QindaTK UI
  contract of ADR-0231 stand.
- **Superseded by:** None

## Context

On 2026-09-25 the operator's World of Warcraft became unplayable on
`qinda-top`: the frame rate swung between ~30 and ~1 fps. The cause was not
the hardware, the compositor or the game. The Battle.net launcher script
asked umu for `PROTONPATH=GE-Proton`, a floating alias that silently
downloaded GE-Proton11-7 on 2026-09-17. That build stalls WoW's render loop
for ~1.2 s every ~2 s (55% of 50 ms samples with no present; 0–2% on
GE-Proton11-6 with identical settings). Nobody chose the change, and nothing
recorded which build the game had last worked on. The desktop workstation
carried the same floating alias and would have broken on its next launch.

The operator then asked for three things:

1. It must not happen to any other game, on either machine or for users of
   the QindaGentoo distribution.
2. Installing games must be as easy as possible for non-technical people,
   whatever the store: Steam, Epic Games Store, EA app, Battle.net, GOG,
   Amazon Games, and ideally Microsoft Store purchases.
3. A maintained database of per-game compatibility and workarounds, using
   what the ecosystem already knows (umu-protonfixes, winetricks,
   protontricks, ProtonDB, PCGamingWiki, AreWeAntiCheatYet, Lutris install
   scripts), shipped through the overlay with a manual "check for newer
   information" refresh. Proton builds must be manageable in the app the way
   Steam manages them.

ADR-0231 made QindaLutris a read-only library that never installs or
downloads anything. That boundary cannot deliver (2) or (3), so this ADR
reverses it deliberately.

## Decision

### 1. Every Windows title runs through umu with an exact Proton build

A Windows title — a store launcher, a store-client game, or a game installed
from a setup file — is launched by `umu-run` with:

- `PROTONPATH` set to the **absolute directory of one concrete build**. The
  floating aliases umu accepts (`GE-Proton`, `GE-Latest`, `UMU-Latest`, an
  empty value) are refused by the planner, never passed through.
- `UMU_RUNTIME_UPDATE=0`, so the Steam Runtime container changes only when
  someone updates it deliberately.
- `GAMEID` and `STORE` from the compatibility database when the title is
  known (so umu-protonfixes apply their fixes), otherwise `umu-0` / `none`.
- `WINEPREFIX` set to the title's own prefix, by default
  `~/Games/<slug>`. An existing prefix, such as `~/Games/battlenet`, is
  adopted in place rather than copied.

The title's record stores the build it was installed with. **A title never
moves to another build by itself.** Installing a newer Proton, or changing
the default build, affects only titles installed afterwards. Moving an
existing title is an explicit per-title action, and it warns when the target
build is a known-bad build for that title. The plain `wine` runner of
ADR-0231 remains for hand-added entries that ask for it.

### 2. QindaLutris manages Proton builds as Steam does

The **Proton manager** lists every usable build found in these roots, in
this order:

1. `/usr/share/steam/compatibilitytools.d` — builds installed by Portage
   (`app-emulation/ge-proton-bin`, one slot per release). These are
   **Tested by QindaQt** when the shipped database names them tested.
2. `~/.local/share/Steam/compatibilitytools.d` and
   `~/.steam/root/compatibilitytools.d` — user-installed builds, shown as
   **Not tested by QindaQt**.
3. Valve Proton builds under Steam libraries (`steamapps/common/Proton*`).

A build's identity is its directory name plus its `version` file. The
manager can:

- choose the **default build** for new installs; the first-run default is
  the database's recommended build;
- **download a GE-Proton release** into the user root. It is fetched over
  HTTPS from the GloriousEggroll/proton-ge-custom GitHub releases only,
  verified against the release's `.sha512sum`, extracted into a temporary
  directory and renamed into place atomically;
- **remove a user-installed build**. Removal is refused while any title is
  pinned to it. System builds belong to Portage and are never removed by the
  app.

### 3. A shipped, refreshable compatibility database

The database is one JSON document with an exact, bounded schema
(`compat-db-v1.json`). It carries:

- the recommended default build;
- a status per known build;
- per-game records matched by umu id, Steam appid, store id (Epic, GOG,
  Amazon, Ubisoft, EA, Battle.net), executable name, or normalized title;
- per game: the recommended build, **builds to avoid, each with a reason and
  its source**, environment variables, winetricks verbs, launch arguments,
  anti-cheat status, ProtonDB tier, notes, and source links.

It is produced by a generator that merges:

- umu-database and the umu-protonfixes ids;
- AreWeAntiCheatYet;
- ProtonDB tiers from its per-appid summary endpoint, and Valve's Steam
  Deck compatibility reports (Verified / Playable / Unsupported with the
  individual test results), both cached, rate-limited and capped;
- winetricks verbs and environment variables from Lutris install scripts;
- a hand-curated overrides file, where observations like the WoW/11-7 stall
  and PCGamingWiki fixes are recorded with their evidence.

The generated database ships as a dated overlay package
(`games-util/qindalutris-compat-db`) under `/usr/share/qindalutris/`. The
manual refresh fetches the same generator output from one fixed HTTPS
location into `$XDG_DATA_HOME/qindalutris/`. The copy with the newer
`generated` stamp wins, and a document failing validation is refused
whole.

The database is advice that the app applies at install and launch time:

- Its environment, arguments and one-time winetricks verbs are applied (the
  verbs run through `umu-run winetricks`).
- The recommended build becomes the pin for a new install.
- Anti-cheat status and warnings are shown before the user commits to a
  download.

A refresh never re-pins an installed title. Where a refresh marks a title's
current build as known-bad, the app shows a warning that offers the move.

### 4. Stores

- **Steam** runs as the native Linux client (`games-util/steam-launcher`).
  QindaLutris lists and launches Steam games as before, hands installs to
  `steam://install/<appid>`, and does not write Steam's configuration.
  Steam lists the Portage-installed builds, so Steam users can pick the same
  tested builds.
- **Official launchers**: Battle.net, EA app and Ubisoft Connect, plus the
  Epic Games Launcher, GOG Galaxy and Amazon Games as fallbacks. Each is a
  one-click **store recipe**: a vendor HTTPS installer URL, silent-install
  arguments, a prefix, a umu id, the launcher executable to register, and
  post-install fixes. Games installed inside a launcher are started through
  that launcher.
- **Built-in sign-in** is the default for Epic Games Store, GOG and Amazon
  Games, using the open-source store clients `legendary`, `gogdl` and `nile`
  as helper processes. They are packaged by the overlay and never bundled.
  The user signs in once in an embedded browser page, sees their owned
  games, and installs, updates, uninstalls and plays them in QindaLutris.
  Games launch through umu with the pinned build and the store id's umu id.
- **Any setup file**: choose an installer. The app creates a prefix, runs
  the installer, then offers the executables the installer created for the
  user to confirm as the game.
- **Microsoft Store** purchases (UWP/MSIX, DRM-bound to Windows) cannot run
  under Wine or Proton. The app says so plainly and shows whether the same
  title is sold on a store that works.

### 4a. Games that run inside a store launcher share its pin

A game started through a store launcher (World of Warcraft and Warcraft III
through Battle.net) runs in the launcher's Wine session, so it runs on the
**launcher's** pinned build whatever its own database record recommends. The
launcher's pin must therefore satisfy every game installed through it: the
app combines the games' recommended builds and `avoid` lists and warns when
no single build satisfies all of them, rather than silently picking one.
Where one build fixes a game but regresses another, the fix is a new,
separately tested build that satisfies both — as GE-Proton11-6 plus the
upstream crypt32 fix did for WoW and Warcraft III on 2026-09-25.

### 4b. Every running title can be stopped completely

Each launch runs in its own transient systemd user scope. **Force quit**
stops that scope, which ends the game, the store launcher, Wine's services
and umu's container together; a hung Windows window can therefore always be
closed from QindaLutris, the task list or the dock. Proton's own
`wineserver -k` cannot reach a session inside umu's container, which is why
the scope, not Wine, is the authority.

### 4c. A game can run on its own screen

A per-title **Run in its own screen** option starts the title under
gamescope. Games that switch display modes are isolated there, so an
emulated mode change (Warcraft III switching to 720p) can never leave the
launcher or the desktop drawn at the wrong size. The database may recommend
the option for a title; the user can always turn it off.

### 5. Idiot-proofing is a requirement

- Every flow is one button with visible progress.
- Every failure ends in one plain-language sentence, plus a "Copy details"
  log for someone helping.
- Before any download, the app checks free disk space, a working Vulkan
  driver, and the presence of umu and at least one build. Each missing piece
  names the package that provides it.
- The user never sees a terminal, a prefix path or an environment variable
  unless they open "Advanced".

### 6. Boundaries

- The app never runs as root and never invokes Portage. System-wide pieces —
  Proton builds, umu, the database, the store clients and Steam — arrive as
  overlay packages.
- The app downloads only four kinds of files, only over HTTPS, and only from
  an allowlist of hosts:
  - vendor launcher installers;
  - GE-Proton releases;
  - the database refresh;
  - store content fetched by the store clients themselves.
- Process start stays behind the `GameProcessLauncher` seam of ADR-0231. Long
  work — downloads, installers, winetricks, store-client transfers — runs as
  cancellable, bounded jobs that report progress to the UI.
- Persistence stays app-local, exact-schema, bounded and atomic (ADR-0198 and
  ADR-0231). Titles installed or adopted by the app live in a new
  `titles-v1.json`, recording store, prefix, pinned build, umu id and
  launcher linkage. The ADR-0231 documents keep their schemas.

### 7. What the ecosystem survey changed

A survey of more than 35 Linux-gaming projects (recorded in
[QindaLutris prior art](../apps/qindalutris-prior-art.md)) adds these
requirements:

- **Snapshots and one-click rollback.** Before any change to a title — moving
  its Proton build, installing a dependency, updating its launcher — the app
  snapshots the title's prefix and records the known-good combination (umu,
  Proton build, fixes applied). After a failed or crashing launch that follows
  a change, it offers **Go back to the version that worked**. Rolling back the
  Proton build alone is not enough, because Proton refuses a prefix that a
  newer build has already touched.
- **A verdict card before install.** Plain-language levels in the style of
  CrossOver, tied to the pinned build and the date it was tested, expanding
  into the evidence: anti-cheat status, Valve's Steam Deck result with its
  checks, and the ProtonDB tier labelled as a community rating. Titles whose
  anti-cheat refuses Linux (for example Vanguard, Ricochet, FACEIT, EA
  Javelin) show **Can't run on Linux** with Install disabled.
- **Self-checking store recipes.** Each launcher shows Installed or Needs
  repair, with a Repair action; config changes a recipe needs are scripted, never
  printed as instructions.
- **A "Fix a problem" menu.** Force quit (§4b), reset the launcher while
  keeping saves, and show what went wrong; umu's own errors are always
  surfaced, never swallowed.
- **Honest fallbacks.** Microsoft Store and Game Pass titles stay unsupported
  (§4); the app points to cloud play where it exists (Xbox Cloud Gaming,
  GeForce NOW) instead of attempting an install.
- **Data licensing.** PCGamingWiki is link-out only (its text is
  CC BY-NC-SA and its API now needs a bot login). Lutris install scripts carry
  no license, so only uncopyrightable facts (winetricks verb names,
  environment variable settings) are taken from them, with attribution. ProtonDB
  data is attributed as the ODbL requires.
- **No shader-cache distribution.** DXVK 2.7 removed its state cache; the app
  keeps a persistent per-title cache instead.

## Consequences

- A known-good game keeps its known-good Proton build across every update on
  both machines. A regression reaches a game only when someone moves it, and
  the move shows the database's warning first.
- QindaLutris takes on network access and long-running jobs, which ADR-0231
  deliberately avoided. Both are confined to the allowlist and the job
  runner, and each needs failure-mode tests: offline, checksum mismatch,
  disk full, installer exit codes, cancellation, and interrupted
  extraction.
- The overlay gains `app-emulation/ge-proton-bin` (added 2026-09-25) and
  will gain `games-util/qindalutris-compat-db`, `gogdl` and `nile`.
  `games-util/umu-launcher` and `legendary` come from ::guru. The profile
  accepts only exact versions and masks newer umu.
- The database needs maintenance: the generator is re-run and the curated
  overrides reviewed before each overlay bump. A stale database degrades to
  "no advice", never to a wrong pin.
- QindaLutris needs its own `gui-apps/qindalutris` package; today
  `gui-wm/qindaqt-desktop` installs the binary. `docs/wiki/apps/qindalutris.md`
  is rewritten to match this ADR.

## Revisit when

- umu gains a native, non-floating version-pin mechanism. The absolute-path
  rule can then defer to it.
- Wine or Proton can run UWP/MSIX titles. The Microsoft Store refusal is
  then reconsidered.
- A store publishes an official Linux client, or a store client's upstream
  becomes unmaintained.
- The compatibility database needs data this schema cannot express. That
  calls for a `v2` schema with a migration, never an in-place change.
