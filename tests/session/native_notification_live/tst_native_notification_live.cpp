// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_popup_fixture.h"
#include "notificationliveruntime.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Test;
using Services::SessionLockState::LockState;

class NativeNotificationLiveTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QCOMPARE(qEnvironmentVariable("QINDAQT_NATIVE_POPUP_PRIVATE_BUS"), QStringLiteral("1"));
        QVERIFY(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"));
        QVERIFY(qEnvironmentVariableIsEmpty("DISPLAY"));
        QVERIFY(!QFileInfo(qEnvironmentVariable("QINDAQT_NATIVE_POPUP_LOCKER")).exists());
    }
    void selectedOwnerLossRetiresActualPopup()
    {
        QString error;
        NativePopupFixture fixture(QStringLiteral("owner-loss"));
        QVERIFY2(fixture.start(&error), qPrintable(error));
        QJsonObject evidence;
        QVERIFY2(fixture.mapPopup(&evidence, &error), qPrintable(error));
        QVERIFY(fixture.session.unregisterService(QStringLiteral("org.qindaqt.Session1")));
        // Query current admission before delivering queued owner notifications.
        QVERIFY(!fixture.attachment->live());
        QCOMPARE(fixture.monitor->state(), LockState::Unknown);
        QVERIFY2(fixture.retired(&evidence, &error), qPrintable(error));
        QVERIFY2(fixture.submitCritical(&error), qPrintable(error));
        processProbeEventsFor(300);
        QVERIFY2(fixture.retired(&evidence, &error), qPrintable(error));
        QVERIFY(!fixture.malformedReceipt);
        QVERIFY(!fixture.receiptNonces.isEmpty());
        evidence.insert(QStringLiteral("ownerLostUnknown"), true);
        evidence.insert(QStringLiteral("liveShellAndHost"), true);
        qInfo().noquote() << "QINDAQT_NATIVE_POPUP_OWNER_LOSS="
                         << QJsonDocument(evidence).toJson(QJsonDocument::Compact);
    }
    void actualNativeProtectionRetiresPopup()
    {
        QString error;
        NativePopupFixture fixture(QStringLiteral("native-lock"));
        QVERIFY2(fixture.start(&error), qPrintable(error));
        QJsonObject evidence;
        QVERIFY2(fixture.mapPopup(&evidence, &error), qPrintable(error));
        const auto receiptsBefore = fixture.receiptNonces.size();
        QDBusInterface native(fixture.compositorOwner, QString(CompositorNames::nativeLockPath),
                              QString(CompositorNames::nativeLockInterface));
        const QDBusReply<bool> reply = native.call(QStringLiteral("RequestLock"));
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        // AGENT-GUARD: absence of the fixed root-managed locker is deliberate.
        // The real fork first locks with black fallback, then reports launcher
        // refusal. No PAM process or fake successful admission is substituted.
        QVERIFY(!reply.value());
        QVERIFY2(awaitNotificationLiveCondition([&] {
            return fixture.monitor->state() == LockState::Locked
                && fixture.monitor->presentationProtected();
        }), "actual current native Protected receipt did not arrive");
        QVERIFY(fixture.receiptNonces.size() > receiptsBefore);
        QVERIFY(!fixture.malformedReceipt);
        QVERIFY(fixture.states.contains(LockState::Unknown)
                || fixture.states.contains(LockState::Locking));
        QVERIFY2(fixture.retired(&evidence, &error), qPrintable(error));
        QVERIFY2(fixture.submitCritical(&error), qPrintable(error));
        processProbeEventsFor(300);
        QVERIFY2(fixture.retired(&evidence, &error), qPrintable(error));
        evidence.insert(QStringLiteral("currentProtectedNativeReceipt"), true);
        evidence.insert(QStringLiteral("receiptNonceCount"), fixture.receiptNonces.size());
        evidence.insert(QStringLiteral("clientlessBlackProtection"), true);
        evidence.insert(QStringLiteral("lockerLaunchRefused"), true);
        evidence.insert(QStringLiteral("liveShellAndHost"), true);
        qInfo().noquote() << "QINDAQT_NATIVE_POPUP_LOCK="
                         << QJsonDocument(evidence).toJson(QJsonDocument::Compact);
    }
};
QTEST_GUILESS_MAIN(NativeNotificationLiveTest)
#include "tst_native_notification_live.moc"
