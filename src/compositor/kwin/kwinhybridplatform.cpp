// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene.h"

#include "managedwindowregistry.h"

#include <effect/globals.h>
#include <virtualdesktops.h>
#include <window.h>
#include <workspace.h>

#include <QList>
#include <QUuid>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

using HybridConstraints::MemberSizeConstraints;
using HybridConstraints::WindowRestoreState;

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

QString windowFocusToken(const KWin::Window *window)
{
    return window ? window->internalId().toString(QUuid::WithoutBraces) : QString{};
}

class RegistryHybridPlatform final : public KWinHybridScenePlatform
{
public:
    explicit RegistryHybridPlatform(ManagedWindowRegistry &registry)
        : m_registry(registry)
    {
    }

    QStringList windowIds() const override { return m_registry.windowIds(); }

    bool windowExists(const QString &windowId) const override
    {
        return m_registry.window(windowId) != nullptr;
    }

    QString owner(const QString &windowId) const override
    {
        return m_registry.owner(windowId);
    }

    std::optional<QRectF> currentFrame(
        const QString &windowId, QString *error) const override
    {
        const auto frame = m_registry.targetFrame(windowId);
        if (!frame.isValid()) {
            fail(error, QStringLiteral("window '%1' has no valid frame").arg(windowId));
            return std::nullopt;
        }
        return frame;
    }

    std::optional<QRectF> placementArea(
        const QString &windowId,
        const QString &outputId,
        QString *error) const override
    {
        auto *window = m_registry.window(windowId);
        auto *output = KWin::workspace()->findOutput(outputId);
        if (!window) {
            fail(error, QStringLiteral("unknown managed window '%1'").arg(windowId));
            return std::nullopt;
        }
        if (!output) {
            fail(error, QStringLiteral("unknown output '%1'").arg(outputId));
            return std::nullopt;
        }
        const QRectF area = KWin::workspace()->clientArea(
            KWin::MaximizeArea, window, output);
        if (!area.isValid()) {
            fail(error, QStringLiteral("output '%1' has no valid work area").arg(outputId));
            return std::nullopt;
        }
        return area;
    }

    std::optional<WindowRestoreState> captureState(
        const QString &windowId, QString *error) const override
    {
        auto *window = m_registry.window(windowId);
        if (!window) {
            fail(error, QStringLiteral("unknown managed window '%1'").arg(windowId));
            return std::nullopt;
        }
        QRectF restoreFrame = m_registry.targetFrame(windowId);
        if (window->isFullScreen() && window->fullscreenGeometryRestore().isValid()) {
            restoreFrame = window->fullscreenGeometryRestore();
        } else if ((window->maximizeMode() != KWin::MaximizeRestore
                    || bool(window->quickTileMode()))
                   && window->geometryRestore().isValid()) {
            restoreFrame = window->geometryRestore();
        }
        WindowRestoreState result{
            .geometry = restoreFrame,
            .minimized = window->isMinimized(),
            .maximizedAxes = maximizeAxes(window->maximizeMode()),
            .quickTileEdges = quickTileEdges(window->quickTileMode()),
            .fullscreen = window->isFullScreen(),
            .outputId = window->output() ? window->output()->name() : QString{},
            .desktopIds = window->desktopIds(),
            .activityIds = window->activities(),
            .keepAbove = window->keepAbove(),
            .keepBelow = window->keepBelow(),
            .focused = window->isActive(),
            .skipTaskbar = window->skipTaskbar(),
            .skipSwitcher = window->skipSwitcher(),
        };
        if (!result.isValid(error)) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<MemberSizeConstraints> sizeConstraints(
        const QString &windowId, QString *error) const override
    {
        auto *window = m_registry.window(windowId);
        if (!window) {
            fail(error, QStringLiteral("unknown managed window '%1'").arg(windowId));
            return std::nullopt;
        }
        const auto margins = window->frameMargins();
        const auto minimum = window->minSize();
        MemberSizeConstraints result;
        result.minimumSize = QSize(
            boundedSize(minimum.width() + margins.left() + margins.right(), true),
            boundedSize(minimum.height() + margins.top() + margins.bottom(), true));
        if (!window->isResizable()) {
            const auto frame = m_registry.targetFrame(windowId).size();
            result.fixedSize = QSize(boundedSize(frame.width(), false),
                                     boundedSize(frame.height(), false));
        } else {
            const auto maximum = window->maxSize();
            const auto maximumWidth = boundedMaximum(
                maximum.width(), margins.left() + margins.right());
            const auto maximumHeight = boundedMaximum(
                maximum.height(), margins.top() + margins.bottom());
            if (maximumWidth.has_value() || maximumHeight.has_value()) {
                result.maximumSize = QSize(
                    maximumWidth.value_or(std::numeric_limits<int>::max()),
                    maximumHeight.value_or(std::numeric_limits<int>::max()));
            }
        }
        if (!result.isValid(error)) {
            return std::nullopt;
        }
        return result;
    }

    bool validateState(const QString &windowId,
                       const WindowRestoreState &state,
                       QString *error) const override
    {
        if (!windowExists(windowId)) {
            return fail(error, QStringLiteral("unknown managed window '%1'").arg(windowId));
        }
        if (!state.isValid(error)) {
            return false;
        }
        if (state.isMaximized() && state.isQuickTiled()) {
            return fail(error, QStringLiteral(
                                   "KWin cannot restore maximize and quick-tile together"));
        }
        if (!state.outputId.isEmpty() && !KWin::workspace()->findOutput(state.outputId)) {
            return fail(error, QStringLiteral("unknown output '%1'").arg(state.outputId));
        }
        for (const auto &desktopId : state.desktopIds) {
            if (!KWin::VirtualDesktopManager::self()->desktopForId(desktopId)) {
                return fail(error, QStringLiteral("unknown desktop '%1'").arg(desktopId));
            }
        }
        return true;
    }

    bool applyState(const QString &windowId,
                    const WindowRestoreState &state,
                    QString *error) override
    {
        auto *window = m_registry.window(windowId);
        if (!window || !validateState(windowId, state, error)) {
            return false;
        }
        QList<KWin::VirtualDesktop *> desktops;
        for (const auto &desktopId : state.desktopIds) {
            desktops.append(KWin::VirtualDesktopManager::self()->desktopForId(desktopId));
        }

        // AGENT-GUARD: Publish task identity before any unminimize signal so
        // KWin's task/switcher consumers never observe a revealed secondary.
        // The session mutes task-policy callbacks for this whole scene apply;
        // an ordinary external unminimize remains routed by that policy.
        window->setSkipTaskbar(state.skipTaskbar);
        window->setSkipSwitcher(state.skipSwitcher);
        window->setMinimized(false);
        window->setFullScreen(false);
        window->setQuickTileMode({}, state.geometry.center());
        window->maximize(KWin::MaximizeRestore, state.geometry);
        if (!state.outputId.isEmpty()) {
            window->sendToOutput(KWin::workspace()->findOutput(state.outputId));
        }
        window->setDesktops(desktops);
        window->setOnActivities(state.activityIds);
        window->setKeepAbove(false);
        window->setKeepBelow(false);
        window->setKeepAbove(state.keepAbove);
        window->setKeepBelow(state.keepBelow);
        window->moveResize(state.geometry);
        window->maximize(kwinMaximizeMode(state.maximizedAxes), state.geometry);
        if (state.isQuickTiled()) {
            window->setQuickTileMode(kwinQuickTileMode(state.quickTileEdges),
                                     state.geometry.center());
        }
        window->setFullScreen(state.fullscreen);
        window->setMinimized(state.minimized);
        return true;
    }

    QString activeWindowId() const override
    {
        return windowFocusToken(KWin::workspace()->activeWindow());
    }

    bool activateWindow(const QString &windowId, QString *error) override
    {
        auto *window = m_registry.window(windowId);
        if (!window) {
            return fail(error,
                        QStringLiteral("cannot focus closed window '%1'").arg(windowId));
        }
        KWin::workspace()->activateWindow(window, true);
        return true;
    }

    bool restoreFocus(const QString &focusToken, QString *error) override
    {
        if (focusToken.isEmpty()) {
            KWin::workspace()->resetFocus();
            return true;
        }
        if (auto *window = focusWindow(focusToken)) {
            KWin::workspace()->activateWindow(window, true);
            return true;
        }

        // The captured target may legitimately close while a synchronous
        // transaction applies member state. Preserve the prior rollback result
        // by restoring an unfocused workspace in that case.
        Q_UNUSED(error);
        KWin::workspace()->resetFocus();
        return true;
    }

    bool finalizeOwners(
        const QHash<QString, QString> &expectedOwners,
        const QHash<QString, QString> &candidateOwners,
        const QHash<QString, QRectF> &targetFrames,
        const QSet<QString> &allowedMissingWindowIds,
        QString *error) override
    {
        return m_registry.transitionTopologyOwners(
            expectedOwners, candidateOwners, targetFrames, allowedMissingWindowIds, error);
    }

private:
    [[nodiscard]] KWin::Window *focusWindow(const QString &focusToken) const
    {
        // AGENT-GUARD: Dialogs and transients are intentionally absent from
        // ManagedWindowRegistry but remain valid rollback focus targets. Keep
        // this resolver workspace-wide; m_registry.window() would turn those
        // live tokens into resetFocus() requests.
        for (auto *window : KWin::workspace()->windows()) {
            if (window && !window->isDeleted()
                && windowFocusToken(window) == focusToken) {
                return window;
            }
        }
        return nullptr;
    }

    static int boundedSize(qreal value, bool roundUp)
    {
        if (!std::isfinite(value) || value >= std::numeric_limits<int>::max()) {
            return std::numeric_limits<int>::max();
        }
        return std::max(0, roundUp ? int(std::ceil(value)) : qRound(value));
    }

    static std::optional<int> boundedMaximum(qreal client, int frameMargins)
    {
        constexpr qreal unbounded = qreal(std::numeric_limits<int>::max()) / 4.0;
        if (!std::isfinite(client) || client <= 0.0 || client >= unbounded) {
            return std::nullopt;
        }
        return std::max(0, int(std::floor(client + frameMargins)));
    }

    static HybridConstraints::MaximizeAxes maximizeAxes(KWin::MaximizeMode mode)
    {
        const int bits = ((int(mode) & int(KWin::MaximizeHorizontal)) ? 0x1 : 0)
            | ((int(mode) & int(KWin::MaximizeVertical)) ? 0x2 : 0);
        return HybridConstraints::MaximizeAxes::fromInt(
            static_cast<HybridConstraints::MaximizeAxes::Int>(bits));
    }

    static KWin::MaximizeMode kwinMaximizeMode(HybridConstraints::MaximizeAxes axes)
    {
        const int result = (axes.testFlag(HybridConstraints::MaximizeAxis::Horizontal)
                                ? int(KWin::MaximizeHorizontal)
                                : 0)
            | (axes.testFlag(HybridConstraints::MaximizeAxis::Vertical)
                   ? int(KWin::MaximizeVertical)
                   : 0);
        return static_cast<KWin::MaximizeMode>(result);
    }

    static HybridConstraints::QuickTileEdges quickTileEdges(KWin::QuickTileMode mode)
    {
        return HybridConstraints::QuickTileEdges::fromInt(
            static_cast<HybridConstraints::QuickTileEdges::Int>(mode.toInt() & 0x0f));
    }

    static KWin::QuickTileMode kwinQuickTileMode(
        HybridConstraints::QuickTileEdges edges)
    {
        return KWin::QuickTileMode::fromInt(
            static_cast<KWin::QuickTileMode::Int>(edges.toInt()));
    }

    ManagedWindowRegistry &m_registry;
};

} // namespace

std::unique_ptr<KWinHybridScenePlatform> makeRegistryHybridScenePlatform(
    ManagedWindowRegistry &registry)
{
    return std::make_unique<RegistryHybridPlatform>(registry);
}

} // namespace QindaQt::Compositor::KWinIntegration
