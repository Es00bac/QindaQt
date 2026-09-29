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

    // The container's title (ADR-0281): the rename override when one exists,
    // otherwise defaultContainerName(). Never empty, and never a member's
    // application or window title. Pure read: looking at a container never
    // changes what it is called. (The ADR-0163 "Container N" numbering was
    // retired by ADR-0281.)
    [[nodiscard]] QString displayName(const QString &containerId) const;
    // True when displayName() is the user's rename rather than the default.
    [[nodiscard]] bool hasCustomName(const QString &containerId) const;

    void forgetContainer(const QString &containerId) noexcept;
    void clear() noexcept;

    // A read-only snapshot of every current override, for the persistence
    // boundary (ContainerAppearanceLedger) to serialize. Never a display
    // source itself: use displayName()/hasCustomName() for that, since this
    // includes only raw, possibly-empty overrides.
    [[nodiscard]] QHash<QString, Compositor::ContainerAppearance> snapshot() const;

private:
    QHash<QString, Compositor::ContainerAppearance> m_byContainer;
};

} // namespace QindaQt::Compositor::KWinIntegration
