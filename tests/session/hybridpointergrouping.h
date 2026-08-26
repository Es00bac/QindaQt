// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositordevelopmentworkflow.h"
#include "compositorprobeclient.h"
#include "hybridpointergeometry.h"
#include "hybridpointerinventory.h"
#include "hybridtestinputdriver.h"

#include <QJsonObject>

#include <functional>
#include <optional>

namespace QindaQt::Test {

struct HybridPointerGroupedState final
{
    WindowInventory initial;
    WindowInventory grouped;
    HybridDiagnostics initialHybrid;
    HybridDiagnostics groupedHybrid;
    PublicContainerEvidence publicContainer;
    DockGestureGeometry gesture;
    SplitEvidence split;
    QString bystander;
    QRectF output;
};

// Owns the input producers for the whole gesture lifecycle. Both the complete
// dock/detach proof and the plugin-unload proof reuse this boundary so they
// cannot accidentally diverge on modifiers, admission, or public-state gates.
class HybridPointerGrouping final
{
public:
    HybridPointerGrouping(CompositorProbeClient &client,
                          ProbeWindowTitles titles);

    [[nodiscard]] std::optional<HybridPointerGroupedState>
    group(const QString &dotoolPath, QString *error);
    [[nodiscard]] bool drag(const QPointF &start,
                            const QPointF &end,
                            bool metaShift,
                            QString *error);
    [[nodiscard]] bool activateFirstContextMenuAction(
        const QPointF &point,
        QString *error);
    [[nodiscard]] bool dotoolRunning() const;
    [[nodiscard]] QString dotoolDiagnostics() const;
    [[nodiscard]] QJsonObject inputEvidence() const;

private:
    using DragGesture = std::function<bool(const QPointF &, const QPointF &,
                                           bool, QString *)>;
    using ContextMenuActivation =
        std::function<bool(const QPointF &, QString *)>;

    [[nodiscard]] bool selectInputDriver(const QString &dotoolPath,
                                         const QPointF &initialPoint,
                                         const QRectF &output,
                                         QString *error);

    CompositorProbeClient &m_client;
    ProbeWindowTitles m_titles;
    DotoolProcess m_dotool;
    DevelopmentInputDriver m_developmentInput;
    DragGesture m_drag;
    ContextMenuActivation m_activateFirstContextMenuAction;
    QString m_injector;
    bool m_uinputAdmitted = false;
    QJsonArray m_uinputDevices;
    QString m_uinputAdmissionFailure;
    bool m_groupAttempted = false;
};

} // namespace QindaQt::Test
