// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridiconifycontroller.h"

#include <algorithm>
#include <cmath>
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

bool finiteFrame(const QRectF &frame)
{
    return std::isfinite(frame.x()) && std::isfinite(frame.y())
        && std::isfinite(frame.width()) && std::isfinite(frame.height())
        && frame.isValid();
}

QPointF clampTopLeft(const QPointF &topLeft, const QSizeF &size, const QRectF &bounds)
{
    if (!bounds.isValid()) {
        return topLeft;
    }
    // Bounds narrower than the chip keep its leading edge, never push it out
    // of the far side (same rule as ChromeIconChip::layout).
    const qreal x = std::max(bounds.left(),
                             std::min(topLeft.x(), bounds.right() - size.width()));
    const qreal y = std::max(bounds.top(),
                             std::min(topLeft.y(), bounds.bottom() - size.height()));
    return {x, y};
}

} // namespace

HybridIconifyController::HybridIconifyController(HybridIconifyPlatform &platform)
    : m_platform(platform)
{
}

qsizetype HybridIconifyController::indexOf(const QString &windowId) const noexcept
{
    for (qsizetype index = 0; index < m_records.size(); ++index) {
        if (m_records.at(index).windowId == windowId) {
            return index;
        }
    }
    return -1;
}

bool HybridIconifyController::iconify(const QString &windowId,
                                      const QRectF &restoreFrame,
                                      const QRectF &chipFrame,
                                      bool wasActive,
                                      QString *error)
{
    if (windowId.isEmpty()) {
        return fail(error, QStringLiteral("iconify names no window"));
    }
    if (isIconified(windowId)) {
        return fail(error, QStringLiteral("window '%1' is already iconified").arg(windowId));
    }
    if (!finiteFrame(restoreFrame) || !finiteFrame(chipFrame)) {
        return fail(error, QStringLiteral("iconify frames for '%1' are invalid").arg(windowId));
    }
    if (!m_platform.hideWindow(windowId, error)) {
        return false;
    }
    m_records.append({windowId, restoreFrame, chipFrame, wasActive});
    return true;
}

std::optional<IconifiedWindowRecord> HybridIconifyController::restore(
    const QString &windowId, QString *error)
{
    const auto index = indexOf(windowId);
    if (index < 0) {
        fail(error, QStringLiteral("window '%1' is not iconified").arg(windowId));
        return std::nullopt;
    }
    const auto record = m_records.at(index);
    m_records.removeAt(index);
    if (!m_platform.showWindow(windowId, error)) {
        return std::nullopt;
    }
    return record;
}

std::optional<IconifiedWindowRecord> HybridIconifyController::revealed(
    const QString &windowId)
{
    const auto index = indexOf(windowId);
    if (index < 0) {
        return std::nullopt;
    }
    const auto record = m_records.at(index);
    m_records.removeAt(index);
    // AGENT-GUARD: KWin already cleared hidden; the platform must still undo
    // the content/shadow/opacity treatment or the revealed window would be an
    // input-eligible client with invisible content. A failure here has
    // nothing left to roll back, so the record is dropped regardless.
    QString ignored;
    static_cast<void>(m_platform.showWindow(windowId, &ignored));
    return record;
}

bool HybridIconifyController::relocateChip(const QString &windowId,
                                           const QPointF &topLeft,
                                           const QRectF &bounds,
                                           QString *error)
{
    const auto index = indexOf(windowId);
    if (index < 0) {
        return fail(error, QStringLiteral("window '%1' is not iconified").arg(windowId));
    }
    if (!std::isfinite(topLeft.x()) || !std::isfinite(topLeft.y())) {
        return fail(error, QStringLiteral("chip position for '%1' is not finite").arg(windowId));
    }
    auto &record = m_records[index];
    const auto clamped = clampTopLeft(topLeft, record.chipFrame.size(), bounds);
    const auto delta = clamped - record.chipFrame.topLeft();
    record.chipFrame.moveTopLeft(clamped);
    record.restoreFrame.translate(delta);
    return true;
}

bool HybridIconifyController::reapply(const QString &windowId, QString *error)
{
    if (!isIconified(windowId)) {
        return fail(error, QStringLiteral("window '%1' is not iconified").arg(windowId));
    }
    return m_platform.hideWindow(windowId, error);
}

void HybridIconifyController::windowClosed(const QString &windowId) noexcept
{
    const auto index = indexOf(windowId);
    if (index >= 0) {
        m_records.removeAt(index);
    }
}

QVector<IconifiedWindowRecord> HybridIconifyController::restoreAll(QString *error)
{
    const auto records = std::move(m_records);
    m_records.clear();
    bool allOk = true;
    for (const auto &record : records) {
        QString showError;
        if (!m_platform.showWindow(record.windowId, &showError) && allOk) {
            allOk = false;
            fail(error, showError);
        }
    }
    return records;
}

bool HybridIconifyController::isIconified(const QString &windowId) const noexcept
{
    return indexOf(windowId) >= 0;
}

std::optional<IconifiedWindowRecord> HybridIconifyController::record(
    const QString &windowId) const
{
    const auto index = indexOf(windowId);
    return index < 0 ? std::nullopt : std::optional(m_records.at(index));
}

QStringList HybridIconifyController::iconifiedWindowIds() const
{
    QStringList ids;
    ids.reserve(m_records.size());
    for (const auto &record : m_records) {
        ids.append(record.windowId);
    }
    return ids;
}

} // namespace QindaQt::Compositor::KWinIntegration
