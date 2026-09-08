// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/desktop_controls/desktop_shortcut_set.h"
#include "qindaqt/session/desktop_controls/freedesktop_feedback_notifier.h"
#include "qindaqt/session/desktop_controls/powerdevil_brightness_feedback_observer.h"
#include "qindaqt/session/desktop_controls/settings1_idle_preferences.h"
#include "qindaqt/session/desktop_controls/powerdevil_idle_preferences_binding.h"
#include "qindaqt/session/desktop_controls/volume_key_controller.h"

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_client/qt_audio_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include "kglobal_accel_registrar.h"

#include <qindaqt/session/powerdevil_idle/powerdevil_idle_adapter.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QtDBus/QDBusConnection>
#include <QTextStream>

using QindaQt::Session::DesktopControls::VolumeKeyController;

namespace {

// Reason codes are stable identifiers; the user sees friendly text with the
// code only when nothing specific matches.
QString audioUnavailableText(const QString &reasonCode)
{
    if (reasonCode == QLatin1String("no-default-output")) {
        return QStringLiteral("No default output device is selected.");
    }
    if (reasonCode == QLatin1String("volume-unsupported")
        || reasonCode == QLatin1String("mute-unsupported")) {
        return QStringLiteral("The default output does not support this control.");
    }
    if (reasonCode == QLatin1String("operation-busy")) {
        return QStringLiteral("Audio is busy; try again in a moment.");
    }
    return QStringLiteral("Audio control is unavailable (%1).").arg(reasonCode);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("qindaqt-desktop-controls"));
    QGuiApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QGuiApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));
    // Media-key QActions and the PowerDevil D-Bus observers need a running GUI
    // event loop even though this process draws nothing of its own.
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QindaQt media keys and idle display-off policy."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("no-idle-policy"),
                      QStringLiteral("Do not turn displays off after user idle time.")});
    parser.process(application);

    const QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected()) {
        QTextStream(stderr) << "qindaqt-desktop-controls: no session bus\n";
        return 2;
    }

    QindaQt::Audio::QtAudioTransport audioTransport(sessionBus);
    QindaQt::Audio::AudioClient audioClient(&audioTransport);
    audioClient.start();

    QindaQt::Session::DesktopControls::FreedesktopFeedbackNotifier notifier(sessionBus);

    QindaQt::Session::DesktopControls::PowerDevilBrightnessFeedbackObserver brightnessObserver(
        sessionBus, &application);
    QObject::connect(
        &brightnessObserver,
        &QindaQt::Session::DesktopControls::PowerDevilBrightnessFeedbackObserver::brightnessFeedbackRequested,
        &notifier, [&notifier](int percent) { notifier.showBrightness(percent); });
    brightnessObserver.start();

    QindaQt::Session::DesktopControls::VolumeKeyController volumeController(audioClient);
    QObject::connect(&volumeController, &VolumeKeyController::volumeFeedbackRequested,
                     &notifier, [&notifier](int percent, bool muted) {
                         notifier.showVolume(percent, muted);
                     });
    QObject::connect(&volumeController, &VolumeKeyController::volumeUnavailable,
                     &notifier, [&notifier](const QString &reasonCode) {
                         notifier.showNotice(
                             QStringLiteral("Volume"), audioUnavailableText(reasonCode),
                             QStringLiteral("audio-volume-muted"));
                     });

    QindaQt::Session::DesktopControls::KGlobalAccelRegistrar registrar;
    QindaQt::Session::DesktopControls::DesktopShortcutSet shortcuts(
        registrar,
        QindaQt::Session::DesktopControls::DesktopShortcutTriggers{
            .volumeUp = [&volumeController] { volumeController.raiseVolume(); },
            .volumeDown = [&volumeController] { volumeController.lowerVolume(); },
            .toggleMute = [&volumeController] { volumeController.toggleMute(); },
            .brightnessUp = {},
            .brightnessDown = {},
            .takeScreenshot = {},
        },
        &application,
        QindaQt::Session::DesktopControls::DesktopShortcutRegistrationOptions{
            .registerBrightness = false,
            .registerScreenshot = false,
        });

    QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport(sessionBus);
    QindaQt::Services::SettingsClient::SettingsClient settingsClient(
        settingsTransport,
        QindaQt::Session::DesktopControls::Settings1IdlePreferences::scopedKey());
    QString settingsError;
    if (!settingsClient.start(&settingsError)) {
        QTextStream(stderr) << "qindaqt-desktop-controls: settings client failed: "
                            << settingsError << '\n';
    }
    QindaQt::Session::DesktopControls::Settings1IdlePreferences idlePreferences(settingsClient);

    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter powerDevilIdle(sessionBus);
    QindaQt::Session::DesktopControls::PowerDevilIdlePreferencesBinding idleBinding(
        idlePreferences, powerDevilIdle, &application);
    if (!parser.isSet(QStringLiteral("no-idle-policy"))) {
        idleBinding.start();
    }

    return application.exec();
}
