// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

// AGENT-CONTRACT: these bounds are the canonical menu/action model's hostile-
// input ceiling. The exporter validates every snapshot against them before it
// becomes the applet's or a future transport's authoritative tree; nothing
// downstream may accept an unbounded tree. Widen only with an ADR, mirroring
// Settings1/Audio1/Display1 wire-limit governance.
inline constexpr int kMaxDepth = 6;
inline constexpr int kMaxChildrenPerItem = 128;
inline constexpr int kMaxTotalItems = 1024;
inline constexpr qsizetype kMaxIdUtf8Bytes = 256;
inline constexpr qsizetype kMaxTextUtf8Bytes = 512;
inline constexpr qsizetype kMaxShortcutUtf8Bytes = 64;
inline constexpr qsizetype kMaxRadioGroupUtf8Bytes = 256;
inline constexpr qsizetype kMaxProviderUniqueNameUtf8Bytes = 256;

} // namespace QindaQt::Shell::GlobalMenu::Protocol
