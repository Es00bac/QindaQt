// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QVector>

#include <functional>
#include <map>
#include <memory>
#include <optional>

namespace KWin {
class ImageItem;
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

enum class MinimizedPagerButton { Previous, Next };

struct MinimizedPagerHit final
{
    QString outputId;
    MinimizedPagerButton button = MinimizedPagerButton::Next;
    friend bool operator==(const MinimizedPagerHit &, const MinimizedPagerHit &) = default;
};

class MinimizedGatherPagerRouter final
{
public:
    using HitTest = std::function<std::optional<MinimizedPagerHit>(const QPointF &)>;

    explicit MinimizedGatherPagerRouter(HitTest hitTest);
    [[nodiscard]] bool pointerPress(const QPointF &position, bool leftButton);
    [[nodiscard]] bool pointerMove(const QPointF &position) const noexcept;
    [[nodiscard]] bool pointerActive() const noexcept;
    [[nodiscard]] std::optional<MinimizedPagerHit> pointerRelease(
        const QPointF &position);
    [[nodiscard]] bool touchDown(qint32 id, const QPointF &position);
    [[nodiscard]] bool touchMotion(qint32 id, const QPointF &position) const noexcept;
    [[nodiscard]] bool touchActive(qint32 id) const noexcept;
    [[nodiscard]] std::optional<MinimizedPagerHit> touchUp(
        qint32 id, const QPointF &position);
    void cancel() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    [[nodiscard]] std::optional<MinimizedPagerHit> release(
        const QPointF &position);

    HitTest m_hitTest;
    std::optional<MinimizedPagerHit> m_pressed;
    std::optional<qint32> m_touchId;
};

class KWinMinimizedGatherPager final
{
public:
    explicit KWinMinimizedGatherPager(ManagedWindowRegistry &registry);
    ~KWinMinimizedGatherPager();

    KWinMinimizedGatherPager(const KWinMinimizedGatherPager &) = delete;
    KWinMinimizedGatherPager &operator=(const KWinMinimizedGatherPager &) = delete;

    [[nodiscard]] bool publish(const QString &outputId,
                               const QString &anchorWindowId,
                               const QRectF &workArea,
                               int currentPage,
                               int pageCount,
                               QString *error = nullptr);
    void remove(const QString &outputId) noexcept;
    void releaseSceneItems() noexcept;
    void clear() noexcept;
    [[nodiscard]] std::optional<MinimizedPagerHit> hitAt(
        const QPointF &position) const;
    [[nodiscard]] int currentPage(const QString &outputId) const noexcept;
    [[nodiscard]] int pageCount(const QString &outputId) const noexcept;

private:
    struct Entry final
    {
        int currentPage = 0;
        int pageCount = 0;
        QRectF frame;
        QRectF previousButton;
        QRectF nextButton;
        QImage image;
        std::unique_ptr<KWin::ImageItem> item;
        QPointer<KWin::Window> anchor;
    };

    [[nodiscard]] bool anchorItem(const QString &windowId, Entry &entry,
                                  QString *error);
    static void render(Entry &entry, const QRectF &workArea);
    static void updateItem(Entry &entry) noexcept;
    static void dropItem(Entry &entry) noexcept;

    ManagedWindowRegistry &m_registry;
    std::map<QString, Entry> m_entries;
};

} // namespace QindaQt::Compositor::KWinIntegration
