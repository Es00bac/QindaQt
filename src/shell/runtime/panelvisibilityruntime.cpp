// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilityruntime.h"

#include "panelvisibilityanimation.h"
#include "panelvisibilitypointer.h"
#include "panelvisibilitypopup.h"
#include "panelvisibilityshortcut.h"
#include "panelvisibilitytimer.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QDebug>
#include <QGuiApplication>
#include <QHash>
#include <QMetaType>
#include <QSet>

#include <algorithm>
#include <utility>

namespace QindaQt::Shell {
namespace {

constexpr auto ReducedMotionKey = "accessibility.reducedMotion";
constexpr auto AutoHideDelayKey = "panels.autoHideDelayMs";
constexpr int ReducedMotionMaximumMilliseconds = 80;

bool exactBool(const QVariant &value, bool *result)
{
    if (value.metaType().id() != QMetaType::Bool) {
        return false;
    }
    *result = value.toBool();
    return true;
}

bool boundedDelay(const QVariant &value, int *result)
{
    // AGENT-GUARD: Settings1 canonicalizes every accepted integer to signed
    // 64-bit. Narrow source-language ints make unit fixtures pass while real
    // service snapshots are silently ignored (review finding P1-1).
    if (value.metaType().id() != QMetaType::LongLong) {
        return false;
    }
    const qint64 delay = value.toLongLong();
    if (delay < 0 || delay > 5'000) {
        return false;
    }
    *result = static_cast<int>(delay);
    return true;
}

} // namespace

class PanelVisibilityRuntime::Private final {
public:
    Private(QGuiApplication &application,
            ShellOrchestration::PanelInteractionStore &interactions,
            GlobalShortcutRegistrar &registrar)
        : timer()
        , animationPort()
        , pointer(application, interactions, timer)
        , edge(pointer)
        , popup(application, interactions, timer)
        , shortcut(registrar, interactions, timer)
        , animation(application, interactions, animationPort)
    {
    }

    QtPanelVisibilityTimer timer;
    QtPanelVisibilityAnimation animationPort;
    PanelVisibilityPointerProducer pointer;
    PanelVisibilityEdgeSurfaceProducer edge;
    PanelVisibilityPopupProducer popup;
    PanelVisibilityShortcutProducer shortcut;
    PanelVisibilityAnimationProducer animation;
    QSet<QString> hideablePanelIds;
    int themeMotionDuration = 0;
    int autoHideDelay = 250;
    bool reducedMotion = true;
    QString activeWindowId;
    QString activeWindowOutputId;
    QRect activeWindowFrame;
    bool activeWindowMaximized = false;
    bool activeWindowFullscreenLike = false;
    bool observedForeground = false;
};

PanelVisibilityRuntime::PanelVisibilityRuntime(
    QGuiApplication &application,
    ShellOrchestration::PanelInteractionStore &interactions,
    Services::SettingsClient::SettingsClient &settings,
    GlobalShortcutRegistrar &shortcutRegistrar,
    const Profiles::LayoutProfile &profile, int themeMotionDuration,
    QObject *parent)
    : QObject(parent)
    , m_private(new Private(application, interactions, shortcutRegistrar))
{
    m_private->themeMotionDuration = std::clamp(themeMotionDuration, 0, 1'000);
    // The hide-mode inventory is owned by applyProfile; construction adopts
    // the startup profile through the same path a live layout switch uses.
    applyProfile(profile);
    connect(&settings,
            &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, [this, &settings] {
                if (settings.snapshot()) {
                    applySettings(settings.snapshot()->values);
                }
            });
    // AGENT-GUARD: without this the hide path depends on an accident. The
    // fade-completion callback releases the visibility-hold lease, and that
    // release happens to emit PanelInteractionStore::interactionsChanged,
    // which the shell runtime turns into a debounced reconcile. Nothing
    // declares that dependency, so a future change to the hold's lifetime
    // would silently leave faded-out panels mapped at zero opacity --
    // invisible but still taking input. Forward the producer's own signal.
    connect(&m_private->animation,
            &PanelVisibilityAnimationProducer::reconcileRequested, this,
            &PanelVisibilityRuntime::reconcileRequested);
    if (!m_private->shortcut.registrationRequestAccepted()) {
        qWarning().noquote()
            << "QindaQt shell could not submit the panel reveal shortcut;"
               " edge and popup producers remain available";
    }
}

PanelVisibilityRuntime::~PanelVisibilityRuntime()
{
    delete m_private;
}

bool PanelVisibilityRuntime::synchronize(
    const ShellSurface::PanelSurfacePlan &plan,
    bool compositorAuthorityAvailable, bool *immediateReconcile,
    QString *error)
{
    if (immediateReconcile == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("panel visibility reconcile output is null");
        }
        return false;
    }
    *immediateReconcile = false;
    QVector<ShellVisibility::PanelSurfaceIdentity> hideable;
    for (const auto &surface : plan.surfaces) {
        if (m_private->hideablePanelIds.contains(surface.identity.panelId)) {
            hideable.append({surface.identity.panelId,
                             surface.identity.outputId});
        }
    }
    m_private->pointer.setIdentities(hideable);
    m_private->pointer.setLeaveDelayMilliseconds(m_private->autoHideDelay);
    m_private->popup.setIdentities(hideable);
    m_private->popup.synchronizePopupObjects();
    m_private->shortcut.setIdentities(hideable);
    m_private->shortcut.setHoldMilliseconds(
        std::clamp(m_private->autoHideDelay
                       + animationDurationMilliseconds() + 1'000,
                   1'000, 10'000));
    QString edgeError;
    if (!m_private->edge.synchronize(plan, hideable, &edgeError)) {
        if (error != nullptr) {
            *error = std::move(edgeError);
        }
        return false;
    }
    *immediateReconcile = m_private->animation.synchronize(
        plan, hideable, compositorAuthorityAvailable,
        animationDurationMilliseconds());
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

QStringList PanelVisibilityRuntime::observeForeground(
    const ShellVisibility::CompositorVisibilitySnapshot &snapshot)
{
    QString activeId;
    QString activeOutputId;
    QRect activeFrame;
    bool maximized = false;
    bool fullscreenLike = false;
    for (const auto &window : snapshot.windows) {
        if (!window.active || window.minimized || window.hidden) {
            continue;
        }
        activeId = window.id;
        activeOutputId = window.outputId;
        activeFrame = window.frameGeometry;
        maximized = window.maximized;
        fullscreenLike = window.fullscreen;
        for (const auto &output : snapshot.outputs) {
            const QRect &frame = window.frameGeometry;
            const QRect &screen = output.geometry;
            if (!maximized && frame.left() <= screen.left()
                && frame.top() <= screen.top()
                && frame.right() >= screen.right()
                && frame.bottom() >= screen.bottom()) {
                fullscreenLike = true;
                break;
            }
        }
        break;
    }
    // AGENT-GUARD: If Wayland transfers/grabs the pointer during activation
    // without delivering Leave to the old layer surface, its reveal lease can
    // pin the dock over the new maximized window until shell restart. An
    // assigned output is insufficient here: spanning frames can overlap a
    // panel on another output, and a maximized-to-fullscreen transition must
    // invalidate a reveal even when the window ID does not change.
    QStringList affectedOutputs;
    if (m_private->observedForeground
        && (activeId != m_private->activeWindowId
            || activeOutputId != m_private->activeWindowOutputId
            || (maximized && !m_private->activeWindowMaximized)
            || (fullscreenLike && !m_private->activeWindowFullscreenLike))) {
        auto addOutput = [&affectedOutputs](const QString &id) {
            if (!id.isEmpty() && !affectedOutputs.contains(id)) {
                affectedOutputs.append(id);
            }
        };
        addOutput(m_private->activeWindowOutputId);
        addOutput(activeOutputId);
        for (const auto &output : snapshot.outputs) {
            if (output.geometry.intersects(m_private->activeWindowFrame)
                || output.geometry.intersects(activeFrame)) {
                addOutput(output.id);
            }
        }
        for (const auto &id : affectedOutputs) {
            m_private->pointer.clearReveals(id);
        }
    }
    m_private->activeWindowId = std::move(activeId);
    m_private->activeWindowOutputId = std::move(activeOutputId);
    m_private->activeWindowFrame = activeFrame;
    m_private->activeWindowMaximized = maximized;
    m_private->activeWindowFullscreenLike = fullscreenLike;
    m_private->observedForeground = true;
    return affectedOutputs;
}

void PanelVisibilityRuntime::applyProfile(const Profiles::LayoutProfile &profile)
{
    m_private->hideablePanelIds.clear();
    for (const auto &panel : profile.panels) {
        if (panel.hideMode != Profiles::HideMode::Never) {
            m_private->hideablePanelIds.insert(panel.id);
        }
    }
}

void PanelVisibilityRuntime::applySettings(const QVariantMap &values)
{
    bool reduced = true;
    int delay = 250;
    if (!exactBool(values.value(QLatin1StringView(ReducedMotionKey)), &reduced)
        || !boundedDelay(values.value(QLatin1StringView(AutoHideDelayKey)),
                         &delay)) {
        return;
    }
    m_private->reducedMotion = reduced;
    m_private->autoHideDelay = delay;
}

bool PanelVisibilityRuntime::reducedMotion() const noexcept
{
    return m_private->reducedMotion;
}

int PanelVisibilityRuntime::animationDurationMilliseconds() const noexcept
{
    return m_private->reducedMotion
        ? std::min(m_private->themeMotionDuration,
                   ReducedMotionMaximumMilliseconds)
        : m_private->themeMotionDuration;
}

} // namespace QindaQt::Shell
