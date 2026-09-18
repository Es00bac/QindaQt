// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwiniconchippresenter.h"

#include "managedwindowregistry.h"

#include <compositor.h>
#include <scene/imageitem.h>
#include <scene/itemrenderer.h>
#include <scene/windowitem.h>
#include <scene/workspacescene.h>
#include <window.h>

#include <QPainter>

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

} // namespace

KWinIconChipPresenter::KWinIconChipPresenter(ManagedWindowRegistry &registry)
    : m_registry(registry)
{
}

KWinIconChipPresenter::~KWinIconChipPresenter()
{
    clear();
}

bool KWinIconChipPresenter::anchorItem(const QString &windowId, Entry &entry, QString *error)
{
    auto *const window = m_registry.window(windowId);
    auto *const windowItem = window ? window->windowItem() : nullptr;
    auto *const scene = KWin::Compositor::self() ? KWin::Compositor::self()->scene() : nullptr;
    if (!window || window->isDeleted() || !windowItem || !scene || !scene->renderer()) {
        return fail(error, QStringLiteral("iconified window '%1' has no paintable scene item")
                               .arg(windowId));
    }
    if (!entry.item) {
        entry.item = scene->renderer()->createImageItem(windowItem);
        if (!entry.item) {
            return fail(error, QStringLiteral("could not create a chip scene image for '%1'")
                                   .arg(windowId));
        }
        // WindowItem's windowContainer uses z=0; a positive child paints after
        // the (hidden) surface while the window's own stack slot keeps
        // unrelated windows above the chip.
        entry.item->setZ(1);
    } else if (entry.item->parentItem() != windowItem) {
        entry.item->setParentItem(windowItem);
    }
    entry.anchor = window;
    return true;
}

bool KWinIconChipPresenter::publish(const QString &windowId,
                                    const HybridChrome::IconChipPlan &plan,
                                    QString *error)
{
    if (!plan.isValid() || plan.windowId != windowId) {
        return fail(error, QStringLiteral("icon chip plan for '%1' is invalid").arg(windowId));
    }
    auto found = m_entries.find(windowId);
    if (found == m_entries.end()) {
        Entry staged;
        staged.plan = plan;
        if (!anchorItem(windowId, staged, error)) {
            return false;
        }
        found = m_entries.emplace(windowId, std::move(staged)).first;
    } else {
        found->second.plan = plan;
        if (!anchorItem(windowId, found->second, error)) {
            return false;
        }
    }
    auto &entry = found->second;
    entry.state.hovered = m_hover && m_hover->windowId == windowId;
    entry.state.closeHovered = entry.state.hovered
        && m_hover->target == HybridChrome::IconChipHitKind::Close;
    render(entry);
    updateItem(entry);
    return true;
}

void KWinIconChipPresenter::setPointerHover(std::optional<IconChipPointerHit> hit)
{
    if (hit && (!hit->isValid() || !m_entries.contains(hit->windowId))) {
        hit.reset();
    }
    m_hover = std::move(hit);
    for (auto &[windowId, entry] : m_entries) {
        const bool hovered = m_hover && m_hover->windowId == windowId;
        const bool closeHovered = hovered
            && m_hover->target == HybridChrome::IconChipHitKind::Close;
        if (entry.state.hovered == hovered && entry.state.closeHovered == closeHovered) {
            continue;
        }
        entry.state.hovered = hovered;
        entry.state.closeHovered = closeHovered;
        render(entry);
        updateItem(entry);
    }
}

void KWinIconChipPresenter::setVisible(const QString &windowId, bool visible)
{
    const auto found = m_entries.find(windowId);
    if (found == m_entries.end() || found->second.visible == visible) {
        return;
    }
    found->second.visible = visible;
    updateItem(found->second);
}

void KWinIconChipPresenter::remove(const QString &windowId) noexcept
{
    const auto found = m_entries.find(windowId);
    if (found == m_entries.end()) {
        return;
    }
    dropItem(found->second);
    m_entries.erase(found);
    if (m_hover && m_hover->windowId == windowId) {
        m_hover.reset();
    }
}

void KWinIconChipPresenter::releaseSceneItems() noexcept
{
    for (auto &[windowId, entry] : m_entries) {
        Q_UNUSED(windowId)
        dropItem(entry);
    }
}

void KWinIconChipPresenter::clear() noexcept
{
    releaseSceneItems();
    m_entries.clear();
    m_hover.reset();
}

std::optional<HybridChrome::IconChipPlan> KWinIconChipPresenter::plan(
    const QString &windowId) const
{
    const auto found = m_entries.find(windowId);
    return found == m_entries.end() ? std::nullopt : std::optional(found->second.plan);
}

bool KWinIconChipPresenter::isVisible(const QString &windowId) const noexcept
{
    const auto found = m_entries.find(windowId);
    return found != m_entries.end() && found->second.visible && found->second.anchor
        && found->second.item && found->second.item->isVisible();
}

bool KWinIconChipPresenter::hasSceneItem(const QString &windowId) const noexcept
{
    const auto found = m_entries.find(windowId);
    return found != m_entries.end() && found->second.item && found->second.anchor;
}

qsizetype KWinIconChipPresenter::count() const noexcept
{
    return static_cast<qsizetype>(m_entries.size());
}

qsizetype KWinIconChipPresenter::visibleAnchoredCount() const noexcept
{
    return static_cast<qsizetype>(std::count_if(
        m_entries.cbegin(), m_entries.cend(), [](const auto &entry) {
            return entry.second.visible && entry.second.anchor && entry.second.item
                && entry.second.item->isVisible();
        }));
}

QStringList KWinIconChipPresenter::windowIds() const
{
    QStringList ids;
    for (const auto &[windowId, entry] : m_entries) {
        Q_UNUSED(entry)
        ids.append(windowId);
    }
    return ids;
}

void KWinIconChipPresenter::render(Entry &entry)
{
    const auto &plan = entry.plan;
    if (!plan.imageRect.isValid() || !std::isfinite(plan.devicePixelRatio)
        || plan.devicePixelRatio <= 0.0) {
        entry.image = {};
        return;
    }
    const QSize physicalSize(qCeil(plan.imageRect.width() * plan.devicePixelRatio),
                             qCeil(plan.imageRect.height() * plan.devicePixelRatio));
    if (physicalSize.isEmpty()) {
        entry.image = {};
        return;
    }
    entry.image = QImage(physicalSize, QImage::Format_ARGB32_Premultiplied);
    entry.image.setDevicePixelRatio(plan.devicePixelRatio);
    entry.image.fill(Qt::transparent);
    QPainter painter(&entry.image);
    // The plan is global; the image starts at the chip's image rectangle.
    painter.translate(-plan.imageRect.topLeft());
    HybridChrome::ChromeIconChip::paint(painter, plan, entry.state);
}

void KWinIconChipPresenter::updateItem(Entry &entry) noexcept
{
    if (!entry.item) {
        return;
    }
    const bool renderable = entry.visible && entry.anchor && !entry.image.isNull();
    entry.item->setVisible(renderable);
    if (!renderable) {
        return;
    }
    entry.item->setImage(entry.image);
    entry.item->setSize(entry.plan.imageRect.size());
    entry.item->setPosition(entry.plan.imageRect.topLeft() - entry.anchor->pos());
}

void KWinIconChipPresenter::dropItem(Entry &entry) noexcept
{
    if (entry.item) {
        entry.item->setVisible(false);
        entry.item->setParentItem(nullptr);
        entry.item.reset();
    }
    entry.anchor.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
