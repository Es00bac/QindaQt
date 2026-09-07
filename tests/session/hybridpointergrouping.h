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

    // AGENT-CONTRACT: explicit, opt-in bypass of dotool selection entirely,
    // for environments where the dotool binary itself is absent (not merely
    // unable to admit uinput devices into the nested seat, the normal
    // fallback this class already handles). Ordinary dotool-first selection
    // is completely unchanged when this is left false; call before group().
    // See docs/wiki/development/testing-harness.md for when to use this.
    void forceDevelopmentInput(bool force) noexcept { m_forceDevelopmentInput = force; }

    [[nodiscard]] std::optional<HybridPointerGroupedState>
    group(const QString &dotoolPath, QString *error);
    [[nodiscard]] bool drag(const QPointF &start,
                            const QPointF &end,
                            bool metaShift,
                            QString *error);
    [[nodiscard]] bool activateFirstContextMenuAction(
        const QPointF &point,
        QString *error);
    // See DotoolProcess::activateContextMenuActionAt for the exact contract.
    [[nodiscard]] bool activateContextMenuActionAt(
        const QPointF &point,
        int index,
        QString *error);
    [[nodiscard]] bool dotoolRunning() const;
    [[nodiscard]] QString dotoolDiagnostics() const;
    [[nodiscard]] QJsonObject inputEvidence() const;
    // True once selectInputDriver() has run and dotool was never started
    // (forceDevelopmentInput) or its uinput devices were not admitted (the
    // pre-existing fallback). False for ordinary dotool-uinput selection.
    [[nodiscard]] bool usingDevelopmentInput() const noexcept;
    // AGENT-CONTRACT: the correct liveness gate for input calls below. dotool
    // liveness only matters when dotool is the actual selected driver;
    // forcing development input never starts dotool at all, so treating a
    // dead/absent dotool process as a failure there would be wrong. See
    // docs/wiki/development/testing-harness.md.
    [[nodiscard]] bool inputSessionHealthy() const;

private:
    using DragGesture = std::function<bool(const QPointF &, const QPointF &,
                                           bool, QString *)>;
    using ContextMenuActivation =
        std::function<bool(const QPointF &, QString *)>;
    using ContextMenuActivationAt =
        std::function<bool(const QPointF &, int, QString *)>;

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
    ContextMenuActivationAt m_activateContextMenuActionAt;
    QString m_injector;
    bool m_uinputAdmitted = false;
    QJsonArray m_uinputDevices;
    QString m_uinputAdmissionFailure;
    bool m_groupAttempted = false;
    bool m_forceDevelopmentInput = false;
};

} // namespace QindaQt::Test
