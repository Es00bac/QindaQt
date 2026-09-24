# ADR-0257: Search Settings routes through a QindaTK command palette

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** First-party Settings Center
- **Supersedes:** None (extends ADR-0048's route descriptor and amends its
  clause 5 for the search palette only)
- **Superseded by:** None

## Context

The [Settings Center](../apps/settings-center.md) registers 21 routes. Only the
first ten have a Ctrl+digit shortcut
([ADR-0128](0128-accessibility-settings-route.md)); the other eleven can be
reached only by scrolling the sidebar or the compact tab strip. Sub-pages
inside a route, such as Input's Shortcuts, need a second step once the route
is open. Nothing let a user type what they are looking for ("wifi",
"battery", "shortcuts").

QindaTK's `Tk.CommandPalette` is a finished Ctrl+K palette: a modal popup with
a filter field, results grouped by section, arrow keys and Enter. It is in the
installed QindaTK release, and the Settings Audio route already renders
QindaTK controls through the `QindaTK.QindaQt` token bridge
([ADR-0227](0227-the-audio-console-on-qindatk.md)).
[ADR-0048](0048-settings-center-navigation-and-route-ownership.md) keeps route
authority in typed descriptors and says the navigation shell uses only QST-1
and QindaQt.Controls.

Alternatives considered: a hand-built QindaQt.Controls search field over the
sidebar (duplicates a toolkit control and its keyboard model); asking each
route page for its sub-pages at run time (the shell would reach into route
QML internals, against the module-boundary rule); and fuzzy full-text search
over page contents (no index exists, and results would point at controls that
the shell cannot open).

## Decision

1. **Search metadata is route-descriptor data.** `SettingsRoute` gains two
   optional, bounded fields: `keywords` (search terms besides the title) and
   `destinations` (the route page's own deep-link sub-pages, each an id in
   route-id syntax, a title and keywords, unique per route). The registry
   attaches them to the built-in routes from one table beside it, with an
   exhaustive switch over the closed component enum, so a new route cannot
   compile without search terms. Every built-in route has keywords. A route
   lists destinations only when its page honours `requestedDestination`,
   including while the page is already open; today that is Input's five
   sub-pages, whose ids and titles must match the page's own list. The
   controller's `routes` projection carries both fields to QML.
2. **The shell searches with `Tk.CommandPalette`.** A Settings-owned wrapper
   builds one command per route (sectioned and ordered like the sidebar,
   printing the route's Ctrl+digit shortcut where it has one) and one per
   destination. It ranks matches so Enter takes the obvious result: title
   prefix, then exact keyword, then label substring, then keyword prefix. It
   only calls `selectRoute` or `selectRouteDestination`. It instantiates
   `QindaQtTheme` so the palette wears the session's QST-1 theme. This amends
   ADR-0048 clause 5 for the palette only: the rest of the navigation shell
   stays on QindaQt.Controls.
3. **Ctrl+K and visible buttons open it.** The shell has a Ctrl+K shortcut and
   a "Search settings" button in both the wide sidebar and the compact header.
4. **Unavailable routes stay listed with their reason, like the sidebar.**
   Choosing one does not select it, and an unavailable route offers no
   destinations.
5. **Escape belongs to the palette while it is visible.** The host Escape
   shortcut is disabled while the palette is visible, alongside the existing
   Bluetooth pairing-prompt exception, because two enabled window shortcuts
   with one sequence are ambiguous and Qt activates neither.
6. **A repeated deep link reaches an open page.** When
   `selectRouteDestination` repeats the current request for the active route,
   the controller clears the link and sets it again, so the open page observes
   it. The Input page opens a destination that arrives while it is open.

## Consequences

- Every descriptor grows two fields with `{}` default member initializers. The
  initializers are required: the strict build fails otherwise, because every
  descriptor's designated initializer stops at `unavailableReason`.
- Adding a route now also means writing its search keywords. Giving a route
  destinations means its page must accept `requestedDestination` both at
  construction and while open, and the destination ids must match the page.
  `qindaqt.settings-route-search` compares Input's ids and titles with
  `InputPage.qml`.
- The Settings executable needs QindaTK and its `QindaTK.QindaQt` bridge on
  Qt's QML import path even when the Audio route is never opened. Nothing
  links the toolkit's C++ API, and the build tree resolves it from the
  installed `dev-libs/qindatk`, as ADR-0227 already requires. The desktop
  package's QindaTK lower bound must be a release that ships `CommandPalette`.
- Keywords are translatable per route, as one comma-separated list, so a
  translation can change which synonyms it offers.
- A shortcut-capture control that does not claim `ShortcutOverride` loses
  Ctrl+K to the palette, just as it already loses Ctrl+digit to route
  selection.
- `qindaqt.settings-command-palette` drives the real `Main.qml` offscreen
  with `QT_FATAL_WARNINGS`: Ctrl+K, keyword search, ranking, destinations
  with Input already open, Escape standing down, the unavailable route, and
  the 420×320 compact window by keyboard only. The route-construction
  witness ([ADR-0250](0250-require-active-loader-witness-for-every-settings-route.md))
  and its installed-package row are unchanged and must still pass.

## Revisit when

- A second route page adopts deep-link destinations (Appearance tabs,
  Network sections), or pages need to publish destinations that vary at run
  time. A static registry table would then no longer fit.
- QindaTK's palette gains a subtitle line or per-row accessible descriptions.
  The unavailable reason could then move out of the label.
- Settings grows past what substring matching serves well, for example
  searching individual controls rather than pages.
