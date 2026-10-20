// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/controllers/input_sink.h"
#include "voice_button.h"
#include "../held_input_state_p.h"
#include <core/inputdevice.h>
#include <QMap>

namespace QindaQt::Controllers {
class ControllerInputDevice final : public KWin::InputDevice, public InputSink {
    Q_OBJECT
public:
    ControllerInputDevice();
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
    void motion(QPointF delta) override;
    void scroll(QPointF delta) override;
    void reset(const QString &id = {}) override;
    static bool contextAllowed();
Q_SIGNALS:
    void dictationFailed(const QString &reason);
private:
    void deliver(const QList<InputChange> &changes);
    QList<quint32> sequenceKeys(const QString &sequence) const;
    void release(const QString &token, bool cancel);
    bool m_enabled = true;
    HeldInputState m_held;
    VoiceButton m_voice;
};
} // namespace QindaQt::Controllers
