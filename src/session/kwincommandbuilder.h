// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "sessionoptions.h"

#include <QStringList>
#include <QStringView>

namespace QindaQt::Session {

struct KWinCommandCapabilities final
{
    bool lockscreenOption = true;
    bool noLockscreenOption = true;
};

class KWinCommandBuilder final
{
public:
    // AGENT-CONTRACT: The first item is always the executable. This vector is
    // passed directly to execvp; it must never contain shell syntax.
    [[nodiscard]] static QStringList build(const SessionOptions &options,
                                           QString *error = nullptr,
                                           KWinCommandCapabilities capabilities = {});

    [[nodiscard]] static KWinCommandCapabilities capabilitiesFromHelpText(
        QStringView helpText);
};

} // namespace QindaQt::Session
