// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Session {

class SessionDefaults final
{
public:
    // Seeds only missing, installation-dependent keys of qindaqt-kwin's
    // $configHome/qindaqt/kwinrc (the on-screen keyboard, the Meta launcher
    // call, the overview edge repair); qindaqt-kwin's compiled-in defaults
    // cover the rest (ADR-0289). MIME defaults are packaged desktop policy,
    // never login writes to user mimeapps.list. Existing values are user
    // policy and survive every subsequent login.
    [[nodiscard]] static bool ensure(const QString &configHome,
                                     QString *error = nullptr);
};

} // namespace QindaQt::Session
