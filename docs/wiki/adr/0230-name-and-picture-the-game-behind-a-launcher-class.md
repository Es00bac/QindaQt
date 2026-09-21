# ADR-0230: name and picture the game behind a launcher class

- **Status:** Accepted
- **Date:** 2026-09-21
- **Owners:** Compositor (window identity), Shell (iconography)
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0169](0169-report-the-program-behind-an-opaque-window-class.md) repaired
the *identifier* of a window whose reported class is a launcher artifact: a
Battle.net window arriving as `WM_CLASS = ("steam_app_0", "steam_app_0")` is
reported as `battle.net.exe`, which the shell's existing desktop-entry lookup
resolves when the host happens to ship a matching entry.

Two halves of the user-visible problem were left open, and both are the normal
case for a gaming session:

- **No name.** A Steam game installed through Steam has no host `.desktop`
  entry at all, so `steam_app_620` resolves to nothing and the task list
  shows the launcher key. The title the user recognises does exist on disk —
  Steam writes it into `appmanifest_620.acf` — but nothing read it.
- **No picture.** A Wine or Proton program's icon lives inside its Windows
  `.exe`, in the PE resource directory. Nothing in the session could read
  that, so every Wine window fell back to a generic glyph.

Both inputs are **attacker-adjacent**: an ACF file is synced from Steam's
content servers and a game executable is downloaded content, and the code that
reads them runs inside the compositor process. Neither may be parsed
credulously, and neither may be allowed to block the compositor on unbounded
filesystem work.

The shell keeps all naming and icon *policy*
([Module boundaries](../architecture/module-boundaries.md),
[Iconography](../shell/iconography.md)); the compositor answers only "which
program is this, and where is its picture".

## Decision

Close both gaps behind the seams ADR-0169 already established, keeping raw
identity in the compositor and presentation in the shell.

- **Bounded readers, not parsers of convenience.**
  `src/compositor/src/steamappidentity.cpp` reads Valve's KeyValues text with
  explicit ceilings: a 1 MiB document cap (`kMaxVdfBytes`), a nesting cap, and
  rejection — never a guess — for an unterminated string, a missing brace, a
  bare token where a quoted string belongs, trailing garbage after a complete
  document, or an absent root object. `parseSteamLibraryFolders()` accepts
  both the modern per-index-object and the legacy flat forms and decodes
  escaped separators; `parseSteamAppManifestName()` takes `name` only from
  inside `AppState`, refuses names carrying control characters, and yields
  empty rather than a partial value. `steamAppIdFromClass()` accepts only a
  `steam_app_<digits>` key and refuses an overflowing digit run instead of
  wrapping it.
- **PE icon extraction treats the executable as hostile.**
  `src/compositor/src/peicon.cpp` walks `RT_GROUP_ICON`/`RT_ICON` with every
  header, table and offset validated against the real device size before a
  byte is read. No length field is ever trusted for allocation. The walk is
  bounded by `kMaxPeSections`, `kMaxResourceDirectories`,
  `kMaxResourceEntriesPerDirectory`, `kMaxGroupIconEntries`,
  `kMaxIconPayloadBytes`, `kMaxIconDimension` and `kMaxIconPixels`, and a
  depth cap of 3 that doubles as the cycle defence — a directory entry
  pointing back at an ancestor cannot recurse past it. Reads are **ranged**:
  a few fixed-size headers, directory chunks, and one payload read per
  candidate, so a several-hundred-megabyte game executable costs only those
  reads and is never slurped. Both the PNG-in-ICO and BMP-in-ICO (DIB with
  AND mask, 1/4/8/24/32 bpp) forms decode. Any violation yields a null
  `QImage`, and the caller keeps its existing fallback.
- **Selection is a stated rule, not whatever comes first.** Among 32-bit
  colour entries, the smallest size at or above `kWineIconTargetSize` (48 px —
  the task list's 18 logical px at 2× with dock headroom), else the largest
  below it; with no 32-bit entry the same size rule runs over every entry;
  ties take the lowest resource id. A DIB's AND mask supplies transparency
  **only** when the source carries no alpha channel of its own, because a
  32-bit entry with a fully zeroed alpha plane and a set mask bit is a real
  shape on disk and must not be turned transparent.
- **One stateful cache, confined to one thread.**
  `WineIdentityCache` (`src/compositor/`) holds both answers for the
  task-facts publisher. `steamNameForClass()` resolves a title from the
  library roots `libraryfolders.vdf` declares plus the two conventional ones,
  memoizing discovery and per-id names against file mtime and size;
  **misses are not cached**, so a game installed later appears on the next
  query, and Steam being absent answers empty cheaply.
  `ensureIconForClient()` extracts the icon of the executable the command line
  names — mapped through the Wine prefix from the client's environment — into
  the cache root, keyed by (path, mtime, size) in a sidecar signature file.
  When extraction fails it **removes** a stale icon pair belonging to a
  different source, so the shell never shows the wrong game. The class is not
  thread-safe by contract; production confines it to KWin's GUI thread.
- **The picture crosses to the shell as a name, not as pixels.** The
  compositor writes `<generic cache>/qindaqt/wine-icons/<name>.png`; the shell
  appends that root **last** to its confined icon roots, so every real themed
  icon still wins, and resolves the file through its ordinary unthemed
  direct-root rule. The name function
  (`Battle.net.exe` → `qindaqt-wine-battle.net`) exists on both sides —
  `WineIdentityCache::cacheIconNameForApplicationId` and
  `IconRuntime::wineCacheIconNameForAppId` — and the two implementations are
  pinned byte-identical by shared test vectors, because one side writes the
  file and the other looks it up by name. The root is app-independent on
  purpose: KWin and the shell have different application names.
- **Name policy stays where it was.** `resolveApplicationName()` prefers a
  reported class that carries identity, then the Steam manifest name **only**
  when the class is a `steam_app_<n>` key and the resolver actually found one,
  then the client executable, then the resolved id. It never invents text.

Passing the icon bytes over the existing window-identity channel was rejected:
it would put decoded image data on a hot compositor path for every window
update, and the shell already has a confined, cached, theme-aware path that
resolves an icon by name.

## Consequences

- A Steam game shows its real title and a Wine/Proton program its real icon in
  the task list, dock and window list with no applet change; the
  request-checklist row for opaque launcher windows keeps its `committed`
  state and now covers name and picture, not just the identifier.
- `src/compositor` links `Qt6::Gui` for `QImage` decode and encode — a
  dependency confined there by the compositor's boundary test.
- Both readers are pure and tested against tiny synthetic fixtures — no real
  game data and no copyrighted art — covering malformed, oversized,
  over-nested and control-character inputs, the legacy and modern
  `libraryfolders.vdf` forms, every ICO sub-format, the selection rule
  including the 32-bit preference and the id tie-break, and the
  alpha-versus-AND-mask rule.
- A first sighting of a new executable costs one bounded extraction on the
  publisher's thread. Repeat sightings cost a signature check. Steam absent,
  Wine absent, and an icon-less executable are all cheap, silent, normal.
- The cache root is a cache: deleting it costs one re-extraction per program
  and nothing else.

## Revisit when

- A program's icon must change while it runs (currently the signature only
  invalidates on a changed executable).
- Steam ships a different manifest format, or a non-Steam launcher needs the
  same treatment — each is an additive resolver behind the same seam.
- Extraction cost shows up on the publisher's thread in a real session, at
  which point it moves to a worker and the cache becomes the synchronisation
  point rather than the confinement rule.
