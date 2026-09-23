# ADR-0245: Write only supported default-application associations

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Settings Default Applications
- **Supersedes:** None
- **Superseded by:** None

## Context

[Default Applications](../apps/default-applications.md) groups several MIME types
under one choice, such as JPEG, PNG, GIF, and WebP under Image viewer. The
chooser admitted a handler declaring any MIME type in a category, the store
validated only the first, and the write set covered every MIME type. A PNG-only
viewer was offered but refused; a JPEG-only handler could be selected and
assigned types it never advertised. Reading only the first MIME type hid
different effective handlers in the rest of the category.

Restricting candidates to handlers supporting every type would make narrow
installed applications impossible to choose. Writing all types would make
unsupported associations ineffective and could replace a deliberate split
choice. The XDG MIME Applications files already hold per-type truth.

## Decision

The Settings route resolves effective defaults and handler support per MIME
type using the same ordered XDG files and current installed-application scan.
Applicable Added and Removed Associations affect both chooser eligibility and
persistence validation. A category presents a single current handler only when
all managed MIME types resolve identically; otherwise it presents a mixed
state. A partial candidate labels the MIME types it can change.

An explicit category choice writes only MIME types in that category that the
selected handler effectively supports. Other category MIME keys remain
unchanged. The existing highest-priority user desktop-specific file owning any
category key remains the write target; otherwise Settings uses the generic
user file. “Use inherited default” removes that category's keys from both
desktop-specific and generic user files so no stale user override reappears.
Administrator and packaged files remain untouched. The route refreshes its
installed-handler scan when desktop entries change, with a bounded periodic
fallback for previously absent roots.

## Consequences

- Existing all-type associations remain valid and are not rewritten merely
  because Settings opens.
- A narrow selection can leave the category mixed; this is visible instead of
  being reported as a uniform default.
- Reset removes user overrides across both user precedence files, revealing
  lower-priority defaults.
- Store, catalog, model, and offscreen page regressions cover partial writes,
  mixed readback, Added/Removed eligibility, precedence, reset, and catalog
  replacement. No launch through a deployed desktop session is claimed.

## Revisit when

A standard desktop-wide terminal preference or another category-spanning
launch policy is defined; it belongs to that launch boundary, not to MIME
associations.
