// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/access_adaptor.h>
#include <qindaqt/services/portal/process_consent.h>
#include <qindaqt/services/portal/email_adaptor.h>
#include <qindaqt/application_catalog/application_directory_scan.h>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <fcntl.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusPendingCallWatcher>
#include <QDBusConnectionInterface>
#include <QDBusPendingReply>
#include <QDBusInterface>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <signal.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
class ExportProcess final : public QProcess {
public:
    ~ExportProcess() override { if (state() != QProcess::NotRunning) { kill(); waitForFinished(1000); } }
};
class NativeConsentTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        registerAccessTypes();
        QVERIFY(bus.isConnected()); QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        backend = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("native-portal-backend")));
        QVERIFY(backend->isConnected());
        QTRY_VERIFY(bus.interface()->serviceOwner(QString(QindaQt::CompositorNames::service)).isValid());
        binding = std::make_unique<PortalSessionBinding>(*backend, qEnvironmentVariable("XDG_RUNTIME_DIR"));
        QVERIFY(binding->start());
        auto call = QDBusMessage::createMethodCall(QString::fromLatin1(kNativePortalService), QString::fromLatin1(kNativePortalPath),
            QString::fromLatin1(kNativePortalService), QStringLiteral("AttachSessionWithDisplay"));
        call << QStringLiteral("qindaqt-7");
        QDBusPendingCallWatcher attached(bus.asyncCall(call)); QTRY_VERIFY(attached.isFinished());
        const QDBusPendingReply<bool> result = attached; QVERIFY(!result.isError()); QVERIFY(result.value()); QVERIFY(binding->live());
        registry = std::make_unique<RequestRegistry>(*backend);
        consent = std::make_unique<ProcessAccessConsent>(*binding, *backend, qEnvironmentVariable("QINDAQT_PORTAL_TEST_HELPER"));
        new AccessAdaptor(host, *registry, *consent);
        const QString dataRoot = qEnvironmentVariable("HOME") + QStringLiteral("/mail-data");
        QVERIFY(QDir().mkpath(dataRoot + QStringLiteral("/applications")));
        QFile desktop(dataRoot + QStringLiteral("/applications/org.test.Mail.desktop")); QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write("[Desktop Entry]\nType=Application\nName=Private Mail\nNoDisplay=true\nMimeType=x-scheme-handler/mailto;\nExec="
            + qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL").toUtf8() + " %u\n"); desktop.close();
        QFile defaults(dataRoot + QStringLiteral("/mimeapps.list")); QVERIFY(defaults.open(QIODevice::WriteOnly));
        defaults.write("[Default Applications]\nx-scheme-handler/mailto=org.test.Mail.desktop;\n"); defaults.close();
        auto scan = QindaQt::ApplicationCatalog::scanApplicationDirectories({dataRoot}, QindaQt::ApplicationCatalog::ApplicationVisibility::IncludeNoDisplay);
        QCOMPARE(scan.applications.size(), 1);
        mailStore = std::make_unique<QindaQt::Apps::SettingsDefaultApps::MimeAppsDefaultApplicationsStore>(defaults.fileName(), QStringList{defaults.fileName()}, scan);
        uri = std::make_unique<QindaQt::Services::ApplicationUri::DefaultApplicationUriOpener>(*mailStore, scan,
            qEnvironmentVariable("QINDAQT_PORTAL_TEST_RELAY"), [this](quint64 token) { return registry->live(token) && consent->admitted(); },
            [this] { return binding->openDisplay(); });
        new EmailAdaptor(host, *registry, *uri, [this] { return consent->admitted(); });

        QVERIFY(backend->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), &host, QDBusConnection::ExportAdaptors));
        QVERIFY(backend->registerService(QStringLiteral("org.test.PortalJourney")));
        QTRY_VERIFY(consent->admitted());
    }
    void init() {
        QFile(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT")).remove();
        qputenv("QINDAQT_PORTAL_TEST_MODE", "grant");
    }
    void visibleGrantChoices() {
        const AccessChoices choices{{QStringLiteral("audio"), QStringLiteral("Share sound"), {}, QStringLiteral("true")},
            {QStringLiteral("device"), QStringLiteral("Device"), {{QStringLiteral("one"), QStringLiteral("Private device")}}, QStringLiteral("one")}};
        auto reply = call({}, {{QStringLiteral("choices"), QVariant::fromValue(choices)}});
        QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 10000);
        const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QVERIFY(!result.isError()); QCOMPARE(result.argumentAt<0>(), 0U);
        QCOMPARE(qdbus_cast<ChoiceValues>(result.argumentAt<1>().value(QStringLiteral("choices"))).size(), 2);
        QVERIFY(audit().contains(" ordinary-wayland exact-peer mapped grant"));
    }
    void visibleDeny() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "deny"); auto reply = call();
        QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 10000); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 1U); QVERIFY(result.argumentAt<1>().isEmpty());
        QVERIFY(audit().contains(" ordinary-wayland exact-peer mapped deny"));
    }
    void closeMappedConsent() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "hold"); auto reply = call();
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped hold"), 10000); const auto pid = helperPid(); QVERIFY(pid > 0);
        auto close = QDBusMessage::createMethodCall(QStringLiteral("org.test.PortalJourney"), path,
            QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher closing(bus.asyncCall(close)); QTRY_VERIFY(closing.isFinished());
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 1U); QTRY_VERIFY(kill(pid, 0) < 0);
    }
    void frontendLossRetiresMappedConsent() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "hold"); auto reply = call();
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped hold"), 10000); const auto pid = helperPid();
        QVERIFY(bus.unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 2U); QTRY_VERIFY(kill(pid, 0) < 0);
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
    }
    void foreignInvalidHandleFails() {
        auto reply = call(QStringLiteral("wayland:nonexistent-private-handle"));
        QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 10000); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 2U); QVERIFY(audit().isEmpty());
    }

    void foreignParentGrantAndLoss() {
        ExportProcess exporter;
        const int fd = binding->openDisplay(); QVERIFY(fd >= 0);
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("DISPLAY")); environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
        environment.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(fd));
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
        exporter.setProcessEnvironment(environment);
        exporter.setChildProcessModifier([fd] { if (fcntl(fd, F_SETFD, 0) < 0) _exit(2); });
        exporter.start(QStringLiteral(QINDAQT_PORTAL_FOREIGN_EXPORTER));
        const bool started = exporter.waitForStarted(); close(fd); QVERIFY(started);
        QByteArray output;
        QTRY_VERIFY_WITH_TIMEOUT((output += exporter.readAllStandardOutput()).contains('\n'), 5000);
        const QString parent = QString::fromUtf8(output.trimmed()); QVERIFY(parent.startsWith(QStringLiteral("wayland:")));
        auto granted = call(parent); QTRY_VERIFY_WITH_TIMEOUT(granted->isFinished(), 10000);
        const QDBusPendingReply<quint32, QVariantMap> grant = *granted;
        QVERIFY(!grant.isError()); QCOMPARE(grant.argumentAt<0>(), 0U); QVERIFY(audit().contains(" mapped grant"));
        QFile(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT")).remove();
        qputenv("QINDAQT_PORTAL_TEST_MODE", "hold"); auto held = call(parent);
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped hold"), 10000);
        exporter.kill(); QVERIFY(exporter.waitForFinished());
        QTRY_VERIFY_WITH_TIMEOUT(held->isFinished(), 10000);
        const QDBusPendingReply<quint32, QVariantMap> lost = *held;
        QVERIFY(!lost.isError()); QCOMPARE(lost.argumentAt<0>(), 2U);
    }
    void emailDraftUsesNativeDefaultAndSelectedFd() {
        QFile(qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL_AUDIT")).remove();
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.PortalJourney"),
            QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.impl.portal.Email"), QStringLiteral("ComposeEmail"));
        message.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QStringLiteral("org.test.PrivateApp"), QString{},
            QVariantMap{{QStringLiteral("addresses"), QStringList{QStringLiteral("fixture@example.invalid")}},
                {QStringLiteral("subject"), QStringLiteral("Synthetic %0D & draft")}, {QStringLiteral("body"), QStringLiteral("Private test only")}}});
        QDBusPendingCallWatcher reply(bus.asyncCall(message)); QTRY_VERIFY_WITH_TIMEOUT(reply.isFinished(), 10000);
        const QDBusPendingReply<quint32, QVariantMap> result = reply; QVERIFY(!result.isError()); QCOMPARE(result.argumentAt<0>(), 0U);
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL_AUDIT")), 10000);
        QFile audit(qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL_AUDIT")); QVERIFY(audit.open(QIODevice::ReadOnly));
        const auto evidence = QJsonDocument::fromJson(audit.readAll()).object();
        QVERIFY(evidence.value(QStringLiteral("mapped")).toBool()); QVERIFY(evidence.value(QStringLiteral("exactPeer")).toBool());
        const QUrl actual(evidence.value(QStringLiteral("uri")).toString()); QCOMPARE(actual.scheme(), QStringLiteral("mailto"));
        QCOMPARE(QUrlQuery(actual).queryItemValue(QStringLiteral("subject"), QUrl::FullyDecoded), QStringLiteral("Synthetic %0D & draft"));
        QTest::qWait(150); // The synthetic independent app exits after its mapped receipt.
    }
    void nativeLockRetiresMappedConsent() {
        qputenv("QINDAQT_PORTAL_TEST_MODE", "hold"); auto reply = call();
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped hold"), 10000); const auto pid = helperPid();
        auto request = QDBusMessage::createMethodCall(QString(QindaQt::CompositorNames::service),
            QString(QindaQt::CompositorNames::nativeLockPath), QString(QindaQt::CompositorNames::nativeLockInterface),
            QStringLiteral("RequestLockWithReceipt")); request << QUuid::createUuid().toString(QUuid::Id128);
        bus.asyncCall(request); QTRY_VERIFY(!consent->admitted());
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 2U); QTRY_VERIFY(kill(pid, 0) < 0);
        // Production authorization is OFF: this qualifies native Locked
        // fallback retirement, not PAM/trusted-locker/authenticated unlock.
    }
    void cleanupTestCase() {
        backend->unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop"));
        qDeleteAll(host.children()); uri.reset(); mailStore.reset(); consent.reset(); registry.reset(); binding.reset();
        backend->unregisterService(QStringLiteral("org.test.PortalJourney"));
        bus.unregisterService(QStringLiteral("org.freedesktop.portal.Desktop"));
    }
private:
    std::unique_ptr<QDBusPendingCallWatcher> call(const QString &parent = {}, const QVariantMap &options = {}) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.PortalJourney"),
            QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.impl.portal.Access"), QStringLiteral("AccessDialog"));
        message.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QStringLiteral("org.test.PrivateApp"),
            parent, QStringLiteral("Allow private test access?"), QStringLiteral("Synthetic local application"),
            QStringLiteral("This request uses only the isolated compositor."), options});
        return std::make_unique<QDBusPendingCallWatcher>(bus.asyncCall(message));
    }
    QByteArray audit() {
        QFile file(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT"));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }
    pid_t helperPid() { return static_cast<pid_t>(audit().split(' ').value(0).toInt()); }
    QDBusConnection bus = QDBusConnection::sessionBus(); QObject host;
    std::unique_ptr<QDBusConnection> backend;
    std::unique_ptr<PortalSessionBinding> binding;
    std::unique_ptr<RequestRegistry> registry;
    std::unique_ptr<ProcessAccessConsent> consent;
    std::unique_ptr<QindaQt::Apps::SettingsDefaultApps::DefaultApplicationsStore> mailStore;
    std::unique_ptr<QindaQt::Services::ApplicationUri::DefaultApplicationUriOpener> uri;
    const QString path = QStringLiteral("/org/freedesktop/portal/desktop/request/fixture/native");
};
QTEST_GUILESS_MAIN(NativeConsentTest)
#include "tst_native_consent.moc"
