// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Session {

class SessionDefaults final
{
public:
    // Seeds only missing desktop-owned keys. Existing values are user policy
    // and must survive every subsequent login, including a deliberate switch
    // back to a third-party KDecoration plugin.
    [[nodiscard]] static bool ensure(const QString &configHome,
                                     QString *error = nullptr);
};

} // namespace QindaQt::Session
