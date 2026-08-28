// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>

#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtCore/QString>

namespace QindaQt::DisplayTopology
{

enum class TopologyError {
    None,
    InvalidSnapshot,
    InvalidCandidate,
    StaleLineage,
    OutputSetMismatch,
    AllOutputsDisabled,
    InvalidPrimary,
    InvalidPriority,
    UnknownMode,
    InvalidScale,
    InvalidCoordinate,
    CoordinateOverflow,
    Overlap,
    UnknownMirrorSource,
    MirrorSelfReference,
    MirrorCycle,
};

enum class TopologyWarningKind {
    DisconnectedGap,
    NonIntegralLogicalExtent,
};

enum class DiffField {
    Enabled,
    Primary,
    Mode,
    Position,
    Scale,
    Transform,
    Priority,
    ReplicationSource,
};

struct TopologyWarning {
    TopologyWarningKind kind = TopologyWarningKind::DisconnectedGap;
    QString stableId;

    friend bool operator==(const TopologyWarning &, const TopologyWarning &) = default;
};

struct OutputGeometry {
    QString stableId;
    QRect logicalRect;
    bool replicated = false;

    friend bool operator==(const OutputGeometry &, const OutputGeometry &) = default;
};

struct CandidateDiff {
    QString stableId;
    QList<DiffField> fields;

    friend bool operator==(const CandidateDiff &, const CandidateDiff &) = default;
};

struct ValidationResult {
    Display::Candidate normalizedCandidate;
    QList<OutputGeometry> geometries;
    QList<TopologyWarning> warnings;
    QList<CandidateDiff> differences;
    QByteArray fingerprint;
    TopologyError error = TopologyError::None;
    QString reasonCode;
    QString offendingStableId;
    bool noOp = false;

    [[nodiscard]] bool accepted() const noexcept { return error == TopologyError::None; }
};

} // namespace QindaQt::DisplayTopology
