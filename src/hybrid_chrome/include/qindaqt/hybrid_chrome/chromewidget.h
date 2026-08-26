// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chrometypes.h"

#include <QWidget>

namespace QindaQt::HybridChrome {

class ChromeWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ChromeWidget(QWidget *parent = nullptr);

    // The widget owns a copy. Set and use it only on the QWidget GUI thread.
    void setRenderPlan(ChromeRenderPlan plan);
    [[nodiscard]] const ChromeRenderPlan &renderPlan() const { return m_plan; }
    [[nodiscard]] ChromeHitTarget hitTargetAt(const QPointF &logicalPosition) const;
    // Production overlays are input-transparent; the compositor input router
    // publishes global hover identity through this value-only paint seam.
    void setPointerHoverTarget(ChromeHitTarget target);
    // Cancels an active drag without synthesizing a click. Global input owners
    // call this when an external grab or compositor transition takes control.
    void cancelActiveDrag();

Q_SIGNALS:
    void targetActivated(const QindaQt::HybridChrome::ChromeHitTarget &target);
    void dragLifecycle(const QindaQt::HybridChrome::ChromeDragEvent &event);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateHover(const QPointF &logicalPosition);
    void finishPointerSequence(bool cancel, const QPointF &globalPosition);
    [[nodiscard]] static bool isDragTarget(const ChromeHitTarget &target);

    ChromeRenderPlan m_plan;
    ChromePaintState m_paintState;
    QPointF m_dragOriginGlobal;
    QPointF m_lastGlobalPosition;
    bool m_pointerPressed = false;
    bool m_dragActive = false;
};

} // namespace QindaQt::HybridChrome
