// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "sessionoptions.h"

#include <QStringList>

#include <optional>

namespace QindaQt::Session {

class SessionCommandLine final
{
public:
    [[nodiscard]] static std::optional<SessionOptions> parse(const QStringList &arguments,
                                                             QString *error = nullptr);
    [[nodiscard]] static QString helpText();
};

} // namespace QindaQt::Session
