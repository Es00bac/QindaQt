// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Session {

class SessionDefaults final
{
public:
    // Seeds only missing desktop-owned KWin presentation and pointer-policy
    // keys, plus the first-party directory-handler default in mimeapps.list
    // when the user has no inode/directory choice at all. Existing values are
    // user policy and survive every subsequent login, including deliberate
    // decoration, switcher, edge, or default-application choices.
    [[nodiscard]] static bool ensure(const QString &configHome,
                                     QString *error = nullptr);
};

} // namespace QindaQt::Session
