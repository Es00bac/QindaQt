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
    QString connectedReason(ControllerRuntime &runtime) {
        for (const auto &entry : snapshot(runtime).value("controllers").toArray())
            if (entry.toObject().value("connected").toBool()) return entry.toObject().value("reason").toString();
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
        runtime.start(); QTRY_COMPARE(connectedReason(runtime), "active");
        button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime);
        QVERIFY(!sink.held.isEmpty()); QCOMPARE(sink.events.last().first, "dictate");
        button(SDL_GAMEPAD_BUTTON_SOUTH, false, runtime); QVERIFY(sink.held.isEmpty());
        button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime); const auto count = sink.events.size();
        runtime.setContext(false, "fullscreen"); QVERIFY(sink.held.isEmpty());
        runtime.setContext(true, {}); runtime.tick(); QCOMPARE(sink.events.size(), count);
        button(SDL_GAMEPAD_BUTTON_SOUTH, false, runtime); button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime);
        QVERIFY(sink.events.size() > count);
    }
    void dualSenseButtonsTriggersAndReset() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        ControllerRuntime runtime(sink, dir.filePath("controllers.json"), nullptr, {dir.filePath("proc")});
        runtime.start(); QTRY_COMPARE(connectedReason(runtime), "active");
        const auto id = connectedId(runtime); QVERIFY(!id.isEmpty());
        const QMap<SDL_GamepadButton, QString> actions{
            {SDL_GAMEPAD_BUTTON_BACK, "back"}, {SDL_GAMEPAD_BUTTON_START, "back"},
            {SDL_GAMEPAD_BUTTON_GUIDE, "launcher"}, {SDL_GAMEPAD_BUTTON_DPAD_UP, "shortcut"},
            {SDL_GAMEPAD_BUTTON_DPAD_LEFT, "shortcut"}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "shortcut"},
            {SDL_GAMEPAD_BUTTON_DPAD_DOWN, "accept"}, {SDL_GAMEPAD_BUTTON_LEFT_STICK, "previous-window"},
            {SDL_GAMEPAD_BUTTON_RIGHT_STICK, "next-window"}, {SDL_GAMEPAD_BUTTON_NORTH, "maximize"},
            {SDL_GAMEPAD_BUTTON_EAST, "overview"}, {SDL_GAMEPAD_BUTTON_WEST, "voice-command"},
            {SDL_GAMEPAD_BUTTON_SOUTH, "dictate"}, {SDL_GAMEPAD_BUTTON_MISC1, "none"},
            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "right-click"}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "left-click"}};
        for (auto it = actions.begin(); it != actions.end(); ++it) {
            button(it.key(), true, runtime); QCOMPARE(sink.events.last().first, it.value());
            button(it.key(), false, runtime);
        }
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767)); runtime.tick();
        QCOMPARE(sink.events.last().first, "left-click-meta-shift");
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767)); runtime.tick();
        QCOMPARE(sink.events.last().first, "right-click-meta-shift");
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768));
        QVERIFY(SDL_SetJoystickVirtualAxis(m_joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768)); runtime.tick();
        auto revision = snapshot(runtime).value("revision").toString().toULongLong();
        QVERIFY(QJsonDocument::fromJson(runtime.apply(id, "{\"bindings\":{\"south\":{\"action\":\"accept\"}}}", revision).toUtf8()).object().value("ok").toBool());
        revision = snapshot(runtime).value("revision").toString().toULongLong();
        QVERIFY(QJsonDocument::fromJson(runtime.resetProfile(id, revision).toUtf8()).object().value("ok").toBool());
        runtime.tick(); button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime);
        QCOMPARE(sink.events.last().first, "dictate");
    }
    void touchpadTogglesGyroOncePerPressAndPersists() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        ControllerRuntime runtime(sink, dir.filePath("controllers.json"), nullptr, {dir.filePath("proc")});
        runtime.start(); QTRY_COMPARE(connectedReason(runtime), "active");
        const auto id = connectedId(runtime);
        auto revision = snapshot(runtime).value("revision").toString().toULongLong();
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, true, runtime);
        QCOMPARE(snapshot(runtime).value("revision").toString().toULongLong(), revision + 1);
        QVERIFY(sink.events.isEmpty());
        ProfileStore readback(dir.filePath("controllers.json")); QString reason;
        QVERIFY(readback.load(reason)); QVERIFY(readback.profile(id).gyro);
        runtime.tick(); runtime.tick();
        QCOMPARE(snapshot(runtime).value("revision").toString().toULongLong(), revision + 1);
        const auto gyroCount = sink.moves.size(); const float values[3]{0, 1, 0};
        QVERIFY(SDL_SendJoystickVirtualSensorData(m_joystick, SDL_SENSOR_GYRO, 1, values, 3));
        QTest::qWait(20); runtime.tick(); QVERIFY(sink.moves.size() > gyroCount);
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, false, runtime);
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, true, runtime);
        QCOMPARE(snapshot(runtime).value("revision").toString().toULongLong(), revision + 2);
        QVERIFY(readback.load(reason)); QVERIFY(!readback.profile(id).gyro);
        const auto offCount = sink.moves.size(); QTest::qWait(20); runtime.tick(); QCOMPARE(sink.moves.size(), offCount);
        runtime.setContext(false, "locked"); runtime.setContext(true, {}); runtime.tick();
        QCOMPARE(connectedReason(runtime), "release-controls");
        QVERIFY(readback.load(reason)); QVERIFY(!readback.profile(id).gyro);
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, false, runtime);
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, true, runtime);
        QVERIFY(readback.load(reason)); QVERIFY(readback.profile(id).gyro);
    }
    void failedGyroSaveKeepsConfirmedPreference() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        const auto path = dir.filePath("controllers.json");
        ControllerRuntime runtime(sink, path, nullptr, {dir.filePath("proc")});
        runtime.start(); QTRY_COMPARE(connectedReason(runtime), "active");
        const auto id = connectedId(runtime);
        QVERIFY(QFile::remove(path)); QVERIFY(QDir().mkdir(path));
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, true, runtime);
        QCOMPARE(snapshot(runtime).value("error").toString(), "config-unsaved");
        for (const auto &entry : snapshot(runtime).value("controllers").toArray()) {
            const auto row = entry.toObject();
            if (row.value("id").toString() == id) QVERIFY(!row.value("config").toObject().value("gyro").toBool());
        }
        QVERIFY(QDir().rmdir(path));
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, false, runtime);
        button(SDL_GAMEPAD_BUTTON_TOUCHPAD, true, runtime);
        ProfileStore readback(path); QString reason;
        QVERIFY(readback.load(reason)); QVERIFY(readback.profile(id).gyro);
        QVERIFY(snapshot(runtime).value("error").toString().isEmpty());
    }
    void steamStartupClosesDesktopInput() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        ControllerRuntime runtime(sink, dir.filePath("controllers.json"), nullptr, {dir.filePath("proc")});
        runtime.start(); QTRY_COMPARE(connectedReason(runtime), "active");
        button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime); QVERIFY(!sink.held.isEmpty());
        QDir().mkpath(dir.filePath("proc/42/fd")); QFile comm(dir.filePath("proc/42/comm"));
        QVERIFY(comm.open(QIODevice::WriteOnly)); comm.write("steam\n"); comm.close();
        runtime.refreshPriority(); QVERIFY(sink.held.isEmpty());
        QTRY_VERIFY(snapshot(runtime).value("steam").toBool());
        const auto count = sink.events.size(); runtime.tick(); QCOMPARE(sink.events.size(), count);
        QVERIFY(QDir(dir.filePath("proc/42")).removeRecursively()); runtime.refreshPriority(); runtime.tick();
        QTRY_VERIFY(!snapshot(runtime).value("steam").toBool());
        QTRY_COMPARE(connectedReason(runtime), "release-controls");
        QCOMPARE(sink.events.size(), count);
        button(SDL_GAMEPAD_BUTTON_SOUTH, false, runtime); button(SDL_GAMEPAD_BUTTON_SOUTH, true, runtime);
        QVERIFY(sink.events.size() > count);
    }
    void stickTouchpadAndGyro() {
        QTemporaryDir dir; Sink sink; QDir().mkpath(dir.filePath("proc"));
        ControllerRuntime runtime(sink, dir.filePath("controllers.json"), nullptr, {dir.filePath("proc")});
        runtime.start(); QTRY_COMPARE(connectedReason(runtime), "active");
        const auto id = connectedId(runtime); QVERIFY(!id.isEmpty());
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
        QVERIFY(SDL_SendJoystickVirtualSensorData(m_joystick, SDL_SENSOR_GYRO, 1, values, 3));
        QTest::qWait(20); runtime.tick(); QVERIFY(sink.moves.size() > gyroCount);
    }
};
QTEST_MAIN(ControllerRuntimeTest)
#include "tst_controller_runtime.moc"
