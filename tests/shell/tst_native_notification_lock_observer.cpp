// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_notification_lock_observer.h"
#include "../services/session_lock_state/support/private_session_bus.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/services/notification_presentation_policy/notification_privacy_policy.h>
#include <qindaqt/services/notification_presentation_model/notification_list_model.h>
#include <QDBusContext>
#include <QDBusMessage>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using QindaQt::Shell::NativeNotificationLockObserver;
using QindaQt::TestSupport::PrivateSessionBus;
namespace {
QString compositorService() { return QString(QindaQt::CompositorNames::service); }
QString nativePath() { return QString(QindaQt::CompositorNames::nativeLockPath); }
QString nativeInterface() { return QString(QindaQt::CompositorNames::nativeLockInterface); }

// A real kernel peer and real bus-daemon identities exercise the composition
// seam. It deliberately advertises no legacy locker on the compositor owner.
class OrdinaryListener final {
public:
    explicit OrdinaryListener(const QString &path) : pathname(path) {
        fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return;
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const auto bytes = QFile::encodeName(path);
        if (bytes.size() >= qsizetype(sizeof(address.sun_path))) return;
        std::memcpy(address.sun_path, bytes.constData(), size_t(bytes.size()) + 1);
        ready = bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 &&
                listen(fd, 16) == 0 &&
                QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    ~OrdinaryListener() { if (fd >= 0) close(fd); QFile::remove(pathname); }
    QString pathname;
    int fd = -1;
    bool ready = false;
};
class NativeBackend final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.NativeLock1")
public:
    explicit NativeBackend(QDBusConnection connection) : bus(std::move(connection)) {}
    bool expose() {
        return bus.registerObject(nativePath(), this,
                QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals) &&
                bus.registerService(compositorService());
    }
    void setState(bool nextLocked, bool nextProtected) {
        locked = nextLocked;
        protectedPresentation = nextProtected;
        Q_EMIT lockedChanged(locked);
        Q_EMIT protectedChanged(protectedPresentation);
    }
    bool withhold = false;
public Q_SLOTS:
    void RequestStateWithReceipt(const QString &nonce) {
        if (withhold) return;
        auto receipt = QDBusMessage::createTargetedSignal(message().service(),
                nativePath(), nativeInterface(), QStringLiteral("stateReceipt"));
        receipt << nonce << locked << protectedPresentation;
        bus.send(receipt);
    }
Q_SIGNALS:
    void lockedChanged(bool value);
    void protectedChanged(bool value);
private:
    QDBusConnection bus;
    bool locked = false, protectedPresentation = false;
};
}

class NativeNotificationLockObserverTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void nativeReceiptWithSeparateSupervisorOwner() {
        PrivateSessionBus fixture;
        QVERIFY(fixture.start());
        auto server = fixture.connect("native-shell-server-"),
             session = fixture.connect("native-shell-session-"),
             client = fixture.connect("native-shell-client-");
        QVERIFY(session.registerService(QStringLiteral("org.qindaqt.Session1")));
        // Native Lock1 facades reside with the supervisor, not the compositor.
        QVERIFY(session.registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
        QVERIFY(session.registerService(QStringLiteral("org.kde.screensaver")));
        QVERIFY(session.baseService() != server.baseService());
        NativeBackend backend(server);
        QVERIFY(backend.expose());
        QTemporaryDir runtime;
        const auto basename = QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
        OrdinaryListener listener(runtime.filePath(basename));
        QVERIFY(listener.ready);
        NativeNotificationLockObserver observer(client, getpid(), runtime.path(), basename);
        QVERIFY(!observer.contentMayBeShown());
        QSignalSpy changed(&observer, &NativeNotificationLockObserver::contentMayBeShownChanged);
        QVERIFY(observer.start());
        QTRY_VERIFY(observer.contentMayBeShown());
        backend.setState(true, false);
        QTRY_VERIFY(!observer.contentMayBeShown());
        backend.setState(true, true);
        QTest::qWait(30);
        QVERIFY(!observer.contentMayBeShown());
        backend.setState(false, false);
        QTRY_VERIFY(observer.contentMayBeShown());
        observer.stop();
        QVERIFY(!observer.contentMayBeShown());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(changed.last().at(0).toBool(), false);
    }
    void wrongTrustedPidCannotOpenPrivacy() {
        PrivateSessionBus fixture;
        QVERIFY(fixture.start());
        auto server = fixture.connect("native-shell-server-"),
             session = fixture.connect("native-shell-session-"),
             client = fixture.connect("native-shell-client-");
        QVERIFY(session.registerService(QStringLiteral("org.qindaqt.Session1")));
        NativeBackend backend(server);
        QVERIFY(backend.expose());
        QTemporaryDir runtime;
        const auto basename = QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
        OrdinaryListener listener(runtime.filePath(basename));
        QVERIFY(listener.ready);
        NativeNotificationLockObserver observer(client, qint64(getpid()) + 1,
                                                runtime.path(), basename);
        QString error;
        QVERIFY(!observer.start(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!observer.contentMayBeShown());
    }
    void lostSessionOwnerClosesBeforeQueuedWatchers() {
        PrivateSessionBus fixture;
        QVERIFY(fixture.start());
        auto server = fixture.connect("native-shell-server-"),
             session = fixture.connect("native-shell-session-"),
             replacement = fixture.connect("native-shell-replacement-"),
             client = fixture.connect("native-shell-client-");
        const auto sessionName = QStringLiteral("org.qindaqt.Session1");
        QVERIFY(session.registerService(sessionName));
        NativeBackend backend(server);
        QVERIFY(backend.expose());
        QTemporaryDir runtime;
        const auto basename = QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
        OrdinaryListener listener(runtime.filePath(basename));
        QVERIFY(listener.ready);
        NativeNotificationLockObserver observer(client, getpid(), runtime.path(), basename);
        QVERIFY(observer.start());
        QTRY_VERIFY(observer.contentMayBeShown());
        QindaQt::Services::NotificationPresentationPolicy::NotificationPrivacyPolicy policy(
            [&] { return observer.contentMayBeShown(); });
        connect(&observer, &NativeNotificationLockObserver::contentMayBeShownChanged,
            &policy, &QindaQt::Services::NotificationPresentationPolicy::NotificationPrivacyPolicy::setPrivatePresentationAllowed);
        policy.setPrivatePresentationAllowed(observer.contentMayBeShown());
        QindaQt::Services::NotificationPresentationModel::NotificationListModel projection(
            [&] { return policy.privatePresentationAllowed(); });
        QindaQt::Services::NotificationPresentation::PresentationNotification privateNotification;
        privateNotification.id = 9;
        privateNotification.summary = QStringLiteral("Private retained title");
        projection.replace({{privateNotification, true}});
        const auto retainedIndex = projection.index(0);
        QVERIFY(projection.data(retainedIndex, projection.SummaryRole).isValid());
        QSignalSpy changed(&observer, &NativeNotificationLockObserver::contentMayBeShownChanged);
        QVERIFY(session.unregisterService(sessionName));
        QVERIFY(replacement.registerService(sessionName));
        // No event-loop turn or prior observer getter: a retained production
        // model pointer must not declassify through its cached role/index.
        QVERIFY(!projection.data(retainedIndex, projection.SummaryRole).isValid());
        QCOMPARE(projection.rowCount(), 0);
        QVERIFY(!policy.privatePresentationAllowed());
        QVERIFY(!observer.contentMayBeShown());
        QTRY_VERIFY(!changed.isEmpty());
        QCOMPARE(changed.last().at(0).toBool(), false);
        backend.setState(false, false);
        QTest::qWait(30);
        QVERIFY(!observer.contentMayBeShown());
    }
    void samePidCompositorReplacementRemainsClosed() {
        PrivateSessionBus fixture;
        QVERIFY(fixture.start());
        auto server = fixture.connect("native-shell-server-"),
             session = fixture.connect("native-shell-session-"),
             replacement = fixture.connect("native-shell-replacement-"),
             client = fixture.connect("native-shell-client-");
        QVERIFY(session.registerService(QStringLiteral("org.qindaqt.Session1")));
        NativeBackend first(server), second(replacement);
        QVERIFY(first.expose());
        QTemporaryDir runtime;
        const auto basename = QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
        OrdinaryListener listener(runtime.filePath(basename));
        QVERIFY(listener.ready);
        NativeNotificationLockObserver observer(client, getpid(), runtime.path(), basename);
        QVERIFY(observer.start());
        QTRY_VERIFY(observer.contentMayBeShown());
        QVERIFY(server.unregisterService(compositorService()));
        QVERIFY(second.expose());
        QVERIFY(!observer.contentMayBeShown());
        QTest::qWait(50);
        QVERIFY(!observer.contentMayBeShown());
    }
    void missingReceiptKeepsInitialUnknown() {
        PrivateSessionBus fixture;
        QVERIFY(fixture.start());
        auto server = fixture.connect("native-shell-server-"),
             session = fixture.connect("native-shell-session-"),
             client = fixture.connect("native-shell-client-");
        QVERIFY(session.registerService(QStringLiteral("org.qindaqt.Session1")));
        NativeBackend backend(server);
        backend.withhold = true;
        QVERIFY(backend.expose());
        QTemporaryDir runtime;
        const auto basename = QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
        OrdinaryListener listener(runtime.filePath(basename));
        QVERIFY(listener.ready);
        NativeNotificationLockObserver observer(client, getpid(), runtime.path(), basename);
        QVERIFY(observer.start());
        QTest::qWait(100);
        QVERIFY(!observer.contentMayBeShown());
    }
};
QTEST_GUILESS_MAIN(NativeNotificationLockObserverTest)
#include "tst_native_notification_lock_observer.moc"
