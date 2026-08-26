// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

#include <utility>

namespace QindaQt::Core {

struct ValidationResult final
{
    [[nodiscard]] static ValidationResult success() { return {true, {}}; }
    [[nodiscard]] static ValidationResult failure(QString message)
    {
        return {false, std::move(message)};
    }

    bool valid = false;
    QString message;
};

} // namespace QindaQt::Core
