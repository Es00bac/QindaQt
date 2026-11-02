// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "scripted_phone.h"

#include <qindaqt/services/link_transport/hub.h>

#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>

namespace QindaQt::Link::Test {

// A hub with a throwaway identity and trust store in a QTemporaryDir,
// listening on an ephemeral loopback port.
struct HubFixture {
    explicit HubFixture(SessionTimings timings = {}, int maxUnpaired = 8)
        : store(dir.path()), hub(generateIdentity(QStringLiteral("test-desktop")), store, options(timings, maxUnpaired))
    {
        store.load();
        listening = hub.listen(QHostAddress::LocalHost, 0);
    }
    static HubOptions options(SessionTimings t, int maxUnpaired)
    {
        HubOptions o;
        o.local = {QStringLiteral("desktop"), QStringLiteral("test"), QStringLiteral("QindaQt"),
                   QStringLiteral("Test OS"), QStringLiteral("0.0.0"), {}, false};
        o.timings = t;
        o.maxUnpaired = maxUnpaired;
        return o;
    }
    [[nodiscard]] quint16 port() const { return hub.port(); }

    // Full successful pairing; the phone ends READY.
    bool pair(ScriptedPhone &phone, const QStringList &caps = {QStringLiteral("status")})
    {
        QSignalSpy requested(&hub, &Hub::pairingRequested);
        if (!phone.connectTo(port()) || !phone.waitFor(QStringLiteral("hello"))) return false;
        phone.sendHello(false, caps);
        QString code;
        if (!phone.pairUntilCode(&code) || !waitUntil([&] { return !requested.isEmpty(); })) return false;
        if (requested.first().at(3).toString() != code) return false;
        hub.confirmPairing(phone.deviceId(), true);
        Envelope c;
        if (!phone.waitFor(QStringLiteral("pair.confirm"), &c) || !c.b.value(QStringLiteral("accept")).toBool()) return false;
        Envelope accept;
        accept.t = QStringLiteral("pair.confirm");
        accept.b = QJsonObject{{QStringLiteral("accept"), true}};
        phone.send(accept);
        return waitUntil([&] { return store.contains(phone.deviceId()); });
    }

    QTemporaryDir dir;
    TrustStore store;
    Hub hub;
    bool listening = false;
};

} // namespace QindaQt::Link::Test
