// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"
#include "pe_fixture.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <cerrno>
using namespace QindaQt::Apps::WindowsHandler;

struct Fixture final {
    QTemporaryDir dir;
    ProgramInspection program;
    std::shared_ptr<InstalledWine> runtime = std::make_shared<InstalledWine>();
    QString dataRoot;
    Fixture()
    {
        const QString path = dir.filePath(QStringLiteral("program ;$.exe"));
        if (!writePe(path)) { return; }
        auto inspected = QindaQt::WindowsRunner::inspectWindowsExecutable(path);
        program.program = std::shared_ptr<QindaQt::WindowsRunner::WindowsExecutableFile>(std::move(inspected.file));
        auto makePin = [&](const QString &name) {
            const QString file = dir.filePath(name); QFile output(file);
            if (!output.open(QIODevice::WriteOnly) || output.write("fixture runtime payload") < 0) { return std::shared_ptr<PinnedFile>{}; }
            output.close(); return pinFile(file, static_cast<quint32>(::getuid()));
        };
        runtime->id = QStringLiteral("fixture/installed-runtime-1"); runtime->label = QStringLiteral("Fixture runtime");
        runtime->preloader = makePin(QStringLiteral("preloader")); runtime->loader = makePin(QStringLiteral("loader"));
        runtime->server = makePin(QStringLiteral("server")); runtime->bubblewrap = makePin(QStringLiteral("bwrap"));
        program.runtimes.append(runtime); dataRoot = dir.filePath(QStringLiteral("private-profiles"));
    }
};

class UserProfileTests final : public QObject {
    Q_OBJECT
private slots:
    void firstRunAndRelaunchPreserveRegistrationPrefixAndLiteralFiles()
    {
        Fixture f; QVERIFY(f.program.program); QVERIFY(f.runtime->current()); int submissions = 0;
        NativeSpawn submit = [&](const QStringList &arguments, const QProcessEnvironment &environment,
            const QVector<int> &descriptors, int &pid) {
            ++submissions; pid = 123;
            if (descriptors.size() != 6 || arguments.constLast() != f.program.program->canonicalPath()) { return EINVAL; }
            if (arguments.contains(QStringLiteral("--preserve-fds"))
                || arguments.count(QStringLiteral("--ro-bind-fd")) != 4
                || arguments.value(arguments.indexOf(QStringLiteral("--sync-fd")) + 1) != QStringLiteral("8")) { return EINVAL; }
            if (environment.value(QStringLiteral("WINEPREFIX")).isEmpty()
                || environment.contains(QStringLiteral("LD_PRELOAD")) || environment.contains(QStringLiteral("PYTHONPATH"))) { return EINVAL; }
            return 0;
        };
        QProcessEnvironment hostile; hostile.insert(QStringLiteral("LD_PRELOAD"), QStringLiteral("hostile"));
        hostile.insert(QStringLiteral("PYTHONPATH"), QStringLiteral("hostile"));
        const auto first = launchProgram(f.program, f.runtime, ProgramKind::Portable, f.dataRoot, hostile, {}, submit);
        QVERIFY2(first.submitted, qPrintable(first.message));
        const QString retained = first.profileRoot + QStringLiteral("/prefix/user-document.txt");
        QVERIFY(writePe(retained, QByteArrayLiteral("user data")));
        const auto second = launchProgram(f.program, f.runtime, ProgramKind::Portable, f.dataRoot, {}, {}, submit);
        QVERIFY(second.submitted); QCOMPARE(second.registrationId, first.registrationId);
        QCOMPARE(second.profileRoot, first.profileRoot); QVERIFY(second.launchId != first.launchId);
        QVERIFY(QFileInfo::exists(retained)); QCOMPARE(submissions, 2);
        QFile receipt(first.profileRoot + QStringLiteral("/registration.json")); QVERIFY(receipt.open(QIODevice::ReadOnly));
        const auto stored = QJsonDocument::fromJson(receipt.readAll()).object();
        QCOMPARE(stored.value(QStringLiteral("provenance")).toString(), QStringLiteral("user-selected-file"));
        QCOMPARE(stored.value(QStringLiteral("revision")).toString(), QStringLiteral("1"));
        QCOMPARE(QDir(first.profileRoot + QStringLiteral("/images")).entryList({QStringLiteral("*.exe")}, QDir::Files).size(), 1);
    }
    void dispatchHoldsTheProfileTransaction()
    {
        Fixture f; bool held = false;
        NativeSpawn submit = [&](const QStringList &, const QProcessEnvironment &, const QVector<int> &descriptors, int &pid) {
            pid = 123;
            const QByteArray path = QFile::encodeName(f.dataRoot + QLatin1Char('/')
                + QString::fromLatin1(QCryptographicHash::hash(f.program.program->canonicalPath().toUtf8(), QCryptographicHash::Sha256).toHex())
                + QStringLiteral("/dispatch.lock"));
            const int other = ::open(path.constData(), O_RDWR | O_CLOEXEC);
            held = other >= 0 && ::flock(other, LOCK_EX | LOCK_NB) != 0;
            if (other >= 0) { ::close(other); }
            return held && descriptors.last() >= 0 ? 0 : EINVAL;
        };
        QVERIFY(launchProgram(f.program, f.runtime, ProgramKind::Portable, f.dataRoot, {}, {}, submit).submitted);
        QVERIFY(held);
    }
    void changedRuntimeNeverReachesSubmission()
    {
        Fixture f; QVERIFY(writePe(f.runtime->loader->path, QByteArrayLiteral("changed"))); int submissions = 0;
        NativeSpawn submit = [&](const QStringList &, const QProcessEnvironment &, const QVector<int> &, int &) { ++submissions; return 0; };
        QVERIFY(!launchProgram(f.program, f.runtime, ProgramKind::Portable, f.dataRoot, {}, {}, submit).submitted);
        QCOMPARE(submissions, 0);
    }
    void unknownReceiptSchemaIsRetained()
    {
        Fixture f;
        NativeSpawn submit = [](const QStringList &, const QProcessEnvironment &, const QVector<int> &, int &pid) { pid = 123; return 0; };
        const auto first = launchProgram(f.program, f.runtime, ProgramKind::Installer, f.dataRoot, {}, {}, submit); QVERIFY(first.submitted);
        const QString receipt = first.profileRoot + QStringLiteral("/registration.json");
        const QByteArray unknown = QByteArrayLiteral("{\"schema\":99,\"future\":true}"); QVERIFY(writePe(receipt, unknown));
        QVERIFY(!launchProgram(f.program, f.runtime, ProgramKind::Installer, f.dataRoot, {}, {}, submit).submitted);
        QFile retained(receipt); QVERIFY(retained.open(QIODevice::ReadOnly)); QCOMPARE(retained.readAll(), unknown);
    }
    void cancelledPreparationCannotSubmit()
    {
        Fixture f; int submissions = 0;
        NativeSpawn submit = [&](const QStringList &, const QProcessEnvironment &, const QVector<int> &, int &) { ++submissions; return 0; };
        auto cancelled = std::make_shared<std::atomic_bool>(true);
        QVERIFY(!launchProgram(f.program, f.runtime, ProgramKind::Portable, f.dataRoot, {}, {}, submit, cancelled).submitted);
        QCOMPARE(submissions, 0);
    }
};
QTEST_GUILESS_MAIN(UserProfileTests)
#include "tst_user_profile.moc"
