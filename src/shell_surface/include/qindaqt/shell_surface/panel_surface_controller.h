// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_backend.h"

#include <QtTypes>

#include <limits>
#include <memory>

namespace QindaQt::ShellLayout {
struct PanelLayoutResult;
}

namespace QindaQt::ShellSurface {

enum class PanelSurfaceControllerErrorCode {
    None,
    PlanningFailed,
    BackendPrepareFailed,
    BackendPublishFailed,
    BackendReconfigureFailed,
    RevisionExhausted,
};

struct PanelSurfaceControllerResult {
    PanelSurfaceControllerErrorCode code = PanelSurfaceControllerErrorCode::None;
    quint64 revision = 0;
    bool changed = false;
    QString message;

    [[nodiscard]] bool ok() const noexcept
    {
        return code == PanelSurfaceControllerErrorCode::None;
    }
};

class PanelSurfaceController final {
public:
    // backend is borrowed and must outlive the controller. The controller and
    // backend are confined to the shell GUI thread. Published windows are
    // destroyed before the backend reference becomes unreachable.
    explicit PanelSurfaceController(PanelSurfaceBackend &backend,
                                    quint64 initialRevision = 0);
    ~PanelSurfaceController();

    PanelSurfaceController(const PanelSurfaceController &) = delete;
    PanelSurfaceController &operator=(const PanelSurfaceController &) = delete;

    [[nodiscard]] PanelSurfaceControllerResult reconcile(
        const ShellLayout::PanelLayoutResult &layout);
    // Accepts an already validated base/runtime plan so visibility and
    // reservation can be replaced together without asking the controller to
    // understand window policy.
    [[nodiscard]] PanelSurfaceControllerResult reconcilePlan(
        PanelSurfacePlan candidate);

    [[nodiscard]] quint64 revision() const noexcept;
    [[nodiscard]] const PanelSurfacePlan &currentPlan() const noexcept;
    [[nodiscard]] bool hasPublishedSet() const noexcept;

private:
    PanelSurfaceBackend &m_backend;
    std::unique_ptr<PublishedSurfaceSet> m_published;
    PanelSurfacePlan m_plan;
    quint64 m_revision = 0;
};

} // namespace QindaQt::ShellSurface
