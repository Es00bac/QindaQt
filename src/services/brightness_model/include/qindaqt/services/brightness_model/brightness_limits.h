// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Brightness {

// AGENT-GUARD: This independent cap matches the accepted brightness-lane
// fixture boundary. Do not obtain it by including Display1 headers before D7;
// that would reverse the dependency fixed in the Power architecture.
inline constexpr qsizetype kMaxFixtureDisplays = 32;
inline constexpr qsizetype kMaxStableIdUtf8Bytes = 128;
inline constexpr qsizetype kMaxFixtureEpochUtf8Bytes = 128;

} // namespace QindaQt::Brightness
