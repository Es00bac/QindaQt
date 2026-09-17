// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridiconchiprouter.h"

#include "qindaqt/hybrid_chrome/chromeiconchip.h"

#include <QImage>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <optional>

namespace KWin {
class ImageItem;
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

// Paints one iconified-window chip (ADR-0191) per rolled-up window as a
// paint-only KWin scene ImageItem parented to that window's own WindowItem,
// exactly like shared container chrome (ADR-0005): the chip creates no
// QWindow, input surface, or managed client, so it can only be reached
// through the compositor's own pointer routing. The registry is borrowed
// and must outlive the presenter; everything runs on the compositor thread.
class KWinIconChipPresenter final
{
public:
    explicit KWinIconChipPresenter(ManagedWindowRegistry &registry);
    ~KWinIconChipPresenter();

    KWinIconChipPresenter(const KWinIconChipPresenter &) = delete;
    KWinIconChipPresenter &operator=(const KWinIconChipPresenter &) = delete;

    // Creates or updates the chip for windowId and anchors it to the window's
    // live WindowItem. Fails, keeping the previous entry, when the window or
    // its scene item is unavailable.
    [[nodiscard]] bool publish(const QString &windowId,
                               const HybridChrome::IconChipPlan &plan,
                               QString *error = nullptr);
    void setPointerHover(std::optional<IconChipPointerHit> hit);
    // Presentation visibility (a minimized iconified window shows no chip).
    void setVisible(const QString &windowId, bool visible);
    void remove(const QString &windowId) noexcept;
    // Scene teardown: drop every scene item before KWin dismantles its
    // WindowItem tree, keeping plans so publish() can recreate the items.
    void releaseSceneItems() noexcept;
    void clear() noexcept;

    [[nodiscard]] std::optional<HybridChrome::IconChipPlan> plan(
        const QString &windowId) const;
    [[nodiscard]] bool isVisible(const QString &windowId) const noexcept;
    [[nodiscard]] bool hasSceneItem(const QString &windowId) const noexcept;
    [[nodiscard]] qsizetype count() const noexcept;
    [[nodiscard]] qsizetype visibleAnchoredCount() const noexcept;
    [[nodiscard]] QStringList windowIds() const;

private:
    struct Entry final
    {
        HybridChrome::IconChipPlan plan;
        HybridChrome::IconChipPaintState state;
        QImage image;
        std::unique_ptr<KWin::ImageItem> item;
        QPointer<KWin::Window> anchor;
        bool visible = true;
    };

    [[nodiscard]] bool anchorItem(const QString &windowId, Entry &entry, QString *error);
    static void render(Entry &entry);
    static void updateItem(Entry &entry) noexcept;
    static void dropItem(Entry &entry) noexcept;

    ManagedWindowRegistry &m_registry;
    std::map<QString, Entry> m_entries;
    std::optional<IconChipPointerHit> m_hover;
};

} // namespace QindaQt::Compositor::KWinIntegration
