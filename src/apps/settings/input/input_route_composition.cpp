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

#include <QtCore/QDir>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>
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
          shortcutsModel(shortcutPort) {}

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

} // namespace QindaQt::Apps::SettingsInput
