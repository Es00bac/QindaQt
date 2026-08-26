// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwindevelopmentinputinjector.h"

#include <core/inputdevice.h>
#include <input.h>

#include <QPointer>
#include <QSet>
#include <QThread>

#include <chrono>
#include <linux/input-event-codes.h>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

class InputRedirectionRegistrar final : public DevelopmentInputDeviceRegistrar
{
public:
    explicit InputRedirectionRegistrar(KWin::InputRedirection *input)
        : m_input(input)
    {
    }

    bool isAvailable() const override { return !m_input.isNull(); }

    void addInputDevice(KWin::InputDevice *device) override
    {
        if (m_input) {
            m_input->addInputDevice(device);
        }
    }

    void removeInputDevice(KWin::InputDevice *device) override
    {
        if (m_input) {
            m_input->removeInputDevice(device);
        }
    }

private:
    QPointer<KWin::InputRedirection> m_input;
};

std::chrono::microseconds eventTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

quint32 linuxKeyCode(DevelopmentInputKey key)
{
    switch (key) {
    case DevelopmentInputKey::LeftMeta:
        return KEY_LEFTMETA;
    case DevelopmentInputKey::LeftShift:
        return KEY_LEFTSHIFT;
    case DevelopmentInputKey::Down:
        return KEY_DOWN;
    case DevelopmentInputKey::Enter:
        return KEY_ENTER;
    }
    Q_UNREACHABLE_RETURN(0);
}

quint32 linuxButtonCode(DevelopmentInputButton button)
{
    switch (button) {
    case DevelopmentInputButton::Left:
        return BTN_LEFT;
    case DevelopmentInputButton::Right:
        return BTN_RIGHT;
    }
    Q_UNREACHABLE_RETURN(0);
}

} // namespace

class KWinDevelopmentInputInjector::Device final : public KWin::InputDevice
{
public:
    QString sysPath() const override { return developmentInputDeviceId(); }
    QString name() const override { return QStringLiteral("QindaQt Development Input"); }
    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    bool isKeyboard() const override { return true; }
    bool isPointer() const override { return true; }
    bool isTouchpad() const override { return false; }
    bool isTouch() const override { return false; }
    bool isTabletTool() const override { return false; }
    bool isTabletPad() const override { return false; }
    bool isTabletModeSwitch() const override { return false; }
    bool isLidSwitch() const override { return false; }

    void dispatch(const DevelopmentInputEvent &event)
    {
        const auto timestamp = eventTime();
        switch (event.type) {
        case DevelopmentInputEventType::PointerAbsolute:
            Q_EMIT pointerMotionAbsolute(event.position, timestamp, this);
            // AGENT-CONTRACT: KWin batches pointer transitions at frames.
            // Omitting this signal strands the event before input filters.
            Q_EMIT pointerFrame(this);
            break;
        case DevelopmentInputEventType::Key:
            Q_EMIT keyChanged(linuxKeyCode(event.key),
                              event.pressed ? KWin::KeyboardKeyState::Pressed
                                            : KWin::KeyboardKeyState::Released,
                              timestamp, this);
            if (event.pressed) {
                m_pressedKeys.insert(event.key);
            } else {
                m_pressedKeys.remove(event.key);
            }
            break;
        case DevelopmentInputEventType::Button:
            Q_EMIT pointerButtonChanged(
                linuxButtonCode(event.button),
                event.pressed ? KWin::PointerButtonState::Pressed
                              : KWin::PointerButtonState::Released,
                timestamp, this);
            Q_EMIT pointerFrame(this);
            if (event.pressed) {
                m_pressedButtons.insert(event.button);
            } else {
                m_pressedButtons.remove(event.button);
            }
            break;
        }
    }

    void releaseHeldInputs()
    {
        // AGENT-GUARD: Removing a keyboard/pointer device does not synthesize
        // releases in KWin 6.6.5. Clear held test state first or an interrupted
        // scenario can leave compositor modifiers/buttons logically stuck.
        for (const auto key : {DevelopmentInputKey::LeftMeta,
                               DevelopmentInputKey::LeftShift,
                               DevelopmentInputKey::Down,
                               DevelopmentInputKey::Enter}) {
            if (m_pressedKeys.contains(key)) {
                dispatch({.type = DevelopmentInputEventType::Key,
                          .position = {},
                          .key = key,
                          .pressed = false,
                          .button = DevelopmentInputButton::Left});
            }
        }
        for (const auto button : {DevelopmentInputButton::Left,
                                  DevelopmentInputButton::Right}) {
            if (m_pressedButtons.contains(button)) {
                dispatch({.type = DevelopmentInputEventType::Button,
                          .position = {},
                          .key = DevelopmentInputKey::LeftMeta,
                          .pressed = false,
                          .button = button});
            }
        }
    }

private:
    bool m_enabled = true;
    QSet<DevelopmentInputKey> m_pressedKeys;
    QSet<DevelopmentInputButton> m_pressedButtons;
};

KWinDevelopmentInputInjector::KWinDevelopmentInputInjector(
    KWin::InputRedirection *input, QObject *parent)
    : KWinDevelopmentInputInjector(
          std::make_unique<InputRedirectionRegistrar>(input), parent)
{
}

KWinDevelopmentInputInjector::KWinDevelopmentInputInjector(
    std::unique_ptr<DevelopmentInputDeviceRegistrar> registrar, QObject *parent)
    : QObject(parent)
    , m_registrar(std::move(registrar))
    , m_device(std::make_unique<Device>())
{
    if (m_registrar && m_registrar->isAvailable()) {
        // AGENT-CONTRACT: addInputDevice wires these inherited signals through
        // KWin's normal spies and filters; direct filter calls are not proof of
        // a real Hybrid gesture path.
        m_registrar->addInputDevice(m_device.get());
        m_registered = true;
    }
}

KWinDevelopmentInputInjector::~KWinDevelopmentInputInjector()
{
    // AGENT-GUARD: Remove the raw device pointer before destroying the owned
    // object. KWin retains it in InputRedirection and signal connections until
    // removeInputDevice runs.
    if (m_registered && m_registrar) {
        if (m_registrar->isAvailable()) {
            m_device->releaseHeldInputs();
        }
        m_registrar->removeInputDevice(m_device.get());
        m_registered = false;
    }
    m_device.reset();
}

bool KWinDevelopmentInputInjector::isAvailable() const
{
    return m_registered && m_registrar && m_registrar->isAvailable()
        && m_device && m_device->isEnabled();
}

bool KWinDevelopmentInputInjector::inject(const DevelopmentInputBatch &batch)
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (!isAvailable()) {
        return false;
    }
    for (const auto &event : batch.events) {
        m_device->dispatch(event);
    }
    return true;
}

} // namespace QindaQt::Compositor::KWinIntegration
