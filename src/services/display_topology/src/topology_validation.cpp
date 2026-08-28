// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_topology/topology.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include "topology_validation_p.h"

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace QindaQt::DisplayTopology
{
namespace
{

using VisitState = quint8;
inline constexpr VisitState kVisiting = 1;
inline constexpr VisitState kVisited = 2;

bool hasMirrorCycle(const QString &stableId, const QHash<QString, QString> &sources,
                    QHash<QString, VisitState> &states)
{
    if (states.value(stableId) == kVisiting) {
        return true;
    }
    if (states.value(stableId) == kVisited) {
        return false;
    }
    states.insert(stableId, kVisiting);
    const QString source = sources.value(stableId);
    if (!source.isEmpty() && hasMirrorCycle(source, sources, states)) {
        return true;
    }
    states.insert(stableId, kVisited);
    return false;
}

bool positiveOverlap(const QRect &left, const QRect &right)
{
    const qint64 leftRight = static_cast<qint64>(left.x()) + left.width();
    const qint64 rightRight = static_cast<qint64>(right.x()) + right.width();
    const qint64 leftBottom = static_cast<qint64>(left.y()) + left.height();
    const qint64 rightBottom = static_cast<qint64>(right.y()) + right.height();
    return static_cast<qint64>(left.x()) < rightRight
        && static_cast<qint64>(right.x()) < leftRight
        && static_cast<qint64>(left.y()) < rightBottom
        && static_cast<qint64>(right.y()) < leftBottom;
}

bool touchesEdge(const QRect &left, const QRect &right)
{
    const qint64 leftRight = static_cast<qint64>(left.x()) + left.width();
    const qint64 rightRight = static_cast<qint64>(right.x()) + right.width();
    const qint64 leftBottom = static_cast<qint64>(left.y()) + left.height();
    const qint64 rightBottom = static_cast<qint64>(right.y()) + right.height();
    const bool verticalOverlap = static_cast<qint64>(left.y()) < rightBottom
        && static_cast<qint64>(right.y()) < leftBottom;
    const bool horizontalOverlap = static_cast<qint64>(left.x()) < rightRight
        && static_cast<qint64>(right.x()) < leftRight;
    return ((leftRight == right.x() || rightRight == left.x()) && verticalOverlap)
        || ((leftBottom == right.y() || rightBottom == left.y()) && horizontalOverlap);
}

bool geometrySetHasGap(const QList<OutputGeometry> &geometries)
{
    QList<qsizetype> nonReplicated;
    for (qsizetype index = 0; index < geometries.size(); ++index) {
        if (!geometries.at(index).replicated) {
            nonReplicated.push_back(index);
        }
    }
    if (nonReplicated.size() < 2) {
        return false;
    }
    QSet<qsizetype> reached{nonReplicated.first()};
    bool advanced = true;
    while (advanced) {
        advanced = false;
        for (const qsizetype index : nonReplicated) {
            if (reached.contains(index)) {
                continue;
            }
            const QList<qsizetype> reachedIndices = reached.values();
            for (const qsizetype reachedIndex : reachedIndices) {
                if (touchesEdge(geometries.at(index).logicalRect,
                                geometries.at(reachedIndex).logicalRect)) {
                    reached.insert(index);
                    advanced = true;
                    break;
                }
            }
        }
    }
    return reached.size() != nonReplicated.size();
}

struct CandidateScan {
    qsizetype enabledCount = 0;
    int minimumX = Display::kCoordinateBound;
    int minimumY = Display::kCoordinateBound;
    QHash<QString, QString> mirrorSources;
};

std::optional<ValidationResult> validateLineageAndOutputSet(
    const Display::Snapshot &snapshot, const Display::Candidate &candidate)
{
    if (const auto validation = Display::validateSnapshot(snapshot); !validation.accepted) {
        return Private::failure(TopologyError::InvalidSnapshot, "invalid-snapshot");
    }
    if (const auto validation = Display::validateCandidate(candidate); !validation.accepted) {
        return Private::failure(TopologyError::InvalidCandidate, "invalid-candidate");
    }
    if (candidate.baseEpoch != snapshot.serviceEpoch
        || candidate.baseRevision != snapshot.revision) {
        return Private::failure(TopologyError::StaleLineage, "stale-candidate-lineage");
    }

    QSet<QString> snapshotIds;
    QSet<QString> candidateIds;
    for (const Display::Output &output : snapshot.outputs) {
        snapshotIds.insert(output.stableId);
    }
    for (const Display::CandidateOutput &output : candidate.outputs) {
        candidateIds.insert(output.stableId);
    }
    if (snapshotIds != candidateIds) {
        return Private::failure(TopologyError::OutputSetMismatch, "output-set-mismatch");
    }
    return std::nullopt;
}

std::optional<ValidationResult> scanCandidate(Display::Candidate &candidate,
                                               CandidateScan &scan)
{
    qsizetype primaryCount = 0;
    QSet<quint32> priorities;
    for (Display::CandidateOutput &output : candidate.outputs) {
        scan.mirrorSources.insert(output.stableId, output.replicationSourceStableId);
        if (!output.enabled) {
            output.primary = false;
            output.priority = 0;
            output.position = {};
            output.replicationSourceStableId.clear();
            scan.mirrorSources[output.stableId].clear();
            continue;
        }
        ++scan.enabledCount;
        primaryCount += output.primary ? 1 : 0;
        if (output.replicationSourceStableId.isEmpty()) {
            scan.minimumX = std::min(scan.minimumX, output.position.x());
            scan.minimumY = std::min(scan.minimumY, output.position.y());
        }
        if (output.priority == 0 || priorities.contains(output.priority)) {
            return Private::failure(TopologyError::InvalidPriority, "invalid-priority",
                                    output.stableId);
        }
        priorities.insert(output.priority);
    }
    if (scan.enabledCount == 0) {
        return Private::failure(TopologyError::AllOutputsDisabled, "all-outputs-disabled");
    }
    if (primaryCount != 1) {
        return Private::failure(TopologyError::InvalidPrimary, "invalid-primary-set");
    }
    for (quint32 priority = 1; priority <= static_cast<quint32>(scan.enabledCount);
         ++priority) {
        if (!priorities.contains(priority)) {
            return Private::failure(TopologyError::InvalidPriority,
                                    "noncontiguous-priority");
        }
    }
    return std::nullopt;
}

std::optional<ValidationResult> validateMirrorGraph(
    const Display::Candidate &candidate, const QHash<QString, QString> &sources)
{
    for (const Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled || output.replicationSourceStableId.isEmpty()) {
            continue;
        }
        const auto source = std::find_if(
            candidate.outputs.cbegin(), candidate.outputs.cend(),
            [&](const Display::CandidateOutput &other) {
                return other.stableId == output.replicationSourceStableId;
            });
        if (source == candidate.outputs.cend() || !source->enabled) {
            return Private::failure(TopologyError::UnknownMirrorSource,
                                    "unknown-mirror-source", output.stableId);
        }
        if (source->stableId == output.stableId) {
            return Private::failure(TopologyError::MirrorSelfReference,
                                    "mirror-self-reference", output.stableId);
        }
    }

    QHash<QString, VisitState> visitStates;
    for (const Display::CandidateOutput &output : candidate.outputs) {
        if (output.enabled && hasMirrorCycle(output.stableId, sources, visitStates)) {
            return Private::failure(TopologyError::MirrorCycle, "mirror-cycle",
                                    output.stableId);
        }
    }
    return std::nullopt;
}

const Display::CandidateOutput *findCandidate(const Display::Candidate &candidate,
                                               const QString &stableId)
{
    const auto found = std::find_if(candidate.outputs.cbegin(), candidate.outputs.cend(),
                                    [&](const Display::CandidateOutput &output) {
                                        return output.stableId == stableId;
                                    });
    return found == candidate.outputs.cend() ? nullptr : &*found;
}

std::optional<ValidationResult> canonicalizeMirrors(
    const Display::Snapshot &snapshot, Display::Candidate &candidate,
    const CandidateScan &scan)
{
    for (Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled || output.replicationSourceStableId.isEmpty()) {
            continue;
        }
        QString rootId = output.replicationSourceStableId;
        while (!scan.mirrorSources.value(rootId).isEmpty()) {
            rootId = scan.mirrorSources.value(rootId);
        }
        const Display::CandidateOutput *root = findCandidate(candidate, rootId);
        const Display::Output *live = Private::findOutput(snapshot, output.stableId);
        if (root == nullptr || live == nullptr) {
            return Private::failure(TopologyError::UnknownMirrorSource,
                                    "unknown-mirror-root", output.stableId);
        }
        // AGENT-CONTRACT: KWin derives mirror placement and scale from the
        // source but target mode/transform remain per-output configuration.
        // D1 erases only the caller-supplied derived fields before diffing or
        // fingerprinting; the D2 adapter must do the same projection.
        output.position = root->position;
        output.scale = root->scale;
    }
    return std::nullopt;
}

std::optional<ValidationResult> normalizePositions(Display::Candidate &candidate,
                                                    const CandidateScan &scan)
{
    for (Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled) {
            continue;
        }
        const qint64 normalizedX = static_cast<qint64>(output.position.x()) - scan.minimumX;
        const qint64 normalizedY = static_cast<qint64>(output.position.y()) - scan.minimumY;
        if (normalizedX < 0 || normalizedY < 0
            || normalizedX > Display::kCoordinateBound
            || normalizedY > Display::kCoordinateBound) {
            return Private::failure(TopologyError::InvalidCoordinate,
                                    "coordinate-normalization-failed", output.stableId);
        }
        output.position = QPoint(static_cast<int>(normalizedX), static_cast<int>(normalizedY));
    }
    return std::nullopt;
}

std::optional<ValidationResult> buildGeometries(
    const Display::Snapshot &snapshot, const Display::Candidate &candidate,
    QList<OutputGeometry> &geometries, QList<TopologyWarning> &warnings)
{
    for (const Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled) {
            continue;
        }
        const Display::Output *live = Private::findOutput(snapshot, output.stableId);
        const Display::Mode *mode = live == nullptr ? nullptr
                                                    : Private::findMode(*live, output.modeId);
        if (mode == nullptr) {
            return Private::failure(TopologyError::UnknownMode, "unknown-output-mode",
                                    output.stableId);
        }
        if (!std::isfinite(output.scale) || output.scale < Display::kMinimumScale
            || output.scale > Display::kMaximumScale) {
            return Private::failure(TopologyError::InvalidScale, "invalid-output-scale",
                                    output.stableId);
        }
        if (!output.replicationSourceStableId.isEmpty()) {
            continue;
        }
        QRect logicalRect;
        if (!Private::checkedRect(output.position,
                                  logicalSizeForMode(*mode, output.scale, output.transform),
                                  logicalRect)) {
            return Private::failure(TopologyError::CoordinateOverflow,
                                    "logical-coordinate-overflow", output.stableId);
        }
        geometries.push_back({output.stableId, logicalRect, false});
        if (!hasIntegralLogicalExtent(*mode, output.scale, output.transform)) {
            warnings.push_back({TopologyWarningKind::NonIntegralLogicalExtent,
                                output.stableId});
        }
    }

    for (const Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled || output.replicationSourceStableId.isEmpty()) {
            continue;
        }
        QString rootId = output.replicationSourceStableId;
        const Display::CandidateOutput *root = findCandidate(candidate, rootId);
        while (root != nullptr && !root->replicationSourceStableId.isEmpty()) {
            rootId = root->replicationSourceStableId;
            root = findCandidate(candidate, rootId);
        }
        const auto rootGeometry = std::find_if(
            geometries.cbegin(), geometries.cend(), [&](const OutputGeometry &geometry) {
                return geometry.stableId == rootId;
            });
        if (rootGeometry == geometries.cend()) {
            return Private::failure(TopologyError::UnknownMirrorSource,
                                    "unknown-mirror-root", output.stableId);
        }
        geometries.push_back({output.stableId, rootGeometry->logicalRect, true});
    }
    return std::nullopt;
}

std::optional<ValidationResult> rejectOverlaps(const QList<OutputGeometry> &geometries)
{
    for (qsizetype left = 0; left < geometries.size(); ++left) {
        for (qsizetype right = left + 1; right < geometries.size(); ++right) {
            if (!geometries.at(left).replicated && !geometries.at(right).replicated
                && positiveOverlap(geometries.at(left).logicalRect,
                                   geometries.at(right).logicalRect)) {
                return Private::failure(TopologyError::Overlap, "outputs-overlap",
                                        geometries.at(right).stableId);
            }
        }
    }
    return std::nullopt;
}

void sortResult(Display::Candidate &candidate, QList<OutputGeometry> &geometries,
                QList<TopologyWarning> &warnings)
{
    std::sort(candidate.outputs.begin(), candidate.outputs.end(),
              [](const Display::CandidateOutput &left,
                 const Display::CandidateOutput &right) {
                  return left.stableId < right.stableId;
              });
    std::sort(geometries.begin(), geometries.end(),
              [](const OutputGeometry &left, const OutputGeometry &right) {
                  return left.stableId < right.stableId;
              });
    std::sort(warnings.begin(), warnings.end(),
              [](const TopologyWarning &left, const TopologyWarning &right) {
                  return left.kind == right.kind ? left.stableId < right.stableId
                                                 : left.kind < right.kind;
              });
}

} // namespace

ValidationResult validateAndNormalize(const Display::Snapshot &snapshot,
                                      const Display::Candidate &candidate)
{
    Display::Candidate normalized = candidate;
    CandidateScan scan;
    if (const auto failure = validateLineageAndOutputSet(snapshot, candidate)) {
        return *failure;
    }
    if (const auto failure = scanCandidate(normalized, scan)) {
        return *failure;
    }
    if (const auto failure = validateMirrorGraph(normalized, scan.mirrorSources)) {
        return *failure;
    }
    if (const auto failure = canonicalizeMirrors(snapshot, normalized, scan)) {
        return *failure;
    }
    if (const auto failure = normalizePositions(normalized, scan)) {
        return *failure;
    }
    QList<OutputGeometry> geometries;
    QList<TopologyWarning> warnings;
    geometries.reserve(scan.enabledCount);
    if (const auto failure = buildGeometries(snapshot, normalized, geometries, warnings)) {
        return *failure;
    }
    if (const auto failure = rejectOverlaps(geometries)) {
        return *failure;
    }
    if (geometrySetHasGap(geometries)) {
        warnings.push_back({.kind = TopologyWarningKind::DisconnectedGap, .stableId = {}});
    }
    sortResult(normalized, geometries, warnings);
    QList<CandidateDiff> differences = diff(snapshot, normalized);
    return {.normalizedCandidate = normalized,
            .geometries = std::move(geometries),
            .warnings = std::move(warnings),
            .differences = differences,
            .fingerprint = canonicalFingerprint(normalized),
            .error = TopologyError::None,
            .reasonCode = {},
            .offendingStableId = {},
            .noOp = differences.isEmpty()};
}

} // namespace QindaQt::DisplayTopology
