// SPDX-License-Identifier: GPL-3.0-or-later
#include "minimizedgatherpager.h"

#include "managedwindowregistry.h"

#include <compositor.h>
#include <scene/imageitem.h>
#include <scene/itemrenderer.h>
#include <scene/windowitem.h>
#include <scene/workspacescene.h>
#include <window.h>

#include <QApplication>
#include <QJsonObject>
#include <QPainter>
#include <QPalette>
#include <QPolygonF>

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

} // namespace

KWinMinimizedGatherPager::KWinMinimizedGatherPager(ManagedWindowRegistry &registry)
    : m_registry(registry)
{
}

KWinMinimizedGatherPager::~KWinMinimizedGatherPager()
{
    clear();
}

bool KWinMinimizedGatherPager::anchorItem(const QString &windowId,
                                           Entry &entry,
                                           QString *error)
{
    auto *const window = m_registry.window(windowId);
    auto *const windowItem = window ? window->windowItem() : nullptr;
    auto *const scene = KWin::Compositor::self() ? KWin::Compositor::self()->scene() : nullptr;
    if (!window || window->isDeleted() || !windowItem || !scene || !scene->renderer()) {
        return fail(error, QStringLiteral("minimized pager has no paintable anchor"));
    }
    if (!entry.item) {
        entry.item = scene->renderer()->createImageItem(windowItem);
        if (!entry.item) {
            return fail(error, QStringLiteral("could not create the minimized pager scene item"));
        }
        // The pager is above gathered chips and chrome anchored to this member.
        entry.item->setZ(3);
    } else if (entry.item->parentItem() != windowItem) {
        entry.item->setParentItem(windowItem);
    }
    entry.anchor = window;
    return true;
}

bool KWinMinimizedGatherPager::publish(const QString &outputId,
                                       const QString &anchorWindowId,
                                       const QRectF &frame,
                                       const QRectF &previousButton,
                                       const QRectF &counter,
                                       const QRectF &nextButton,
                                       const int currentPage,
                                       const int pageCount,
                                       QString *error)
{
    if (outputId.isEmpty() || !frame.isValid() || frame.isEmpty()
        || !frame.contains(previousButton) || !frame.contains(nextButton)
        || (counter.isValid() && !counter.isEmpty() && !frame.contains(counter))
        || pageCount < 2 || currentPage < 0 || currentPage >= pageCount) {
        return fail(error, QStringLiteral("minimized pager request is invalid"));
    }
    auto found = m_entries.find(outputId);
    if (found == m_entries.end()) {
        found = m_entries.emplace(outputId, Entry{}).first;
    }
    auto &entry = found->second;
    if (!anchorItem(anchorWindowId, entry, error)) {
        return false;
    }
    entry.currentPage = currentPage;
    entry.pageCount = pageCount;
    render(entry, frame, previousButton, counter, nextButton);
    updateItem(entry);
    return true;
}

void KWinMinimizedGatherPager::remove(const QString &outputId) noexcept
{
    const auto found = m_entries.find(outputId);
    if (found == m_entries.end()) {
        return;
    }
    dropItem(found->second);
    m_entries.erase(found);
}

void KWinMinimizedGatherPager::releaseSceneItems() noexcept
{
    for (auto &[outputId, entry] : m_entries) {
        Q_UNUSED(outputId)
        dropItem(entry);
    }
}

void KWinMinimizedGatherPager::clear() noexcept
{
    releaseSceneItems();
    m_entries.clear();
}

std::optional<MinimizedPagerHit> KWinMinimizedGatherPager::hitAt(
    const QPointF &position) const
{
    for (auto entry = m_entries.crbegin(); entry != m_entries.crend(); ++entry) {
        const auto &[outputId, surface] = *entry;
        if (!surface.item || !surface.anchor || !surface.item->isVisible()) {
            continue;
        }
        if (surface.previousButton.contains(position)) {
            return MinimizedPagerHit{outputId, MinimizedPagerButton::Previous};
        }
        if (surface.nextButton.contains(position)) {
            return MinimizedPagerHit{outputId, MinimizedPagerButton::Next};
        }
    }
    return std::nullopt;
}

int KWinMinimizedGatherPager::currentPage(const QString &outputId) const noexcept
{
    const auto found = m_entries.find(outputId);
    return found == m_entries.end() ? 0 : found->second.currentPage;
}

int KWinMinimizedGatherPager::pageCount(const QString &outputId) const noexcept
{
    const auto found = m_entries.find(outputId);
    return found == m_entries.end() ? 0 : found->second.pageCount;
}

QJsonArray KWinMinimizedGatherPager::diagnosticStates() const
{
    const auto rectJson = [](const QRectF &rect) {
        return QJsonObject{{QStringLiteral("x"), rect.x()},
                           {QStringLiteral("y"), rect.y()},
                           {QStringLiteral("width"), rect.width()},
                           {QStringLiteral("height"), rect.height()}};
    };
    QJsonArray result;
    for (const auto &[outputId, entry] : m_entries) {
        result.append(QJsonObject{
            {QStringLiteral("outputId"), outputId},
            {QStringLiteral("frame"), rectJson(entry.frame)},
            {QStringLiteral("previousButton"), rectJson(entry.previousButton)},
            {QStringLiteral("counter"), rectJson(entry.counter)},
            {QStringLiteral("nextButton"), rectJson(entry.nextButton)},
            {QStringLiteral("currentPage"), entry.currentPage},
            {QStringLiteral("pageCount"), entry.pageCount}});
    }
    return result;
}

void KWinMinimizedGatherPager::render(Entry &entry, const QRectF &frame,
                                      const QRectF &previousButton,
                                      const QRectF &counter,
                                      const QRectF &nextButton)
{
    entry.frame = frame;
    entry.previousButton = previousButton;
    entry.counter = counter;
    entry.nextButton = nextButton;
    const QSize physicalSize(qCeil(frame.width()), qCeil(frame.height()));
    entry.image = QImage(physicalSize, QImage::Format_ARGB32_Premultiplied);
    entry.image.fill(Qt::transparent);
    QPainter painter(&entry.image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette palette = QApplication::palette();
    const QRectF localFrame(QPointF(0, 0), frame.size());
    painter.setPen(palette.color(QPalette::Mid));
    painter.setBrush(palette.color(QPalette::Window));
    painter.drawRoundedRect(localFrame, std::min(frame.width(), frame.height()) / 2,
                            std::min(frame.width(), frame.height()) / 2);
    painter.setPen(palette.color(QPalette::Text));
    const auto drawArrow = [&painter, &frame](const QRectF &globalButton,
                                              const bool right) {
        const QRectF button(globalButton.topLeft() - frame.topLeft(),
                            globalButton.size());
        const QPointF center = button.center();
        const qreal side = std::min(button.width(), button.height()) / 6;
        QPolygonF triangle;
        triangle << QPointF(center.x() + (right ? side : -side), center.y())
                 << QPointF(center.x() + (right ? -side : side), center.y() - side)
                 << QPointF(center.x() + (right ? -side : side), center.y() + side);
        painter.setBrush(painter.pen().color());
        painter.drawPolygon(triangle);
    };
    drawArrow(previousButton, false);
    drawArrow(nextButton, true);
    if (!counter.isEmpty()) {
        const QRectF localCounter(counter.topLeft() - frame.topLeft(), counter.size());
        painter.setPen(palette.color(QPalette::Text));
        painter.setBrush(Qt::NoBrush);
        painter.drawText(localCounter, Qt::AlignCenter,
                         QStringLiteral("%1 / %2").arg(entry.currentPage + 1)
                             .arg(entry.pageCount));
    }
}

void KWinMinimizedGatherPager::updateItem(Entry &entry) noexcept
{
    if (!entry.item || !entry.anchor || entry.image.isNull()) {
        return;
    }
    entry.item->setVisible(true);
    entry.item->setImage(entry.image);
    entry.item->setSize(entry.frame.size());
    entry.item->setPosition(entry.frame.topLeft() - entry.anchor->pos());
}

void KWinMinimizedGatherPager::dropItem(Entry &entry) noexcept
{
    if (entry.item) {
        entry.item->setVisible(false);
        entry.item->setParentItem(nullptr);
        entry.item.reset();
    }
    entry.anchor.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
