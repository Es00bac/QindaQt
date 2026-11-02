// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/input_actions/macro_runner.h>
#include <qindaqt/services/link_input/input_sink.h>
#include <qindaqt/services/link_input/text_typer.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

namespace QindaQt::Link::Macros {

// The shared MacroRunner's output, implemented on the RemoteDesktop portal
// session (ADR-0436). Runs on the GUI thread; borrows the sink and the gate.
//
// allowed() is false while the portal session is not active or the gate is
// closed (screen locked or unknown), which makes MacroRunner cancel at its
// next tick, including in the middle of a wait.
// AGENT-NOTE: a portal cannot see the focused window, so target() is one
// stable sentinel. The runner's "focus changed, cancel" rule therefore never
// fires for QindaLink runs; window-management actions (maximize, close window,
// show desktop) are compositor calls the portal cannot make and fail the macro
// with "input-unavailable" instead of doing something else.
// AGENT-GUARD: every hold goes through the per-key reference counts here.
// reset() and releaseAll() must leave the counts empty or keys stick.
class PortalMacroOutput final : public QObject, public InputActions::MacroOutput {
    Q_OBJECT
public:
    PortalMacroOutput(InputSink &sink, InputGate &gate, QObject *parent = nullptr);
    ~PortalMacroOutput() override;

    QObject *target() const override { return const_cast<PortalMacroOutput *>(this); }
    bool allowed() const override { return m_sink.active() && m_gate.open(); }
    bool invoke(const QString &token, const InputActions::Binding &binding, bool down) override;
    void motion(QPointF delta) override;
    void scroll(QPointF delta) override;
    void reset(const QString &prefix) override;
    TextState textState(const QString &token) const override;

    void releaseAll() { reset({}); }
    [[nodiscard]] bool holding() const { return !m_holds.isEmpty(); }

private:
    struct Hold {
        QList<qint32> keys;
        QList<qint32> buttons;
    };
    bool resolve(const InputActions::Binding &binding, Hold &hold, bool &nudge, QPointF &delta) const;
    void press(const QString &token, const Hold &hold);
    void release(const QString &token);

    InputSink &m_sink;
    InputGate &m_gate;
    TextTyper m_text;
    QHash<QString, Hold> m_holds;
    QHash<qint32, int> m_keyCounts, m_buttonCounts;
    QHash<QString, QPointF> m_nudges;
    QTimer m_nudgeTimer;
};

} // namespace QindaQt::Link::Macros
