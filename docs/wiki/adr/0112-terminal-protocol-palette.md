# ADR-0112: Keep Terminal protocol colors independent of status badges

- Status: Accepted
- Date: 2026-09-08
- Owners: Terminal presentation
- Supersedes: The status-token ANSI adaptation recorded in the Terminal wiki; ADR-0040 PTY ownership is unchanged.

## Context

ANSI green, yellow and cyan used QST foregrounds intended for paired status
badges. On the terminal background they could become indistinguishable or
unreadable. Magenta used a translucent accent and bright slots were generated
by mechanically lightening every base color, which fails on light themes.

## Decision

Terminal owns a pure, thread-neutral sixteen-slot protocol palette adapter.
It fixes recognizable red, green, ochre/yellow, blue, orchid/magenta and cyan
hues independently of the desktop accent. Each base and intense slot fits HSL
lightness against the opaque QST content background. Normal text and ANSI target
4.5:1; high contrast targets 7:1. A mathematically impossible custom background
uses the strongest available black or white. No second theme schema or
persistence authority is introduced. All window chrome continues to consume
public semantic QST tokens.

Default intense foreground/background keep the default ink/canvas instead of
changing the entire terminal surface. Truecolor and extended colors emitted by
a program remain program-owned. A saved profile retains its explicit scheme
across desktop changes; high contrast takes temporary precedence and the
original scheme returns when it is disabled. Font preferences and local zoom
remain independent.

## Consequences

Terminal ANSI colors no longer track unrelated desktop status or accent hue
changes. Colors called black and white may use readable neutral ink on opposing
light/dark backgrounds. Hue distinction remains part of normal built-in-theme
tests, while impossible high-contrast custom colors may converge at black or
white. Real PTY pixel tests verify all sixteen Konsole-format groups through
live theme changes; palette-document validity alone is insufficient evidence.

See [Terminal](../apps/terminal.md) for the presentation and verification contract.
