// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilityanimation.h"

#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QGuiApplication>
#include <QHash>
#include <QPointer>
#include <QPropertyAnimation>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QWindow>

#include <map>
#include <memory>
#include <utility>

namespace QindaQt::Shell {
namespace {

using Identity = ShellVisibility::PanelSurfaceIdentity;
using Lease = ShellOrchestration::PanelInteractionLease;

QString identityKey(const Identity &identity)
{
    return identity.outputId + QChar(0x1f) + identity.panelId;
}

Identity visibilityIdentity(const ShellSurface::PanelSurfaceIdentity &identity)
{
    return {identity.panelId, identity.outputId};
}

QString panelWindowName(const Identity &identity)
{
    return QStringLiteral("qindaqt-panel-%1@%2")
        .arg(identity.panelId, identity.outputId);
}

QWindow *findPanelWindow(QGuiApplication &application, const Identity &identity)
{
    const QString name = panelWindowName(identity);
    const auto windows = application.allWindows();
    for (QWindow *window : windows) {
        if (window != nullptr && window->objectName() == name) {
            return window;
        }
    }
    return nullptr;
}

// AGENT-NOTE: fade the scene root, not the window. Qt's Wayland platform
// implements no window opacity - QWaylandWindow has no setOpacity override, so
// QWindow::setOpacity() reaches QPlatformWindow's base implementation, warns
// "This plugin does not support setting window opacity", and changes nothing.
// The panel's pixels live in the QQuickWindow scene graph, where opacity is
// honoured, so the content item is the target that actually fades. Verified on
// Qt 6.11.1 against the session compositor. See ADR-0236.
//
// The QWindow fallback is not dead code: it is what an X11 session and the
// non-Quick test doubles use, and both support window opacity.
QObject *fadeTarget(QWindow &window)
{
    if (auto *const quick = qobject_cast<QQuickWindow *>(&window)) {
        if (QQuickItem *const root = quick->contentItem()) {
            return root;
        }
    }
    return &window;
}

void setFadeOpacity(QWindow &window, qreal value)
{
    fadeTarget(window)->setProperty("opacity", value);
}

} // namespace

class QtPanelVisibilityAnimation::Private final {
public:
    QHash<QWindow *, QPropertyAnimation *> running;
};

QtPanelVisibilityAnimation::QtPanelVisibilityAnimation(QObject *parent)
    : QObject(parent)
    , m_private(new Private)
{
}

QtPanelVisibilityAnimation::~QtPanelVisibilityAnimation()
{
    const auto animations = m_private->running.values();
    for (QPropertyAnimation *animation : animations) {
        delete animation;
    }
    delete m_private;
}

void QtPanelVisibilityAnimation::animate(
    QWindow &window, qreal from, qreal to, int durationMilliseconds,
    std::function<void()> completed)
{
    cancel(window);
    setFadeOpacity(window, from);
    if (durationMilliseconds <= 0) {
        setFadeOpacity(window, to);
        if (completed) {
            completed();
        }
        return;
    }
    // Both QQuickItem and QWindow expose a qreal "opacity" property, so the
    // animation is identical either way; only the target object differs.
    auto *const animation =
        new QPropertyAnimation(fadeTarget(window), "opacity", this);
    animation->setStartValue(from);
    animation->setEndValue(to);
    animation->setDuration(durationMilliseconds);
    m_private->running.insert(&window, animation);
    connect(animation, &QPropertyAnimation::finished, this,
            [this, guarded = QPointer<QWindow>(&window),
             callback = std::move(completed)]() mutable {
                if (guarded) {
                    if (QPropertyAnimation *const item =
                            m_private->running.take(guarded.data())) {
                        item->deleteLater();
                    }
                }
                if (callback) {
                    callback();
                }
            });
    animation->start();
}

void QtPanelVisibilityAnimation::cancel(QWindow &window)
{
    if (QPropertyAnimation *const animation = m_private->running.take(&window)) {
        animation->stop();
        delete animation;
    }
}

void QtPanelVisibilityAnimation::restore(QWindow &window)
{
    cancel(window);
    setFadeOpacity(window, 1.0);
}

class PanelVisibilityAnimationProducer::Private final {
public:
    enum class Phase { Settled, Hiding, CommitHide, Revealing };
    struct State {
        Identity identity;
        ShellSurface::PanelSurfaceMapping mapping =
            ShellSurface::PanelSurfaceMapping::Mapped;
        Phase phase = Phase::Settled;
        QPointer<QWindow> window;
        std::optional<Lease> hold;
    };

    QGuiApplication &application;
    ShellOrchestration::PanelInteractionStore &interactions;
    PanelVisibilityAnimationPort &animation;
    std::map<QString, State> states;
};

PanelVisibilityAnimationProducer::PanelVisibilityAnimationProducer(
    QGuiApplication &application,
    ShellOrchestration::PanelInteractionStore &interactions,
    PanelVisibilityAnimationPort &animation, QObject *parent)
    : QObject(parent)
    , m_private(new Private{application, interactions, animation, {}})
{
}

PanelVisibilityAnimationProducer::~PanelVisibilityAnimationProducer()
{
    delete m_private;
}

bool PanelVisibilityAnimationProducer::synchronize(
    const ShellSurface::PanelSurfacePlan &plan,
    const QVector<Identity> &hideable, bool authorityAvailable,
    int durationMilliseconds)
{
    QSet<QString> admitted;
    for (const Identity &identity : hideable) {
        admitted.insert(identityKey(identity));
    }
    bool immediateReconcile = false;
    for (const auto &surface : plan.surfaces) {
        const Identity identity = visibilityIdentity(surface.identity);
        const QString key = identityKey(identity);
        if (!admitted.contains(key)) {
            continue;
        }
        auto [iterator, inserted] = m_private->states.try_emplace(
            key, Private::State{identity, surface.mapping,
                                Private::Phase::Settled, nullptr,
                                std::nullopt});
        auto &state = iterator->second;
        QWindow *const window = findPanelWindow(m_private->application,
                                                identity);
        if (window != nullptr) {
            state.window = window;
        }
        if (!authorityAvailable) {
            if (state.window) {
                m_private->animation.restore(*state.window);
            }
            state.hold.reset();
            state.phase = Private::Phase::Settled;
            state.mapping = ShellSurface::PanelSurfaceMapping::Mapped;
            continue;
        }
        if (inserted || durationMilliseconds <= 0 || !state.window) {
            state.mapping = surface.mapping;
            state.phase = Private::Phase::Settled;
            if (state.window) {
                m_private->animation.restore(*state.window);
            }
            continue;
        }
        if (state.phase == Private::Phase::Hiding) {
            continue;
        }
        if (state.phase == Private::Phase::CommitHide) {
            if (surface.mapping == ShellSurface::PanelSurfaceMapping::Unmapped) {
                m_private->animation.restore(*state.window);
                state.mapping = surface.mapping;
                state.phase = Private::Phase::Settled;
                continue;
            }
            state.phase = Private::Phase::Settled;
            state.mapping = ShellSurface::PanelSurfaceMapping::Unmapped;
        }
        if (state.mapping == surface.mapping) {
            continue;
        }
        if (surface.mapping == ShellSurface::PanelSurfaceMapping::Unmapped) {
            QString error;
            state.hold = m_private->interactions.acquire(
                identity,
                ShellOrchestration::PanelInteractionKind::VisibilityHold,
                &error);
            if (!state.hold) {
                state.mapping = surface.mapping;
                continue;
            }
            state.phase = Private::Phase::Hiding;
            m_private->animation.animate(
                *state.window, 1.0, 0.0, durationMilliseconds,
                [this, key] {
                    const auto item = m_private->states.find(key);
                    if (item == m_private->states.end()) {
                        return;
                    }
                    item->second.phase = Private::Phase::CommitHide;
                    item->second.hold.reset();
                    Q_EMIT reconcileRequested();
                });
            immediateReconcile = true;
            continue;
        }
        state.mapping = surface.mapping;
        state.phase = Private::Phase::Revealing;
        m_private->animation.animate(
            *state.window, 0.0, 1.0, durationMilliseconds,
            [this, key] {
                const auto item = m_private->states.find(key);
                if (item != m_private->states.end()) {
                    item->second.phase = Private::Phase::Settled;
                    item->second.hold.reset();
                }
            });
    }
    for (auto item = m_private->states.begin(); item != m_private->states.end();) {
        if (!admitted.contains(item->first)) {
            if (item->second.window) {
                // A panel leaving the admitted set must not be dropped
                // mid-fade: restore, or it stays part-transparent forever.
                m_private->animation.restore(*item->second.window);
            }
            item = m_private->states.erase(item);
        } else {
            ++item;
        }
    }
    return immediateReconcile;
}

} // namespace QindaQt::Shell
