// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
// SPDX-FileCopyrightText: 2026 QindaQt contributors
// Adapted from xdg-desktop-portal-kde 6.6.6 src/inputcapture.cpp and
// src/inputcapturebarrier.cpp (9a5cc0e8). KGlobalAccel, KNotification, the
// Plasma dialog, qGuiApp screens and blocking KWin calls are replaced by native
// consent, compositor zones and asynchronous calls on the selected owner.
#include <qindaqt/services/portal/remote_input/input_capture_adaptor.h>
#include "remote_sessions_p.h"
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QPointF>
#include <QPointer>
#include <QSet>

namespace {
struct Zone { uint width = 0, height = 0; int x = 0, y = 0; };
QDBusArgument &operator<<(QDBusArgument &argument, const Zone &zone) {
    argument.beginStructure(); argument << zone.width << zone.height << zone.x << zone.y; argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, Zone &zone) {
    argument.beginStructure(); argument >> zone.width >> zone.height >> zone.x >> zone.y; argument.endStructure();
    return argument;
}
}
Q_DECLARE_METATYPE(Zone)

namespace QindaQt::Services::Portal::RemoteInput {
namespace {
constexpr auto kManagerPath = "/org/kde/KWin/EIS/InputCapture";
constexpr auto kManager = "org.kde.KWin.EIS.InputCaptureManager";
constexpr auto kCapture = "org.kde.KWin.EIS.InputCapture";
constexpr auto kPortal = "org.freedesktop.impl.portal.InputCapture";
constexpr int kDisabled = 0, kEnabled = 1, kActivated = 2;
std::optional<quint32> requestedCapabilities(const QVariantMap &options) {
    const auto it = options.constFind(QStringLiteral("capabilities"));
    if (it == options.cend() || it->metaType() != QMetaType::fromType<uint>()) return std::nullopt;
    const quint32 capabilities = it->toUInt();
    if (!capabilities || (capabilities & ~kAllDeviceTypes)) return std::nullopt;
    return capabilities;
}
std::optional<AccessQuestion> consentQuestion(const QString &app, const QString &parent, quint32 capabilities) {
    QStringList names;
    if (capabilities & Keyboard) names << QStringLiteral("keyboard");
    if (capabilities & Pointer) names << QStringLiteral("pointer");
    if (capabilities & Touchscreen) names << QStringLiteral("touchscreen");
    return accessQuestion(app, parent, QStringLiteral("Allow input capture?"),
        QStringLiteral("Captured devices: %1").arg(names.join(QStringLiteral(", "))),
        QStringLiteral("When the pointer crosses an edge the application chooses, your input goes only to "
                       "that application until it releases it, you press Meta+Shift+Escape or the screen locks."), {});
}
}
std::optional<QPair<QPoint, QPoint>> pointerBarrier(int x1, int y1, int x2, int y2, const QList<QRect> &zones) {
    // A barrier must fully cover one outer edge of exactly one zone and may not
    // lie between zones. Diagonals are refused.
    if (x1 != x2 && y1 != y2) return std::nullopt;
    bool found = false, transpose = false, farEdge = false;
    if (x1 != x2) { std::swap(x1, y1); std::swap(x2, y2); transpose = true; }
    if (y1 > y2) std::swap(y1, y2);
    for (auto geometry : zones) {
        if (transpose) { geometry = geometry.transposed(); geometry.moveTo(geometry.y(), geometry.x()); }
        if (y1 > geometry.bottom() || geometry.y() > y2) continue;
        if (x1 == geometry.x() || x1 == geometry.x() + geometry.width()) {
            if (y1 != geometry.y() || y2 != geometry.bottom() || found) return std::nullopt;
            found = true;
            farEdge = x1 == geometry.x() + geometry.width();
        }
    }
    if (!found) return std::nullopt;
    // Right/bottom edges arrive one past the zone; the compositor expects the
    // last pixel row/column, like left/top barriers.
    if (farEdge) { --x1; --x2; }
    if (transpose) { std::swap(x1, y1); std::swap(x2, y2); }
    return QPair<QPoint, QPoint>{{x1, y1}, {x2, y2}};
}
class InputCaptureAdaptor::Private {
public:
    InputCaptureAdaptor *q;
    RequestRegistry &requests;
    AccessConsent &consent;
    CompositorEis &eis;
    QDBusConnection bus;
    RemoteSessions sessions;
    CompositorSignals sink;
    QHash<RequestToken, QString> asking;
    QSet<RequestToken> starting;
    QString zonesOwner;
    quint32 zoneSet = 1;
    Private(InputCaptureAdaptor *adaptor, RequestRegistry &r, AccessConsent &c, CompositorEis &e, QDBusConnection connection)
        : q(adaptor), requests(r), consent(c), eis(e), bus(connection), sessions(std::move(connection), r) {}
    RequestToken begin(const QDBusMessage &call, const QString &handle, const QString &path, const QString &app) {
        const auto slot = std::make_shared<RequestToken>(0);
        const auto token = requests.begin(call, handle, app, [this, slot, path](RequestResponse response) {
            asking.remove(*slot);
            if (starting.remove(*slot) && response != RequestResponse::Success) {
                consent.cancel(*slot);
                sessions.close(path);
            }
        }, 100000);
        *slot = token;
        return token;
    }
    // Requires the authenticated caller's armed session and current authority.
    RemoteSessions::Entry *armed(const QDBusMessage &call, const QString &path, const QString &app) {
        auto *entry = sessions.entry(path);
        if (!entry || entry->capture.isEmpty() || !sessions.authenticated(call, path, app) || !consent.admitted()) return nullptr;
        return entry;
    }
    void reply(const QDBusMessage &call, bool success) {
        bus.send(call.createReply(QVariantList{success ? 0U : 2U, QVariant::fromValue(QVariantMap{})}));
    }
    void notify(const RemoteSessions::Entry &entry, const QString &path, const char *name, const QVariantMap &options) {
        auto signal = QDBusMessage::createTargetedSignal(entry.frontend, QStringLiteral("/org/freedesktop/portal/desktop"),
                                                         QLatin1String(kPortal), QLatin1String(name));
        signal << QVariant::fromValue(QDBusObjectPath(path)) << options;
        bus.send(signal);
    }
    QString pathForCapture(const QString &capture) const {
        for (const auto &path : sessions.paths()) if (const auto *e = sessions.find(path); e && e->capture == capture) return path;
        return {};
    }
    void subscribe(const QString &owner, const QString &capture, bool on) {
        for (const auto *member : {"disabled", "activated", "deactivated"}) {
            if (on) bus.connect(owner, capture, QLatin1String(kCapture), QLatin1String(member), &sink, SLOT(received(QDBusMessage)));
            else bus.disconnect(owner, capture, QLatin1String(kCapture), QLatin1String(member), &sink, SLOT(received(QDBusMessage)));
        }
        if (on && zonesOwner != owner) {
            if (!zonesOwner.isEmpty())
                bus.disconnect(zonesOwner, QLatin1String(kManagerPath), QLatin1String(kManager), QStringLiteral("zonesChanged"), &sink, SLOT(received(QDBusMessage)));
            zonesOwner = owner;
            bus.connect(owner, QLatin1String(kManagerPath), QLatin1String(kManager), QStringLiteral("zonesChanged"), &sink, SLOT(received(QDBusMessage)));
        }
    }
    void compositorSignal(const QDBusMessage &message) {
        // AGENT-GUARD: only the selected compositor's own capture objects drive
        // activation state; another peer cannot forge Activated for a session.
        if (message.service() != eis.compositor()) return;
        if (message.member() == QLatin1String("zonesChanged") && message.path() == QLatin1String(kManagerPath)) {
            ++zoneSet;
            for (const auto &path : sessions.paths()) {
                auto *entry = sessions.entry(path);
                if (!entry || entry->capture.isEmpty()) continue;
                if (entry->captureState != kDisabled)
                    eis.call(entry->capture, QLatin1String(kCapture), QStringLiteral("disable"), {}, [](const QDBusMessage &, const QString &) {});
                entry->captureState = kDisabled; entry->barriers.clear(); entry->zones.clear();
                notify(*entry, path, "ZonesChanged", {{QStringLiteral("zone_set"), zoneSet}});
            }
            return;
        }
        const QString path = pathForCapture(message.path());
        auto *entry = sessions.entry(path);
        if (!entry) return;
        const auto args = message.arguments();
        if (message.member() == QLatin1String("disabled")) {
            entry->captureState = kDisabled;
            notify(*entry, path, "Disabled", {});
        } else if (message.member() == QLatin1String("activated") && args.size() == 2) {
            entry->captureState = kActivated;
            const QPointF position = qdbus_cast<QPointF>(args.at(1));
            notify(*entry, path, "Activated", {{QStringLiteral("activation_id"), args.at(0).toUInt()},
                                               {QStringLiteral("cursor_position"), QVariant::fromValue(position)}});
        } else if (message.member() == QLatin1String("deactivated") && args.size() == 1) {
            if (entry->captureState == kActivated) entry->captureState = kEnabled;
            notify(*entry, path, "Deactivated", {{QStringLiteral("activation_id"), args.at(0).toUInt()}});
        }
    }
    void consentCompleted(RequestToken token, RequestResponse response) {
        const QString path = asking.take(token);
        if (path.isEmpty()) return;
        auto *entry = sessions.entry(path);
        if (!entry || !sessions.live(path) || !requests.live(token) || !consent.admitted()) response = RequestResponse::Failed;
        if (response != RequestResponse::Success) { requests.finish(token, response); return; }
        const quint32 capabilities = entry->devices;
        const QPointer<InputCaptureAdaptor> guard(q);
        const bool sent = eis.call(QLatin1String(kManagerPath), QLatin1String(kManager), QStringLiteral("addInputCapture"),
            {capabilities}, [this, guard, token, path, capabilities](const QDBusMessage &reply, const QString &owner) {
                const QString capture = reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1
                    ? qdbus_cast<QDBusObjectPath>(reply.arguments().at(0)).path() : QString{};
                if (!guard) return;
                auto *entry = sessions.entry(path);
                if (!entry || capture.isEmpty() || !requests.live(token) || !sessions.live(path) || !consent.admitted()
                    || owner != eis.compositor()) {
                    // A late or orphaned capture object is removed, never armed.
                    if (!capture.isEmpty())
                        eis.call(QLatin1String(kManagerPath), QLatin1String(kManager), QStringLiteral("removeInputCapture"),
                                 {QVariant::fromValue(QDBusObjectPath(capture))}, [](const QDBusMessage &, const QString &) {});
                    requests.finish(token, RequestResponse::Failed);
                    return;
                }
                entry->capture = capture;
                entry->phase = RemotePhase::Started;
                entry->pending = 0;
                subscribe(owner, capture, true);
                requests.finish(token, RequestResponse::Success, {{QStringLiteral("capabilities"), capabilities}});
            });
        if (!sent) requests.finish(token, RequestResponse::Failed);
    }
    void retired(const QString &, const RemoteSessions::Entry &entry) {
        if (entry.capture.isEmpty()) return;
        subscribe(eis.compositor(), entry.capture, false);
        eis.call(QLatin1String(kManagerPath), QLatin1String(kManager), QStringLiteral("removeInputCapture"),
                 {QVariant::fromValue(QDBusObjectPath(entry.capture))}, [](const QDBusMessage &, const QString &) {});
    }
};
InputCaptureAdaptor::InputCaptureAdaptor(QObject &host, RequestRegistry &requests, AccessConsent &consent,
                                         CompositorEis &eis, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), d(std::make_unique<Private>(this, requests, consent, eis, std::move(bus))) {
    registerAccessTypes();
    qDBusRegisterMetaType<Zone>();
    qDBusRegisterMetaType<QList<Zone>>();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    qDBusRegisterMetaType<QPair<QPoint, QPoint>>();
    qDBusRegisterMetaType<QList<QPair<QPoint, QPoint>>>();
    d->sink.handler = [this](const QDBusMessage &message) { d->compositorSignal(message); };
    connect(&consent, &AccessConsent::completed, this,
            [this](RequestToken token, RequestResponse response, const ChoiceValues &) { d->consentCompleted(token, response); });
    // Native lock, lock uncertainty or selected-session loss ends every session.
    connect(&consent, &AccessConsent::authorityLost, this, [this] { d->sessions.clear(); });
    connect(&d->sessions, &RemoteSessions::retired, this,
            [this](const QString &path, const RemoteSessions::Entry &entry) { d->retired(path, entry); });
}
InputCaptureAdaptor::~InputCaptureAdaptor() { d->sessions.clear(); }
quint32 InputCaptureAdaptor::CreateSession(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app,
    const QString &parent, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const auto token = d->begin(call, handle.path(), session.path(), app);
    if (!token) return 2;
    const auto capabilities = requestedCapabilities(options);
    if (!d->consent.admitted() || !capabilities || !d->sessions.create(call, handle.path(), session.path(), app)) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    d->starting.insert(token);
    auto *entry = d->sessions.entry(session.path());
    const auto question = consentQuestion(app, parent, *capabilities);
    if (!question) { d->requests.finish(token, RequestResponse::Failed); return 2; }
    entry->devices = *capabilities;
    entry->phase = RemotePhase::Starting;
    entry->pending = token;
    d->asking.insert(token, session.path());
    d->consent.ask(token, *question);
    return 2;
}
quint32 InputCaptureAdaptor::GetZones(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app,
    const QVariantMap &, const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const auto token = d->begin(call, handle.path(), session.path(), app);
    if (!token) return 2;
    const QString path = session.path();
    if (!d->armed(call, path, app) || !d->sessions.requestMatches(path, handle.path())) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    const QPointer<InputCaptureAdaptor> guard(this);
    const bool sent = d->eis.call(QLatin1String(kManagerPath), QLatin1String(kManager), QStringLiteral("zones"), {},
        [this, guard, token, path](const QDBusMessage &reply, const QString &owner) {
            if (!guard) return;
            auto *entry = d->sessions.entry(path);
            const auto rects = reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1
                ? qdbus_cast<QList<QRect>>(reply.arguments().at(0)) : QList<QRect>{};
            if (!entry || !d->requests.live(token) || owner != d->eis.compositor() || rects.isEmpty() || rects.size() > 32) {
                d->requests.finish(token, RequestResponse::Failed);
                return;
            }
            QList<Zone> zones;
            for (const auto &rect : rects) {
                if (rect.width() <= 0 || rect.height() <= 0) { d->requests.finish(token, RequestResponse::Failed); return; }
                zones.append({static_cast<uint>(rect.width()), static_cast<uint>(rect.height()), rect.x(), rect.y()});
            }
            entry->zones = rects;
            entry->zoneSet = d->zoneSet;
            d->requests.finish(token, RequestResponse::Success, {{QStringLiteral("zones"), QVariant::fromValue(zones)},
                                                                 {QStringLiteral("zone_set"), d->zoneSet}});
        });
    if (!sent) d->requests.finish(token, RequestResponse::Failed);
    return 2;
}
quint32 InputCaptureAdaptor::SetPointerBarriers(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &app,
    const QVariantMap &, const QList<QVariantMap> &barriers, uint zoneSet, const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const auto token = d->begin(call, handle.path(), session.path(), app);
    if (!token) return 2;
    auto *entry = d->armed(call, session.path(), app);
    if (!entry || !d->sessions.requestMatches(session.path(), handle.path()) || entry->zones.isEmpty()
        || zoneSet != entry->zoneSet || barriers.size() > 64) {
        d->requests.finish(token, RequestResponse::Failed);
        return 2;
    }
    // New barriers never apply to an armed capture; the client must Enable again.
    if (entry->captureState != kDisabled)
        d->eis.call(entry->capture, QLatin1String(kCapture), QStringLiteral("disable"), {}, [](const QDBusMessage &, const QString &) {});
    entry->captureState = kDisabled;
    entry->barriers.clear();
    QList<uint> failed;
    for (const auto &barrier : barriers) {
        const auto id = barrier.value(QStringLiteral("barrier_id"));
        const auto position = barrier.value(QStringLiteral("position"));
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        bool valid = id.metaType() == QMetaType::fromType<uint>() && id.toUInt() != 0
            && position.metaType() == QMetaType::fromType<QDBusArgument>()
            && position.value<QDBusArgument>().currentSignature() == QLatin1String("(iiii)");
        if (valid) {
            const auto argument = position.value<QDBusArgument>();
            argument.beginStructure(); argument >> x1 >> y1 >> x2 >> y2; argument.endStructure();
            if (const auto made = pointerBarrier(x1, y1, x2, y2, entry->zones)) entry->barriers.append(*made);
            else valid = false;
        }
        if (!valid) failed.append(id.toUInt());
    }
    d->requests.finish(token, RequestResponse::Success, {{QStringLiteral("failed_barriers"), QVariant::fromValue(failed)}});
    return 2;
}
quint32 InputCaptureAdaptor::Enable(const QDBusObjectPath &session, const QString &app, const QVariantMap &,
                                    const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const QString path = session.path();
    auto *entry = d->armed(call, path, app);
    if (!entry || entry->captureState != kDisabled || entry->barriers.isEmpty()) return 2;
    call.setDelayedReply(true);
    const QPointer<InputCaptureAdaptor> guard(this);
    if (!d->eis.call(entry->capture, QLatin1String(kCapture), QStringLiteral("enable"), {QVariant::fromValue(entry->barriers)},
            [this, guard, call, path](const QDBusMessage &reply, const QString &owner) {
                if (!guard) return;
                auto *entry = d->sessions.entry(path);
                const bool ok = entry && reply.type() == QDBusMessage::ReplyMessage && owner == d->eis.compositor()
                    && d->sessions.live(path) && d->consent.admitted();
                if (ok) entry->captureState = kEnabled;
                d->reply(call, ok);
            }))
        d->reply(call, false);
    return 2;
}
quint32 InputCaptureAdaptor::Disable(const QDBusObjectPath &session, const QString &app, const QVariantMap &,
                                     const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    auto *entry = d->armed(call, session.path(), app);
    if (!entry || entry->captureState == kDisabled) return 2;
    call.setDelayedReply(true);
    const QPointer<InputCaptureAdaptor> guard(this);
    if (!d->eis.call(entry->capture, QLatin1String(kCapture), QStringLiteral("disable"), {},
            [this, guard, call](const QDBusMessage &reply, const QString &) {
                if (guard) d->reply(call, reply.type() == QDBusMessage::ReplyMessage);
            }))
        d->reply(call, false);
    return 2;
}
quint32 InputCaptureAdaptor::Release(const QDBusObjectPath &session, const QString &app, const QVariantMap &options,
                                     const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    auto *entry = d->armed(call, session.path(), app);
    if (!entry || entry->captureState != kActivated) return 2;
    const auto position = options.value(QStringLiteral("cursor_position"));
    const bool apply = position.isValid();
    if (apply && (position.metaType() != QMetaType::fromType<QDBusArgument>()
                  || position.value<QDBusArgument>().currentSignature() != QLatin1String("(dd)"))) return 2;
    const QPointF cursor = apply ? qdbus_cast<QPointF>(position) : QPointF{};
    call.setDelayedReply(true);
    const QPointer<InputCaptureAdaptor> guard(this);
    if (!d->eis.call(entry->capture, QLatin1String(kCapture), QStringLiteral("release"), {cursor, apply},
            [this, guard, call](const QDBusMessage &reply, const QString &) {
                if (guard) d->reply(call, reply.type() == QDBusMessage::ReplyMessage);
            }))
        d->reply(call, false);
    return 2;
}
QDBusUnixFileDescriptor InputCaptureAdaptor::ConnectToEIS(const QDBusObjectPath &session, const QString &app,
                                                          const QVariantMap &, const QDBusMessage &call) {
    call.setDelayedReply(true);
    const QString path = session.path();
    auto *entry = d->armed(call, path, app);
    // One receiver per capture, connected while disabled, as upstream requires.
    if (!entry || entry->captureState != kDisabled || entry->cookie) {
        d->bus.send(call.createErrorReply(QStringLiteral("org.freedesktop.portal.Error.NotAllowed"), QStringLiteral("Input capture refused")));
        return {};
    }
    entry->cookie = 1;
    const QPointer<InputCaptureAdaptor> guard(this);
    if (!d->eis.call(entry->capture, QLatin1String(kCapture), QStringLiteral("connectToEIS"), {},
            [this, guard, call, path](const QDBusMessage &reply, const QString &owner) {
                if (!guard) return;
                auto *entry = d->sessions.entry(path);
                const auto fd = reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1
                    ? qdbus_cast<QDBusUnixFileDescriptor>(reply.arguments().at(0)) : QDBusUnixFileDescriptor{};
                if (!entry || !fd.isValid() || owner != d->eis.compositor() || !d->sessions.live(path) || !d->consent.admitted()
                    || !d->requests.authenticated(call)) {
                    if (entry) entry->cookie = 0;
                    d->bus.send(call.createErrorReply(QStringLiteral("org.freedesktop.portal.Error.Failed"), QStringLiteral("Input capture refused")));
                    return;
                }
                d->bus.send(call.createReply(QVariant::fromValue(fd)));
            })) {
        entry->cookie = 0;
        d->bus.send(call.createErrorReply(QStringLiteral("org.freedesktop.portal.Error.Failed"), QStringLiteral("Input capture refused")));
    }
    return {};
}
} // namespace QindaQt::Services::Portal::RemoteInput
