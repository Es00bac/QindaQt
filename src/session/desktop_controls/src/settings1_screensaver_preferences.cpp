// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/settings1_screensaver_preferences.h"

#include <QVariant>

namespace QindaQt::Session::DesktopControls {

const QStringList &Settings1ScreensaverPreferences::scopedKeys()
{
    static const QStringList keys{QStringLiteral("power.screensaver"),
                                  QStringLiteral("power.screensaverMinutes")};
    return keys;
}

Settings1ScreensaverPreferences::Settings1ScreensaverPreferences(
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : ScreensaverPreferencesProvider(parent), m_client(client)
{
    connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &Settings1ScreensaverPreferences::onSnapshotChanged);
}

Settings1ScreensaverPreferences::~Settings1ScreensaverPreferences() = default;

ScreensaverPreferences Settings1ScreensaverPreferences::currentPreferences() const
{
    return m_current;
}

void Settings1ScreensaverPreferences::refresh()
{
    m_client.refresh();
}

void Settings1ScreensaverPreferences::onSnapshotChanged()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot.has_value()) {
        // No confirmed owner yet: never start a saver on a guess.
        return;
    }
    const QVariant saver = snapshot->values.value(QStringLiteral("power.screensaver"));
    const QVariant minutes =
        snapshot->values.value(QStringLiteral("power.screensaverMinutes"));
    const qint64 persistedMinutes = minutes.isValid() && minutes.canConvert<qint64>()
        ? minutes.toLongLong()
        : qint64{ScreensaverPreferences::defaultTimeoutMinutes()};
    const ScreensaverPreferences next =
        ScreensaverPreferences::fromPersisted(saver.toString(), persistedMinutes);
    if (next == m_current) {
        return;
    }
    m_current = next;
    Q_EMIT preferencesChanged(m_current);
}

} // namespace QindaQt::Session::DesktopControls
