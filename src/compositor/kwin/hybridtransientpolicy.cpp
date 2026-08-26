// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridtransientpolicy.h"

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

bool HybridTransientPolicy::synchronize(
    QVector<TransientSnapshot> transients,
    const QSet<QString> &groupedMemberIds,
    QString *error)
{
    if (error) {
        error->clear();
    }
    QSet<QString> ids;
    QVector<TransientAssociation> next;
    next.reserve(transients.size());
    for (const auto &transient : transients) {
        if (transient.transientId.trimmed().isEmpty()
            || transient.ownerWindowId.trimmed().isEmpty()
            || transient.ownerContainerId.trimmed().isEmpty()
            || !transient.transientFrame.isValid() || !transient.ownerFrame.isValid()) {
            return fail(error, QStringLiteral("transient association has invalid identity or geometry"));
        }
        if (ids.contains(transient.transientId)) {
            return fail(error, QStringLiteral("duplicate transient '%1'")
                                   .arg(transient.transientId));
        }
        if (groupedMemberIds.contains(transient.transientId)) {
            // AGENT-GUARD: A dialog/transient can follow a group but can never
            // become a leaf in its topology tree.
            return fail(error, QStringLiteral("transient '%1' is also a tiled member")
                                   .arg(transient.transientId));
        }
        if (!groupedMemberIds.contains(transient.ownerWindowId)) {
            return fail(error, QStringLiteral("transient owner '%1' is not grouped")
                                   .arg(transient.ownerWindowId));
        }
        ids.insert(transient.transientId);

        QPointF offset = transient.transientFrame.topLeft()
            - transient.ownerFrame.topLeft();
        if (const auto previous = association(transient.transientId);
            previous && previous->ownerWindowId == transient.ownerWindowId) {
            // Preserve user/client placement across owner geometry updates.
            // The next desired position is always cumulative from this stable
            // relationship, never from a rounded prior delta.
            offset = previous->ownerOffset;
        }
        next.append({transient.transientId,
                     transient.ownerWindowId,
                     transient.ownerContainerId,
                     transient.transientFrame,
                     transient.ownerFrame,
                     offset});
    }
    m_associations = std::move(next);
    return true;
}

QVector<TransientPlacement> HybridTransientPolicy::ownerFrameChanged(
    const QString &ownerWindowId, const QRectF &frame)
{
    QVector<TransientPlacement> placements;
    if (!frame.isValid()) {
        return placements;
    }
    for (auto &association : m_associations) {
        if (association.ownerWindowId != ownerWindowId) {
            continue;
        }
        const QRectF desired(frame.topLeft() + association.ownerOffset,
                             association.transientFrame.size());
        association.ownerFrame = frame;
        association.transientFrame = desired;
        placements.append({association.transientId,
                           association.ownerWindowId,
                           association.ownerContainerId,
                           desired});
    }
    return placements;
}

bool HybridTransientPolicy::transientFrameChanged(const QString &transientId,
                                                  const QRectF &frame,
                                                  QString *error)
{
    if (error) {
        error->clear();
    }
    if (!frame.isValid()) {
        return fail(error, QStringLiteral("transient frame must be valid"));
    }
    for (auto &association : m_associations) {
        if (association.transientId == transientId) {
            association.transientFrame = frame;
            association.ownerOffset = frame.topLeft()
                - association.ownerFrame.topLeft();
            return true;
        }
    }
    return fail(error, QStringLiteral("unknown transient '%1'").arg(transientId));
}

std::optional<TransientAssociation> HybridTransientPolicy::association(
    const QString &transientId) const
{
    for (const auto &candidate : m_associations) {
        if (candidate.transientId == transientId) {
            return candidate;
        }
    }
    return std::nullopt;
}

QVector<TransientAssociation> HybridTransientPolicy::associations() const
{
    return m_associations;
}

} // namespace QindaQt::Compositor::KWinIntegration
