// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/kwin_window_management_writer.h"

#include <KConfig>
#include <KConfigGroup>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Session::WindowManagement;

namespace {

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class KWinWindowManagementWriterTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void writesBothGroupsIntoAFreshFile();
    void readbackDetectsAConcurrentMismatch();
    void preservesForeignGroupsAndReportsNoChangeWhenEqual();
    void anUnwritablePathFails();
};

void KWinWindowManagementWriterTest::writesBothGroupsIntoAFreshFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KWinWindowManagementWriter writer(directory.filePath(QStringLiteral("kwinrc")));
    WindowManagementPreferences preferences;
    preferences.focusPolicy = FocusPolicy::FocusFollowsMouse;
    preferences.dockingModifier = DockingModifier::Alt;
    preferences.snapDistance = 20;
    preferences.sessionRestore = false;
    preferences.closeContainerPolicy = CloseContainerPolicy::Ungroup;
    const KWinWriteOutcome outcome = writer.write(preferences);
    QVERIFY2(outcome.ok, qPrintable(outcome.error));
    QVERIFY(outcome.changed);
    const QString text = readAll(writer.path());
    QVERIFY2(text.contains(QLatin1String("[Windows]")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("FocusPolicy=FocusFollowsMouse")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("BorderSnapZone=20")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("WindowSnapZone=20")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("[QindaQt]")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("DockingModifier=alt")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("CloseContainerPolicy=ungroup")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("SessionRestore=false")), qPrintable(text));
    const KWinReadbackOutcome readback = writer.readback(preferences);
    QVERIFY2(readback.matches, qPrintable(readback.error));
}

void KWinWindowManagementWriterTest::readbackDetectsAConcurrentMismatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("kwinrc"));
    const KWinWindowManagementWriter writer(path);
    const WindowManagementPreferences preferences{};
    QVERIFY(writer.write(preferences).ok);
    QVERIFY(writer.readback(preferences).matches);

    KConfig config(path, KConfig::SimpleConfig);
    config.group(QStringLiteral("Windows")).writeEntry("WindowSnapZone", 20);
    QVERIFY(config.sync());
    const KWinReadbackOutcome mismatch = writer.readback(preferences);
    QVERIFY(!mismatch.matches);
    QVERIFY(mismatch.error.contains(path));
}

void KWinWindowManagementWriterTest::preservesForeignGroupsAndReportsNoChangeWhenEqual()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("kwinrc"));
    {
        QFile seed(path);
        QVERIFY(seed.open(QIODevice::WriteOnly | QIODevice::Text));
        seed.write("[MouseBindings]\nCommandAll3=Nothing\n\n[Windows]\n"
                   "ElectricBorderTiling=false\nFocusPolicy=ClickToFocus\n"
                   "BorderSnapZone=12\nWindowSnapZone=12\n\n[org.kde.kdecoration2]\n"
                   "library=org.qindaqt\n");
    }
    const KWinWindowManagementWriter writer(path);
    // The file already agrees with the defaults for the [Windows] keys, but
    // the [QindaQt] group is missing: that is a change.
    KWinWriteOutcome outcome = writer.write(WindowManagementPreferences{});
    QVERIFY2(outcome.ok, qPrintable(outcome.error));
    QVERIFY(outcome.changed);
    QString text = readAll(path);
    QVERIFY2(text.contains(QLatin1String("CommandAll3=Nothing")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("ElectricBorderTiling=false")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("library=org.qindaqt")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("DockingModifier=super")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("CloseContainerPolicy=ask")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("SessionRestore=true")), qPrintable(text));

    // Now everything agrees: no write, no reconfigure worth asking for.
    outcome = writer.write(WindowManagementPreferences{});
    QVERIFY2(outcome.ok, qPrintable(outcome.error));
    QVERIFY(!outcome.changed);
    QCOMPARE(readAll(path), text);

    // One differing entry is a change again, and only that entry moves.
    WindowManagementPreferences preferences;
    preferences.snapDistance = 0;
    outcome = writer.write(preferences);
    QVERIFY(outcome.ok && outcome.changed);
    text = readAll(path);
    QVERIFY2(text.contains(QLatin1String("BorderSnapZone=0")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("WindowSnapZone=0")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("FocusPolicy=ClickToFocus")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("CommandAll3=Nothing")), qPrintable(text));
}

void KWinWindowManagementWriterTest::anUnwritablePathFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    // A directory in the file's place: KConfig cannot open it for writing.
    const QString path = directory.filePath(QStringLiteral("kwinrc"));
    QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("kwinrc")));
    const KWinWindowManagementWriter writer(path);
    const KWinWriteOutcome outcome = writer.write(WindowManagementPreferences{});
    QVERIFY(!outcome.ok);
    QVERIFY(!outcome.changed);
    QVERIFY2(outcome.error.contains(path), qPrintable(outcome.error));
}

QTEST_GUILESS_MAIN(KWinWindowManagementWriterTest)
#include "tst_kwin_window_management_writer.moc"
