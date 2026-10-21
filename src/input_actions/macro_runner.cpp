// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/input_actions/macro_runner.h"
#include <QUuid>
namespace QindaQt::InputActions {
MacroRunner::MacroRunner(MacroOutput &output, QObject *parent) : QObject(parent), m_output(output) {
    m_timer.setSingleShot(true); connect(&m_timer, &QTimer::timeout, this, &MacroRunner::advance);
}
MacroRunner::~MacroRunner() { cancel(); }
bool MacroRunner::start(const QString &trigger, const Macro &macro, int delay) {
    Macro valid; QString reason;
    if (!readMacro(macroJson(macro), valid, reason) || !m_output.allowed() || (!m_output.target() && delay == 0)) return false;
    cancel(); m_macro = valid; m_trigger = trigger; m_owner = "macro/" + QUuid::createUuid().toString(QUuid::Id128);
    m_target = m_output.target(); m_index = 0; m_adoptFocus = delay > 0;
    schedule(std::clamp(delay, 0, 5000)); Q_EMIT changed(); return true;
}
void MacroRunner::cancel(const QString &prefix) {
    if (!running() || (!prefix.isEmpty() && m_trigger != prefix && !m_trigger.startsWith(prefix + '/'))) return;
    const auto owner = m_owner;
    m_timer.stop(); m_owner.clear(); m_trigger.clear(); m_tap.clear(); m_text.clear(); m_target.clear(); m_adoptFocus = false; m_followAfterTap = false;
    // Clear state before resetting output; the compositor sink can re-enter
    // cancellation while it releases this owner's keys and text queue.
    m_output.reset(owner); Q_EMIT changed();
}
void MacroRunner::schedule(int milliseconds) {
    m_wait.setRemainingTime(std::max(0, milliseconds), Qt::PreciseTimer);
    m_timer.start(std::clamp(milliseconds, 0, 16));
}
void MacroRunner::advance() {
    if (!running()) return;
    if (!m_output.allowed()) { cancel(); return; }
    if (m_wait.remainingTime() > 0) {
        if (!m_adoptFocus && !m_followAfterTap && (!m_target || m_target != m_output.target())) { cancel(); return; }
        // remainingTime() > 0 here, so the bounded value always fits an int interval.
        m_timer.start(static_cast<int>(std::min<qint64>(16, m_wait.remainingTime()))); return;
    }
    if (m_adoptFocus) { m_target = m_output.target(); m_adoptFocus = false; }
    if ((!m_target || m_target != m_output.target()) && !m_followAfterTap) { cancel(); return; }
    if (!m_tap.isEmpty()) {
        m_output.invoke(m_tap, {}, false); m_tap.clear();
        if (m_followAfterTap) { m_followAfterTap = false; m_adoptFocus = true; schedule(0); return; }
    }
    if (!m_text.isEmpty()) {
        const auto state = m_output.textState(m_text);
        if (state == MacroOutput::Pending) { schedule(4); return; }
        m_text.clear();
        if (state == MacroOutput::Failed) { cancel(); Q_EMIT failed("text-failed"); return; }
    }
    if (m_index >= m_macro.steps.size()) { cancel(); return; }
    const auto &step = m_macro.steps[m_index++];
    const auto slot = m_owner + "/hold-" + QString::number(step.slot);
    if (step.kind == "wait") { schedule(step.milliseconds); return; }
    if (step.kind == "press" && !m_output.invoke(slot, step.binding, true)) { cancel(); Q_EMIT failed("input-unavailable"); return; }
    if (step.kind == "release") m_output.invoke(slot, {}, false);
    if (step.kind == "move") m_output.motion({step.x, step.y});
    if (step.kind == "scroll") m_output.scroll({step.x, step.y});
    if (step.kind == "text") {
        m_text = m_owner + "/text-" + QString::number(m_index);
        if (!m_output.invoke(m_text, step.binding, true)) { cancel(); Q_EMIT failed("text-failed"); return; }
        m_output.invoke(m_text, {}, false);
    }
    if (step.kind == "tap") {
        m_tap = m_owner + "/tap-" + QString::number(m_index);
        if (!m_output.invoke(m_tap, step.binding, true)) { cancel(); Q_EMIT failed("input-unavailable"); return; }
        // Window-navigation actions intentionally change target. Adopt their
        // result on the next event turn; all other focus changes cancel.
        m_followAfterTap = QStringList{"next-window", "previous-window", "launcher", "overview", "show-desktop", "close-window"}.contains(step.binding.action);
        schedule(step.milliseconds); return;
    }
    schedule(0);
}
} // namespace QindaQt::InputActions
