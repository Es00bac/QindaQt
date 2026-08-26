// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtTypes>

namespace QindaQt::Compositor {

struct ControlLimits final
{
    static constexpr qsizetype MaxRequestBytes = 256 * 1024;
    static constexpr qsizetype MaxOperations = 128;
    static constexpr qsizetype MaxIdentifierCharacters = 256;
};

} // namespace QindaQt::Compositor
