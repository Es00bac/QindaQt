// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/controller_runtime.h"
#include <SDL3/SDL.h>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::Controllers;
class Sink final : public InputSink {
public:
    void button(const QString &token, const Binding &b, bool down) override {
        if (down) held.insert(token); else held.remove(token);
        events.append({b.action, down});
    }
    void motion(QPointF delta) override { if (!delta.isNull()) moves.append(delta); }
    void scroll(QPointF) override {}
    void reset(const QString &id) override {
        ++resets;
        for (const auto &token : held.values()) if (id.isEmpty() || token.startsWith(id + '/')) held.remove(token);
    }
    QList<QPair<QString, bool>> events;
    QList<QPointF> moves;
    QSet<QString> held;
    int resets = 0;
};
class ControllerRuntimeTest final : public QObject {
    Q_OBJECT
    SDL_JoystickID m_id = 0;
    SDL_Joystick *m_joystick = nullptr;
    QJsonObject snapshot(ControllerRuntime &runtime) { return QJsonDocument::fromJson(runtime.snapshot().toUtf8()).object(); }
    QString connectedId(ControllerRuntime &runtime) {
        for (const auto &entry : snapshot(runtime).value("controllers").toArray())
            if (entry.toObject().value("connected").toBool()) return entry.toObject().value("id").toString();
        return {};
    }
    void button(SDL_GamepadButton b, bool down, ControllerRuntime &runtime) {
        QVERIFY(SDL_SetJoystickVirtualButton(m_joystick, b, down)); SDL_UpdateGamepads(); runtime.tick();
    }
private Q_SLOTS:
    void initTestCase() { QVERIFY2(SDL_InitSubSystem(SDL_INIT_GAMEPAD), SDL_GetError()); }
    void init() {
        SDL_VirtualJoystickTouchpadDesc touch{}; touch.nfingers = 1;
        SDL_VirtualJoystickSensorDesc sensor{}; sensor.type = SDL_SENSOR_GYRO; sensor.rate = 60;
        SDL_VirtualJoystickDesc desc; SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD; desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT; desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
        desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        desc.vendor_id = 0x054c; desc.product_id = 0x0ce6; desc.name = "QindaQt virtual DualSense";
        desc.ntouchpads = 1; desc.touchpads = &touch; desc.nsensors = 1; desc.sensors = &sensor;
        desc.SetSensorsEnabled = [](void *, bool) { return true; };
        m_id = SDL_AttachVirtualJoystick(&desc); QVERIFY2(m_id != 0, SDL_GetError());
        m_joystick = SDL_OpenJoystick(m_id); QVERIFY(m_joystick);
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768));
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768));
        SDL_UpdateGamepads();
    }
    void cleanup() { SDL_CloseJoystick(m_joystick); m_joystick = nullptr; SDL_DetachVirtualJoystick(m_id); }
    void cleanupTestCase() { SDL_QuitSubSystem(SDL_INIT_GAMEPAD); }
    void dictationAndHeldButtonSuppression() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        ControllerRuntime runtime(sink, dir.filePath("controllers.json"), nullptr, {dir.filePath("proc")});
        runtime.start(); QVERIFY(!connectedId(runtime).isEmpty());
        button(SDL_GAMEPAD_BUTTON_BACK, true, runtime);
        QVERIFY(!sink.held.isEmpty()); QCOMPARE(sink.events.last().first, "dictate");
        button(SDL_GAMEPAD_BUTTON_BACK, false, runtime); QVERIFY(sink.held.isEmpty());
        button(SDL_GAMEPAD_BUTTON_BACK, true, runtime); const auto count = sink.events.size();
        runtime.setContext(false, "fullscreen"); QVERIFY(sink.held.isEmpty());
        runtime.setContext(true, {}); runtime.tick(); QCOMPARE(sink.events.size(), count);
        button(SDL_GAMEPAD_BUTTON_BACK, false, runtime); button(SDL_GAMEPAD_BUTTON_BACK, true, runtime);
        QVERIFY(sink.events.size() > count);
    }
    void steamStartupClosesDesktopInput() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        ControllerRuntime runtime(sink, dir.filePath("controllers.json"), nullptr, {dir.filePath("proc")});
        runtime.start(); button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime); QVERIFY(!sink.held.isEmpty());
        QDir().mkpath(dir.filePath("proc/42/fd")); QFile comm(dir.filePath("proc/42/comm"));
        QVERIFY(comm.open(QIODevice::WriteOnly)); comm.write("steam\n"); comm.close();
        runtime.refreshPriority(); QVERIFY(snapshot(runtime).value("steam").toBool()); QVERIFY(sink.held.isEmpty());
        const auto count = sink.events.size(); runtime.tick(); QCOMPARE(sink.events.size(), count);
        QVERIFY(QDir(dir.filePath("proc/42")).removeRecursively()); runtime.refreshPriority(); runtime.tick();
        QVERIFY(!snapshot(runtime).value("steam").toBool()); QCOMPARE(sink.events.size(), count);
        button(SDL_GAMEPAD_BUTTON_SOUTH, false, runtime); button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime);
        QVERIFY(sink.events.size() > count);
    }
    void stickTouchpadAndGyro() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        ControllerRuntime runtime(sink, dir.filePath("controllers.json"), nullptr, {dir.filePath("proc")});
        runtime.start(); const auto id = connectedId(runtime); QVERIFY(!id.isEmpty());
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_LEFTX, 22000));
        QTest::qWait(20); runtime.tick(); QVERIFY(!sink.moves.isEmpty());
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_LEFTX, 0)); runtime.tick();
        QVERIFY(SDL_SetJoystickVirtualTouchpad(m_joystick, 0, 0, true, 0.2f, 0.2f, 1)); runtime.tick();
        const auto count = sink.moves.size();
        QVERIFY(SDL_SetJoystickVirtualTouchpad(m_joystick, 0, 0, true, 0.3f, 0.2f, 1)); runtime.tick();
        QVERIFY(sink.moves.size() > count);
        QVERIFY(SDL_SetJoystickVirtualTouchpad(m_joystick, 0, 0, false, 0.3f, 0.2f, 0)); runtime.tick();
        const auto revision = snapshot(runtime).value("revision").toString().toULongLong();
        const auto result = QJsonDocument::fromJson(runtime.apply(id, "{\"gyro\":true}", revision).toUtf8()).object();
        QVERIFY(result.value("ok").toBool()); runtime.tick();
        const auto gyroCount = sink.moves.size(); const float values[3]{0, 1, 0};
        QVERIFY(SDL_SetJoystickVirtualSensorData(m_joystick, SDL_SENSOR_GYRO, 1, values, 3));
        QTest::qWait(20); runtime.tick(); QVERIFY(sink.moves.size() > gyroCount);
    }
};
QTEST_MAIN(ControllerRuntimeTest)
#include "tst_controller_runtime.moc"
