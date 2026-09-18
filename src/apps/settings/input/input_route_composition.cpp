// SPDX-License-Identifier: GPL-3.0-or-later

#include "input_route_composition.h"

#include <qindaqt/apps/settings_input/keyboard_config_port.h>
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>
#include <qindaqt/apps/settings_input/keyboard_layouts_model.h>
#include <qindaqt/apps/settings_input/keyboard_settings_model.h>
#include <qindaqt/apps/settings_input/pointer_device_port.h>
#include <qindaqt/apps/settings_input/pointer_devices_model.h>
#include <qindaqt/apps/settings_input/shortcut_port.h>
#include <qindaqt/apps/settings_input/shortcuts_model.h>
#include <qindaqt/apps/settings_input/tablet_devices_model.h>

#include <qindaqt/services/tablet_devices/kwin_tablet_devices.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <QtCore/QDir>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>
#include <qindaqt/services/settings_client/qt_settings_transport.h>

#include <QtDBus/QDBusConnection>
#include <QtQml/QQmlEngine>

namespace QindaQt::Apps::SettingsInput {
namespace {

// The xkb rules catalog is display data from the installed xkeyboard-config
// package; the environment override exists for hermetic QML tests.
QString evdevCatalogPath() {
    const QString override =
        qEnvironmentVariable("QINDAQT_INPUT_EVDEV_CATALOG");
    if (!override.isEmpty()) {
        return override;
    }
    return QStringLiteral("/usr/share/X11/xkb/rules/evdev.xml");
}

} // namespace

class InputRouteComposition::Private final {
public:
    Private()
        : bus(QDBusConnection::sessionBus()),
          pointerPort(bus),
          configFilePath(QDir(QStandardPaths::writableLocation(
                                    QStandardPaths::StandardLocation::
                                        ConfigLocation))
                             .absoluteFilePath(QStringLiteral("kcminputrc"))),
          keyboardConfigPort(configFilePath, bus),
          kxkbrcPath(QDir(QStandardPaths::writableLocation(
                              QStandardPaths::StandardLocation::ConfigLocation))
                         .absoluteFilePath(QStringLiteral("kxkbrc"))),
          layoutPort(kxkbrcPath, bus),
          shortcutPort(bus,
                       QStandardPaths::writableLocation(
                           QStandardPaths::StandardLocation::GenericDataLocation)),
          pointerModel(pointerPort),
          keyboardModel(keyboardConfigPort),
          layoutsModel(layoutPort),
          shortcutsModel(shortcutPort),
          tabletPort(bus),
          tabletSettingsTransport(bus),
          tabletSettingsClient(
              tabletSettingsTransport,
              Services::TabletDevices::Settings1TabletMappings::scopedKey()),
          tabletMappings(tabletSettingsClient),
          tabletModel(tabletPort, tabletOutputs, &tabletMappings) {
        // AGENT-CONTRACT: the ledger client starts here, not lazily: the
        // route must be able to record a choice the moment the user makes
        // one. A Settings1 owner that is not up yet leaves the store
        // unloaded, and the route says the choice could not be remembered
        // rather than pretending it was.
        QString error;
        if (!tabletSettingsClient.start(&error)) {
            tabletSettingsError = error;
        }
    }

    QDBusConnection bus;
    KWinPointerDevicePort pointerPort;
    QString configFilePath;
    QtKeyboardConfigPort keyboardConfigPort;
    QString kxkbrcPath;
    QtKeyboardLayoutPort layoutPort;
    QtShortcutPort shortcutPort;
    PointerDevicesModel pointerModel;
    KeyboardSettingsModel keyboardModel;
    KeyboardLayoutsModel layoutsModel;
    ShortcutsModel shortcutsModel;
    Services::TabletDevices::KWinTabletDevicePort tabletPort;
    Services::TabletDevices::ScreenTabletOutputs tabletOutputs;
    Services::SettingsClient::QtSettingsTransport tabletSettingsTransport;
    Services::SettingsClient::SettingsClient tabletSettingsClient;
    Services::TabletDevices::Settings1TabletMappings tabletMappings;
    QString tabletSettingsError;
    TabletDevicesModel tabletModel;
};

InputRouteComposition::InputRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {
    // AGENT-CONTRACT: Construction performs no IO and no D-Bus calls. Every
    // model fills itself when its tab becomes visible (refresh() from QML),
    // so importing this module from Main.qml stays cheap for every route,
    // including when the desktop authority is unreachable.
    d->layoutsModel.setCatalogPath(evdevCatalogPath());
}

InputRouteComposition::~InputRouteComposition() = default;

QObject *InputRouteComposition::pointerDevices() const {
    return &d->pointerModel;
}

QObject *InputRouteComposition::keyboard() const { return &d->keyboardModel; }

QObject *InputRouteComposition::layouts() const { return &d->layoutsModel; }

QObject *InputRouteComposition::shortcuts() const {
    return &d->shortcutsModel;
}

QObject *InputRouteComposition::tabletDevices() const {
    return &d->tabletModel;
}

} // namespace QindaQt::Apps::SettingsInput
