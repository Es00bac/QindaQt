// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input_actions.h"
#include <QPointF>
#include <QDeadlineTimer>
#include <QPointer>
#include <QTimer>
namespace QindaQt::InputActions {
class MacroOutput {
public:
    enum TextState { Pending, Complete, Failed };
    virtual ~MacroOutput() = default;
    virtual QObject *target() const = 0;
    virtual bool allowed() const = 0;
    virtual void button(const QString &token, const Binding &binding, bool down) = 0;
    virtual void motion(QPointF delta) = 0;
    virtual void scroll(QPointF delta) = 0;
    virtual void reset(const QString &prefix) = 0;
    virtual TextState textState(const QString &token) const = 0;
};
// Borrowed GUI-thread output outlives the runner. One bounded sequence at a
// time; timers never sleep the compositor. Cancel releases every owned hold.
class MacroRunner final : public QObject {
    Q_OBJECT
public:
    explicit MacroRunner(MacroOutput &output, QObject *parent = nullptr);
    bool start(const QString &trigger, const Macro &macro, int delay = 0);
    void cancel(const QString &triggerPrefix = {});
    bool running() const { return !m_owner.isEmpty(); }
    QString macroId() const { return running() ? m_macro.id : QString{}; }
Q_SIGNALS:
    void changed();
    void failed(const QString &reason);
private:
    void advance();
    void schedule(int milliseconds);
    MacroOutput &m_output;
    QTimer m_timer;
    QDeadlineTimer m_wait;
    QPointer<QObject> m_target;
    Macro m_macro;
    QString m_owner, m_trigger, m_tap, m_text;
    int m_index = 0;
    bool m_adoptFocus = false, m_followAfterTap = false;
};
} // namespace QindaQt::InputActions
