// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gain_loaded_image.h"
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QSet>

namespace QindaQt::Audio::Test
{
// Private same-thread, no-event-loop observations; no parser deadline or writer
// exclusion is claimed. Files are bounded, single-link and held while read.
[[nodiscard]] bool gainExactKeys(const QJsonObject &, const QSet<QString> &);
[[nodiscard]] bool gainInteger(const QJsonValue &, qint64 minimum, qint64 maximum);
[[nodiscard]] QByteArray gainReadEvidence(const GainImageArtifact &, qint64 limit = 16 * 1024 * 1024);
[[nodiscard]] bool gainEvidenceRecord(const QJsonValue &, const QString &snapshotRoot,
    GainImageArtifact *, QByteArray *);
} // namespace QindaQt::Audio::Test
