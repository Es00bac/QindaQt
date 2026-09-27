# ADR-0276: One theme choice and a real corner-tab input cutout

- Status: Accepted
- Date: 2026-09-26

## Context

Appearance exposed Themes and Windows as separate top-level destinations. A
theme card changed a theme id and scheme, leaving earlier window/container
decoration overrides in place. The user could select Qinda Marigold and still
see another theme's buttons or title shape. KDecoration3's button-group
`Position` constructor also imported the machine's `kwinrc` button list before
QindaQt added its authored buttons, yielding duplicate controls that differed
between computers.

Corner Bar's title is a short painted tab. The rest of the top border is clear,
but KWin 6.6.6 builds a full-width decoration input region from uniform
borders. Changing KDecoration3's `titleBar` alone leaves the clear strip in
KWin's resize hit area. Pointer events cannot reach the window underneath.

## Decision

1. A theme card drafts a theme, its matching scheme, and all default window
   and container chrome settings in one route action. The advanced controls
   live under **Fine tune this theme** in Themes, and they remain ordinary
   persisted overrides. Selecting a theme again clears those overrides.
   Layout profiles keep their `defaultTheme`; changing layout does not erase a
   user's theme choice (ADR-0074 and ADR-0268).
2. QindaQt creates empty KDecoration3 button groups and fills them only from
   its authored arrangement. The host KWin button order never enters the
   QindaQt decoration. Installed non-QindaQt decorations remain a separate
   explicit platform choice in advanced controls (ADR-0160).
3. The shared painter exports the exact title-tab width and unpainted cutout
   region. The KDecoration plugin publishes the cutout as a process-local
   `QRegion` property on its decoration and makes `titleBar` end at the painted
   tab. A pinned, narrow KWin 6.6.6 patch subtracts that region from its
   normal decoration input shape and recomputes on `titleBarChanged`. The
   subtraction cannot expand a window's input. With no cutout property, KWin's
   existing behavior is unchanged. The patch is listed with a SHA-256 in
   `compositor/patches/series.json` and must be applied to the exact pinned
   KWin source when packaging QindaQt.
4. The tab color and radius come from theme data and the existing decoration
   document/preferences resolution. Qinda Marigold, Qinda Sea Glass, and
   Qinda Lilac ship as three Corner Bar treatments with distinct colors and
   radii. Advanced Appearance controls can change the button style, side,
   corner preset, title height, and other chrome options. Imported theme files
   can author further colors and radii.

## Consequences

The theme card and advanced chrome controls now share one draft and Apply
boundary. Switching a theme resets earlier chrome overrides; this is visible
in the preview before Apply. A custom imported theme can provide a different
Corner Bar palette without changing its layout profile.

The click-through behavior requires both the QindaQt decoration and the
downstream KWin patch. A decoration-only build will paint the correct tab but
will still have KWin's full-width input border. Release packaging and nested
compositor verification must include the patched KWin before this behavior is
claimed on a shipped desktop.
