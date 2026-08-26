// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_surface/panel_window_factory.h"

#include <QHash>
#include <QVariantMap>

#include <memory>

class QQmlComponent;
class QQmlEngine;

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Shell {

class RuntimePanelWindowFactory final : public ShellSurface::PanelWindowFactory {
public:
    RuntimePanelWindowFactory(QQmlEngine &engine,
                              const Profiles::LayoutProfile &profile,
                              QVariantMap theme,
                              const Applets::ManifestCatalog &applets,
                              const AppletHost::CapabilityPolicy &policy);
    ~RuntimePanelWindowFactory() override;

    [[nodiscard]] std::unique_ptr<QQuickWindow> createWindow(
        const ShellSurface::PanelSurfaceConfiguration &configuration,
        QString *error = nullptr) override;

private:
    [[nodiscard]] bool ensureComponent(QString *error);

    QQmlEngine &m_engine;
    QHash<QString, QVariantMap> m_panels;
    QVariantMap m_theme;
    std::unique_ptr<QQmlComponent> m_component;
};

} // namespace QindaQt::Shell
