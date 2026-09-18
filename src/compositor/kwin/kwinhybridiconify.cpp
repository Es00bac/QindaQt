// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridiconchiprouter.h"
#include "hybridiconifycontroller.h"
#include "hybridinteractionruntime.h"
#include "kwinchromescenelifecycle.h"
#include "kwiniconchippresenter.h"
#include "kwiniconifyplatform.h"
#include "managedwindowregistry.h"

#include "qindaqt/hybrid_chrome/chromeiconchip.h"

#include <compositor.h>
#include <core/output.h>
#include <window.h>
#include <workspace.h>

#include <QApplication>
#include <QIcon>
#include <QPixmap>

#include <optional>
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

QImage chipIcon(const KWin::Window *window, qreal devicePixelRatio)
{
    const QIcon &icon = window->icon();
    if (icon.isNull()) {
        return {};
    }
    const int extent = qRound(HybridChrome::ChromeIconChip::IconExtent);
    return icon.pixmap(QSize(extent, extent), devicePixelRatio).toImage();
}

HybridChrome::IconChipRequest chipRequest(const QString &windowId,
                                         const KWin::Window *window,
                                         const QPointF &anchor,
                                         const QRectF &bounds,
                                         const HybridChrome::ChromePalette &palette)
{
    const qreal devicePixelRatio = window->output() ? window->output()->scale() : 1.0;
    HybridChrome::IconChipRequest request;
    request.windowId = windowId;
    request.title = window->caption();
    request.icon = chipIcon(window, devicePixelRatio);
    request.anchor = anchor;
    request.bounds = bounds;
    request.devicePixelRatio = devicePixelRatio;
    request.palette = palette;
    return request;
}

} // namespace

void KWinHybridSession::ensureIconify()
{
    if (m_iconify) {
        return;
    }
    IconifyPlatformCallbacks callbacks;
    callbacks.revealed = [this](const QString &windowId) { handleIconifiedReveal(windowId); };
    callbacks.closed = [this](const QString &windowId) { handleIconifiedClosed(windowId); };
    callbacks.minimizedChanged = [this](const QString &windowId, bool minimized) {
        if (m_iconChips) {
            m_iconChips->setVisible(windowId, !minimized);
        }
    };
    m_iconifyPlatform = std::make_unique<KWinIconifyPlatform>(m_registry, std::move(callbacks));
    m_iconify = std::make_unique<HybridIconifyController>(*m_iconifyPlatform);
    m_iconChips = std::make_unique<KWinIconChipPresenter>(m_registry);
    m_iconChipRouter = std::make_unique<HybridIconChipRouter>(
        [this](const QPointF &position) { return iconChipHitAt(position); },
        QApplication::startDragDistance());
}

bool KWinHybridSession::isWindowIconified(const QString &windowId) const noexcept
{
    return m_iconify && m_iconify->isIconified(windowId);
}

qsizetype KWinHybridSession::iconifiedWindowCount() const noexcept
{
    return m_iconify ? m_iconify->count() : 0;
}

qsizetype KWinHybridSession::visibleIconChipCount() const noexcept
{
    return m_iconChips ? m_iconChips->visibleAnchoredCount() : 0;
}

QRectF KWinHybridSession::iconChipBounds(const QString &windowId) const
{
    auto *const window = m_registry.window(windowId);
    return window && window->output() ? QRectF(window->output()->geometry()) : QRectF();
}

bool KWinHybridSession::iconifyWindow(const QString &windowId, QString *error)
{
    if (!ready() || !m_iconify || !m_iconChips) {
        return fail(error, QStringLiteral("Hybrid session is not ready"));
    }
    auto *const window = m_registry.window(windowId);
    if (!window || window->isDeleted()) {
        return fail(error, QStringLiteral("window '%1' is unavailable").arg(windowId));
    }
    if (m_iconify->isIconified(windowId)) {
        return true;
    }
    if (!m_registry.owner(windowId).isEmpty() || !m_runtime->topology().isIndependent(windowId)) {
        return fail(error, QStringLiteral("only an independent window can roll up to its icon"));
    }
    if (window->isMinimized()) {
        return fail(error, QStringLiteral("a minimized window cannot roll up to its icon"));
    }
    if (!window->windowItem()) {
        return fail(error, QStringLiteral("window '%1' has no scene item").arg(windowId));
    }
    const QRectF frame = window->frameGeometry();
    const QRectF bounds = iconChipBounds(windowId);
    // The chip anchors at the title bar's leading edge: the frame's top-left.
    const auto plan = HybridChrome::ChromeIconChip::layout(
        chipRequest(windowId, window, frame.topLeft(), bounds, m_chromeStyle.palette), error);
    if (!plan) {
        return false;
    }
    const bool wasActive = window->isActive();
    if (!m_iconify->iconify(windowId, frame, plan->frame, wasActive, error)) {
        return false;
    }
    if (!m_iconChips->publish(windowId, *plan, error)) {
        QString restoreError;
        static_cast<void>(m_iconify->restore(windowId, &restoreError));
        return false;
    }
    // A hidden window must not keep keyboard focus; the focus chain never
    // selects a hidden window.
    auto *const workspace = KWin::workspace();
    if (wasActive && workspace && workspace->activeWindow() == window) {
        workspace->activateNextWindow(window);
    }
    Q_EMIT shellVisibilityStateChanged();
    return true;
}

bool KWinHybridSession::restoreIconifiedWindow(const QString &windowId,
                                               bool activate,
                                               QString *error)
{
    if (!m_iconify || !m_iconify->isIconified(windowId)) {
        return fail(error, QStringLiteral("window '%1' is not iconified").arg(windowId));
    }
    const auto record = m_iconify->restore(windowId, error);
    if (m_iconChips) {
        m_iconChips->remove(windowId);
    }
    m_iconChipDragBaselines.remove(windowId);
    if (!record) {
        return false;
    }
    auto *const window = m_registry.window(windowId);
    auto *const workspace = KWin::workspace();
    if (window && !window->isDeleted()) {
        // AGENT-CONTRACT: the chip may have been dragged; the window reappears
        // with its title bar under the chip, at its original size.
        if (QRectF(window->frameGeometry()) != record->restoreFrame) {
            window->moveResize(record->restoreFrame);
        }
        if (activate && workspace) {
            workspace->activateWindow(window);
        }
    }
    Q_EMIT shellVisibilityStateChanged();
    return true;
}

void KWinHybridSession::handleIconifiedReveal(const QString &windowId)
{
    if (!m_iconify) {
        return;
    }
    const auto record = m_iconify->revealed(windowId);
    if (!record) {
        return;
    }
    if (m_iconChips) {
        m_iconChips->remove(windowId);
    }
    m_iconChipDragBaselines.remove(windowId);
    auto *const window = m_registry.window(windowId);
    if (window && !window->isDeleted()
        && QRectF(window->frameGeometry()) != record->restoreFrame) {
        window->moveResize(record->restoreFrame);
    }
    Q_EMIT shellVisibilityStateChanged();
}

void KWinHybridSession::handleIconifiedClosed(const QString &windowId)
{
    if (m_iconify) {
        m_iconify->windowClosed(windowId);
    }
    if (m_iconChips) {
        m_iconChips->remove(windowId);
    }
    m_iconChipDragBaselines.remove(windowId);
    Q_EMIT shellVisibilityStateChanged();
}

bool KWinHybridSession::publishIconChip(const QString &windowId, QString *error)
{
    auto *const window = m_registry.window(windowId);
    const auto record = m_iconify ? m_iconify->record(windowId) : std::nullopt;
    if (!window || window->isDeleted() || !record || !m_iconChips) {
        return fail(error, QStringLiteral("window '%1' is not iconified").arg(windowId));
    }
    const QRectF bounds = iconChipBounds(windowId);
    const auto plan = HybridChrome::ChromeIconChip::layout(
        chipRequest(windowId, window, record->chipFrame.topLeft(), bounds,
                    m_chromeStyle.palette), error);
    if (!plan) {
        return false;
    }
    if (plan->frame.topLeft() != record->chipFrame.topLeft()) {
        // The output shrank or moved: keep the recorded chip where it is painted.
        QString relocateError;
        static_cast<void>(m_iconify->relocateChip(windowId, plan->frame.topLeft(), bounds,
                                                  &relocateError));
    }
    return m_iconChips->publish(windowId, *plan, error);
}

void KWinHybridSession::synchronizeIconChips()
{
    if (!m_iconify || !m_iconChips || m_iconify->count() == 0) {
        return;
    }
    if (m_chromeSceneLifecycle && !m_chromeSceneLifecycle->sceneAvailable()) {
        return;
    }
    const auto ids = m_iconify->iconifiedWindowIds();
    for (const auto &windowId : ids) {
        auto *const window = m_registry.window(windowId);
        if (!window || window->isDeleted()) {
            m_iconify->windowClosed(windowId);
            m_iconChips->remove(windowId);
            continue;
        }
        QString error;
        if (!m_iconChips->hasSceneItem(windowId)) {
            // A scene restart recreated every WindowItem: re-apply the hide
            // treatment to the new item before anchoring a fresh chip to it.
            if (!m_iconify->reapply(windowId, &error)) {
                qWarning("QindaQt could not re-hide iconified '%s' after a scene restart: %s",
                         qPrintable(windowId), qPrintable(error));
            }
        }
        if (!publishIconChip(windowId, &error)) {
            qWarning("QindaQt could not publish the chip for '%s': %s",
                     qPrintable(windowId), qPrintable(error));
        }
    }
}

void KWinHybridSession::releaseIconChipSceneItems() noexcept
{
    if (m_iconChips) {
        m_iconChips->releaseSceneItems();
    }
}

void KWinHybridSession::restoreIconifiedForShutdown()
{
    if (!m_iconify) {
        return;
    }
    QString error;
    const auto records = m_iconify->restoreAll(&error);
    if (!error.isEmpty()) {
        qWarning("QindaQt iconify teardown could not restore every window: %s",
                 qPrintable(error));
    }
    for (const auto &record : records) {
        auto *const window = m_registry.window(record.windowId);
        if (window && !window->isDeleted()
            && QRectF(window->frameGeometry()) != record.restoreFrame) {
            window->moveResize(record.restoreFrame);
        }
    }
    if (m_iconChips) {
        m_iconChips->clear();
    }
    m_iconChipDragBaselines.clear();
}

QJsonArray KWinHybridSession::iconifiedWindowsJson() const
{
    QJsonArray result;
    if (!m_iconify) {
        return result;
    }
    const auto rectJson = [](const QRectF &rect) {
        return QJsonObject{{QStringLiteral("x"), rect.x()},
                           {QStringLiteral("y"), rect.y()},
                           {QStringLiteral("width"), rect.width()},
                           {QStringLiteral("height"), rect.height()}};
    };
    for (const auto &windowId : m_iconify->iconifiedWindowIds()) {
        const auto record = m_iconify->record(windowId);
        if (!record) {
            continue;
        }
        result.append(QJsonObject{
            {QStringLiteral("windowId"), windowId},
            {QStringLiteral("chipFrame"), rectJson(record->chipFrame)},
            {QStringLiteral("restoreFrame"), rectJson(record->restoreFrame)},
            {QStringLiteral("chipVisible"), m_iconChips && m_iconChips->isVisible(windowId)},
        });
    }
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
