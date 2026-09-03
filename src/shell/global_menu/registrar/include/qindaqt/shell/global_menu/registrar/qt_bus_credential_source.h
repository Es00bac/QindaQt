// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/credential_source.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

// Bus-daemon-backed implementation of G0's credential seam. The connection
// is injected, never discovered globally, and must outlive this object.
class QtBusCredentialSource final : public Ownership::CredentialSource
{
public:
    explicit QtBusCredentialSource(QDBusConnection connection);

    [[nodiscard]] std::optional<qint64> processIdForUniqueName(
        const QString &uniqueName) const override;

private:
    QDBusConnection m_connection;
};

} // namespace QindaQt::Shell::GlobalMenu::Registrar
