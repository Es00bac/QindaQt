// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/link_mirror/input_mapper.h"

#include "qindaqt/link_mirror/mirror_messages.h"

#include <algorithm>

namespace qindaqt::link_mirror {

QRectF ViewportMapper::contentRect() const
{
    if (m_display.isEmpty() || m_view.isEmpty()) {
        return {};
    }
    const QSizeF fitted = QSizeF(m_display).scaled(m_view, Qt::KeepAspectRatio);
    const QPointF origin((m_view.width() - fitted.width()) / 2.0, (m_view.height() - fitted.height()) / 2.0);
    return {origin, fitted};
}

std::optional<QPointF> ViewportMapper::toDisplay(QPointF viewPosition) const
{
    const QRectF content = contentRect();
    if (content.isEmpty() || viewPosition.x() < content.left() || viewPosition.y() < content.top()
        || viewPosition.x() >= content.right() || viewPosition.y() >= content.bottom()) {
        return std::nullopt;
    }
    return toDisplayClamped(viewPosition);
}

std::optional<QPointF> ViewportMapper::toDisplayClamped(QPointF viewPosition) const
{
    const QRectF content = contentRect();
    if (content.isEmpty()) {
        return std::nullopt;
    }
    const double scaleX = m_display.width() / content.width();
    const double scaleY = m_display.height() / content.height();
    const double x = (viewPosition.x() - content.left()) * scaleX;
    const double y = (viewPosition.y() - content.top()) * scaleY;
    // AGENT-GUARD: the last addressable pixel is size - 1; Android drops touches outside the
    // display bounds, which would leave a stuck finger on the phone.
    return QPointF(std::clamp(x, 0.0, static_cast<double>(m_display.width() - 1)),
                   std::clamp(y, 0.0, static_cast<double>(m_display.height() - 1)));
}

int androidMetaState(Qt::KeyboardModifiers modifiers)
{
    int meta = 0;
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        meta |= android_key::MetaShiftOn;
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        meta |= android_key::MetaAltOn;
    }
    if (modifiers.testFlag(Qt::ControlModifier)) {
        meta |= android_key::MetaCtrlOn;
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        meta |= android_key::MetaMetaOn;
    }
    return meta;
}

QList<QByteArray> InputTranslator::mousePress(QPointF viewPosition, Qt::MouseButton button)
{
    if (button == Qt::RightButton || button == Qt::BackButton) {
        return {keyFrame(true, android_key::Back, 0, 0), keyFrame(false, android_key::Back, 0, 0)};
    }
    if (button != Qt::LeftButton || m_touching) {
        return {};
    }
    const auto position = m_mapper.toDisplay(viewPosition);
    if (!position) {
        return {};
    }
    m_touching = true;
    m_lastTouch = *position;
    return {touchFrame(TouchAction::Down, 0, *position, 1.0)};
}

QList<QByteArray> InputTranslator::mouseMove(QPointF viewPosition)
{
    if (!m_touching) {
        return {};
    }
    const auto position = m_mapper.toDisplayClamped(viewPosition);
    if (!position || *position == m_lastTouch) {
        return {};
    }
    m_lastTouch = *position;
    return {touchFrame(TouchAction::Move, 0, *position, 1.0)};
}

QList<QByteArray> InputTranslator::mouseRelease(QPointF viewPosition, Qt::MouseButton button)
{
    if (button != Qt::LeftButton || !m_touching) {
        return {};
    }
    m_touching = false;
    const QPointF position = m_mapper.toDisplayClamped(viewPosition).value_or(m_lastTouch);
    return {touchFrame(TouchAction::Up, 0, position, 0.0)};
}

QList<QByteArray> InputTranslator::wheel(QPointF viewPosition, QPoint angleDelta)
{
    const auto position = m_mapper.toDisplay(viewPosition);
    if (!position || angleDelta.isNull()) {
        return {};
    }
    // Android AXIS_VSCROLL is +1 per notch away from the user, like Qt's positive angleDelta.
    // AXIS_HSCROLL is +1 to the right, Qt's horizontal delta is positive to the left.
    const double vertical = angleDelta.y() / 120.0;
    const double horizontal = -angleDelta.x() / 120.0;
    return {scrollFrame(*position, horizontal, vertical)};
}

QList<QByteArray> InputTranslator::keyPress(int qtKey, Qt::KeyboardModifiers modifiers, const QString &text,
                                            bool autoRepeat)
{
    const bool chord = (modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != 0;
    const bool printable = !text.isEmpty() && text.at(0).isPrint();
    if (printable && !chord) {
        return {textFrame(text)};
    }
    const int keycode = androidKeycode(qtKey);
    if (keycode == 0) {
        return {};
    }
    const int repeat = autoRepeat && m_downKeys.contains(keycode) ? ++m_downKeys[keycode] : 0;
    if (!autoRepeat) {
        m_downKeys.insert(keycode, 0);
    }
    return {keyFrame(true, keycode, androidMetaState(modifiers), repeat)};
}

QList<QByteArray> InputTranslator::keyRelease(int qtKey, Qt::KeyboardModifiers modifiers, bool autoRepeat)
{
    if (autoRepeat) {
        return {}; // Android repeats are down events with a growing repeat count.
    }
    // AGENT-GUARD: only keys whose press went out as a key frame get an up. Printable keys
    // went out as text, and a modifier released first must not strand a chord's key-down.
    const int keycode = androidKeycode(qtKey);
    if (keycode == 0 || !m_downKeys.remove(keycode)) {
        return {};
    }
    return {keyFrame(false, keycode, androidMetaState(modifiers), 0)};
}

QList<QByteArray> InputTranslator::releaseAll()
{
    QList<QByteArray> frames = cancel();
    for (auto it = m_downKeys.cbegin(); it != m_downKeys.cend(); ++it) {
        frames.append(keyFrame(false, it.key(), 0, 0));
    }
    m_downKeys.clear();
    return frames;
}

QList<QByteArray> InputTranslator::cancel()
{
    if (!m_touching) {
        return {};
    }
    m_touching = false;
    return {touchFrame(TouchAction::Cancel, 0, m_lastTouch, 0.0)};
}

} // namespace qindaqt::link_mirror
