// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_surface/panel_surface_configuration.h"

#include <QObject>

#include <memory>

class QGuiApplication;

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::ShellOrchestration {
class PanelInteractionStore;
}

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;

// Production composition for interaction facts only. Every producer acquires
// leases in PanelInteractionStore; PanelVisibilityPolicy remains the sole
// authority deciding whether those facts make a panel visible or reserving.
class PanelVisibilityRuntime final : public QObject {
    Q_OBJECT
public:
    PanelVisibilityRuntime(
        QGuiApplication &application,
        ShellOrchestration::PanelInteractionStore &interactions,
        Services::SettingsClient::SettingsClient &settings,
        GlobalShortcutRegistrar &shortcutRegistrar,
        const Profiles::LayoutProfile &profile, int themeMotionDuration,
        QObject *parent = nullptr);
    ~PanelVisibilityRuntime() override;

    [[nodiscard]] bool synchronize(
        const ShellSurface::PanelSurfacePlan &plan,
        bool compositorAuthorityAvailable, bool *immediateReconcile,
        QString *error = nullptr);

    // Deterministic settings boundary. Missing/malformed first truth keeps
    // reduced motion enabled; later service loss retains the last confirmation.
    void applySettings(const QVariantMap &values);
    [[nodiscard]] bool reducedMotion() const noexcept;
    [[nodiscard]] int animationDurationMilliseconds() const noexcept;

private:
    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::Shell
