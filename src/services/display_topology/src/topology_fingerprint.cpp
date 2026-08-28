// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_topology/topology.h>

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QHash>

#include <algorithm>

namespace QindaQt::DisplayTopology
{
namespace
{

void writeText(QDataStream &stream, const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    stream << static_cast<quint32>(bytes.size());
    stream.writeRawData(bytes.constData(), bytes.size());
}

const Display::CandidateOutput *findCandidateOutput(
    const Display::Candidate &candidate, const QString &stableId)
{
    for (const Display::CandidateOutput &output : candidate.outputs) {
        if (output.stableId == stableId) {
            return &output;
        }
    }
    return nullptr;
}

void appendDifference(QList<DiffField> &fields, const bool differs, const DiffField field)
{
    if (differs) {
        fields.push_back(field);
    }
}

} // namespace

Display::Candidate candidateFromSnapshot(const Display::Snapshot &snapshot)
{
    Display::Candidate candidate{.protocolVersion = snapshot.protocolVersion,
                                 .baseEpoch = snapshot.serviceEpoch,
                                 .baseRevision = snapshot.revision,
                                 .outputs = {},
                                 .wireValid = true};
    candidate.outputs.reserve(snapshot.outputs.size());
    for (const Display::Output &output : snapshot.outputs) {
        candidate.outputs.push_back({.stableId = output.stableId,
                                     .enabled = output.enabled,
                                     .primary = output.primary,
                                     .modeId = output.modeId,
                                     .position = output.position,
                                     .scale = output.scale,
                                     .transform = output.transform,
                                     .priority = output.priority,
                                     .replicationSourceStableId =
                                         output.replicationSourceStableId});
    }
    QHash<QString, qsizetype> indices;
    for (qsizetype index = 0; index < candidate.outputs.size(); ++index) {
        indices.insert(candidate.outputs.at(index).stableId, index);
    }
    for (Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled) {
            output.primary = false;
            output.position = {};
            output.priority = 0;
            output.replicationSourceStableId.clear();
        }
    }
    bool hasOrigin = false;
    int minimumX = 0;
    int minimumY = 0;
    for (const Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled || !output.replicationSourceStableId.isEmpty()) {
            continue;
        }
        if (!hasOrigin) {
            minimumX = output.position.x();
            minimumY = output.position.y();
            hasOrigin = true;
        } else {
            minimumX = std::min(minimumX, output.position.x());
            minimumY = std::min(minimumY, output.position.y());
        }
    }
    for (Display::CandidateOutput &output : candidate.outputs) {
        if (!output.enabled) {
            continue;
        }
        QString rootId = output.replicationSourceStableId;
        qsizetype steps = 0;
        while (!rootId.isEmpty() && indices.contains(rootId)
               && steps++ < candidate.outputs.size()) {
            const Display::CandidateOutput &root = candidate.outputs.at(indices.value(rootId));
            if (root.replicationSourceStableId.isEmpty()) {
                output.position = root.position;
                output.scale = root.scale;
                break;
            }
            rootId = root.replicationSourceStableId;
        }
        output.position -= QPoint(minimumX, minimumY);
    }
    std::sort(candidate.outputs.begin(), candidate.outputs.end(),
              [](const Display::CandidateOutput &left,
                 const Display::CandidateOutput &right) {
                  return left.stableId < right.stableId;
              });
    return candidate;
}

QByteArray canonicalFingerprint(const Display::Candidate &normalizedCandidate)
{
    QList<Display::CandidateOutput> outputs = normalizedCandidate.outputs;
    std::sort(outputs.begin(), outputs.end(),
              [](const Display::CandidateOutput &left,
                 const Display::CandidateOutput &right) {
                  return left.stableId < right.stableId;
              });
    QByteArray canonical;
    QBuffer buffer(&canonical);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.writeRawData("QDTF", 4);
    stream << static_cast<quint32>(1) << static_cast<quint32>(outputs.size());
    for (const Display::CandidateOutput &output : outputs) {
        writeText(stream, output.stableId);
        stream << output.enabled << output.primary;
        writeText(stream, output.modeId);
        stream << static_cast<qint32>(output.position.x())
               << static_cast<qint32>(output.position.y()) << output.scale
               << static_cast<quint32>(output.transform) << output.priority;
        writeText(stream, output.replicationSourceStableId);
    }
    buffer.close();
    return QCryptographicHash::hash(canonical, QCryptographicHash::Sha256);
}

QList<CandidateDiff> diff(const Display::Snapshot &snapshot,
                          const Display::Candidate &normalizedCandidate)
{
    QList<CandidateDiff> differences;
    const Display::Candidate liveProjection = candidateFromSnapshot(snapshot);
    for (const Display::CandidateOutput &live : liveProjection.outputs) {
        const Display::CandidateOutput *candidate = findCandidateOutput(normalizedCandidate,
                                                                        live.stableId);
        if (candidate == nullptr) {
            continue;
        }
        QList<DiffField> fields;
        appendDifference(fields, live.enabled != candidate->enabled, DiffField::Enabled);
        appendDifference(fields, live.primary != candidate->primary, DiffField::Primary);
        appendDifference(fields, live.modeId != candidate->modeId, DiffField::Mode);
        if (candidate->replicationSourceStableId.isEmpty()) {
            appendDifference(fields, live.position != candidate->position, DiffField::Position);
            appendDifference(fields, live.scale != candidate->scale, DiffField::Scale);
        }
        appendDifference(fields, live.transform != candidate->transform,
                         DiffField::Transform);
        appendDifference(fields, live.priority != candidate->priority, DiffField::Priority);
        appendDifference(fields,
                         live.replicationSourceStableId
                             != candidate->replicationSourceStableId,
                         DiffField::ReplicationSource);
        if (!fields.isEmpty()) {
            differences.push_back({.stableId = live.stableId, .fields = std::move(fields)});
        }
    }
    std::sort(differences.begin(), differences.end(),
              [](const CandidateDiff &left, const CandidateDiff &right) {
                  return left.stableId < right.stableId;
              });
    return differences;
}

} // namespace QindaQt::DisplayTopology
