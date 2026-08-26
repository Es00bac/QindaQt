// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_configuration.h"

#include <QString>

#include <memory>

namespace QindaQt::ShellSurface {

class PublishedSurfaceSet {
public:
    virtual ~PublishedSurfaceSet() = default;

    // A compositor may dismiss a mapped layer surface when its output
    // disappears. Retaining the C++ owner does not make that protocol object
    // reusable, so identical-plan elision is permitted only while the entire
    // published set remains live.
    [[nodiscard]] virtual bool isLive() const noexcept = 0;
};

class PreparedSurfaceSet {
public:
    virtual ~PreparedSurfaceSet() = default;

    // Publication transfers ownership of the complete set. A null result must
    // leave the previously published set untouched and provide a diagnostic.
    [[nodiscard]] virtual std::unique_ptr<PublishedSurfaceSet> publish(
        QString *error = nullptr) = 0;
};

class PanelSurfaceBackend {
public:
    virtual ~PanelSurfaceBackend() = default;

    // The backend must create/configure every replacement surface without
    // mapping it. Failure destroys staged resources and cannot mutate the set
    // currently retained by PanelSurfaceController.
    [[nodiscard]] virtual std::unique_ptr<PreparedSurfaceSet> prepare(
        const QVector<PanelSurfaceConfiguration> &configurations,
        QString *error = nullptr) = 0;
};

} // namespace QindaQt::ShellSurface
