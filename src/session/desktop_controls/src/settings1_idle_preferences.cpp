// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/settings1_idle_preferences.h"

#include <QVariant>

namespace QindaQt::Session::DesktopControls {

const QStringList &Settings1IdlePreferences::scopedKey()
{
    static const QStringList keys{QStringLiteral("power.idleDisplayOffMinutes")};
    return keys;
}

Settings1IdlePreferences::Settings1IdlePreferences(
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : IdlePreferencesProvider(parent), m_client(client)
{
    connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &Settings1IdlePreferences::onSnapshotChanged);
}

Settings1IdlePreferences::~Settings1IdlePreferences() = default;

IdleDisplayPreferences Settings1IdlePreferences::currentPreferences() const
{
    return m_current;
}

void Settings1IdlePreferences::refresh()
{
    m_client.refresh();
}

void Settings1IdlePreferences::onSnapshotChanged()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot.has_value()) {
        // No confirmed owner yet: keep the documented default rather than
        // disabling the policy on a transport blip.
        return;
    }
    const QVariant value =
        snapshot->values.value(QStringLiteral("power.idleDisplayOffMinutes"));
    const IdleDisplayPreferences next = value.isValid() && value.canConvert<qint64>()
        ? IdleDisplayPreferences::fromMinutes(value.toLongLong())
        : IdleDisplayPreferences{true, IdleDisplayPreferences::defaultTimeoutMinutes()};
    if (next == m_current) {
        return;
    }
    m_current = next;
    Q_EMIT preferencesChanged(m_current);
}

} // namespace QindaQt::Session::DesktopControls
