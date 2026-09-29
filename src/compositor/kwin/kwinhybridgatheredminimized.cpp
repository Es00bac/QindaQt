// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridcontainerplacement.h"
#include "hybridiconifycontroller.h"
#include "kwinchromemanager.h"
#include "kwiniconchippresenter.h"
#include "managedwindowregistry.h"
#include "minimizedgatherpager.h"
#include "kwinhybridscene.h"

#include "qindaqt/hybrid_chrome/chromeiconchip.h"
#include "qindaqt/hybrid_gather/gather_layout.h"

#include <core/output.h>
#include <window.h>
#include <workspace.h>

#include <QMap>
#include <QSet>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

struct OutputGather final
{
    HybridGather::MinimizedGatherRequest request;
    QString anchorWindowId;
};

void setOutputArea(OutputGather *gather, KWin::Window *window)
{
    if (!gather->request.workArea.isEmpty() || !window) {
        return;
    }
    auto *const workspace = KWin::workspace();
    if (workspace) {
        gather->request.workArea = QRectF(
            workspace->clientArea(KWin::MaximizeArea, window).toAlignedRect());
    }
}

} // namespace

void KWinHybridSession::synchronizeMinimizedGather()
{
    if (m_shutdown || !ready() || !m_placement || !m_iconify
        || !m_iconChips || !m_minimizedGatherPager) {
        return;
    }

    QMap<QString, OutputGather> outputs;
    for (const QString &windowId : m_iconify->iconifiedWindowIds()) {
        auto *const window = m_registry.window(windowId);
        if (!window || window->isDeleted() || !window->output()) {
            continue;
        }
        const QString outputId = window->output()->name();
        auto &gather = outputs[outputId];
        setOutputArea(&gather, window);
        gather.request.iconifiedWindowIds.append(windowId);
        gather.request.iconExtent = HybridChrome::ChromeIconChip::ChipExtent;
        if (gather.anchorWindowId.isEmpty()) {
            gather.anchorWindowId = windowId;
        }
    }

    const auto &topology = m_runtime->topology();
    for (const QString &containerId : topology.containerIds()) {
        const auto frame = m_placement->shadedFrame(containerId);
        if (!frame) {
            continue;
        }
        const auto *container = topology.container(containerId);
        auto *const anchor = container
            ? m_registry.window(activeRepresentative(*container)) : nullptr;
        if (!anchor || anchor->isDeleted() || !anchor->output()) {
            continue;
        }
        const QString outputId = anchor->output()->name();
        auto &gather = outputs[outputId];
        setOutputArea(&gather, anchor);
        gather.request.shadedContainers.append(
            {containerId, QSizeF(frame->size())});
        if (gather.anchorWindowId.isEmpty()) {
            gather.anchorWindowId = m_registry.windowId(anchor);
        }
    }

    const QSet<QString> liveOutputs(outputs.keyBegin(), outputs.keyEnd());
    for (auto iterator = m_minimizedGatherPages.begin();
         iterator != m_minimizedGatherPages.end();) {
        if (!liveOutputs.contains(iterator.key())) {
            if (m_minimizedGatherPagerRouter) {
                m_minimizedGatherPagerRouter->cancel();
            }
            m_minimizedGatherPager->remove(iterator.key());
            iterator = m_minimizedGatherPages.erase(iterator);
        } else {
            ++iterator;
        }
    }

    for (auto iterator = outputs.begin(); iterator != outputs.end(); ++iterator) {
        const QString outputId = iterator.key();
        auto &gather = iterator.value();
        if (gather.request.workArea.isEmpty()) {
            m_minimizedGatherPager->remove(outputId);
            continue;
        }
        const auto layout = HybridGather::planMinimizedGather(
            gather.request, m_minimizedGatherPages.value(outputId, 0));
        if (!layout.ok) {
            qWarning("QindaQt minimized gather for output '%s' failed: %s",
                     qPrintable(outputId), qPrintable(layout.diagnostic));
            m_minimizedGatherPager->remove(outputId);
            continue;
        }
        m_minimizedGatherPages.insert(outputId, layout.appliedPage);

        QMap<QString, QPointF> iconLocations;
        for (const auto &placement : layout.icons) {
            iconLocations.insert(placement.id, placement.frame.topLeft());
        }
        QSet<QString> visibleIcons;
        for (const QString &windowId : gather.request.iconifiedWindowIds) {
            auto *const window = m_registry.window(windowId);
            const auto location = iconLocations.constFind(windowId);
            if (location != iconLocations.cend()) {
                QString error;
                if (m_iconify->placeChipForGather(windowId, *location, &error)) {
                    visibleIcons.insert(windowId);
                    if (!publishIconChip(windowId, &error)) {
                        qWarning("QindaQt could not publish gathered chip '%s': %s",
                                 qPrintable(windowId), qPrintable(error));
                    }
                } else {
                    qWarning("QindaQt could not place gathered chip '%s': %s",
                             qPrintable(windowId), qPrintable(error));
                }
            }
            m_iconChips->setVisible(windowId,
                                    visibleIcons.contains(windowId)
                                        && window && !window->isMinimized());
        }

        QMap<QString, QPoint> containerLocations;
        for (const auto &placement : layout.containers) {
            containerLocations.insert(placement.id,
                                      placement.frame.topLeft().toPoint());
        }
        for (const auto &tile : gather.request.shadedContainers) {
            const auto location = containerLocations.constFind(tile.id);
            if (location != containerLocations.cend()) {
                static_cast<void>(m_placement->placeShadeStripForGather(tile.id, *location));
            }
            if (m_chromeManager) {
                m_chromeManager->setOverlayVisible(
                    tile.id, location != containerLocations.cend());
            }
        }

        if (layout.pageCount > 1) {
            QString anchorId = gather.anchorWindowId;
            if (!layout.icons.isEmpty()) {
                anchorId = layout.icons.constFirst().id;
            } else if (!layout.containers.isEmpty()) {
                const auto *container = topology.container(layout.containers.constFirst().id);
                if (container) {
                    anchorId = activeRepresentative(*container);
                }
            }
            QString error;
            if (!m_minimizedGatherPager->publish(
                    outputId, anchorId, gather.request.workArea,
                    layout.appliedPage, layout.pageCount, &error)) {
                qWarning("QindaQt minimized pager for output '%s' failed: %s",
                         qPrintable(outputId), qPrintable(error));
            }
        } else {
            m_minimizedGatherPager->remove(outputId);
        }
    }
}

void KWinHybridSession::handleMinimizedPagerHit(const MinimizedPagerHit &hit)
{
    if (!ready() || !m_minimizedGatherPager) {
        return;
    }
    const int step = hit.button == MinimizedPagerButton::Next ? 1 : -1;
    const int last = m_minimizedGatherPager->pageCount(hit.outputId) - 1;
    if (last < 1) {
        return;
    }
    const int current = m_minimizedGatherPager->currentPage(hit.outputId);
    m_minimizedGatherPages.insert(hit.outputId, std::clamp(current + step, 0, last));
    synchronizeMinimizedGather();
    synchronizeChrome();
    Q_EMIT shellVisibilityStateChanged();
}

void KWinHybridSession::changeActiveMinimizedGatherPage(const int delta)
{
    if (!ready() || delta == 0 || !m_minimizedGatherPager) {
        return;
    }
    auto *const active = KWin::workspace()->activeWindow();
    const QString outputId = active && active->output()
        ? active->output()->name() : QString{};
    if (m_minimizedGatherPager->pageCount(outputId) < 2) {
        return;
    }
    handleMinimizedPagerHit({
        outputId, delta > 0 ? MinimizedPagerButton::Next
                            : MinimizedPagerButton::Previous});
}

} // namespace QindaQt::Compositor::KWinIntegration
