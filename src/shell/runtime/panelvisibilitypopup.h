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

class PanelVisibilityTimerPort;

class PanelVisibilityPopupProducer final : public QObject {
    Q_OBJECT
public:
    static constexpr qsizetype MaximumSources = 32;
    static constexpr qsizetype MaximumLeases = 128;
    static constexpr qsizetype MaximumSourceIdLength = 128;
    static constexpr int MaximumHoldMilliseconds = 30'000;

    PanelVisibilityPopupProducer(
        QGuiApplication &application,
        ShellOrchestration::PanelInteractionStore &interactions,
        PanelVisibilityTimerPort &timer,
        QObject *parent = nullptr);
    ~PanelVisibilityPopupProducer() override;

    void setIdentities(
        QVector<ShellVisibility::PanelSurfaceIdentity> identities);
    // Explicit shell-owned sources must supply their QObject lifetime owner.
    // Empty output means every expanded panel, never an inferred primary.
    // One uninterrupted visible admission expires after the fixed maximum;
    // duplicate visible calls do not renew it.
    [[nodiscard]] bool setPopupVisible(QObject *owner,
                                       const QString &sourceId,
                                       const QString &outputId,
                                       bool visible);
    // Discovers shell-owned in-window Qt Quick popups after panel QML has
    // completed. Native popup windows continue through the event filter.
    //
    // Cheap to call repeatedly: the full tree walk runs only when an object
    // has been added somewhere in the process since the last pass. Callers
    // must keep calling it on every synchronize rather than deciding for
    // themselves when discovery is needed.
    void synchronizePopupObjects();

    // Full tree walks performed so far. A panel's object tree grows over a
    // session (notifications, clipboard history, task list) while this runs on
    // every visibility transition, so the skipped-pass count is the thing
    // worth observing. Exposed for tests and diagnostics.
    [[nodiscard]] quint64 discoveryPasses() const noexcept;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void popupObjectVisibilityChanged();

private:
    void releaseSource(const QString &sourceId, quint64 generation);
    void clearSources();

    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::Shell
