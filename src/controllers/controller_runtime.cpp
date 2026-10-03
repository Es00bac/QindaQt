// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/controllers/controller_runtime.h"
#include "qindaqt/controllers/game_priority.h"
#include <SDL3/SDL.h>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QTimer>
#include <optional>
#include <map>
#include <cmath>
#include <sys/inotify.h>
#include <unistd.h>

namespace QindaQt::Controllers {
namespace {
QString family(SDL_GamepadType type) {
    if (type == SDL_GAMEPAD_TYPE_XBOX360 || type == SDL_GAMEPAD_TYPE_XBOXONE) return "xbox";
    if (type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 || type == SDL_GAMEPAD_TYPE_PS5) return "playstation";
    if (type >= SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO && type <= SDL_GAMEPAD_TYPE_GAMECUBE) return "nintendo";
    return "generic";
}
QString faceLabel(SDL_Gamepad *pad, SDL_GamepadButton button) {
    switch (SDL_GetGamepadButtonLabel(pad, button)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "Cross";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "Triangle";
    default: return QString::fromUtf8(SDL_GetGamepadStringForButton(button));
    }
}
QString json(const QJsonObject &obj) { return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)); }
QString buttonId(int index) { return buttonIds().value(index); }
}
class ControllerRuntime::Private {
public:
    struct Device {
        QString id, family, reason = "initializing";
        QJsonObject description;
        QStringList paths;
        SDL_Gamepad *pad = nullptr;
        Profile profile;
        QMap<QString, bool> down;
        std::optional<QPointF> touch;
        bool armed = false, gyroEnabled = false;
        SDL_JoystickID instance = 0;
    };
    Private(ControllerRuntime &owner, InputSink &sink, QString path, RuntimeEnvironment env)
        : q(owner), sink(sink), store(std::move(path)), environment(std::move(env)) {
        sample.setTimerType(Qt::PreciseTimer);
        sample.setInterval(16);
        QObject::connect(&sample, &QTimer::timeout, &q, &ControllerRuntime::tick);
        priority.setInterval(1000);
        QObject::connect(&priority, &QTimer::timeout, &q, &ControllerRuntime::refreshPriority);
        fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
        if (fd >= 0) {
            notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
            QObject::connect(notifier.get(), &QSocketNotifier::activated, &q, [this] {
                alignas(inotify_event) char data[8192];
                while (::read(fd, data, sizeof(data)) > 0) {}
                q.refreshPriority();
            });
        }
        elapsed.start();
    }
    ~Private() { close(); if (fd >= 0) ::close(fd); }
    void bump() { ++revision; Q_EMIT q.changed(revision); }
    void release(Device &device) {
        sink.reset(device.id + '/' + QString::number(device.instance));
        device.armed = false;
        device.down.clear();
        device.touch.reset();
        if (device.pad && device.gyroEnabled) SDL_SetGamepadSensorEnabled(device.pad, SDL_SENSOR_GYRO, false);
        device.gyroEnabled = false;
    }
    void close() {
        for (auto &[instance, device] : devices) { Q_UNUSED(instance); release(device); if (device.pad) SDL_CloseGamepad(device.pad); }
        devices.clear();
        for (int watch : watches) inotify_rm_watch(fd, watch);
        watches.clear();
        watchedPaths.clear();
        if (initialized) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        initialized = false;
    }
    void discover() {
        int count = 0;
        auto *ids = SDL_GetGamepads(&count);
        QSet<SDL_JoystickID> present;
        for (int i = 0; ids && i < std::min(count, 32); ++i) {
            present.insert(ids[i]);
            if (devices.contains(ids[i])) continue;
            auto *pad = SDL_OpenGamepad(ids[i]);
            if (!pad) continue;
            Device dev;
            dev.pad = pad;
            dev.instance = ids[i];
            dev.family = family(SDL_GetGamepadType(pad));
            char guid[33]{};
            SDL_GUIDToString(SDL_GetGamepadGUIDForID(ids[i]), guid, sizeof(guid));
            const auto serial = QString::fromUtf8(SDL_GetGamepadSerial(pad));
            const auto legacyIdentity = QString::fromLatin1(guid) + ':' + serial;
            // SDL GUID includes the transport. A serial identifies the same
            // physical pad across USB/Bluetooth; preserve existing preferences.
            const auto legacyId = "pad:" + QString::fromLatin1(QCryptographicHash::hash(legacyIdentity.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
            dev.id = controllerProfileId(QString::fromLatin1(guid), serial, SDL_GetGamepadVendor(pad), SDL_GetGamepadProduct(pad), dev.family);
            // Two identical pads without serials share a profile but never
            // share held tokens: the runtime appends the SDL instance ID.
            dev.description = {{"id", dev.id}, {"name", QString::fromUtf8(SDL_GetGamepadName(pad))},
                {"family", dev.family}, {"template", false},
                {"hasTouchpad", SDL_GetNumGamepadTouchpads(pad) > 0},
                {"hasGyro", SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO)}};
            QJsonArray buttons;
            for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
                const auto button = static_cast<SDL_GamepadButton>(b);
                if (!SDL_GamepadHasButton(pad, button)) continue;
                buttons.append(QJsonObject{{"id", buttonId(b)},
                                           {"label", b <= SDL_GAMEPAD_BUTTON_NORTH ? faceLabel(pad, button) : QString{}}});
            }
            if (SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) buttons.append(QJsonObject{{"id", "lefttrigger"}});
            if (SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) buttons.append(QJsonObject{{"id", "righttrigger"}});
            dev.description["buttons"] = buttons;
            dev.profile = store.ids().contains(dev.id) ? store.profile(dev.id, dev.family)
                : store.profile(legacyId, dev.family);
            if (!store.ids().contains(dev.id) && store.ids().size() < 64) {
                store.set(dev.id, dev.profile, dev.description);
                store.save(error);
            }
            dev.paths = controllerDevicePaths(SDL_GetGamepadVendor(pad), SDL_GetGamepadProduct(pad), QString::fromUtf8(SDL_GetGamepadPath(pad)));
            for (const auto &path : dev.paths) if (!watchedPaths.contains(path) && fd >= 0) {
                const int watch = inotify_add_watch(fd, path.toUtf8().constData(), IN_OPEN | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | IN_DELETE_SELF);
                if (watch >= 0) { watches.append(watch); watchedPaths.insert(path); }
            }
            devices.emplace(ids[i], std::move(dev));
            q.refreshPriority();
            bump();
        }
        SDL_free(ids);
        for (auto it = devices.begin(); it != devices.end();) {
            if (present.contains(it->first)) { ++it; continue; }
            release(it->second);
            SDL_CloseGamepad(it->second.pad);
            it = devices.erase(it);
            bump();
        }
    }
    void process(SDL_JoystickID instance, Device &dev, double dt) {
        QString reason = !context ? contextReason : !dev.profile.enabled ? "disabled" : QString{};
        for (const auto &path : dev.paths) if (game.busyPaths.contains(path)) reason = "game";
        if (!reason.isEmpty()) {
            if (dev.armed || !dev.down.isEmpty()) release(dev);
            if (dev.reason != reason) { dev.reason = reason; bump(); }
            return;
        }
        QMap<QString, bool> buttons;
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
            buttons[buttonId(b)] = SDL_GetGamepadButton(dev.pad, static_cast<SDL_GamepadButton>(b));
        buttons["lefttrigger"] = SDL_GetGamepadAxis(dev.pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 20000;
        buttons["righttrigger"] = SDL_GetGamepadAxis(dev.pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 20000;
        const auto axis = [&dev](SDL_GamepadAxis a) { return double(SDL_GetGamepadAxis(dev.pad, a)) / 32768.0; };
        const QPointF left(axis(SDL_GAMEPAD_AXIS_LEFTX), axis(SDL_GAMEPAD_AXIS_LEFTY));
        const QPointF right(axis(SDL_GAMEPAD_AXIS_RIGHTX), axis(SDL_GAMEPAD_AXIS_RIGHTY));
        bool touching = false; float x = 0, y = 0;
        if (SDL_GetNumGamepadTouchpads(dev.pad) > 0) SDL_GetGamepadTouchpadFinger(dev.pad, 0, 0, &touching, &x, &y, nullptr);
        if (!dev.armed) {
            // AGENT-GUARD: a held game button may never become a desktop
            // click, shortcut, or dictation when a game/Steam relinquishes it.
            bool held = touching || std::hypot(left.x(), left.y()) > dev.profile.deadzone || std::hypot(right.x(), right.y()) > dev.profile.deadzone;
            for (bool down : buttons) held = held || down;
            if (held) { if (dev.reason != "release-controls") { dev.reason = "release-controls"; bump(); } return; }
            dev.armed = true;
        }
        if (dev.reason != "active") { dev.reason = "active"; bump(); }
        const QString tokenPrefix = dev.id + '/' + QString::number(instance) + '/';
        for (auto b = buttons.begin(); b != buttons.end(); ++b) if (dev.down.value(b.key()) != b.value())
            sink.button(tokenPrefix + b.key(), dev.profile.bindings.value(b.key()), b.value());
        dev.down = buttons;
        if (dev.profile.pointerStick != "off") {
            const bool useLeft = dev.profile.pointerStick == "left";
            sink.motion(stickMotion(useLeft ? left : right, dev.profile.deadzone, dev.profile.pointerSpeed, dt));
            sink.scroll(stickMotion(useLeft ? right : left, dev.profile.deadzone, 500, dt));
        }
        if (dev.profile.touchpad && touching) {
            const QPointF point(x, y);
            if (dev.touch) sink.motion((point - *dev.touch) * dev.profile.pointerSpeed);
            dev.touch = point;
        } else dev.touch.reset();
        const bool gyro = dev.profile.gyro && SDL_GamepadHasSensor(dev.pad, SDL_SENSOR_GYRO);
        if (gyro != dev.gyroEnabled) dev.gyroEnabled = SDL_SetGamepadSensorEnabled(dev.pad, SDL_SENSOR_GYRO, gyro) && gyro;
        if (dev.gyroEnabled) {
            float values[3]{};
            if (SDL_GetGamepadSensorData(dev.pad, SDL_SENSOR_GYRO, values, 3))
                sink.motion(gyroMotion({-values[1], -values[0]}, dev.profile.gyroSpeed, dt));
        }
    }
    ControllerRuntime &q;
    InputSink &sink;
    ProfileStore store;
    RuntimeEnvironment environment;
    QTimer sample, priority;
    QElapsedTimer elapsed;
    qint64 lastDiscover = -1000, lastSample = 0;
    std::map<SDL_JoystickID, Device> devices;
    GamePriorityState game;
    quint64 revision = 1;
    bool initialized = false, context = true;
    QString contextReason, error;
    int fd = -1;
    QList<int> watches;
    QSet<QString> watchedPaths;
    std::unique_ptr<QSocketNotifier> notifier;
};
ControllerRuntime::ControllerRuntime(InputSink &sink, QString path, QObject *parent, RuntimeEnvironment environment)
    : QObject(parent), d(std::make_unique<Private>(*this, sink, std::move(path), std::move(environment))) {}
ControllerRuntime::~ControllerRuntime() = default;
void ControllerRuntime::start() {
    d->store.load(d->error);
    refreshPriority();
    d->priority.start();
    d->sample.start();
    tick();
}
void ControllerRuntime::setContext(bool allowed, QString reason) {
    if (d->context == allowed && d->contextReason == reason) return;
    d->context = allowed; d->contextReason = std::move(reason);
    if (!allowed) for (auto &[id, dev] : d->devices) { Q_UNUSED(id); d->release(dev); }
    tick();
}
void ControllerRuntime::refreshPriority() {
    QSet<QString> paths;
    for (const auto &[id, dev] : d->devices) { Q_UNUSED(id); for (const auto &path : dev.paths) paths.insert(path); }
    const auto game = inspectGamePriority(paths, d->environment.procRoot);
    const bool steamChanged = game.steam != d->game.steam;
    d->game = game;
    if (game.steam && d->initialized) d->close();
    if (steamChanged) d->bump();
    for (auto &[id, dev] : d->devices) {
        Q_UNUSED(id);
        for (const auto &path : dev.paths) if (game.busyPaths.contains(path)) {
            d->release(dev);
            if (dev.reason != "game") { dev.reason = "game"; d->bump(); }
            break;
        }
    }
}
void ControllerRuntime::tick() {
    if (d->game.steam) { d->sample.setInterval(250); return; }
    if (!d->initialized) {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) { d->error = "controller-backend-unavailable"; d->sample.setInterval(1000); return; }
        d->initialized = true;
        d->lastDiscover = -1000;
    }
    SDL_UpdateGamepads();
    const qint64 now = d->elapsed.elapsed();
    const double dt = double(now - d->lastSample) / 1000.0;
    d->lastSample = now;
    if (now - d->lastDiscover >= 1000) { d->lastDiscover = now; d->discover(); }
    for (auto &[id, device] : d->devices) d->process(id, device, dt);
    d->sample.setInterval(d->devices.empty() ? 250 : 16);
}
QString ControllerRuntime::snapshot() const {
    QJsonArray rows;
    for (const auto &id : d->store.ids()) {
        auto row = d->store.description(id);
        row["id"] = id;
        row["config"] = profileJson(d->store.profile(id));
        row["connected"] = false;
        row["reason"] = d->game.steam ? "steam" : "disconnected";
        for (const auto &[instance, dev] : d->devices) { Q_UNUSED(instance); if (dev.id == id) { row = dev.description; row["config"] = profileJson(dev.profile); row["connected"] = true; row["reason"] = dev.reason; break; } }
        rows.append(row);
    }
    return json({{"schemaVersion", 1}, {"revision", QString::number(d->revision)},
                 {"steam", d->game.steam}, {"error", d->error}, {"controllers", rows}});
}
QString ControllerRuntime::apply(const QString &id, const QString &patch, quint64 expected) {
    const auto result = [this](bool ok, const QString &reason) { return json({{"ok", ok}, {"reason", reason}, {"revision", QString::number(d->revision)}}); };
    if (expected != d->revision) return result(false, "revision-stale");
    if (!d->store.ids().contains(id) || patch.toUtf8().size() > 8192) return result(false, "invalid-config");
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(patch.toUtf8(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) return result(false, "invalid-config");
    auto profile = d->store.profile(id);
    QString reason;
    if (!applyPatch(doc.object(), profile, reason)) return result(false, reason);
    const auto before = d->store;
    d->store.set(id, profile);
    if (!d->store.save(reason)) { d->store = before; return result(false, reason); }
    for (auto &[instance, device] : d->devices) { Q_UNUSED(instance); if (device.id == id) { d->release(device); device.profile = profile; } }
    d->bump();
    return result(true, "ok");
}
QString ControllerRuntime::resetProfile(const QString &id, quint64 expected) {
    return apply(id, json(profileJson(defaultProfile())), expected);
}
} // namespace QindaQt::Controllers
