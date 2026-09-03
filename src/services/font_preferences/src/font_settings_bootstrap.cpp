// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_settings_bootstrap.h"

#include "qindaqt/services/font_preferences/font_bootstrap.h"
#include "qindaqt/services/font_preferences/font_preferences_codec.h"
#include "qindaqt/services/font_preferences/font_settings_bridge.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QDBusConnection>
#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

namespace QindaQt::Services::FontPreferences {

namespace {

using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::ClientTiming;
using QindaQt::Services::SettingsClient::QtSettingsTransport;
using QindaQt::Services::SettingsClient::SettingsClient;

void setDiagnostic(QString *output, const QString &message)
{
    if (output != nullptr) {
        *output = message;
    }
}

} // namespace

bool FontSettingsBootstrap::applyPreferences(QGuiApplication &application,
                                             const FontPreferences &preferences,
                                             QString *diagnostic)
{
    if (!preferences.isValid()) {
        setDiagnostic(diagnostic, QStringLiteral("font preferences failed validation"));
        return false;
    }
    application.setFont(FontBootstrap::createApplicationFont(preferences));
    return true;
}

std::optional<FontPreferences> FontSettingsBootstrap::readConfirmedPreferences(
    SettingsClient &client,
    int timeoutMilliseconds,
    QString *diagnostic)
{
    if (timeoutMilliseconds <= 0) {
        setDiagnostic(diagnostic, QStringLiteral("bootstrap timeout must be positive"));
        return std::nullopt;
    }
    QString startError;
    if (!client.start(&startError)) {
        setDiagnostic(diagnostic, startError.isEmpty()
                                      ? QStringLiteral("settings client failed to start")
                                      : startError.left(512));
        return std::nullopt;
    }

    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    bool gotBaseline = false;
    QObject::connect(&client, &SettingsClient::snapshotChanged, &loop, [&]() {
        if (client.state() == ClientState::Ready && client.snapshot().has_value()) {
            gotBaseline = true;
            loop.quit();
        }
    });
    // AGENT-GUARD: An unavailable authority fails fast instead of burning the
    // whole timeout on retries; Degraded stays bounded by the deadline.
    QObject::connect(&client, &SettingsClient::stateChanged, &loop, [&]() {
        if (client.state() == ClientState::Unavailable) {
            loop.quit();
        }
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(timeoutMilliseconds);
    loop.exec();

    QVariantMap confirmed;
    if (gotBaseline && client.state() == ClientState::Ready && client.snapshot().has_value()) {
        confirmed = client.snapshot()->values;
    }
    const QString clientError = client.lastError();
    client.stop();

    if (confirmed.isEmpty()) {
        setDiagnostic(diagnostic, clientError.isEmpty()
                                      ? QStringLiteral("settings snapshot was not confirmed before the deadline")
                                      : QStringLiteral("settings snapshot unavailable: %1").arg(clientError.left(512)));
        return std::nullopt;
    }
    QString decodeError;
    auto preferences = FontPreferencesCodec::fromSettingsMap(confirmed, &decodeError);
    if (!preferences) {
        setDiagnostic(diagnostic, decodeError.isEmpty()
                                      ? QStringLiteral("confirmed font preferences failed to decode")
                                      : decodeError.left(512));
        return std::nullopt;
    }
    return preferences;
}

bool FontSettingsBootstrap::applyFromSessionSettings(QGuiApplication &application,
                                                     QString *diagnostic)
{
    // AGENT-NOTE: Short client timing keeps the synchronous pre-window read
    // bounded well under DefaultBootstrapTimeoutMilliseconds when the
    // Settings1 service is absent from the session bus.
    ClientTiming timing;
    timing.requestTimeoutMilliseconds = 250;
    timing.debounceMilliseconds = 0;
    timing.retryMilliseconds = {50};
    QtSettingsTransport transport(QDBusConnection::sessionBus());
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), timing);
    const auto preferences = readConfirmedPreferences(client, DefaultBootstrapTimeoutMilliseconds, diagnostic);
    if (!preferences) {
        return false;
    }
    return applyPreferences(application, *preferences, diagnostic);
}

} // namespace QindaQt::Services::FontPreferences
