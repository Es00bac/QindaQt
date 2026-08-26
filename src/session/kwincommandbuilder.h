// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "sessionoptions.h"

#include <QStringList>

namespace QindaQt::Session {

class KWinCommandBuilder final
{
public:
    // AGENT-CONTRACT: The first item is always the executable. This vector is
    // passed directly to execvp; it must never contain shell syntax.
    [[nodiscard]] static QStringList build(const SessionOptions &options,
                                           QString *error = nullptr);
};

} // namespace QindaQt::Session
