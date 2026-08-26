// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_backend.h"

namespace QindaQt::ShellSurface {

class PanelWindowFactory;

class LayerShellSurfaceBackend final : public PanelSurfaceBackend {
public:
    // factory is borrowed and must outlive this backend and every controller
    // retaining one of its PublishedSurfaceSet instances. All methods are GUI
    // thread confined because QWindow and QScreen are not worker-thread APIs.
    explicit LayerShellSurfaceBackend(PanelWindowFactory &factory);
    ~LayerShellSurfaceBackend() override;

    [[nodiscard]] std::unique_ptr<PreparedSurfaceSet> prepare(
        const QVector<PanelSurfaceConfiguration> &configurations,
        QString *error = nullptr) override;

private:
    PanelWindowFactory &m_factory;
};

} // namespace QindaQt::ShellSurface
