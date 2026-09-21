// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/desktop_controls/airplane_mode_key_controller.h"
#include "qindaqt/session/desktop_controls/battery_notification_policy.h"
#include "qindaqt/session/desktop_controls/desktop_shortcut_set.h"
#include "qindaqt/session/desktop_controls/freedesktop_feedback_notifier.h"
#include "qindaqt/session/desktop_controls/mic_mute_key_controller.h"
#include "qindaqt/session/desktop_controls/powerdevil_brightness_feedback_observer.h"
#include "qindaqt/session/desktop_controls/settings1_idle_preferences.h"
#include "qindaqt/session/desktop_controls/settings1_screensaver_preferences.h"
#include "qindaqt/session/desktop_controls/powerdevil_idle_preferences_binding.h"
#include "qindaqt/session/desktop_controls/screensaver_catalog.h"
#include "qindaqt/session/desktop_controls/tablet_arrival_notifier.h"
#include "qindaqt/session/desktop_controls/tablet_mapping_policy.h"
#include "qindaqt/session/desktop_controls/tablet_route_launcher.h"
#include "qindaqt/session/desktop_controls/volume_key_controller.h"

#include <qindaqt/services/tablet_devices/kwin_tablet_devices.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_client/qt_audio_transport.h>
#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_qt_transport/qt_network_transport.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include "kglobal_accel_registrar.h"
#include "screensaver_launcher.h"

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

QString micMuteUnavailableText(const QString &reasonCode)
{
    if (reasonCode == QLatin1String("no-default-input")) {
        return QStringLiteral("No default microphone is selected.");
    }
    if (reasonCode == QLatin1String("mute-unsupported")) {
        return QStringLiteral("The default microphone does not support mute.");
    }
    return QStringLiteral("Microphone control is unavailable (%1).").arg(reasonCode);
}

QString airplaneModeUnavailableText(const QString &reasonCode)
{
    if (reasonCode == QLatin1String("no-wifi-radio")) {
        return QStringLiteral("No Wi-Fi radio was reported.");
    }
    if (reasonCode == QLatin1String("no-snapshot")) {
        return QStringLiteral("Network state is not available yet.");
    }
    return QStringLiteral("Airplane mode is unavailable (%1).").arg(reasonCode);
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
    parser.addOption({QStringLiteral("no-tablet-policy"),
                      QStringLiteral("Do not map pen displays to their own screen.")});
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

    QindaQt::Power::QtPowerTransport powerTransport(sessionBus);
    QindaQt::Power::PowerClient powerClient(&powerTransport);
    powerClient.start();
    QindaQt::Session::DesktopControls::BatteryNotificationPolicy batteryNotifications(
        powerClient, notifier, &application);
    batteryNotifications.start();

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

    QindaQt::Session::DesktopControls::MicMuteKeyController micMuteController(audioClient);
    QObject::connect(
        &micMuteController,
        &QindaQt::Session::DesktopControls::MicMuteKeyController::micMuteFeedbackRequested,
        &notifier, [&notifier](bool muted) {
            notifier.showNotice(
                QStringLiteral("Microphone"),
                muted ? QStringLiteral("Microphone muted") : QStringLiteral("Microphone unmuted"),
                muted ? QStringLiteral("microphone-sensitivity-muted")
                     : QStringLiteral("microphone-sensitivity-high"));
        });
    QObject::connect(
        &micMuteController,
        &QindaQt::Session::DesktopControls::MicMuteKeyController::micMuteUnavailable,
        &notifier, [&notifier](const QString &reasonCode) {
            notifier.showNotice(QStringLiteral("Microphone"),
                                micMuteUnavailableText(reasonCode),
                                QStringLiteral("microphone-sensitivity-muted"));
        });

    QindaQt::Network::Client::QtNetworkTransport networkTransport(sessionBus);
    QindaQt::Network::Client::NetworkClient networkClient(networkTransport);
    QString networkError;
    if (!networkClient.start(&networkError)) {
        QTextStream(stderr) << "qindaqt-desktop-controls: network client failed: "
                            << networkError << '\n';
    }

    QindaQt::Session::DesktopControls::AirplaneModeKeyController airplaneModeController(
        networkClient);
    QObject::connect(
        &airplaneModeController,
        &QindaQt::Session::DesktopControls::AirplaneModeKeyController::airplaneModeFeedbackRequested,
        &notifier, [&notifier](bool airplaneModeOn) {
            notifier.showNotice(
                QStringLiteral("Airplane mode"),
                airplaneModeOn ? QStringLiteral("Airplane mode on")
                              : QStringLiteral("Airplane mode off"),
                airplaneModeOn ? QStringLiteral("airplane-mode")
                              : QStringLiteral("network-wireless"));
        });
    QObject::connect(
        &airplaneModeController,
        &QindaQt::Session::DesktopControls::AirplaneModeKeyController::airplaneModeUnavailable,
        &notifier, [&notifier](const QString &reasonCode) {
            notifier.showNotice(QStringLiteral("Airplane mode"),
                                airplaneModeUnavailableText(reasonCode),
                                QStringLiteral("airplane-mode"));
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
            .toggleMicMute = [&micMuteController] { micMuteController.toggleMicMute(); },
            .toggleAirplaneMode =
                [&airplaneModeController] { airplaneModeController.toggleAirplaneMode(); },
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

    // AGENT-CONTRACT: The tablet ledger gets its own Settings1 client scoped
    // to `input.tabletMappings`. Settings1 rejects a whole snapshot on one
    // unknown key (ADR-0126), so widening the idle client's scope instead
    // would put both features behind one schema risk.
    QindaQt::Services::SettingsClient::QtSettingsTransport tabletSettingsTransport(sessionBus);
    QindaQt::Services::SettingsClient::SettingsClient tabletSettingsClient(
        tabletSettingsTransport,
        QindaQt::Services::TabletDevices::Settings1TabletMappings::scopedKey());
    QindaQt::Services::TabletDevices::Settings1TabletMappings tabletMappings(
        tabletSettingsClient);
    QindaQt::Services::TabletDevices::KWinTabletDevicePort tabletPort(sessionBus);
    QindaQt::Services::TabletDevices::KWinTabletDeviceWatcher tabletWatcher(
        sessionBus, &application);
    QindaQt::Services::TabletDevices::ScreenTabletOutputs tabletOutputs(&application);
    QindaQt::Session::DesktopControls::TabletMappingPolicy tabletPolicy(
        tabletPort, tabletWatcher, tabletOutputs, tabletMappings, &application);
    QindaQt::Session::DesktopControls::TabletArrivalNotifier tabletNotifier(
        sessionBus, &application);
    QindaQt::Session::DesktopControls::TabletRouteLauncher tabletRoutes({}, &application);
    QObject::connect(
        &tabletPolicy,
        &QindaQt::Session::DesktopControls::TabletMappingPolicy::tabletAnnounced,
        &tabletNotifier,
        [&tabletNotifier](const QString &group, const QString &name,
                          const QString &output) {
            tabletNotifier.announce(group, name, output);
        });
    QObject::connect(
        &tabletNotifier,
        &QindaQt::Session::DesktopControls::TabletArrivalNotifier::setupRequested,
        &tabletRoutes,
        [&tabletRoutes](const QString &group) {
            tabletRoutes.openTabletSettings(group);
        });
    QObject::connect(
        &tabletNotifier,
        &QindaQt::Session::DesktopControls::TabletArrivalNotifier::useActiveScreenRequested,
        &tabletPolicy, [&tabletPolicy](const QString &group) {
            QString error;
            if (!tabletPolicy.applyUserChoice(
                    group,
                    QindaQt::Services::TabletDevices::TabletMapChoice::FollowActiveScreen,
                    QString{}, &error)) {
                QTextStream(stderr)
                    << "qindaqt-desktop-controls: tablet choice failed: " << error
                    << '\n';
            }
        });
    QObject::connect(
        &tabletPolicy,
        &QindaQt::Session::DesktopControls::TabletMappingPolicy::mappingFailed,
        &application, [](const QString &message) {
            QTextStream(stderr) << "qindaqt-desktop-controls: tablet mapping: "
                                << message << '\n';
        });
    if (!parser.isSet(QStringLiteral("no-tablet-policy"))) {
        QString tabletSettingsError;
        if (!tabletSettingsClient.start(&tabletSettingsError)) {
            QTextStream(stderr)
                << "qindaqt-desktop-controls: tablet settings client failed: "
                << tabletSettingsError << '\n';
        }
        QString notifierError;
        if (!tabletNotifier.start(&notifierError)) {
            QTextStream(stderr) << "qindaqt-desktop-controls: " << notifierError
                                << '\n';
        }
        QString tabletError;
        if (!tabletPolicy.start(&tabletError)) {
            QTextStream(stderr)
                << "qindaqt-desktop-controls: tablet hotplug unavailable: "
                << tabletError << '\n';
        }
    }

    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter powerDevilIdle(sessionBus);
    QindaQt::Session::DesktopControls::PowerDevilIdlePreferencesBinding idleBinding(
        idlePreferences, powerDevilIdle, &application);
    if (!parser.isSet(QStringLiteral("no-idle-policy"))) {
        idleBinding.start();
    }

    // AGENT-CONTRACT: the idle screensaver gets its own Settings1 client
    // scoped to the `power.screensaver` pair, for the same reason the tablet
    // ledger does: Settings1 rejects a whole snapshot on one unknown key
    // (ADR-0126), so widening the idle client would put display-off behind
    // this feature's schema risk.
    QindaQt::Services::SettingsClient::QtSettingsTransport screensaverSettingsTransport(
        sessionBus);
    QindaQt::Services::SettingsClient::SettingsClient screensaverSettingsClient(
        screensaverSettingsTransport,
        QindaQt::Session::DesktopControls::Settings1ScreensaverPreferences::scopedKeys());
    QString screensaverSettingsError;
    if (!screensaverSettingsClient.start(&screensaverSettingsError)) {
        QTextStream(stderr) << "qindaqt-desktop-controls: screensaver settings client failed: "
                            << screensaverSettingsError << '\n';
    }
    // The saver set is discovered from the installed desktop entries, not
    // hard-coded (ADR-0226); the catalog scans, so a newly packaged saver is
    // picked up the next time a snapshot resolves.
    QindaQt::Session::DesktopControls::DesktopEntryScreensaverCatalog
        screensaverCatalog;
    QindaQt::Session::DesktopControls::Settings1ScreensaverPreferences screensaverPreferences(
        screensaverSettingsClient, screensaverCatalog);

    // Idle screensaver (whichever installed saver the operator chose), stopped
    // on activity or lock. Configured in Settings -> Screen saver; the locker
    // stays the only lock authority. Declared after its client so it is
    // destroyed first.
    QindaQt::Session::DesktopControls::ScreensaverLauncher screensaver(
        sessionBus, screensaverPreferences, screensaverCatalog, &application);
    screensaver.start();

    return application.exec();
}
