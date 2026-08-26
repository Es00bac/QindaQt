// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridgroupcontext.h"

#include <QSet>

#include <algorithm>
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

bool hasUniqueNonEmptyIds(const QStringList &ids)
{
    QSet<QString> unique;
    for (const auto &id : ids) {
        if (id.isEmpty() || unique.contains(id)) {
            return false;
        }
        unique.insert(id);
    }
    return true;
}

QRectF keepInside(QRectF frame, const QRectF &area)
{
    frame.setSize(frame.size().boundedTo(area.size()));
    if (frame.right() > area.right() && frame.width() <= area.width()) {
        frame.moveRight(area.right());
    }
    if (frame.bottom() > area.bottom() && frame.height() <= area.height()) {
        frame.moveBottom(area.bottom());
    }
    if (frame.left() < area.left()) {
        frame.moveLeft(area.left());
    }
    if (frame.top() < area.top()) {
        frame.moveTop(area.top());
    }
    return frame;
}

} // namespace

bool HybridGroupContext::isValid(QString *error) const
{
    if (!hasUniqueNonEmptyIds(desktopIds)
        || !hasUniqueNonEmptyIds(activityIds)) {
        return fail(error,
                    QStringLiteral("desktop and activity IDs must be unique and non-empty"));
    }
    if (keepAbove && keepBelow) {
        return fail(error,
                    QStringLiteral("a group cannot be kept above and below simultaneously"));
    }
    return true;
}

HybridGroupContextRecoveryResult recoverHybridGroupContext(
    const QString &containerId,
    const HybridGroupContextOperation &adopt,
    const HybridGroupContextOperation &release,
    const HybridGroupContextSafetyUpdate &updateSafety)
{
    HybridGroupContextRecoveryResult result;
    if (adopt && adopt(&result.adoptionError)) {
        result.status = HybridGroupContextRecoveryStatus::Adopted;
        if (updateSafety) {
            updateSafety(containerId, true);
        }
        return result;
    }
    if (release && release(&result.releaseError)) {
        result.status = HybridGroupContextRecoveryStatus::Released;
        return result;
    }
    result.status = HybridGroupContextRecoveryStatus::Quarantined;
    if (updateSafety) {
        updateSafety(containerId, false);
    }
    return result;
}

std::optional<QRect> mapHybridGroupFrameToArea(
    const QRect &frame,
    const QRectF &oldArea,
    const QRectF &newArea,
    QString *error)
{
    if (!frame.isValid() || !oldArea.isValid() || !newArea.isValid()
        || oldArea.width() <= 0.0 || oldArea.height() <= 0.0) {
        fail(error, QStringLiteral("group frame and output areas must be valid"));
        return std::nullopt;
    }

    QRectF mapped(frame);
    QPointF relativeCenter = mapped.center() - oldArea.center();
    relativeCenter.setX(relativeCenter.x() * newArea.width() / oldArea.width());
    relativeCenter.setY(relativeCenter.y() * newArea.height() / oldArea.height());
    mapped.moveCenter(relativeCenter + newArea.center());
    if (oldArea.contains(QRectF(frame))) {
        mapped = keepInside(mapped, newArea);
    }

    const QRect rounded = mapped.toRect();
    if (!rounded.isValid()) {
        fail(error, QStringLiteral("mapped group frame is invalid after rounding"));
        return std::nullopt;
    }
    return rounded;
}

QMap<QString, QString> retainValidGroupContextSources(
    const QMap<QString, QString> &pendingSourceByContainer,
    const QHash<QString, QString> &containerForWindow,
    const QHash<QString, QString> &liveOwnerForWindow)
{
    QMap<QString, QString> retained;
    for (auto iterator = pendingSourceByContainer.cbegin();
         iterator != pendingSourceByContainer.cend(); ++iterator) {
        if (containerForWindow.value(iterator.value()) == iterator.key()
            && liveOwnerForWindow.value(iterator.value()) == iterator.key()) {
            retained.insert(iterator.key(), iterator.value());
        }
    }
    return retained;
}

} // namespace QindaQt::Compositor::KWinIntegration
