// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellstartuppreferences.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QDBusConnection>
#include <QEventLoop>
#include <QTimer>

namespace QindaQt::Shell {

std::optional<ShellPreferenceValues>
readConfirmedShellPreferences(const QDBusConnection &bus, int timeoutMilliseconds,
                              QString *error)
{
    using namespace Services::SettingsClient;

    QtSettingsTransport transport(bus);
    // One bounded read: the deadline below caps total wait, and startup falls
    // back to built-in defaults instead of waiting out service recovery.
    SettingsClient client(transport, ShellPreferenceValues::scopedKeys(),
                          {.requestTimeoutMilliseconds = timeoutMilliseconds,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {timeoutMilliseconds}});
    QString startError;
    if (!client.start(&startError)) {
        if (error != nullptr) {
            *error = startError;
        }
        return std::nullopt;
    }

    // A synchronously ready snapshot must not wait out the deadline.
    if (!client.snapshot().has_value()) {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&client, &SettingsClient::snapshotChanged, &loop,
                         &QEventLoop::quit);
        deadline.start(timeoutMilliseconds);
        loop.exec();
    }
    const auto snapshot = client.snapshot();
    client.stop();

    if (!snapshot.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Settings1 did not confirm a shell preference snapshot within %1 ms")
                         .arg(timeoutMilliseconds);
        }
        return std::nullopt;
    }
    return ShellPreferenceValues::fromVariantMap(snapshot->values, error);
}

} // namespace QindaQt::Shell
