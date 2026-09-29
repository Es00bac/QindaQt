// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_requester_resolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

using namespace QindaQt::Apps::PolkitAgent;

namespace {

void writeFile(const QString &path, const QString &text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream(&file) << text;
}

} // namespace

class PolkitRequesterResolverTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void matchesADesktopEntryByExecBasename();
    void fallsBackToTheProgramPathWithoutAMatchingDesktopEntry();
    void unreadablePidResolvesToHonestlyEmpty();
};

void PolkitRequesterResolverTest::matchesADesktopEntryByExecBasename()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString procDirectory = root.filePath(QStringLiteral("proc/4242"));
    QVERIFY(QDir().mkpath(procDirectory));
    const QString programPath = root.filePath(QStringLiteral("usr/bin/qindalutris"));
    QVERIFY(QDir().mkpath(QFileInfo(programPath).absolutePath()));
    writeFile(programPath, QStringLiteral("#!/bin/sh\n"));
    QVERIFY(QFile::link(programPath, procDirectory + QStringLiteral("/exe")));
    writeFile(procDirectory + QStringLiteral("/comm"), QStringLiteral("qindalutris\n"));
    const QString applicationsDirectory = root.filePath(QStringLiteral("applications"));
    QVERIFY(QDir().mkpath(applicationsDirectory));
    writeFile(
        QDir(applicationsDirectory).filePath(QStringLiteral("org.qindaqt.QindaLutris.desktop")),
        QStringLiteral("[Desktop Entry]\nType=Application\nName=QindaLutris\n"
                       "Icon=qindalutris\nExec=qindalutris %f\n"));

    const PolkitRequesterResolver resolver(root.filePath(QStringLiteral("proc")),
                                           {applicationsDirectory});
    const RequesterInfo info = resolver.resolve(4242);
    QCOMPARE(info.displayName, QStringLiteral("QindaLutris"));
    QCOMPARE(info.iconName, QStringLiteral("qindalutris"));
    QCOMPARE(info.programPath, programPath);
}

void PolkitRequesterResolverTest::fallsBackToTheProgramPathWithoutAMatchingDesktopEntry()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString procDirectory = root.filePath(QStringLiteral("proc/99"));
    QVERIFY(QDir().mkpath(procDirectory));
    const QString programPath = root.filePath(QStringLiteral("opt/tool/unlisted-tool"));
    QVERIFY(QDir().mkpath(QFileInfo(programPath).absolutePath()));
    writeFile(programPath, QStringLiteral("#!/bin/sh\n"));
    QVERIFY(QFile::link(programPath, procDirectory + QStringLiteral("/exe")));
    writeFile(procDirectory + QStringLiteral("/comm"), QStringLiteral("unlisted-tool\n"));

    const PolkitRequesterResolver resolver(root.filePath(QStringLiteral("proc")), {});
    const RequesterInfo info = resolver.resolve(99);
    QCOMPARE(info.programPath, programPath);
    QCOMPARE(info.displayName, programPath);
    QVERIFY(info.iconName.isEmpty());
}

void PolkitRequesterResolverTest::unreadablePidResolvesToHonestlyEmpty()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const PolkitRequesterResolver resolver(root.filePath(QStringLiteral("proc")), {});
    const RequesterInfo info = resolver.resolve(123456);
    QVERIFY(info.displayName.isEmpty());
    QVERIFY(info.iconName.isEmpty());
    QVERIFY(info.programPath.isEmpty());
}

QTEST_MAIN(PolkitRequesterResolverTest)
#include "tst_polkit_requester_resolver.moc"
