// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/session_binding.h>

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::Portal;

class PortalSessionOwnerTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void selectedCallerLossRetiresOneBackendLifetime() {
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        auto backend = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                      QStringLiteral("portal-owner-backend"));
        QVERIFY(backend.isConnected());
        PortalSessionBinding binding(backend, runtime.path());
        QVERIFY(binding.start());
        QSignalSpy lost(&binding, &PortalSessionBinding::selectedSessionOwnerLost);

        auto request = QDBusMessage::createMethodCall(
            QString::fromLatin1(kNativePortalService),
            QString::fromLatin1(kNativePortalPath),
            QString::fromLatin1(kNativePortalService),
            QStringLiteral("AttachSessionWithDisplay"));
        request << QStringLiteral("qindaqt-7");

        // The named connection closes only after every local reference dies.
        {
            auto caller = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                         QStringLiteral("portal-owner-first"));
            QVERIFY(caller.isConnected());
            QDBusPendingCallWatcher first(caller.asyncCall(request, 1000));
            QTRY_VERIFY_WITH_TIMEOUT(first.isFinished(), 2000);
            const QDBusPendingReply<bool> firstReply = first;
            QVERIFY(!firstReply.isError());
            QVERIFY(!firstReply.value()); // No private compositor in this short test.
            QCOMPARE(lost.count(), 0);
        }

        QDBusConnection::disconnectFromBus(QStringLiteral("portal-owner-first"));
        QTRY_COMPARE_WITH_TIMEOUT(lost.count(), 1, 2000);

        auto replacement = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, QStringLiteral("portal-owner-second"));
        QVERIFY(replacement.isConnected());
        QDBusPendingCallWatcher second(replacement.asyncCall(request, 1000));
        QTRY_VERIFY_WITH_TIMEOUT(second.isFinished(), 2000);
        const QDBusPendingReply<bool> secondReply = second;
        QVERIFY(!secondReply.isError());
        QVERIFY(!secondReply.value());
        QCOMPARE(lost.count(), 1);
        QDBusConnection::disconnectFromBus(QStringLiteral("portal-owner-second"));
        binding.stop();
        QDBusConnection::disconnectFromBus(QStringLiteral("portal-owner-backend"));
    }
};

QTEST_GUILESS_MAIN(PortalSessionOwnerTest)
#include "tst_session_owner_lifetime.moc"
