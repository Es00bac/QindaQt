// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "kwinchromemanager.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QWidget>

#include <functional>
#include <memory>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration::Test {

struct OverlayRecord final
{
    QString containerId;
    HybridChrome::ChromeRenderPlan plan;
    int planCount = 0;
    int showCount = 0;
    int closeCount = 0;
    bool destroyed = false;
    bool visible = false;
    QString anchorId;
    HybridChrome::ChromeHitTarget hoveredTarget;
    std::function<void()> closing;
};

class FakeOverlay final : public ChromeOverlay
{
public:
    explicit FakeOverlay(std::shared_ptr<OverlayRecord> record)
        : m_record(std::move(record))
    {
    }

    ~FakeOverlay() override { m_record->destroyed = true; }
    void setRenderPlan(HybridChrome::ChromeRenderPlan plan) override
    {
        m_record->plan = std::move(plan);
        ++m_record->planCount;
    }
    void setPointerHoverTarget(HybridChrome::ChromeHitTarget target) override
    {
        m_record->hoveredTarget = std::move(target);
    }
    bool setStackingAnchor(const QString &windowId, QString *error) override
    {
        if (windowId.isEmpty()) {
            if (error) {
                *error = QStringLiteral("empty anchor");
            }
            return false;
        }
        m_record->anchorId = windowId;
        return true;
    }
    void showOverlay() override
    {
        ++m_record->showCount;
        m_record->visible = true;
    }
    void hideOverlay() noexcept override { m_record->visible = false; }
    bool isVisible() const noexcept override { return m_record->visible; }
    bool hasStackingAnchor() const noexcept override
    {
        return !m_record->anchorId.isEmpty();
    }
    void closeOverlay() noexcept override
    {
        ++m_record->closeCount;
        m_record->visible = false;
        if (m_record->closing) {
            m_record->closing();
        }
    }
    QWidget *widget() const noexcept override { return nullptr; }

private:
    std::shared_ptr<OverlayRecord> m_record;
};

class FakeOverlayFactory final : public ChromeOverlayFactory
{
public:
    std::unique_ptr<ChromeOverlay> create(const QString &containerId) override
    {
        ++createCount;
        if (containerId == failingContainerId) {
            return {};
        }
        auto record = std::make_shared<OverlayRecord>();
        record->containerId = containerId;
        record->closing = closing;
        records.insert(containerId, record);
        return std::make_unique<FakeOverlay>(std::move(record));
    }

    QString failingContainerId;
    int createCount = 0;
    QMap<QString, std::shared_ptr<OverlayRecord>> records;
    std::function<void()> closing;
};

inline Core::WindowContainer makeContainer(const QString &suffix)
{
    Core::WindowContainer container(QStringLiteral("container-%1").arg(suffix));
    QString error;
    if (!container.addPage(QStringLiteral("page-%1-main").arg(suffix),
                           QStringLiteral("leaf-%1-a").arg(suffix),
                           QStringLiteral("window-%1-a").arg(suffix), &error)
        || !container.splitWindow({QStringLiteral("window-%1-a").arg(suffix),
                                   QStringLiteral("window-%1-b").arg(suffix),
                                   QStringLiteral("leaf-%1-b").arg(suffix),
                                   QStringLiteral("split-%1-main").arg(suffix),
                                   Core::SplitOrientation::Horizontal, 0.5,
                                   Core::InsertPosition::Second}, &error)
        || !container.addPage(QStringLiteral("page-%1-extra").arg(suffix),
                              QStringLiteral("leaf-%1-c").arg(suffix),
                              QStringLiteral("window-%1-c").arg(suffix), &error)) {
        qFatal("topology fixture failed: %s", qPrintable(error));
    }
    return container;
}

inline Hybrid::WindowTopology makeTopology(
    QVector<Core::WindowContainer> containers,
    quint64 revision)
{
    QString error;
    auto topology = Hybrid::WindowTopology::create(
        {}, std::move(containers), revision, &error);
    if (!topology) {
        qFatal("topology fixture failed: %s", qPrintable(error));
    }
    return std::move(*topology);
}

inline HybridChrome::ChromeRenderPlan makePlan(
    const Core::WindowContainer &container,
    HybridChrome::ChromeStyle style = HybridChrome::ChromeStyle::standard(
        HybridChrome::ButtonSide::Right),
    QRectF outerRect = QRectF(0.0, 0.0, 1000.0, 700.0))
{
    HybridChrome::ChromeLayoutRequest request;
    request.containerId = container.id();
    request.outerRect = outerRect;
    request.style = std::move(style);
    for (const auto &page : container.pages()) {
        request.tabs.append(
            {page.id(), page.id(), page.id() == container.activePageId()});
    }

    const QRectF content(outerRect.left() + 1.0, outerRect.top() + 69.0,
                         outerRect.width() - 2.0, outerRect.height() - 70.0);
    const auto *page = container.page(container.activePageId());
    if (page && page->root().isSplit()) {
        const qreal firstWidth = content.width() / 2.0;
        request.members = {
            {page->root().firstChild()->windowId(), QStringLiteral("First"),
             QRectF(content.left(), content.top(), firstWidth, content.height())},
            {page->root().secondChild()->windowId(), QStringLiteral("Second"),
             QRectF(content.left() + firstWidth, content.top(),
                    content.width() - firstWidth, content.height())}};
        request.dividers = {
            {page->root().id(), HybridChrome::DividerOrientation::Vertical,
             content.left() + firstWidth, content.top(), content.bottom()}};
    } else if (page) {
        request.members = {
            {page->root().windowId(), QStringLiteral("Only"), content}};
    }
    QString error;
    auto plan = HybridChrome::ChromeLayoutEngine::build(request, &error);
    if (!plan) {
        qFatal("chrome fixture failed: %s", qPrintable(error));
    }
    return std::move(*plan);
}

inline KWinChromeManager::ChromePlanMap plansFor(
    const Hybrid::WindowTopology &topology,
    HybridChrome::ChromeStyle style = HybridChrome::ChromeStyle::standard(
        HybridChrome::ButtonSide::Right))
{
    KWinChromeManager::ChromePlanMap plans;
    for (const auto &containerId : topology.containerIds()) {
        plans.insert(containerId,
                     makePlan(*topology.container(containerId), style));
    }
    return plans;
}

inline QPointF centerOf(const QRectF &rect)
{
    return rect.center();
}

} // namespace QindaQt::Compositor::KWinIntegration::Test
