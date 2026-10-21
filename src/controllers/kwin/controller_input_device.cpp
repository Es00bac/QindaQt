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
ControllerInputDevice::ControllerInputDevice(InputActions::MacroStore &macros) : m_macros(macros) {
    connect(&m_runner, &InputActions::MacroRunner::changed, this, &ControllerInputDevice::macroChanged);
    connect(&m_runner, &InputActions::MacroRunner::failed, this, &ControllerInputDevice::macroFailed);
    m_nudgeTimer.setInterval(16);
    connect(&m_nudgeTimer, &QTimer::timeout, this, [this] {
        if (!allowed()) { reset(); return; }
        const auto milliseconds = now().count() / 1000;
        for (auto &nudge : m_nudges) if (milliseconds >= nudge.next) { motion(nudge.delta); nudge.next = milliseconds + 60; }
    });
    connect(&m_text, &TextDelivery::failed, this, &ControllerInputDevice::textFailed);
    connect(&m_voice, &VoiceButton::failed, this, &ControllerInputDevice::dictationFailed);
}
ControllerInputDevice::~ControllerInputDevice() { reset(); }
bool ControllerInputDevice::contextAllowed() {
    return KWin::kwinApp() && KWin::kwinApp()->session() && KWin::kwinApp()->session()->isActive()
        && KWin::waylandServer() && !KWin::waylandServer()->isScreenLocked()
        ;
}
QObject *ControllerInputDevice::target() const { return KWin::workspace()->activeWindow(); }
bool ControllerInputDevice::allowed() const { return m_enabled && contextAllowed(); }
InputActions::MacroOutput::TextState ControllerInputDevice::textState(const QString &token) const {
    return m_text.pending(token) ? Pending : m_text.succeeded(token) ? Complete : Failed;
}
bool ControllerInputDevice::runMacro(const QString &trigger, const QString &id, int delay) {
    if (!m_macros.contains(id)) return false;
    return m_runner.start(trigger, m_macros.value(id), delay);
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
    if (binding.action == "back" && !token.startsWith("macro/")) m_runner.cancel();
    KWin::input()->simulateUserActivity();
    QList<quint32> keys;
    QList<quint32> mouse;
    if (binding.action.startsWith("pointer-")) {
        const QMap<QString, QPointF> directions{{"pointer-left", {-10, 0}}, {"pointer-right", {10, 0}}, {"pointer-up", {0, -10}}, {"pointer-down", {0, 10}}};
        const auto delta = directions.value(binding.action);
        if (!delta.isNull()) { motion(delta); m_nudges[token] = {delta, now().count() / 1000 + 250}; m_nudgeTimer.start(); }
    }
    else if (binding.action == "macro") {
        if (!runMacro(token, binding.macro)) Q_EMIT macroFailed("macro-unavailable");
    }
    else if (binding.action == "text") m_text.start(token, binding.text);
    else if (binding.action == "combination") {
        const QMap<QString, quint32> mods{{"Ctrl", KEY_LEFTCTRL}, {"Alt", KEY_LEFTALT}, {"Shift", KEY_LEFTSHIFT}, {"Meta", KEY_LEFTMETA}};
        const QMap<QString, quint32> buttons{{"left", BTN_LEFT}, {"right", BTN_RIGHT}, {"middle", BTN_MIDDLE}, {"back", BTN_SIDE}, {"forward", BTN_EXTRA}};
        for (const auto &mod : binding.modifiers) keys.append(mods.value(mod));
        for (const auto &key : binding.keys) {
            const auto codes = sequenceKeys(key);
            // Resolve every key before emitting any part of a combination.
            if (codes.isEmpty()) return;
            for (const auto code : codes) if (!keys.contains(code)) keys.append(code);
        }
        for (const auto &button : binding.mouseButtons) mouse.append(buttons.value(button));
    }
    else if (binding.action == "dictate" || binding.action == "voice-command")
        m_voice.press(token, binding.action == "voice-command" ? VoiceButton::Mode::Command : VoiceButton::Mode::Dictation);
    else if (binding.action == "left-click" || binding.action == "left-click-meta-shift") mouse = {BTN_LEFT};
    else if (binding.action == "right-click" || binding.action == "right-click-meta-shift") mouse = {BTN_RIGHT};
    else if (binding.action == "middle-click") mouse = {BTN_MIDDLE};
    else if (binding.action == "maximize") { KWin::workspace()->slotWindowMaximize(); return; }
    else if (binding.action == "close-window") { KWin::workspace()->slotWindowClose(); return; }
    else if (binding.action == "show-desktop") { KWin::workspace()->slotToggleShowDesktop(); return; }
    else keys = sequenceKeys(binding.action == "shortcut" ? binding.shortcut : shortcutFor(binding.action));
    if (binding.action == "left-click-meta-shift" || binding.action == "right-click-meta-shift")
        keys = {KEY_LEFTMETA, KEY_LEFTSHIFT};
    deliver(m_held.press(token, keys, mouse));
}
void ControllerInputDevice::release(const QString &token, bool cancel) {
    if (!m_held.contains(token)) return;
    m_nudges.remove(token); if (m_nudges.isEmpty()) m_nudgeTimer.stop();
    m_voice.release(token, cancel);
    deliver(m_held.release(token));
}
void ControllerInputDevice::deliver(const QList<InputChange> &changes) {
    for (const auto &change : changes) {
        if (change.kind == InputChange::Mouse)
            Q_EMIT pointerButtonChanged(change.code, change.down ? KWin::PointerButtonState::Pressed : KWin::PointerButtonState::Released, now(), this);
        else Q_EMIT keyChanged(change.code, change.down ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released, now(), this);
    }
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
    m_runner.cancel(id);
    m_text.cancel(id);
    for (const auto &token : m_held.tokens()) if (id.isEmpty() || token.startsWith(id + '/')) release(token, true);
    if (id.isEmpty()) m_voice.cancel();
}
} // namespace QindaQt::Controllers
