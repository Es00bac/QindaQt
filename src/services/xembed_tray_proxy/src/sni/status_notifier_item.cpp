// SPDX-License-Identifier: GPL-3.0-or-later

#include "status_notifier_item.h"

#include <qindaqt/services/xembed_tray_proxy/tray_icon_image.h>

namespace QindaQt::XEmbedTray
{

namespace {

constexpr qsizetype kMaxTextUtf8Bytes = 256;

[[nodiscard]] QString boundedText(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    if (utf8.size() <= kMaxTextUtf8Bytes) {
        return text;
    }
    // Truncate to the budget without splitting a UTF-8 sequence: drop the
    // trailing run of continuation bytes and the lead byte that owns them.
    qsizetype end = kMaxTextUtf8Bytes;
    while (end > 0
           && (static_cast<unsigned char>(utf8.at(end - 1)) & 0xC0) == 0x80) {
        --end;
    }
    if (end > 0 && (static_cast<unsigned char>(utf8.at(end - 1)) & 0x80) != 0) {
        --end;
    }
    return QString::fromUtf8(utf8.left(end));
}

} // namespace

StatusNotifierItem::StatusNotifierItem(quint32 windowId, QObject *parent)
    : QObject(parent)
    , m_windowId(windowId)
{
}

QString StatusNotifierItem::identity() const
{
    if (!m_title.isEmpty()) {
        return m_title;
    }
    return QStringLiteral("xembed-%1").arg(m_windowId);
}

void StatusNotifierItem::setTitle(const QString &title)
{
    const QString bounded = boundedText(title);
    if (bounded == m_title) {
        return;
    }
    m_title = bounded;
    Q_EMIT NewTitle();
}

void StatusNotifierItem::setIcon(const TrayIconImage &image)
{
    if (image.isNull()) {
        return;
    }
    // AGENT-GUARD: a fully transparent frame must not displace a visible one;
    // Wine emits transparent frames transiently and the tray would flicker
    // blank. The first-ever capture may be transparent (icon not drawn yet)
    // and is then published as-is so the item is truthfully empty.
    if (image.isFullyTransparent() && hasIcon()) {
        return;
    }
    if (m_icon.width == qint32(image.width()) && m_icon.height == qint32(image.height())
        && m_icon.data == image.argb()) {
        return;
    }
    m_icon.width = qint32(image.width());
    m_icon.height = qint32(image.height());
    m_icon.data = image.argb();
    Q_EMIT NewIcon();
}

void StatusNotifierItem::setButtonHandler(ButtonHandler handler)
{
    m_buttonHandler = std::move(handler);
}

void StatusNotifierItem::Activate(int x, int y)
{
    if (m_buttonHandler) {
        m_buttonHandler(1, x, y);
    }
}

void StatusNotifierItem::SecondaryActivate(int x, int y)
{
    if (m_buttonHandler) {
        m_buttonHandler(2, x, y);
    }
}

void StatusNotifierItem::ContextMenu(int x, int y)
{
    if (m_buttonHandler) {
        m_buttonHandler(3, x, y);
    }
}

void StatusNotifierItem::Scroll(int delta, const QString &orientation)
{
    // One wheel step per call, matching every other proxy implementation;
    // hosts send one call per notch in practice.
    quint8 button;
    if (orientation.compare(QLatin1String("vertical"), Qt::CaseInsensitive) == 0) {
        button = delta > 0 ? 4 : 5;
    } else {
        button = delta > 0 ? 6 : 7;
    }
    if (m_buttonHandler) {
        m_buttonHandler(button, 0, 0);
    }
}

} // namespace QindaQt::XEmbedTray
