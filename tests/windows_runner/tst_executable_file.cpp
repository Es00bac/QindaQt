// SPDX-License-Identifier: LGPL-3.0-or-later
#include "pe_fixture.h"
#include <qindaqt/windows_runner/executable_file.h>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::WindowsRunner;

class ExecutableFileTests final : public QObject {
    Q_OBJECT
private slots:
    void admittedArchitectures_data()
    {
        QTest::addColumn<int>("machine");
        QTest::newRow("x86") << 0x014c; QTest::newRow("x64") << 0x8664;
        QTest::newRow("arm64") << 0xaa64;
    }
    void admittedArchitectures()
    {
        QFETCH(int, machine); QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("program.bin"));
        QVERIFY(writePe(path, peFixture(static_cast<quint16>(machine))));
        auto inspected = inspectWindowsExecutable(path);
        QVERIFY2(inspected.file != nullptr, qPrintable(inspected.message));
        QVERIFY(inspected.file->stillCurrent());
        QVERIFY(inspected.file->borrowedDescriptor() >= 0);
    }
    void malformedOrUnsupported_data()
    {
        QTest::addColumn<int>("mutation");
        const QStringList names{QStringLiteral("renamed-text"), QStringLiteral("dos-stub"),
            QStringLiteral("huge-offset"), QStringLiteral("truncated"), QStringLiteral("dll"),
            QStringLiteral("unknown-machine"), QStringLiteral("zero-sections"),
            QStringLiteral("invalid-optional-magic"), QStringLiteral("outside-raw-section"),
            QStringLiteral("entrypoint-not-executable"), QStringLiteral("raw-overlaps-header"),
            QStringLiteral("native-subsystem")};
        for (int i = 0; i < names.size(); ++i) { QTest::newRow(qPrintable(names[i])) << i; }
    }
    void malformedOrUnsupported()
    {
        QFETCH(int, mutation); QTemporaryDir dir; QByteArray bytes = peFixture();
        switch (mutation) {
        case 0: bytes = QByteArrayLiteral("echo renamed script"); break;
        case 1: bytes.replace(128, 4, QByteArrayLiteral("DOS!")); break;
        case 2: put32(bytes, 60, 0xffffffff); break;
        case 3: bytes.truncate(180); break;
        case 4: put16(bytes, 150, 0x2002); break;
        case 5: put16(bytes, 132, 0xffff); break;
        case 6: put16(bytes, 134, 0); break;
        case 7: put16(bytes, 152, 0x010b); break;
        case 8: put32(bytes, 392 + 20, 0xfffffff0); break;
        case 9: put32(bytes, 392 + 36, 0x40000000); break;
        case 10: put32(bytes, 392 + 20, 200); break;
        case 11: put16(bytes, 220, 1); break;
        }
        const QString path = dir.filePath(QStringLiteral("fake.exe")); QVERIFY(writePe(path, bytes));
        auto inspected = inspectWindowsExecutable(path);
        QVERIFY(!inspected.file); QVERIFY(inspected.error != ExecutableFileError::None);
        QVERIFY(!inspected.message.contains(path));
    }
    void pathReplacementCannotInheritSelectedIdentity()
    {
        QTemporaryDir dir; const QString path = dir.filePath(QStringLiteral("program.exe"));
        QVERIFY(writePe(path)); auto inspected = inspectWindowsExecutable(path); QVERIFY(inspected.file);
        QVERIFY(QFile::rename(path, dir.filePath(QStringLiteral("original.exe"))));
        QVERIFY(writePe(path)); QVERIFY(!inspected.file->stillCurrent());
    }
    void contentRewriteCannotInheritSelectedIdentity()
    {
        QTemporaryDir dir; const QString path = dir.filePath(QStringLiteral("program.exe"));
        QVERIFY(writePe(path)); auto inspected = inspectWindowsExecutable(path); QVERIFY(inspected.file);
        QVERIFY(writePe(path, QByteArrayLiteral("changed"))); QVERIFY(!inspected.file->stillCurrent());
    }
    void existingLinkIsResolvedOnce()
    {
        QTemporaryDir dir; const QString path = dir.filePath(QStringLiteral("program.exe"));
        const QString link = dir.filePath(QStringLiteral("shortcut.exe"));
        QVERIFY(writePe(path)); QVERIFY(QFile::link(path, link));
        auto inspected = inspectWindowsExecutable(link); QVERIFY(inspected.file);
        QCOMPARE(inspected.file->canonicalPath(), path);
    }
};
QTEST_GUILESS_MAIN(ExecutableFileTests)
#include "tst_executable_file.moc"
