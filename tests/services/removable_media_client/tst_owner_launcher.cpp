// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/removable_media_client/desktop_owner_launcher.h>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
namespace Media = QindaQt::RemovableMedia;
namespace {
class Starter final : public Media::MediaArgvStarter {
public:
    bool start(const QString &p, const QStringList &a) override { ++calls; program = p; arguments = a; return true; }
    int calls = 0; QString program; QStringList arguments;
};
bool writeEntry(const QString &root, const QByteArray &extra, const QByteArray &exec = "fixed-owner \"literal two words\"") {
    QDir(root).mkpath("applications");
    QFile file(QDir(root).filePath(QString::fromLatin1("applications/") + QString::fromLatin1(Media::kOwnerDesktopId)));
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write("[Desktop Entry]\nType=Application\nName=Removable Media\nExec=" + exec + "\n" + extra) > 0;
}
}
class LauncherTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void inertUntilDeliberateFixedOwnerLaunch() {
        QTemporaryDir temp; Starter starter; QVERIFY(writeEntry(temp.path(), {}));
        Media::DesktopMediaOwnerLauncher launcher({temp.path()}, starter);
        QCOMPARE(starter.calls, 0); QVERIFY(launcher.startOwner()); QCOMPARE(starter.calls, 1);
        QCOMPARE(starter.program, QStringLiteral("fixed-owner"));
        QCOMPARE(starter.arguments, QStringList{QStringLiteral("literal two words")});
    }
    void missingDeletedTerminalAndDbusOnlyDoNotSpawn() {
        QTemporaryDir temp; Starter starter;
        Media::DesktopMediaOwnerLauncher launcher({temp.path()}, starter);
        QVERIFY(!launcher.startOwner()); QCOMPARE(starter.calls, 0);
        for (const auto &extra : {QByteArray("Hidden=true\n"), QByteArray("Terminal=true\n"), QByteArray("DBusActivatable=true\n")}) {
            QVERIFY(writeEntry(temp.path(), extra)); QVERIFY(!launcher.startOwner()); QCOMPARE(starter.calls, 0);
        }
    }
    void higherDeletedEntryMasksLowerInstalledCopy() {
        QTemporaryDir first, second; Starter starter;
        QVERIFY(writeEntry(first.path(), "Hidden=true\n")); QVERIFY(writeEntry(second.path(), {}));
        Media::DesktopMediaOwnerLauncher launcher({first.path(), second.path()}, starter);
        QVERIFY(!launcher.startOwner()); QCOMPARE(starter.calls, 0);
    }
};
QTEST_GUILESS_MAIN(LauncherTests)
#include "tst_owner_launcher.moc"
