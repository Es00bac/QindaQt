// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinsceneadapter.h"

#include "layoutgeometry.h"
#include "managedwindowregistry.h"

#include <effect/globals.h>
#include <window.h>
#include <workspace.h>

#include <QPointer>
#include <QVector>

#include <functional>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

struct WindowChange final
{
    QString id;
    QPointer<KWin::Window> window;
    QRectF originalFrame;
    bool originalMinimized = false;
    QRectF targetFrame;
    bool targetMinimized = false;
};

class KWinSceneTransaction final : public SceneTransaction
{
public:
    KWinSceneTransaction(QVector<WindowChange> changes,
                         std::function<bool(QString *)> finalize)
        : m_changes(std::move(changes))
        , m_finalize(std::move(finalize))
    {
    }

    bool commit(QString *error) override
    {
        for (const auto &change : m_changes) {
            if (!change.window) {
                if (error) {
                    *error = QStringLiteral("window '%1' closed before scene commit").arg(change.id);
                }
                return false;
            }
        }
        qsizetype applied = 0;
        for (const auto &change : m_changes) {
            if (!change.window) {
                rollback(applied);
                if (error) {
                    *error = QStringLiteral("window '%1' closed during scene commit")
                                 .arg(change.id);
                }
                return false;
            }
            if (change.targetMinimized) {
                // KWin ignores or defers frame requests made after minimizing
                // a Wayland client. Anchor inactive pages first, then hide
                // them in the same compositor turn so tab activation cannot
                // resurrect a distant pre-container frame.
                change.window->moveResize(change.targetFrame);
                change.window->setMinimized(true);
            } else {
                change.window->setMinimized(false);
                change.window->moveResize(change.targetFrame);
            }
            ++applied;
        }
        if (!m_finalize(error)) {
            rollback(applied);
            return false;
        }
        return true;
    }

private:
    void rollback(qsizetype applied)
    {
        while (applied > 0) {
            const auto &change = m_changes[--applied];
            if (!change.window) {
                continue;
            }
            // AGENT-GUARD: KWin retains frame geometry while minimized. A
            // failed transaction must restore that hidden frame as well as the
            // minimized bit or a later unminimize reveals rejected geometry.
            change.window->setMinimized(false);
            change.window->moveResize(change.originalFrame);
            change.window->setMinimized(change.originalMinimized);
        }
    }

    QVector<WindowChange> m_changes;
    std::function<bool(QString *)> m_finalize;
};

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

KWinSceneAdapter::KWinSceneAdapter(ManagedWindowRegistry &registry)
    : m_registry(registry)
{
}

QRectF KWinSceneAdapter::outerFrame(const Core::WindowContainer &before,
                                    const Core::WindowContainer &after) const
{
    QRectF result;
    // AGENT-GUARD: Inactive pages are minimized but remain managed. Deriving
    // the outer frame from every page would let a stale/distant hidden frame
    // expand the group when tabs activate. Only the visible page defines the
    // current container frame.
    auto ids = LayoutGeometryPlanner::activeWindowIds(before);
    // AGENT-CONTRACT: Docking collapses the incoming client into the existing
    // container's outer frame. Including a distant new member here would span
    // the group across monitors instead of anchoring it to the target window.
    if (ids.isEmpty()) {
        ids = LayoutGeometryPlanner::activeWindowIds(after);
    }
    for (const auto &id : ids) {
        if (const auto *window = m_registry.window(id)) {
            const QRectF frame = window->frameGeometry();
            result = result.isValid() ? result.united(frame) : frame;
        }
    }
    if (result.isValid()) {
        return result;
    }
    const auto *active = KWin::workspace()->activeWindow();
    return active ? KWin::workspace()->clientArea(KWin::MaximizeArea, active)
                  : KWin::workspace()->geometry();
}

std::unique_ptr<SceneTransaction> KWinSceneAdapter::prepareTransition(
    const Core::WindowContainer &before,
    const Core::WindowContainer &after,
    QString *error)
{
    const auto beforeIds = LayoutGeometryPlanner::windowIds(before);
    const auto afterIds = LayoutGeometryPlanner::windowIds(after);
    for (const auto &id : afterIds) {
        if (!m_registry.window(id)) {
            fail(error, QStringLiteral("unknown or closed managed window '%1'").arg(id));
            return nullptr;
        }
        const auto currentOwner = m_registry.owner(id);
        if (!currentOwner.isEmpty() && currentOwner != after.id()) {
            fail(error,
                 QStringLiteral("window '%1' belongs to container '%2'")
                     .arg(id, currentOwner));
            return nullptr;
        }
    }

    const auto geometry = LayoutGeometryPlanner::plan(after, outerFrame(before, after));
    QVector<WindowChange> changes;
    changes.reserve(afterIds.size() + beforeIds.size());
    QHash<QString, QRectF> stagedRestoreFrames;
    for (const auto &id : afterIds) {
        auto *window = m_registry.window(id);
        if (!m_restoreFrames.contains(id)) {
            stagedRestoreFrames.insert(id, window->frameGeometry());
        }
        changes.append({id,
                        window,
                        window->frameGeometry(),
                        window->isMinimized(),
                        geometry.frames.value(id, window->frameGeometry()),
                        !geometry.visibleWindows.contains(id)});
    }
    const auto removedIds = beforeIds - afterIds;
    for (const auto &id : removedIds) {
        if (auto *window = m_registry.window(id)) {
            changes.append({id,
                            window,
                            window->frameGeometry(),
                            window->isMinimized(),
                            m_restoreFrames.value(id, window->frameGeometry()),
                            false});
        }
    }

    // AGENT-CONTRACT: Ownership changes only after every KWin object has been
    // validated and every geometry mutation has been staged. The bridge then
    // publishes the matching model revision in the same event-loop turn.
    auto finalize = [this, containerId = after.id(), afterIds, removedIds,
                     targetFrames = geometry.frames,
                     stagedRestoreFrames = std::move(stagedRestoreFrames)](QString *finalizeError) {
        if (!m_registry.transitionOwners(containerId, afterIds, removedIds,
                                         targetFrames, finalizeError)) {
            return false;
        }
        for (auto iterator = stagedRestoreFrames.cbegin();
             iterator != stagedRestoreFrames.cend(); ++iterator) {
            m_restoreFrames.insert(iterator.key(), iterator.value());
        }
        for (const auto &id : removedIds) {
            m_restoreFrames.remove(id);
        }
        return true;
    };
    return std::make_unique<KWinSceneTransaction>(std::move(changes), std::move(finalize));
}

} // namespace QindaQt::Compositor::KWinIntegration
