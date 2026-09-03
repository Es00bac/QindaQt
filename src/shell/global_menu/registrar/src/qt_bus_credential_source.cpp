// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/registrar/qt_bus_credential_source.h>

#include <QtDBus/QDBusConnectionInterface>

#include <utility>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

QtBusCredentialSource::QtBusCredentialSource(QDBusConnection connection)
    : m_connection(std::move(connection))
{
}

std::optional<qint64> QtBusCredentialSource::processIdForUniqueName(
    const QString &uniqueName) const
{
    if (!m_connection.isConnected() || m_connection.interface() == nullptr
        || !uniqueName.startsWith(u':')) {
        return std::nullopt;
    }
    const QDBusReply<uint> reply = m_connection.interface()->servicePid(uniqueName);
    if (!reply.isValid() || reply.value() == 0) {
        return std::nullopt;
    }
    return static_cast<qint64>(reply.value());
}

} // namespace QindaQt::Shell::GlobalMenu::Registrar
