// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "notificationoutputselector.h"
#include "notificationwindowcontroller.h"
#include "qtcompositoroutputauthority.h"

#include "qindaqt/shell_layout/panel_layout_solver.h"
#include "qindaqt/shell_orchestration/output_inventory_matcher.h"
#include "qindaqt/shell_orchestration/panel_interaction_store.h"
#include "qindaqt/shell_orchestration/panel_runtime_plan_assembler.h"
#include "qindaqt/shell_orchestration/panel_visibility_inventory_assembler.h"
#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"
#include "qindaqt/shell_surface/panel_surface_controller.h"
#include "qindaqt/shell_surface/qt_output_inventory.h"
#include "qindaqt/shell_visibility_client/compositor_visibility_client.h"

#include <QDebug>
#include <QScreen>

#include <utility>

namespace QindaQt::Shell {
namespace {

QVector<ShellLayout::LogicalOutput> logicalOutputs(
    const ShellVisibility::CompositorVisibilitySnapshot &snapshot)
{
    QVector<ShellLayout::LogicalOutput> result;
    result.reserve(snapshot.outputs.size());
    for (const auto &output : snapshot.outputs) {
        result.append({output.id, output.geometry, output.scale});
    }
    return result;
}

QScreen *notificationScreen(
    const QtCompositorOutputAuthority *authority,
    const ShellVisibility::CompositorVisibilitySnapshot *visibility,
    const QVector<ShellLayout::LogicalOutput> &qtOutputs)
{
    const std::optional<CompositorOutputAuthorityFrame> noAuthority;
    const auto selection = NotificationOutputSelector::select(
        authority != nullptr ? authority->frame() : noAuthority,
        visibility, qtOutputs);
    if (!selection.ok()) {
        if (authority != nullptr && authority->frame().has_value()
            && visibility != nullptr) {
            qWarning().noquote()
                << "QindaQt shell withheld notification surfaces:"
                << selection.message;
        }
        return nullptr;
    }
    QString error;
    QScreen *const screen =
        ShellSurface::QtOutputInventory::screenForId(selection.outputId, &error);
    if (screen == nullptr) {
        qWarning().noquote()
            << "QindaQt shell withheld notification surfaces:" << error;
    }
    return screen;
}

} // namespace

void ShellRuntimeApplication::startNotificationOutputAuthority()
{
    m_outputAuthority = std::make_unique<QtCompositorOutputAuthority>();
    connect(m_outputAuthority.get(), &QtCompositorOutputAuthority::stateChanged,
            this, &ShellRuntimeApplication::scheduleOutputReconcile);
    QString error;
    if (!m_outputAuthority->start(&error)) {
        // Notification windows stay absent until a complete, public
        // Compositor1 output order can be joined to shell visibility.
        qWarning().noquote()
            << "QindaQt shell could not start notification output authority:"
            << error;
    }
}

bool ShellRuntimeApplication::reconcileSurfaces(QString *error)
{
    const auto inventory = ShellSurface::QtOutputInventory::read();
    if (!inventory.ok()) {
        *error = inventory.error;
        return false;
    }
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex < 0
        || static_cast<qsizetype>(profileIndex) >= m_profiles.profiles().size()) {
        *error = QStringLiteral(
            "selected profile disappeared during surface reconciliation");
        return false;
    }

    const auto &profile = m_profiles.profiles().at(profileIndex);
    QVector<ShellLayout::LogicalOutput> selectedOutputs = inventory.outputs;
    const ShellVisibility::CompositorVisibilitySnapshot *visibilitySnapshot = nullptr;
    if (m_visibilityClient && !m_visibilityClient->safeVisibleRequired()
        && m_visibilityClient->snapshot()) {
        const auto compositorOutputs = logicalOutputs(*m_visibilityClient->snapshot());
        const auto outputMatch = ShellOrchestration::OutputInventoryMatcher::match(
            compositorOutputs, inventory.outputs);
        if (outputMatch.ok()) {
            selectedOutputs = compositorOutputs;
            visibilitySnapshot = &*m_visibilityClient->snapshot();
        } else {
            // AGENT-GUARD: Qt and compositor output generations can cross
            // during hotplug. A mixed generation is never evaluated; keeping
            // every panel visible is the fail-safe policy until they converge.
            qWarning().noquote()
                << "QindaQt shell is using safe-visible output fallback:"
                << outputMatch.message;
        }
    }

    const auto layout = ShellLayout::PanelLayoutSolver::solve(profile.panels,
                                                               selectedOutputs);
    if (!layout.ok()) {
        *error = layout.error.message;
        return false;
    }
    if (!m_controller) {
        *error = QStringLiteral("panel surface controller is not initialized");
        return false;
    }
    const auto basePlan =
        ShellSurface::PanelSurfaceConfigurationPlanner::plan(layout);
    if (!basePlan.ok()) {
        *error = basePlan.error.message;
        return false;
    }
    if (!m_interactions) {
        *error = QStringLiteral("panel interaction store is not initialized");
        return false;
    }
    QVector<ShellVisibility::PanelSurfaceIdentity> identities;
    identities.reserve(basePlan.surfaces.size());
    for (const auto &surface : basePlan.surfaces) {
        identities.append({surface.identity.panelId, surface.identity.outputId});
    }
    QString interactionError;
    if (!m_interactions->setIdentities(identities, &interactionError)) {
        *error = std::move(interactionError);
        return false;
    }

    ShellOrchestration::PanelRuntimeAssemblyResult runtime;
    if (visibilitySnapshot != nullptr) {
        const auto visibility =
            ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                profile, layout, *visibilitySnapshot, m_interactions->snapshot());
        if (visibility.ok()) {
            runtime = ShellOrchestration::PanelRuntimePlanAssembler::fromEvaluation(
                basePlan, visibility.evaluation);
        } else {
            qWarning().noquote()
                << "QindaQt shell rejected live visibility and kept panels visible:"
                << visibility.error.message;
            runtime =
                ShellOrchestration::PanelRuntimePlanAssembler::safeVisible(basePlan);
        }
    } else {
        runtime = ShellOrchestration::PanelRuntimePlanAssembler::safeVisible(basePlan);
    }
    if (!runtime.ok()) {
        *error = runtime.error.message;
        return false;
    }
    const auto result = m_controller->reconcilePlan(std::move(runtime.plan));
    if (!result.ok()) {
        *error = result.message;
        return false;
    }
    if (m_notificationWindows) {
        QScreen *const screen = notificationScreen(
            m_outputAuthority.get(), visibilitySnapshot, inventory.outputs);
        if (!m_notificationWindows->reconcile(screen, error)) {
            return false;
        }
    }
    return true;
}

void ShellRuntimeApplication::attachOutputSignals(QScreen *screen)
{
    if (screen == nullptr) {
        return;
    }
    const auto schedule = [this] { scheduleOutputReconcile(); };
    connect(screen, &QScreen::geometryChanged, this, schedule);
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, schedule);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, schedule);
    connect(screen, &QScreen::orientationChanged, this, schedule);
}

void ShellRuntimeApplication::scheduleOutputReconcile()
{
    m_outputDebounce.start();
}

} // namespace QindaQt::Shell
