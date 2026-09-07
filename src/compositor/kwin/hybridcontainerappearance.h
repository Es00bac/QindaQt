// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/containerappearance.h"

#include <QHash>
#include <QString>

namespace QindaQt::Compositor::KWinIntegration {

// Process-local, non-persisted store of per-container rename/color overrides.
// Pure value storage: no KWin, scene, or topology dependency. Entries survive
// only for the life of the container; the owning session must call
// forgetContainer wherever it already clears other transient per-container
// state (minimized set, placement restore frames) so a reused container ID
// cannot inherit a stale appearance.
//
// AGENT-CONTRACT: this is the public boundary a future persistence owner
// (workspaces) reads/writes through via KWinHybridSession::containerAppearance/
// renameContainer/setContainerColor. It never itself touches Core::WindowContainer
// or TopologyCommand; see docs/wiki/architecture/window-containers.md.
class HybridContainerAppearanceStore final
{
public:
    // A blank (whitespace-only or empty) name clears the override and always
    // succeeds. Otherwise the name must pass normalizedContainerName(); a
    // rejected name (unsupported characters, too long) leaves the store
    // unchanged and returns false with *error set.
    [[nodiscard]] bool setName(const QString &containerId, const QString &name,
                               QString *error = nullptr);
    // Same contract as setName: blank clears the override; otherwise colorHex
    // must pass isValidContainerColor() after normalizedContainerColor().
    [[nodiscard]] bool setColor(const QString &containerId, const QString &colorHex,
                                QString *error = nullptr);

    [[nodiscard]] Compositor::ContainerAppearance appearance(
        const QString &containerId) const;

    void forgetContainer(const QString &containerId) noexcept;
    void clear() noexcept;

private:
    QHash<QString, Compositor::ContainerAppearance> m_byContainer;
};

} // namespace QindaQt::Compositor::KWinIntegration
