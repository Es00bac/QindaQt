// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

// AGENT-CONTRACT: these bounds are the canonical menu/action model's hostile-
// input ceiling. The exporter validates every snapshot against them before it
// becomes the applet's or a future transport's authoritative tree; nothing
// downstream may accept an unbounded tree. Widen only with an ADR, mirroring
// Settings1/Audio1/Display1 wire-limit governance. Narrowing to match a
// stricter upstream spec (e.g. the D-Bus 255-byte bus-name maximum) is done
// in place with a note naming the spec.
inline constexpr int kMaxDepth = 6;
inline constexpr int kMaxChildrenPerItem = 128;
inline constexpr int kMaxTotalItems = 1024;
inline constexpr qsizetype kMaxIdUtf8Bytes = 256;
inline constexpr qsizetype kMaxTextUtf8Bytes = 512;
inline constexpr qsizetype kMaxShortcutUtf8Bytes = 64;
inline constexpr qsizetype kMaxRadioGroupUtf8Bytes = 256;
// Exact D-Bus maximum bus-name length (255 bytes), per the D-Bus
// specification's message-protocol names rules; enforced at the ownership
// boundary for provider unique names.
inline constexpr qsizetype kMaxProviderUniqueNameUtf8Bytes = 255;

} // namespace QindaQt::Shell::GlobalMenu::Protocol
