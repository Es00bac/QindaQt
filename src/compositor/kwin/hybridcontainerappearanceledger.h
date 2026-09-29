// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/containerappearance.h"

#include <QHash>
#include <QString>

namespace QindaQt::Compositor::KWinIntegration {

// AGENT-CONTRACT: Disk-backed persistence for HybridContainerAppearanceStore
// entries, keyed by containerId. This is the persistence owner named as
// future work by docs/wiki/architecture/window-containers.md ("Persistence")
// and ADR-0189's "Revisit when" clause. KWinHybridSession is the only
// caller: it reads/writes through this class from exactly the boundary
// methods named in HybridContainerAppearanceStore's own AGENT-CONTRACT
// (renameContainer/setContainerColor/forgetContainer). Pure value I/O: no
// KWin, scene, or topology dependency, so it is unit-testable on its own.
//
// AGENT-NOTE: containerId (HybridInteractionRuntime::structuralId) is a
// per-runtime revision counter, not a durable cross-process identity, and
// container membership/layout itself has no automatic restore across a real
// compositor restart yet (ADR-0189: "there is no automatic save or restore
// of live container topology at compositor start"). A simple, consistent
// grouping order reproduces the same deterministic id after a restart, and
// this ledger then reapplies that container's name/color correctly; a
// different order does not line up, because there is nothing else here that
// reconstructs *which* windows group together. Closing that larger gap is
// separate work, exactly as ADR-0189 anticipates ("Container identity gains
// a persistence owner, at which point the name half ... becomes
// implementable"): this class is that owner for the name/color half only.
class ContainerAppearanceLedger final
{
public:
    // Uses defaultStoragePath().
    ContainerAppearanceLedger();
    // Explicit path for tests; production code should prefer the default
    // constructor so every install shares one ledger location.
    explicit ContainerAppearanceLedger(QString storagePath);

    // The directory saved workspaces already use
    // (QStandardPaths::GenericDataLocation + "qindaqt/"), so every durable
    // per-user QindaQt document lives under one place.
    [[nodiscard]] static QString defaultStoragePath();

    // A missing or damaged file yields an empty map: a fresh install or a
    // corrupted ledger must never block compositor startup.
    [[nodiscard]] QHash<QString, ContainerAppearance> load() const;

    // Replaces the file's complete contents with `entries`. An entry whose
    // name and colorHex are both empty is dropped: a forgotten or
    // never-set container leaves nothing on disk. Returns false, leaving
    // any prior file untouched, when the write fails (e.g. an unwritable
    // directory); callers treat that as best-effort and continue.
    bool save(const QHash<QString, ContainerAppearance> &entries) const;

private:
    QString m_path;
};

} // namespace QindaQt::Compositor::KWinIntegration
