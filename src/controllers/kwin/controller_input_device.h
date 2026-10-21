// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/controllers/input_sink.h"
#include "voice_button.h"
#include "controller_text_output.h"
#include "../held_input_state_p.h"
#include <qindaqt/input_actions/macro_store.h>
#include <qindaqt/input_actions/macro_runner.h>
#include <core/inputdevice.h>
#include <QMap>

namespace QindaQt::Controllers {
class ControllerInputDevice final : public KWin::InputDevice, public InputSink, public InputActions::MacroOutput {
    Q_OBJECT
public:
    explicit ControllerInputDevice(InputActions::MacroStore &macros);
    ~ControllerInputDevice() override;
    QString name() const override { return QStringLiteral("QindaQt controller desktop input"); }
    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool value) override;
    bool isKeyboard() const override { return true; }
    bool isPointer() const override { return true; }
    bool isTouchpad() const override { return false; }
    bool isTouch() const override { return false; }
    bool isTabletTool() const override { return false; }
    bool isTabletPad() const override { return false; }
    bool isTabletModeSwitch() const override { return false; }
    bool isLidSwitch() const override { return false; }
    void button(const QString &token, const Binding &binding, bool down) override;
    bool invoke(const QString &token, const Binding &binding, bool down) override;
    void motion(QPointF delta) override;
    void scroll(QPointF delta) override;
    void reset(const QString &id = {}) override;
    static bool contextAllowed();
    QObject *target() const override;
    bool allowed() const override;
    TextState textState(const QString &token) const override;
    bool runMacro(const QString &trigger, const QString &id, int delay = 0);
    void cancelMacro() { m_runner.cancel(); }
    QString runningMacro() const { return m_runner.macroId(); }
Q_SIGNALS:
    void inputFailed();
    void macroChanged();
    void macroFailed(const QString &reason);
    void textFailed();
    void dictationFailed(const QString &reason);
private:
    void deliver(const QList<InputChange> &changes);
    QList<quint32> sequenceKeys(const QString &sequence) const;
    void release(const QString &token, bool cancel);
    InputActions::MacroStore &m_macros;
    InputActions::MacroRunner m_runner{*this};
    struct Nudge { QPointF delta; qint64 next; };
    QMap<QString, Nudge> m_nudges;
    QTimer m_nudgeTimer;
    bool m_enabled = true;
    HeldInputState m_held;
    VoiceButton m_voice;
    ControllerTextOutput m_textOutput;
    TextDelivery m_text{m_textOutput};
};
} // namespace QindaQt::Controllers
