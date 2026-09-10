// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_surface/panel_window_factory.h"

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QPointer>
#include <QQuickWindow>
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

namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
}

namespace QindaQt::StatusNotifierApplet {
class StatusNotifierAppletController;
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
                              ShellClipboardApplet::ClipboardAppletController *clipboardAppletAccess,
                              ShellTaskListApplet::TaskListAppletController *taskListAppletAccess,
                              StatusNotifierApplet::StatusNotifierAppletController *statusNotifierAppletAccess);
    ~RuntimePanelWindowFactory() override;

    [[nodiscard]] std::unique_ptr<QQuickWindow> createWindow(
        const ShellSurface::PanelSurfaceConfiguration &configuration,
        QString *error = nullptr) override;
    [[nodiscard]] QJsonArray appletEvidence() const;

    // Replaces the theme map for future windows and pushes it onto every live
    // panel window's theme property. QML panel maps are plain (non-readonly)
    // properties precisely so this confirmed-preference update path works.
    void setTheme(const QVariantMap &theme);
    // Replaces the resolved panel inventory for future windows and pushes the
    // updated maps onto live windows whose panel survived the adoption. Live
    // layout adoption (ADR-0122) calls this before reconciling so windows for
    // newly introduced panel ids can be created.
    void adoptProfile(const Profiles::LayoutProfile &profile,
                      const Applets::ManifestCatalog &applets,
                      const AppletHost::CapabilityPolicy &policy);
    // Desktop controls composite facade (docs/wiki/shell/desktop-controls.md).
    // Set after construction so the existing runtime call site keeps
    // compiling; a null value leaves panel QML untouched. The window receives
    // it through QObject::setProperty after creation, which is a harmless
    // no-op while RuntimePanel.qml has not yet declared the property, so the
    // C++ and QML halves of this wiring may land in either order.
    void setDesktopControlsAccess(QObject *access) noexcept;
    void setPanelQuickConfig(QObject *access) noexcept;

private:
    [[nodiscard]] bool ensureComponent(QString *error);

    QQmlEngine &m_engine;
    QVariantMap m_theme;
    QHash<QString, QVariantMap> m_panels;
    // AGENT-NOTE: Live windows are tracked weakly only so setTheme can reach
    // them; ownership stays with the surface backend that took the unique_ptr.
    QList<QPointer<QQuickWindow>> m_liveWindows;
    NotificationCenterAppletAccess *m_notificationCenterAccess = nullptr;
    AudioApplet::AudioAppletController *m_audioAppletAccess = nullptr;
    BluetoothApplet::BluetoothAppletController *m_bluetoothAppletAccess = nullptr;
    PowerApplet::PowerAppletController *m_powerAppletAccess = nullptr;
    Launcher::LauncherAppletController *m_launcherAppletAccess = nullptr;
    GlobalMenu::GlobalMenuAppletAccess *m_globalMenuAppletAccess = nullptr;
    ShellClipboardApplet::ClipboardAppletController *m_clipboardAppletAccess = nullptr;
    ShellTaskListApplet::TaskListAppletController *m_taskListAppletAccess = nullptr;
    StatusNotifierApplet::StatusNotifierAppletController *m_statusNotifierAppletAccess = nullptr;
    QObject *m_desktopControlsAccess = nullptr;
    QObject *m_panelQuickConfig = nullptr;
    std::unique_ptr<QQmlComponent> m_component;
};

} // namespace QindaQt::Shell
