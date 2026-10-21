// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "macro_endpoint.h"
#include <qindaqt/services/shortcuts_client/shortcut_lease.h>
#include <input.h>
namespace QindaQt::Controllers {
class MacroTriggers final : public QObject, public KWin::InputEventFilter {
    Q_OBJECT
public:
    MacroTriggers(InputActions::MacroStore &store, ControllerInputDevice &input, MacroEndpoint &endpoint);
    void start();
    bool pointerButton(KWin::PointerButtonEvent *event) override;
    bool keyboardKey(KWin::KeyboardKeyEvent *event) override;
private:
    void refresh(const QString &editedShortcut = {});
    InputActions::MacroStore &m_store;
    ControllerInputDevice &m_input;
    MacroEndpoint &m_endpoint;
    Services::Shortcuts::ShortcutLease m_shortcuts;
    QSet<quint32> m_consumed;
    QList<QPair<quint64, quint64>> m_intervals;
};
} // namespace QindaQt::Controllers
