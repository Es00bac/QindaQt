// SPDX-License-Identifier: GPL-3.0-or-later
#include "macro_triggers.h"
#include <input_event.h>
#include <workspace.h>
#include <window.h>
#include <core/session.h>
#include <main.h>
#include <wayland_server.h>
#include <QKeySequence>
namespace QindaQt::Controllers {
namespace {
quint64 milliseconds() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
Qt::KeyboardModifiers modifiers(const QStringList &names) {
    Qt::KeyboardModifiers mods;
    if (names.contains("Ctrl")) mods |= Qt::ControlModifier;
    if (names.contains("Alt")) mods |= Qt::AltModifier;
    if (names.contains("Shift")) mods |= Qt::ShiftModifier;
    if (names.contains("Meta")) mods |= Qt::MetaModifier;
    return mods;
}
}
MacroTriggers::MacroTriggers(InputActions::MacroStore &store, ControllerInputDevice &input, MacroEndpoint &endpoint)
    : KWin::InputEventFilter(KWin::InputFilterOrder::GlobalShortcut), m_store(store), m_input(input), m_endpoint(endpoint),
      m_shortcuts(QDBusConnection::sessionBus(), "qindaqt-input-macros") {
    connect(KWin::input(), &KWin::InputRedirection::deviceRemoved, this, [this](KWin::InputDevice *device) { m_consumed.remove(device); });
    connect(KWin::waylandServer(), &KWin::WaylandServer::lockStateChanged, this, [this] { m_consumed.clear(); if (!ControllerInputDevice::contextAllowed()) m_input.cancelMacro(); });
    connect(KWin::kwinApp()->session(), &KWin::Session::activeChanged, this, [this] { m_consumed.clear(); if (!ControllerInputDevice::contextAllowed()) m_input.cancelMacro(); });
    connect(&endpoint, &MacroEndpoint::libraryChanged, this, &MacroTriggers::refresh);
    connect(&m_shortcuts, &Services::Shortcuts::ShortcutLease::status, &endpoint, &MacroEndpoint::shortcutStatus);
    connect(&input, &ControllerInputDevice::macroChanged, this, [this] {
        const auto now = milliseconds();
        if (!m_intervals.isEmpty() && m_intervals.last().second == 0) m_intervals.last().second = now;
        if (!m_input.runningMacro().isEmpty()) m_intervals.append({now, 0});
        while (m_intervals.size() > 64) m_intervals.removeFirst();
    });
    connect(&m_shortcuts, &Services::Shortcuts::ShortcutLease::activated, this, [this](const QString &id, quint64 timestamp) {
        // Native activation carries CLOCK_MONOTONIC milliseconds (ADR-0334).
        // Filter playback-time activations even if their bus signal arrives
        // after playback ends; a macro cannot recursively trigger itself.
        for (const auto &interval : m_intervals) if (timestamp >= interval.first && (!interval.second || timestamp <= interval.second)) return;
        m_input.runMacro("keyboard/" + id, id);
    });
}
void MacroTriggers::start() { if (!m_endpoint.ready()) return; KWin::input()->installInputEventFilter(this); refresh(); }
void MacroTriggers::refresh(const QString &editedShortcut) {
    QList<Services::Shortcuts::Binding> desired;
    for (const auto &macro : m_store.values()) {
        Services::Shortcuts::Binding b; b.component = "qindaqt-input-macros"; b.action = macro.id;
        b.description = macro.name; if (!macro.shortcut.isEmpty()) b.keys = {QKeySequence::fromString(macro.shortcut, QKeySequence::PortableText)};
        desired.append(b);
    }
    m_shortcuts.replace(desired, editedShortcut.isEmpty() ? QSet<QString>{} : QSet<QString>{editedShortcut});
}
bool MacroTriggers::pointerButton(KWin::PointerButtonEvent *event) {
    if (event->device == &m_input) return false;
    if (event->state == KWin::PointerButtonState::Released) {
        auto it = m_consumed.find(event->device); if (it == m_consumed.end()) return false;
        const bool consumed = it->remove(event->nativeButton); if (it->isEmpty()) m_consumed.erase(it); return consumed;
    }
    if (!ControllerInputDevice::contextAllowed() || !m_input.runningMacro().isEmpty()) return false;
    // Preserve access to the editor even when a user binds an unmodified
    // primary mouse button. Controller bindings and explicit Test still work.
    if (auto *window = KWin::workspace()->activeWindow(); window && window->desktopFileName() == "org.qindaqt.Settings") return false;
    const QMap<QString, Qt::MouseButton> buttons{{"left", Qt::LeftButton}, {"right", Qt::RightButton}, {"middle", Qt::MiddleButton}, {"back", Qt::BackButton}, {"forward", Qt::ForwardButton}};
    for (const auto &macro : m_store.values()) {
        if (macro.mouseButton.isEmpty() || buttons.value(macro.mouseButton) != event->button || modifiers(macro.mouseModifiers) != event->modifiersRelevantForShortcuts) continue;
        if (m_input.runMacro("mouse/" + macro.id, macro.id)) { m_consumed[event->device].insert(event->nativeButton); return true; }
    }
    return false;
}
bool MacroTriggers::keyboardKey(KWin::KeyboardKeyEvent *event) {
    if (event->device != &m_input && event->key == Qt::Key_Escape && event->state == KWin::KeyboardKeyState::Pressed) m_input.cancelMacro();
    return false;
}
} // namespace QindaQt::Controllers
