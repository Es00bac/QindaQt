// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <utility>

#include "color_types.h"

namespace QindaQt::DisplayColor
{

// Header validation and summary extraction
std::pair<ProfileValidationStatus, IccHeaderSummary> validateIccHeader(
    const QByteArray &headerData,
    quint32 totalFileSize = 0);

// Descriptor metadata and integrity validation
ProfileValidationStatus validateProfileDescriptor(const IccProfileDescriptor &descriptor);

// Display identity validation
bool validateDisplayStableId(const QString &stableId);

// Output capability validation
bool validateOutputCapabilities(const OutputColorCapabilities &capabilities);

// Output assignment validation
bool validateOutputAssignment(const OutputColorAssignment &assignment);

// Deterministic catalog normalization: filters invalid descriptors,
// collapses exact-equal duplicates, rejects conflicting duplicate IDs
// order-independently (neither entry survives), sorts, then caps at the
// catalog maximum.
QList<IccProfileDescriptor> normalizeAndSortCatalog(const QList<IccProfileDescriptor> &profiles);

// Pure canonical lineage fingerprint derivation. The encoding is
// schema-tagged, domain-tagged, and length-delimited, and covers every
// semantically published snapshot field; see the framing contract in the
// implementation for the exact field set.
QByteArray computeLineageFingerprint(
    const QString &serviceEpoch,
    quint64 revision,
    const ColorCatalog &catalog,
    const QList<OutputColorState> &outputs);

} // namespace QindaQt::DisplayColor
