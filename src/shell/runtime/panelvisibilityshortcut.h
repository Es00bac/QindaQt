// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "panelvisibilitytimer.h"

#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QKeySequence>
#include <QObject>

#include <memory>

class QAction;

namespace QindaQt::ShellOrchestration {
class PanelInteractionStore;
}

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;

class PanelVisibilityShortcutProducer final : public QObject {
    Q_OBJECT
public:
    PanelVisibilityShortcutProducer(
        GlobalShortcutRegistrar &registrar,
        ShellOrchestration::PanelInteractionStore &interactions,
        PanelVisibilityTimerPort &timer, QObject *parent = nullptr);
    ~PanelVisibilityShortcutProducer() override;

    [[nodiscard]] static QString stableActionId();
    [[nodiscard]] static QKeySequence defaultShortcut();
    [[nodiscard]] QAction *action() const noexcept;
    [[nodiscard]] bool registrationRequestAccepted() const noexcept;
    [[nodiscard]] bool activeBindingPresent() const noexcept;

    void setIdentities(
        QVector<ShellVisibility::PanelSurfaceIdentity> identities);
    void setHoldMilliseconds(int holdMilliseconds);
    void triggerReveal();

Q_SIGNALS:
    void activeBindingPresentChanged();

private:
    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::Shell
