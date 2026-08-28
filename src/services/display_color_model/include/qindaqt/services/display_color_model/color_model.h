// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

#include <optional>

#include "color_types.h"

namespace QindaQt::DisplayColor
{

// AGENT-CONTRACT: Pure in-process Display Color C0 model. No event-loop
// object, thread, timer, file, IPC connection, compositor, or display
// hardware access lives here; the owner injects validated values and
// receives complete value snapshots only. All mutators validate their whole
// input first and leave the model byte-identical when they return false
// (atomic reject, no partial state). Single-threaded by ownership: callers
// must not mutate one instance from multiple threads without external
// synchronization.
class ColorModel final
{
public:
    explicit ColorModel(const QString &serviceEpoch = QString());

    // Epoch and lineage management. resetEpoch starts a distinct (or
    // generated) epoch at revision zero; resetting to the epoch already in
    // force is a no-op that never regresses the model-monotonic revision.
    QString serviceEpoch() const;
    quint64 revision() const;
    void resetEpoch(const QString &newEpoch);

    // Profile catalog operations
    bool setCatalog(const QList<IccProfileDescriptor> &profiles, const QString &defaultSrgbId = QString());
    bool registerProfile(const IccProfileDescriptor &profile);
    bool removeProfile(const QString &profileId);
    std::optional<IccProfileDescriptor> findProfile(const QString &profileId) const;

    // Output capability and inventory management
    bool updateCapabilities(const OutputColorCapabilities &capabilities);
    bool removeOutput(const QString &stableId);
    std::optional<OutputColorCapabilities> capabilities(const QString &stableId) const;

    // Output assignment intent
    bool requestAssignment(const OutputColorAssignment &assignment);
    std::optional<OutputColorState> outputState(const QString &stableId) const;

    // External lineage validation: true only for the exact current
    // epoch/revision pair. Stale (older) and out-of-order (newer) revisions
    // from another publisher are both rejected, matching Display1 lineage
    // semantics; revisions are never ordered across different epochs.
    bool validateLineage(const QString &epoch, quint64 revision) const;

    // Snapshot generation
    ColorModelSnapshot snapshot() const;

private:
    void reevaluateOutput(const QString &stableId);
    void reevaluateAllOutputs();
    void refreshDefaultSrgbProfile();
    void advanceRevision();
    int knownOutputCount() const;

    QString m_serviceEpoch;
    quint64 m_revision = 0;

    ColorCatalog m_catalog;
    QHash<QString, IccProfileDescriptor> m_profilesById;

    QHash<QString, OutputColorCapabilities> m_capabilitiesByOutput;
    QHash<QString, OutputColorAssignment> m_requestedByOutput;
    QHash<QString, OutputColorAssignment> m_lkgByOutput;
    QHash<QString, OutputColorState> m_stateByOutput;
};

} // namespace QindaQt::DisplayColor
