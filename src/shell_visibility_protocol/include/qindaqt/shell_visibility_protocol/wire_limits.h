// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtTypes>

namespace QindaQt::ShellVisibilityProtocol {

// AGENT-CONTRACT: Producer and consumer link this one header. Do not copy
// these limits into either side of the D-Bus boundary; accepting a payload the
// other side cannot represent is a protocol compatibility defect.
struct WireLimits final {
    static constexpr qsizetype MaxPayloadBytes = 4 * 1024 * 1024;
    static constexpr qsizetype MaxOutputs = 64;
    static constexpr qsizetype MaxWindows = 4096;
    static constexpr qsizetype MaxScopeMemberships = 256;
    static constexpr qsizetype MaxIdentifierCharacters = 512;
    static constexpr qreal MaxOutputScale = 16.0;
};

} // namespace QindaQt::ShellVisibilityProtocol
