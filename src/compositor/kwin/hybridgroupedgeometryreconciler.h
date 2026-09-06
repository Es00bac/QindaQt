// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace QindaQt::Compositor::KWinIntegration {

struct GroupedWindowGeometry final
{
    QString windowId;
    QString containerId;
    QRectF requestedFrame;
    QRectF targetFrame;
    // Member focus and whole-group minimize temporarily give native KWin
    // presentation authority over the requested frame.
    bool nativeFrameOverride = false;
};

using GroupedGeometrySnapshot = std::function<QVector<GroupedWindowGeometry>()>;
using GroupedGeometryApply =
    std::function<bool(const QString &windowId,
                       const QRectF &targetFrame,
                       QString *error)>;

// Reasserts compositor-owned member frames after KWin has finished a work-area
// rearrangement. Discovery and native mutation are borrowed synchronous
// callbacks; the owner must keep their dependencies alive for this object's
// lifetime and invoke reconcile() only on the compositor thread.
class HybridGroupedGeometryReconciler final
{
public:
    HybridGroupedGeometryReconciler(GroupedGeometrySnapshot snapshot,
                                    GroupedGeometryApply apply);

    // Returns one diagnostic per invalid or rejected member and continues so a
    // single vanished client cannot leave its surviving peers incoherent.
    [[nodiscard]] QStringList reconcile() const;

private:
    GroupedGeometrySnapshot m_snapshot;
    GroupedGeometryApply m_apply;
};

} // namespace QindaQt::Compositor::KWinIntegration
