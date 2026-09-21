// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>

class QIODevice;

namespace QindaQt::Compositor {

// AGENT-CONTRACT: defensive extraction of the best-matching icon from a
// Windows PE executable's resource directory (RT_GROUP_ICON / RT_ICON), the
// Gap-2 half of ADR-0230 layered on ADR-0169. The input is UNTRUSTED binary:
// a game executable is downloaded content parsed inside the compositor
// process, so every header, table, and offset is validated against the real
// device size before a single byte is read, no length field is ever trusted
// for allocation, and any violation yields a null QImage - never a crash,
// never a partial icon. The caller keeps its existing fallback on null.
//
// The device must be seekable (QFile, QBuffer). Reads are RANGED and bounded:
// a few fixed-size header reads, resource-directory chunks bounded by the
// entry caps below, and one payload read per candidate plus the winner. The
// file itself is never slurped, so a multi-hundred-MiB executable costs only
// these bounded reads.

// Walk and payload ceilings. A real resource tree is exactly three directory
// levels (type, name/id, language) with a handful of entries; the depth cap
// is also the CYCLE defense - a directory entry pointing back at an ancestor
// can never recurse past it.
inline constexpr qint64 kMaxPeSections = 96;
inline constexpr qint64 kMaxResourceDirectories = 4096;
inline constexpr qint64 kMaxResourceEntriesPerDirectory = 2048;
inline constexpr int kMaxResourceDepth = 3;
inline constexpr qint64 kMaxGroupIconEntries = 512;
inline constexpr qint64 kMaxIconPayloadBytes = qint64(8) * 1024 * 1024;
inline constexpr int kMaxIconDimension = 1024;
inline constexpr qint64 kMaxIconPixels = qint64(1024) * 1024;

// Preferred edge length when several icons qualify (ADR-0230): at or above
// what the task list asks for (18 logical px, up to 2x device scale) with
// headroom for the dock, matching the provider's default 32px floor.
inline constexpr int kWineIconTargetSize = 48;

// The best icon in `device` (a PE image of `size` bytes), or a null QImage.
// Selection: among 32-bit-colour entries the smallest size at or above
// targetSize, else the largest below it; when no 32-bit entry exists the same
// size rule runs over every entry; ties take the lowest resource id. Both the
// PNG-in-ICO and the BMP-in-ICO (DIB with AND mask, 1/4/8/24/32bpp) forms
// decode. A corrupt, hostile, or icon-less executable returns null.
[[nodiscard]] QImage extractPeIcon(QIODevice &device, qint64 size,
                                   int targetSize);

} // namespace QindaQt::Compositor
