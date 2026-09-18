// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridiconifycontroller.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace KWin {
class Item;
class Window;
class WindowItem;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

struct IconifyPlatformCallbacks final
{
    // KWin unhid the window (activation) or one of its hidden transients.
    std::function<void(const QString &windowId)> revealed;
    // The window closed while iconified; its platform state is dropped.
    std::function<void(const QString &windowId)> closed;
    std::function<void(const QString &windowId, bool minimized)> minimizedChanged;
};

// AGENT-CONTRACT: real KWin adapter for HybridIconifyPlatform, the only place
// that touches KWin scene internals for iconified windows (ADR-0203). It uses
// exactly the ADR-0099 shade technique per window: Window::isHidden() removes
// the window from pointer targeting and the focus chain; the WindowItem stays
// paintable through refVisible(PAINT_DISABLED_BY_HIDDEN) so the chip parented
// to it renders; the window's own container/shadow items are hidden
// explicitly; opacity is held below 1.0 against the occlusion ghost. Unlike
// shade, a reveal is reported and never re-applied: activation is an unroll.
// AGENT-NOTE: this deliberately duplicates the shade adapter's mechanics
// instead of sharing them; kwinhybridshade.cpp is not part of this lane's
// lease. Unify them in a later, dedicated change if both survive.
class KWinIconifyPlatform final : public HybridIconifyPlatform
{
public:
    KWinIconifyPlatform(ManagedWindowRegistry &registry, IconifyPlatformCallbacks callbacks);
    ~KWinIconifyPlatform() override;

    [[nodiscard]] bool hideWindow(const QString &windowId, QString *error) override;
    [[nodiscard]] bool showWindow(const QString &windowId, QString *error) override;

private:
    struct AppliedState final
    {
        QPointer<KWin::Window> window;
        QPointer<KWin::WindowItem> forcedItem;
        QPointer<KWin::Item> hiddenContainer;
        QPointer<KWin::Item> hiddenShadow;
        std::optional<qreal> originalOpacity;
        QList<QPointer<KWin::Window>> hiddenTransients;
        QList<QMetaObject::Connection> connections;
    };

    void hideContent(KWin::Window *window, AppliedState &state);
    void lowerOpacity(KWin::Window *window, AppliedState &state);
    void hideTransients(const QString &windowId, KWin::Window *window,
                        AppliedState &state, int depth);
    static void showTransients(const AppliedState &state);
    [[nodiscard]] QMetaObject::Connection watchReveal(const QString &windowId,
                                                      KWin::Window *window);
    void watch(const QString &windowId, KWin::Window *window, AppliedState &state);
    void updateGlobalWatch();
    void handleWindowAdded(KWin::Window *added);
    static void disconnectAll(AppliedState &state);

    ManagedWindowRegistry &m_registry;
    IconifyPlatformCallbacks m_callbacks;
    QHash<QString, AppliedState> m_applied;
    QMetaObject::Connection m_windowAdded;
};

} // namespace QindaQt::Compositor::KWinIntegration
