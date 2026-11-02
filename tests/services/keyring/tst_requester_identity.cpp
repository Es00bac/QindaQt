// SPDX-License-Identifier: GPL-3.0-or-later
#include "requester_identity.h"
#include <QDBusConnectionInterface>
#include <QtTest>
#include <unistd.h>
using namespace qindaqt::keyring::service;
class RequesterIdentityTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void busCredentialsIdentifyActualProcess() {
        const auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        const auto identity = identifyRequester(bus, bus.baseService());
        QCOMPARE(identity.pid, static_cast<quint32>(getpid()));
        QCOMPARE(identity.uid, static_cast<quint32>(geteuid()));
        QVERIFY(identity.started > 0); QVERIFY(identity.executable.startsWith('/'));
        QVERIFY(identity.parents.size() <= 6);
        QVERIFY(requesterUnchanged(bus, bus.baseService(), identity));
        auto changed = identity; ++changed.started;
        QVERIFY(!requesterUnchanged(bus, bus.baseService(), changed));
        changed = identity; changed.executable = "/spoofed/agent";
        QVERIFY(!requesterUnchanged(bus, bus.baseService(), changed));
        QVERIFY(identifyRequester(bus, "org.freedesktop.DBus").executable.isEmpty());
    }
    void disconnectedOwnerCannotApproveCapturedIdentity() {
        const auto bus = QDBusConnection::sessionBus();
        QString owner; RequesterIdentity identity;
        {
            auto peer = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "requester-test");
            owner = peer.baseService(); identity = identifyRequester(bus, owner);
            QVERIFY(identity.started > 0);
        }
        QDBusConnection::disconnectFromBus("requester-test");
        QTRY_VERIFY(!requesterUnchanged(bus, owner, identity));
    }
};
QTEST_GUILESS_MAIN(RequesterIdentityTest)
#include "tst_requester_identity.moc"
