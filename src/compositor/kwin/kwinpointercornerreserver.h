// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "pointercorner.h"

#include <effect/globals.h>

#include <QByteArray>
#include <QPointer>

namespace QindaQt::Compositor::KWinIntegration {

using KWin::ElectricBorder;

// Adapts KWin's bool(ElectricBorder) callback to the pure gesture's
// bool() callback. KWin invokes its edge slot with one argument: reserving
// PointerCornerGesture directly silently fails its QMetaObject invocation.
class KWinPointerCornerReserver final : public QObject,
                                        public PointerCornerReserver {
    Q_OBJECT
public:
    [[nodiscard]] static bool available();
    void reserve(QObject *object, const char *callback) override;
    void unreserve(QObject *object) override;

public Q_SLOTS:
    bool edgeTriggered(ElectricBorder border);

private:
    QPointer<QObject> m_target;
    QByteArray m_callback;
};

} // namespace QindaQt::Compositor::KWinIntegration
