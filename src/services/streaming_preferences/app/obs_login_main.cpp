// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/streaming_preferences/settings1_streaming_preferences.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QEventLoop>
#include <QDebug>
#include <QTimer>

#include <unistd.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QindaQt::Services::SettingsClient::QtSettingsTransport transport(QDBusConnection::sessionBus());
    QindaQt::Services::SettingsClient::SettingsClient client(
        transport,
        QindaQt::Services::StreamingPreferences::Settings1StreamingPreferences::scopedKeys());
    QindaQt::Services::StreamingPreferences::Settings1StreamingPreferences preferences(client);
    QString error;
    if (!client.start(&error)) {
        qWarning().noquote() << "OBS login skipped: Settings1 unavailable:" << error;
        return 0;
    }
    QEventLoop baselineWait;
    QObject::connect(&preferences,
                     &QindaQt::Services::StreamingPreferences::Settings1StreamingPreferences::preferencesChanged,
                     &baselineWait, [&baselineWait, &preferences] {
                         if (preferences.isLoaded()) baselineWait.quit();
                     });
    // A missed/uncertain baseline cannot authorize launching OBS. This is
    // the only gate between A02's one stable entry and an OBS exec.
    if (!preferences.isLoaded()) QTimer::singleShot(5'000, &baselineWait, &QEventLoop::quit);
    if (!preferences.isLoaded()) baselineWait.exec();
    if (!preferences.isLoaded() || !preferences.startObsAtLogin()) return 0;
    // AGENT-CONTRACT: replace the A02-owned direct child, do not daemonize or
    // startDetached. The session can then stop OBS at logout in the same PID.
    char program[] = "obs";
    char minimize[] = "--minimize-to-tray";
    char *const args[] = {program, minimize, nullptr};
    execvp(program, args);
    return 1;
}
