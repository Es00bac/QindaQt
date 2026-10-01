// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/foundation_composition.h>
#include <qindaqt/services/portal/resident_portal_service.h>
#include <qindaqt/services/portal/appearance_source.h>
#include <qindaqt/services/portal/chooser_types.h>
#include <qindaqt/services/portal/session_binding.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
class ExportProcess final : public QProcess {
public:
    ~ExportProcess() override { if (state() != NotRunning) { kill(); waitForFinished(3000); } }
};
class Appearance final : public AppearanceSource {
public:
    bool start(QString *) override { return true; } void stop() override {}
    const std::optional<AppearanceTruth> &current() const override { return value; }
    QString diagnostic() const override { return {}; }
private: std::optional<AppearanceTruth> value;
};
class Responses final : public QObject {
    Q_OBJECT
public: int count = 0; quint32 response = 99; QVariantMap results;
public Q_SLOTS:
    void receive(quint32 r, const QVariantMap &v) { ++count; response = r; results = v; }
};
class NativeChoosersTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        registerChooserTypes(); QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.portal.Documents")));
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.impl.portal.PermissionStore")));
        backend = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("chooser-backend")));
        selected = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("chooser-supervisor")));
        resident = std::make_unique<ResidentPortalService>(appearance, *backend);
        composition = std::make_unique<PortalFoundationComposition>(resident->backendHost(), *backend, qEnvironmentVariable("XDG_RUNTIME_DIR"),
            qEnvironmentVariable("QINDAQT_PORTAL_TEST_HELPER"), qEnvironmentVariable("QINDAQT_PORTAL_TEST_RELAY"),
            QStringList{qEnvironmentVariable("XDG_DATA_HOME")}, qEnvironmentVariable("QINDAQT_CHOOSER_TEST_HELPER"));
        QVERIFY(composition->start()); QCOMPARE(resident->start(), PortalServiceStartStatus::Started);
        auto attach = QDBusMessage::createMethodCall(QString::fromLatin1(kNativePortalService), QString::fromLatin1(kNativePortalPath), QString::fromLatin1(kNativePortalService), QStringLiteral("AttachSessionWithDisplay"));
        attach << QStringLiteral("qindaqt-7"); QDBusPendingCallWatcher attaching(selected->asyncCall(attach));
        QTRY_VERIFY(attaching.isFinished()); const QDBusPendingReply<bool> result = attaching; QVERIFY(!result.isError()); QVERIFY(result.value());
        fixture = std::make_unique<QTemporaryDir>(qEnvironmentVariable("HOME") + QStringLiteral("/files-XXXXXX")); QVERIFY(fixture->isValid());
        QVERIFY(QDir().mkpath(fixture->filePath(QStringLiteral("folder"))));
        for (const auto *name : {"one.txt", "two.txt", "literal %$` space.txt", "existing.txt"}) {
            QFile file(fixture->filePath(QString::fromLatin1(name))); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("fixture"), 7LL);
        }
        QVERIFY(QDir().mkpath(qEnvironmentVariable("XDG_DATA_HOME") + QStringLiteral("/applications")));
        installApplication(QStringLiteral("org.test.First")); installApplication(QStringLiteral("org.test.Second"));
        startFrontend();
        QVERIFY(bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), {}, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), &responses, SLOT(receive(quint32,QVariantMap))));
    }
    void init() { responses.count = 0; responses.response = 99; responses.results.clear(); QFile(qEnvironmentVariable("QINDAQT_CHOOSER_TEST_AUDIT")).remove(); setInput({}); }
    void openSingleMultipleAndDirectory() {
        setInput({{"name", "literal %$` space.txt"}}); file("OpenFile", {}); success();
        QCOMPARE(uris(), QStringList{QUrl::fromLocalFile(fixture->filePath(QStringLiteral("literal %$` space.txt"))).toString(QUrl::FullyEncoded)});
        reset(); setInput({{"files", QJsonArray{"one.txt", "two.txt"}}}); file("OpenFile", {{QStringLiteral("multiple"), true}}); success(); QCOMPARE(uris().size(), 2);
        reset(); setInput({{"files", QJsonArray{"folder"}}}); file("OpenFile", {{QStringLiteral("directory"), true}}); success(); QCOMPARE(uris(), QStringList{QUrl::fromLocalFile(fixture->filePath(QStringLiteral("folder"))).toString(QUrl::FullyEncoded)});
    }
    void savePathsOverwriteCancellationAndOrderedFiles() {
        setInput({{"name", "new %$.txt"}}); file("SaveFile", {}); success(); QVERIFY(!QFile::exists(fixture->filePath(QStringLiteral("new %$.txt"))));
        reset(); setInput({{"name", "existing.txt"}, {"overwrite", false}}); file("SaveFile", {}); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 1U);
        QFile existing(fixture->filePath(QStringLiteral("existing.txt"))); QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(), QByteArray("fixture"));
        reset(); setInput({{"name", "existing.txt"}, {"overwrite", true}}); file("SaveFile", {}); success();
        reset(); setInput({}); file("SaveFiles", {{QStringLiteral("files"), QVariant::fromValue(FileNames{QByteArray("existing.txt\0", 13), QByteArray("second.txt\0", 11)})}}); success();
        QCOMPARE(uris().size(), 2); QCOMPARE(QFileInfo(QUrl(uris().first()).toLocalFile()).fileName(), QStringLiteral("existing.txt (1)")); QVERIFY(uris().last().endsWith(QStringLiteral("/second.txt")));
    }
    void filtersChoicesAndCancel() {
        const FileFilters filters{{QStringLiteral("All"), {{0, QStringLiteral("*")}}}, {QStringLiteral("Text"), {{1, QStringLiteral("text/plain")}}}};
        const AccessChoices choices{{QStringLiteral("check"), QStringLiteral("Check"), {}, QStringLiteral("false")}};
        setInput({{"name", "one.txt"}, {"filter", 1}, {"check", true}});
        file("OpenFile", {{QStringLiteral("filters"), QVariant::fromValue(filters)}, {QStringLiteral("choices"), QVariant::fromValue(choices)}}); success();
        const auto filter = qdbus_cast<FileFilter>(responses.results.value(QStringLiteral("current_filter"))); QCOMPARE(filter.label, QStringLiteral("Text"));
        const auto values = qdbus_cast<ChoiceValues>(responses.results.value(QStringLiteral("choices"))); QCOMPARE(values.size(), 1); QCOMPARE(values.first().value, QStringLiteral("true"));
        reset(); setInput({{"cancel", true}}); file("OpenFile", {}); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 1U);
    }
    void appCandidatesActualFrontendUpdateAndCancel() {
        setInput({{"app", "org.test.First"}}); openUri(); success();
        reset(); setInput({{"app", "org.test.Third"}}); openUri(); QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped"), 10000);
        installApplication(QStringLiteral("org.test.Third")); success(); // Real frontend GAppInfoMonitor calls UpdateChoices.
        reset(); setInput({{"cancel", true}}); openUri(); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 1U);
    }
    void invalidForeignParentAndValidParentLoss() {
        file("OpenFile", {}, QStringLiteral("wayland:no-such-private-parent")); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 2U); QVERIFY(audit().isEmpty());
        QindaQt::Platform::Compositor::CompositorAttachment attachment(*selected, qEnvironmentVariable("XDG_RUNTIME_DIR"), [this](const QString &owner) { return owner == selected->baseService(); });
        QVERIFY(attachment.attach(selected->baseService(), QStringLiteral("qindaqt-7")));
        const int fd = attachment.openConnection(); QVERIFY(fd >= 0);
        ExportProcess exporter; auto env = QProcessEnvironment::systemEnvironment(); env.remove(QStringLiteral("WAYLAND_DISPLAY")); env.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(fd));
        exporter.setProcessEnvironment(env); exporter.setChildProcessModifier([fd] { if (fcntl(fd, F_SETFD, 0) < 0) _exit(2); });
        exporter.start(qEnvironmentVariable("QINDAQT_PORTAL_FOREIGN_EXPORTER")); const bool started = exporter.waitForStarted(); close(fd); QVERIFY(started);
        QByteArray output; QTRY_VERIFY_WITH_TIMEOUT((output += exporter.readAllStandardOutput()).contains('\n'), 5000); const auto parent = QString::fromUtf8(output.trimmed());
        reset(); setInput({{"name", "one.txt"}}); file("OpenFile", {}, parent); success();
        reset(); setInput({{"hold", true}}); file("OpenFile", {}, parent); QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped"), 10000);
        const auto pid = helperPid(); exporter.kill(); QVERIFY(exporter.waitForFinished()); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 2U); QTRY_VERIFY(kill(pid, 0) < 0);
    }
    void closeAndRequesterLossRetireMappedHelper() {
        setInput({{"hold", true}}); const auto path = file("OpenFile", {}); QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped"), 10000); const auto pid = helperPid();
        auto close = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"), path, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher closing(bus.asyncCall(close)); QTRY_VERIFY(closing.isFinished()); QTRY_VERIFY(kill(pid, 0) < 0); QCOMPARE(responses.count, 0);
        reset(); setInput({{"hold", true}}); auto caller = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("chooser-requester")));
        file("OpenFile", {}, {}, *caller); QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped"), 10000); const auto orphan = helperPid();
        caller.reset(); QDBusConnection::disconnectFromBus(QStringLiteral("chooser-requester")); QTRY_VERIFY(kill(orphan, 0) < 0);
    }
    void frontendAndSupervisorLossRetireMappedHelpers() {
        setInput({{"hold", true}}); file("OpenFile", {}); QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped"), 10000); const auto pid = helperPid();
        frontend.terminate(); QVERIFY(frontend.waitForFinished(5000)); QTRY_VERIFY(kill(pid, 0) < 0); QCOMPARE(responses.count, 0);
        startFrontend(); reset(); setInput({{"hold", true}}); file("OpenFile", {}); QTRY_VERIFY_WITH_TIMEOUT(audit().contains(" mapped"), 10000); const auto next = helperPid();
        selected.reset(); QDBusConnection::disconnectFromBus(QStringLiteral("chooser-supervisor")); QTRY_VERIFY(kill(next, 0) < 0); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 2U);
        reset(); file("OpenFile", {}); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 2U); QVERIFY(audit().isEmpty());
    }
    void cleanupTestCase() {
        frontend.terminate(); if (!frontend.waitForFinished(3000)) { frontend.kill(); frontend.waitForFinished(3000); }
        composition.reset(); resident.reset(); backend.reset(); selected.reset(); QDBusConnection::disconnectFromBus(QStringLiteral("chooser-backend")); QDBusConnection::disconnectFromBus(QStringLiteral("chooser-supervisor"));
    }
private:
    void installApplication(const QString &id) {
        QFile file(qEnvironmentVariable("XDG_DATA_HOME") + QStringLiteral("/applications/") + id + QStringLiteral(".desktop")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Desktop Entry]\nType=Application\nName=" + id.toUtf8() + "\nMimeType=x-scheme-handler/qindaqtfixture;\nExec=/bin/true %u\n"); file.close();
    }
    void setInput(const QJsonObject &object) { qputenv("QINDAQT_CHOOSER_TEST_INPUT", QJsonDocument(object).toJson(QJsonDocument::Compact)); }
    void reset() { responses.count = 0; responses.response = 99; responses.results.clear(); QFile(qEnvironmentVariable("QINDAQT_CHOOSER_TEST_AUDIT")).remove(); }
    void startFrontend() {
        frontend.start(QStringLiteral(QINDAQT_FRONTEND_EXECUTABLE), {QStringLiteral("--verbose")}); QVERIFY(frontend.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.portal.Desktop")).value(), 10000);
    }
    QByteArray audit() { QFile file(qEnvironmentVariable("QINDAQT_CHOOSER_TEST_AUDIT")); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; }
    pid_t helperPid() { return static_cast<pid_t>(audit().split(' ').first().toInt()); }
    QStringList uris() { return responses.results.value(QStringLiteral("uris")).toStringList(); }
    void success() { QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 0U); QVERIFY(audit().contains("ordinary-wayland exact-peer mapped")); }
    QString file(const char *member, QVariantMap options, const QString &parent, const QDBusConnection &caller) {
        options.insert(QStringLiteral("current_folder"), QFile::encodeName(fixture->path()) + '\0');
        return request("org.freedesktop.portal.FileChooser", member, {parent, QStringLiteral("Private native chooser"), options}, caller);
    }
    QString file(const char *member, QVariantMap options, const QString &parent = {}) { return file(member, options, parent, bus); }
    void openUri() { request("org.freedesktop.portal.OpenURI", "OpenURI", {QString{}, QStringLiteral("qindaqtfixture:literal%20%25%24"), QVariantMap{{QStringLiteral("ask"), true}}}, bus); }
    QString request(const char *interface, const char *member, const QVariantList &args, const QDBusConnection &caller) {
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"), QString::fromLatin1(interface), QString::fromLatin1(member)); call.setArguments(args);
        QDBusPendingCallWatcher pending(caller.asyncCall(call, 20000)); QElapsedTimer timer; timer.start();
        while (!pending.isFinished() && timer.elapsed() < 5000) { QCoreApplication::processEvents(); QTest::qWait(5); }
        const QDBusPendingReply<QDBusObjectPath> result = pending; if (result.isError()) { qWarning().noquote() << result.error().message(); return {}; } return result.value().path();
    }
    QDBusConnection bus = QDBusConnection::sessionBus(); QProcess frontend; Appearance appearance; Responses responses;
    std::unique_ptr<QDBusConnection> backend, selected; std::unique_ptr<ResidentPortalService> resident;
    std::unique_ptr<PortalFoundationComposition> composition; std::unique_ptr<QTemporaryDir> fixture;
};
QTEST_GUILESS_MAIN(NativeChoosersTest)
#include "tst_native_choosers.moc"
