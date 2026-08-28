// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwindevelopmentinputinjector.h"

#include <core/inputdevice.h>

#include <QPointer>
#include <QtTest>

#include <chrono>
#include <linux/input-event-codes.h>
#include <memory>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

struct RegistrarState final
{
    int addCalls = 0;
    int removeCalls = 0;
    bool available = true;
    bool deviceAliveAtRemoval = false;
    bool keyboard = false;
    bool pointer = false;
    QPointer<KWin::InputDevice> device;
    QStringList eventOrder;
    QList<QPointF> positions;
    QList<quint32> keys;
    QList<KWin::KeyboardKeyState> keyStates;
    QList<quint32> buttons;
    QList<KWin::PointerButtonState> buttonStates;
    QList<std::chrono::microseconds> timestamps;
};

class RecordingRegistrar final : public DevelopmentInputDeviceRegistrar
{
public:
    explicit RecordingRegistrar(std::shared_ptr<RegistrarState> state)
        : m_state(std::move(state))
    {
    }

    bool isAvailable() const override { return m_state->available; }

    void addInputDevice(KWin::InputDevice *device) override
    {
        ++m_state->addCalls;
        m_state->device = device;
        m_state->keyboard = device->isKeyboard();
        m_state->pointer = device->isPointer();
        const auto state = m_state;
        QObject::connect(
            device, &KWin::InputDevice::pointerMotionAbsolute, device,
            [state](const QPointF &position, std::chrono::microseconds timestamp,
                    KWin::InputDevice *) {
                state->eventOrder.append(QStringLiteral("pointer-absolute"));
                state->positions.append(position);
                state->timestamps.append(timestamp);
            });
        QObject::connect(device, &KWin::InputDevice::keyChanged, device,
                         [state](quint32 key, KWin::KeyboardKeyState keyState,
                                 std::chrono::microseconds timestamp,
                                 KWin::InputDevice *) {
                             state->eventOrder.append(QStringLiteral("key"));
                             state->keys.append(key);
                             state->keyStates.append(keyState);
                             state->timestamps.append(timestamp);
                         });
        QObject::connect(
            device, &KWin::InputDevice::pointerButtonChanged, device,
            [state](quint32 button, KWin::PointerButtonState buttonState,
                    std::chrono::microseconds timestamp, KWin::InputDevice *) {
                state->eventOrder.append(QStringLiteral("button"));
                state->buttons.append(button);
                state->buttonStates.append(buttonState);
                state->timestamps.append(timestamp);
            });
        QObject::connect(device, &KWin::InputDevice::pointerFrame, device,
                         [state](KWin::InputDevice *) {
                             state->eventOrder.append(QStringLiteral("frame"));
                         });
    }

    void removeInputDevice(KWin::InputDevice *device) override
    {
        ++m_state->removeCalls;
        m_state->eventOrder.append(QStringLiteral("remove"));
        m_state->deviceAliveAtRemoval = !m_state->device.isNull()
            && m_state->device.data() == device;
    }

private:
    std::shared_ptr<RegistrarState> m_state;
};

} // namespace

class KWinDevelopmentInputInjectorTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emitsThroughTheRegisteredCombinationDevice();
    void removesDeviceBeforeOwnedLifetimeEnds();
    void remainsUnavailableWithoutARegistrarBackend();
};

void KWinDevelopmentInputInjectorTest::emitsThroughTheRegisteredCombinationDevice()
{
    const auto state = std::make_shared<RegistrarState>();
    KWinDevelopmentInputInjector injector(
        std::make_unique<RecordingRegistrar>(state));
    QVERIFY(injector.isAvailable());
    QCOMPARE(state->addCalls, 1);
    QVERIFY(state->keyboard);
    QVERIFY(state->pointer);
    QCOMPARE(state->device->name(), QStringLiteral("QindaQt Development Input"));
    QCOMPARE(state->device->sysPath(), QStringLiteral("qindaqt-development-input"));

    DevelopmentInputBatch batch;
    batch.events = {
        {DevelopmentInputEventType::PointerAbsolute, QPointF(-80.5, 720.25),
         DevelopmentInputKey::LeftMeta, false, DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::LeftMeta, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::LeftShift, false,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::N, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::N, false,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Tab, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Tab, false,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Escape, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Escape, false,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Space, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Space, false,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Down, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Down, false,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Enter, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Enter, false,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Button, {}, DevelopmentInputKey::LeftMeta, true,
         DevelopmentInputButton::Left},
        {DevelopmentInputEventType::Button, {}, DevelopmentInputKey::LeftMeta, false,
         DevelopmentInputButton::Left},
        {.type = DevelopmentInputEventType::Button,
         .position = {},
         .key = DevelopmentInputKey::LeftMeta,
         .pressed = true,
         .button = DevelopmentInputButton::Right},
        {.type = DevelopmentInputEventType::Button,
         .position = {},
         .key = DevelopmentInputKey::LeftMeta,
         .pressed = false,
         .button = DevelopmentInputButton::Right},
    };
    QVERIFY(injector.inject(batch));

    QCOMPARE(state->eventOrder,
             QStringList({QStringLiteral("pointer-absolute"), QStringLiteral("frame"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("button"), QStringLiteral("frame"),
                          QStringLiteral("button"), QStringLiteral("frame"),
                          QStringLiteral("button"), QStringLiteral("frame"),
                          QStringLiteral("button"), QStringLiteral("frame")}));
    QCOMPARE(state->positions, QList<QPointF>({QPointF(-80.5, 720.25)}));
    QCOMPARE(state->keys,
             QList<quint32>({static_cast<quint32>(KEY_LEFTMETA),
                             static_cast<quint32>(KEY_LEFTSHIFT),
                             static_cast<quint32>(KEY_N),
                             static_cast<quint32>(KEY_N),
                             static_cast<quint32>(KEY_TAB),
                             static_cast<quint32>(KEY_TAB),
                             static_cast<quint32>(KEY_ESC),
                             static_cast<quint32>(KEY_ESC),
                             static_cast<quint32>(KEY_SPACE),
                             static_cast<quint32>(KEY_SPACE),
                             static_cast<quint32>(KEY_DOWN),
                             static_cast<quint32>(KEY_DOWN),
                             static_cast<quint32>(KEY_ENTER),
                             static_cast<quint32>(KEY_ENTER)}));
    QCOMPARE(state->keyStates,
             QList<KWin::KeyboardKeyState>({KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released}));
    QCOMPARE(state->buttons,
             QList<quint32>({static_cast<quint32>(BTN_LEFT),
                             static_cast<quint32>(BTN_LEFT),
                             static_cast<quint32>(BTN_RIGHT),
                             static_cast<quint32>(BTN_RIGHT)}));
    QCOMPARE(state->buttonStates,
             QList<KWin::PointerButtonState>({KWin::PointerButtonState::Pressed,
                                              KWin::PointerButtonState::Released,
                                              KWin::PointerButtonState::Pressed,
                                              KWin::PointerButtonState::Released}));
    QCOMPARE(state->timestamps.size(), 19);
    for (qsizetype index = 1; index < state->timestamps.size(); ++index) {
        QVERIFY(state->timestamps.at(index) >= state->timestamps.at(index - 1));
    }
}

void KWinDevelopmentInputInjectorTest::removesDeviceBeforeOwnedLifetimeEnds()
{
    const auto state = std::make_shared<RegistrarState>();
    {
        KWinDevelopmentInputInjector injector(
            std::make_unique<RecordingRegistrar>(state));
        QVERIFY(!state->device.isNull());
        QCOMPARE(state->removeCalls, 0);
        DevelopmentInputBatch held;
        held.events = {
            {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::LeftMeta, true,
             DevelopmentInputButton::Left},
            {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Enter, true,
             DevelopmentInputButton::Left},
            {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::N, true,
             DevelopmentInputButton::Left},
            {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Tab, true,
             DevelopmentInputButton::Left},
            {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Escape, true,
             DevelopmentInputButton::Left},
            {DevelopmentInputEventType::Key, {}, DevelopmentInputKey::Space, true,
             DevelopmentInputButton::Left},
            {DevelopmentInputEventType::Button, {}, DevelopmentInputKey::LeftMeta, true,
             DevelopmentInputButton::Left},
            {.type = DevelopmentInputEventType::Button,
             .position = {},
             .key = DevelopmentInputKey::LeftMeta,
             .pressed = true,
             .button = DevelopmentInputButton::Right},
        };
        QVERIFY(injector.inject(held));
    }
    QCOMPARE(state->removeCalls, 1);
    QVERIFY(state->deviceAliveAtRemoval);
    QVERIFY(state->device.isNull());
    QCOMPARE(state->eventOrder,
             QStringList({QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("button"), QStringLiteral("frame"),
                          QStringLiteral("button"), QStringLiteral("frame"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("key"), QStringLiteral("key"),
                          QStringLiteral("button"), QStringLiteral("frame"),
                          QStringLiteral("button"), QStringLiteral("frame"),
                          QStringLiteral("remove")}));
    QCOMPARE(state->keyStates,
             QList<KWin::KeyboardKeyState>({KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Pressed,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Released,
                                            KWin::KeyboardKeyState::Released}));
    QCOMPARE(state->buttonStates,
             QList<KWin::PointerButtonState>({KWin::PointerButtonState::Pressed,
                                              KWin::PointerButtonState::Pressed,
                                              KWin::PointerButtonState::Released,
                                              KWin::PointerButtonState::Released}));
}

void KWinDevelopmentInputInjectorTest::remainsUnavailableWithoutARegistrarBackend()
{
    const auto state = std::make_shared<RegistrarState>();
    state->available = false;
    KWinDevelopmentInputInjector injector(
        std::make_unique<RecordingRegistrar>(state));
    QVERIFY(!injector.isAvailable());
    QCOMPARE(state->addCalls, 0);
    DevelopmentInputBatch batch;
    batch.events.append({DevelopmentInputEventType::Key, {},
                         DevelopmentInputKey::LeftMeta, true,
                         DevelopmentInputButton::Left});
    QVERIFY(!injector.inject(batch));
    QCOMPARE(state->removeCalls, 0);
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(
    QindaQt::Compositor::KWinIntegration::KWinDevelopmentInputInjectorTest)

#include "tst_kwindevelopmentinputinjector.moc"
