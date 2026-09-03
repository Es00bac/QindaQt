// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QObject>

class QEvent;
class QGuiApplication;

namespace QindaQt::ShellOrchestration {
class PanelInteractionStore;
}

namespace QindaQt::Shell {

class PanelVisibilityPopupProducer final : public QObject {
    Q_OBJECT
public:
    PanelVisibilityPopupProducer(
        QGuiApplication &application,
        ShellOrchestration::PanelInteractionStore &interactions,
        QObject *parent = nullptr);
    ~PanelVisibilityPopupProducer() override;

    void setIdentities(
        QVector<ShellVisibility::PanelSurfaceIdentity> identities);
    // Explicit shell-owned popup sources can use this same lease boundary.
    // Empty output means every expanded panel, never an inferred primary.
    void setPopupVisible(const QString &sourceId, const QString &outputId,
                         bool visible);
    // Discovers shell-owned in-window Qt Quick popups after panel QML has
    // completed. Native popup windows continue through the event filter.
    void synchronizePopupObjects();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void popupObjectVisibilityChanged();

private:
    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::Shell
