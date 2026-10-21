// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller_text_output.h"
#include "controller_input_device.h"
#include <input.h>
#include <keyboard_input.h>
#include <wayland_server.h>
#include <workspace.h>
#include <window.h>
#include <xkb.h>
#include <wayland/display.h>
#include <wayland/keyboard.h>
#include <wayland/seat.h>
#include <wayland/textinput_v1.h>
#include <wayland/textinput_v2.h>
#include <wayland/textinput_v3.h>
#include <QScopeGuard>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <xkbcommon/xkbcommon.h>

namespace QindaQt::Controllers {
QObject *ControllerTextOutput::target() const {
    auto *window = KWin::workspace()->activeWindow();
    return window && window->isInternal() ? QGuiApplication::focusObject() : static_cast<QObject *>(window);
}
bool ControllerTextOutput::allowed() const {
    const auto window = KWin::workspace()->activeWindow();
    if (!ControllerInputDevice::contextAllowed() || !window || window->isDeleted()) return false;
    if (window->isInternal()) {
        auto *focus = QGuiApplication::focusObject(); if (!focus) return false;
        QInputMethodQueryEvent query(Qt::ImEnabled); QCoreApplication::sendEvent(focus, &query);
        return query.value(Qt::ImEnabled).toBool();
    }
    return KWin::waylandServer()->seat()->keyboard()->focusedSurface();
}
int ControllerTextOutput::chunkCodepoints() const {
    if (auto *window = KWin::workspace()->activeWindow(); window && window->isInternal()) return 128;
    auto *seat = KWin::waylandServer()->seat();
    return (seat->textInputV1() && seat->textInputV1()->isEnabled()) || (seat->textInputV2() && seat->textInputV2()->isEnabled()) || (seat->textInputV3() && seat->textInputV3()->isEnabled()) ? 128 : 1;
}
bool ControllerTextOutput::insert(const QString &text) {
    if (!allowed()) return false;
    if (auto *window = KWin::workspace()->activeWindow(); window && window->isInternal()) {
        QInputMethodEvent event; event.setCommitString(text);
        return QCoreApplication::sendEvent(QGuiApplication::focusObject(), &event);
    }
    auto *seat = KWin::waylandServer()->seat();
    // Mirror KWin InputMethod's public text-input path. <=128 codepoints keeps
    // each commit well below Wayland's message size, including astral Unicode.
    if (auto *t = seat->textInputV1(); t && t->isEnabled()) {
        t->commitString(text); t->setPreEditCursor(0); t->preEdit({}, {}); return true;
    }
    if (auto *t = seat->textInputV2(); t && t->isEnabled()) {
        t->commitString(text); t->setPreEditCursor(0); t->preEdit({}, {}); return true;
    }
    if (auto *t = seat->textInputV3(); t && t->isEnabled()) {
        t->sendPreEditString({}, 0, 0); t->commitString(text); t->done(); return true;
    }
    const auto points = text.toUcs4();
    if (points.size() != 1) return false;
    const auto point = points.first();
    const auto sym = point == '\n' || point == '\r' ? XKB_KEY_Return : point == '\t' ? XKB_KEY_Tab : xkb_utf32_to_keysym(point);
    if (sym == XKB_KEY_NoSymbol) return false;
    auto *xkb = KWin::input()->keyboard()->xkb();
    auto *display = KWin::waylandServer()->display();
    const auto restoreModifiers = qScopeGuard([xkb] { xkb->forwardModifiers(); });
    const auto code = xkb->keycodeFromKeysym(sym);
    if (code) {
        seat->notifyKeyboardModifiers(code->modifiers, 0, 0, xkb->currentLayout());
        seat->notifyKeyboardKey(code->keyCode, KWin::KeyboardKeyState::Pressed, display->nextSerial());
        seat->notifyKeyboardKey(code->keyCode, KWin::KeyboardKeyState::Released, display->nextSerial());
    } else {
        // AGENT-NOTE: KWin's own InputMethod::forwardKeySym uses keycode 247
        // for unmapped Unicode. Restore the seat keymap within this call;
        // suppression/unload can never leave a temporary map installed.
        constexpr quint32 temporaryCode = 247;
        const auto map = xkb->keymapContentsForKeysym(temporaryCode, sym);
        if (map.isEmpty()) return false;
        seat->keyboard()->setKeymap(map);
        const auto restoreMap = qScopeGuard([seat, xkb] { seat->keyboard()->setKeymap(xkb->keymapContents()); });
        seat->notifyKeyboardModifiers(0, 0, 0, 0);
        seat->notifyKeyboardKey(temporaryCode, KWin::KeyboardKeyState::Pressed, display->nextSerial());
        seat->notifyKeyboardKey(temporaryCode, KWin::KeyboardKeyState::Released, display->nextSerial());
    }
    return true;
}
} // namespace QindaQt::Controllers
