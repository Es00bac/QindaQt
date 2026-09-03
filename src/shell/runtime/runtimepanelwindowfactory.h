// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_surface/panel_window_factory.h"

#include <QHash>
#include <QVariantMap>

#include <memory>

class QQmlComponent;
class QQmlEngine;
class QObject;

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::ShellClipboardApplet {
class ClipboardAppletController;
}

namespace QindaQt::Shell {

class NotificationCenterAppletAccess;
namespace AudioApplet {
class AudioAppletController;
}
namespace BluetoothApplet {
class BluetoothAppletController;
}
namespace PowerApplet {
class PowerAppletController;
}
namespace Launcher {
class LauncherAppletController;
}
namespace GlobalMenu {
class GlobalMenuAppletAccess;
}
class RuntimePanelWindowFactory final : public ShellSurface::PanelWindowFactory {
public:
    RuntimePanelWindowFactory(QQmlEngine &engine,
                              const Profiles::LayoutProfile &profile,
                              QVariantMap theme,
                              const Applets::ManifestCatalog &applets,
                              const AppletHost::CapabilityPolicy &policy,
                              NotificationCenterAppletAccess *notificationCenterAccess,
                              AudioApplet::AudioAppletController *audioAppletAccess,
                              BluetoothApplet::BluetoothAppletController *bluetoothAppletAccess,
                              PowerApplet::PowerAppletController *powerAppletAccess,
                              Launcher::LauncherAppletController *launcherAppletAccess,
                              GlobalMenu::GlobalMenuAppletAccess *globalMenuAppletAccess,
                              ShellClipboardApplet::ClipboardAppletController *clipboardAppletAccess);
    ~RuntimePanelWindowFactory() override;

    [[nodiscard]] std::unique_ptr<QQuickWindow> createWindow(
        const ShellSurface::PanelSurfaceConfiguration &configuration,
        QString *error = nullptr) override;

private:
    [[nodiscard]] bool ensureComponent(QString *error);

    QQmlEngine &m_engine;
    QHash<QString, QVariantMap> m_panels;
    QVariantMap m_theme;
    NotificationCenterAppletAccess *m_notificationCenterAccess = nullptr;
    AudioApplet::AudioAppletController *m_audioAppletAccess = nullptr;
    BluetoothApplet::BluetoothAppletController *m_bluetoothAppletAccess = nullptr;
    PowerApplet::PowerAppletController *m_powerAppletAccess = nullptr;
    Launcher::LauncherAppletController *m_launcherAppletAccess = nullptr;
    GlobalMenu::GlobalMenuAppletAccess *m_globalMenuAppletAccess = nullptr;
    ShellClipboardApplet::ClipboardAppletController *m_clipboardAppletAccess = nullptr;
    std::unique_ptr<QQmlComponent> m_component;
};

} // namespace QindaQt::Shell
