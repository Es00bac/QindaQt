// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller_input_device.h"
#include <core/session.h>
#include <input.h>
#include <keyboard_input.h>
#include <main.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>
#include <xkb.h>
#include <QKeySequence>
#include <linux/input-event-codes.h>

namespace QindaQt::Controllers {
namespace {
std::chrono::microseconds now() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
}
QString shortcutFor(const QString &action) {
    const QMap<QString, QString> keys{{"accept", "Return"}, {"back", "Esc"},
        {"up", "Up"}, {"down", "Down"}, {"left", "Left"}, {"right", "Right"},
        {"overview", "Meta+W"}, {"launcher", "Meta"}, {"next-window", "Alt+Tab"},
        {"previous-window", "Alt+Shift+Tab"}, {"desktop-left", "Meta+Ctrl+Left"},
        {"desktop-right", "Meta+Ctrl+Right"}, {"screenshot", "Print"}};
    return keys.value(action);
}
}
ControllerInputDevice::ControllerInputDevice() {
    connect(&m_voice, &VoiceButton::failed, this, &ControllerInputDevice::dictationFailed);
}
ControllerInputDevice::~ControllerInputDevice() { reset(); }
bool ControllerInputDevice::contextAllowed() {
    return KWin::kwinApp() && KWin::kwinApp()->session() && KWin::kwinApp()->session()->isActive()
        && KWin::waylandServer() && !KWin::waylandServer()->isScreenLocked()
        && (!KWin::workspace()->activeWindow() || !KWin::workspace()->activeWindow()->isFullScreen());
}
void ControllerInputDevice::setEnabled(bool value) { if (!value) reset(); m_enabled = value; }
QList<quint32> ControllerInputDevice::sequenceKeys(const QString &text) const {
    const auto seq = QKeySequence::fromString(text, QKeySequence::PortableText);
    if (seq.isEmpty()) return {};
    const auto key = seq[0];
    if (key.key() == Qt::Key_Meta) return {KEY_LEFTMETA};
    QList<quint32> keys;
    const auto mods = key.keyboardModifiers();
    if (mods.testFlag(Qt::ControlModifier)) keys.append(KEY_LEFTCTRL);
    if (mods.testFlag(Qt::AltModifier)) keys.append(KEY_LEFTALT);
    if (mods.testFlag(Qt::ShiftModifier)) keys.append(KEY_LEFTSHIFT);
    if (mods.testFlag(Qt::MetaModifier)) keys.append(KEY_LEFTMETA);
    const auto syms = KWin::Xkb::keysymsFromQtKey(key);
    for (const auto sym : syms) {
        const auto code = KWin::input()->keyboard()->xkb()->keycodeFromKeysym(sym);
        if (!code) continue;
        if (code->level == 1 && !keys.contains(KEY_LEFTSHIFT)) keys.append(KEY_LEFTSHIFT);
        keys.append(code->keyCode);
        return keys;
    }
    return {};
}
void ControllerInputDevice::button(const QString &token, const Binding &binding, bool down) {
    if (!down) { release(token, false); return; }
    if (!m_enabled || !contextAllowed() || m_held.contains(token) || binding.action == "none") return;
    KWin::input()->simulateUserActivity();
    Held held;
    if (binding.action == "dictate") { held.voice = true; m_voice.press(token); }
    else if (binding.action == "left-click") held.mouse = BTN_LEFT;
    else if (binding.action == "right-click") held.mouse = BTN_RIGHT;
    else if (binding.action == "middle-click") held.mouse = BTN_MIDDLE;
    else if (binding.action == "maximize") { KWin::workspace()->slotWindowMaximize(); return; }
    else if (binding.action == "close-window") { KWin::workspace()->slotWindowClose(); return; }
    else if (binding.action == "show-desktop") { KWin::workspace()->slotToggleShowDesktop(); return; }
    else held.keys = sequenceKeys(binding.action == "shortcut" ? binding.shortcut : shortcutFor(binding.action));
    m_held[token] = held;
    if (held.mouse && ++m_buttons[held.mouse] == 1) Q_EMIT pointerButtonChanged(held.mouse, KWin::PointerButtonState::Pressed, now(), this);
    for (quint32 key : held.keys) if (++m_keys[key] == 1) Q_EMIT keyChanged(key, KWin::KeyboardKeyState::Pressed, now(), this);
    Q_EMIT pointerFrame(this);
}
void ControllerInputDevice::release(const QString &token, bool cancel) {
    if (!m_held.contains(token)) return;
    const auto held = m_held.take(token);
    if (held.voice) m_voice.release(token, cancel);
    if (held.mouse && --m_buttons[held.mouse] == 0) Q_EMIT pointerButtonChanged(held.mouse, KWin::PointerButtonState::Released, now(), this);
    for (auto it = held.keys.crbegin(); it != held.keys.crend(); ++it)
        if (--m_keys[*it] == 0) Q_EMIT keyChanged(*it, KWin::KeyboardKeyState::Released, now(), this);
    Q_EMIT pointerFrame(this);
}
void ControllerInputDevice::motion(QPointF delta) {
    if (delta.isNull() || !m_enabled || !contextAllowed()) return;
    KWin::input()->simulateUserActivity();
    Q_EMIT pointerMotion(delta, delta, now(), this); Q_EMIT pointerFrame(this);
}
void ControllerInputDevice::scroll(QPointF delta) {
    if (delta.isNull() || !m_enabled || !contextAllowed()) return;
    KWin::input()->simulateUserActivity();
    if (delta.y() != 0) Q_EMIT pointerAxisChanged(KWin::PointerAxis::Vertical, delta.y(), 0, KWin::PointerAxisSource::Continuous, false, now(), this);
    if (delta.x() != 0) Q_EMIT pointerAxisChanged(KWin::PointerAxis::Horizontal, delta.x(), 0, KWin::PointerAxisSource::Continuous, false, now(), this);
    Q_EMIT pointerFrame(this);
}
void ControllerInputDevice::reset(const QString &id) {
    for (const auto &token : m_held.keys()) if (id.isEmpty() || token.startsWith(id + '/')) release(token, true);
    if (id.isEmpty()) m_voice.cancel();
}
} // namespace QindaQt::Controllers
