# ADR-0100: Bound local image previews and consume public application icons

- Status: Accepted
- Date: 2026-09-08

## Context

The File Manager's text-heavy listing and placeholder glyphs make routine
browsing harder than necessary. Visual browsing needs recognizable file icons
and actual image previews without giving QML filesystem authority or importing
shell-private icon infrastructure. Window grouping remains the compositor's
responsibility; the application continues to browse one folder per window.

## Decision

The application uses the public `QindaQt.Controls` `Icon` presentation boundary
for catalog and user-theme icons. NavigationController projects MIME-extension
classification into stable icon names. No content sniffing or filesystem reads
occur in icon delegates. The default view is an icon grid; details remain
available with the same selection and action identities.

A private, injected `PreviewDecoder` is consumed by an engine-owned
`PreviewProvider`, using a private two-worker pool and a 64 MiB memory-only LRU
image cache. The local implementation accepts PNG, JPEG, BMP and WebP rasters,
rejects inputs over 32 MiB or 40 megapixels before image decode, and outputs at
most 192 × 192 pixels. Unsupported/corrupt entries retain their MIME icon.
There is no disk cache, external process, network fetch, SVG preview, or global
worker-pool authority.

Each URL carries the listing generation and exact decimal device/inode/size/
mtime/mode snapshot. The decoder opens without following the final symlink,
checks regular-file identity before reading, and rechecks descriptor and path
identity afterward. Cache hits also revalidate path identity. Navigation and
refresh cancel the previous generation's work; canceled or obsolete results
publish transparent pixels over the persistent MIME fallback. Cancellation is
cooperative around the bounded decoder call, rather than a promise to interrupt
a codec instruction. Destruction cancels and joins the private workers.

## Consequences

File browsing becomes visual without moving filesystem policy into QML or
creating a shell-private dependency. The private support target adds the public
Qt Quick image-provider dependency. Preview URLs and decoder types are private
implementation interfaces, not a persisted schema or installed ABI. Existing
bookmarks, mutation, Trash and selection contracts are unchanged.

The icon grid and compact navigation must remain usable with keyboard focus,
custom themes and the minimum 480 × 320 window. Screen readers receive file
names/types independently of artwork. The [File Manager](../apps/file-manager.md)
page owns behavior and focused acceptance evidence.
