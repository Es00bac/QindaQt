// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using QindaQt::Session::SessionDefaults;

class SessionDefaultsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void seedsQindaDesktopDefaultsWhenMissing();
    void preservesExplicitDesktopChoices();
    void seedsEachMissingChoiceIndependently();
    void createsMissingConfigurationHome();
    void rejectsEmptyConfigurationHome();
    void seedsDirectoryHandlerWhenMissing();
    void preservesExistingDirectoryHandler();
    void addsDirectoryHandlerToExistingDefaults();
    void ignoresDirectoryHandlerInOtherSections();
};

void SessionDefaultsTest::seedsQindaDesktopDefaultsWhenMissing()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));

    const auto path = QDir(temporary.path()).filePath(QStringLiteral("kwinrc"));
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    QCOMPARE(settings.value(QStringLiteral("library")).toString(),
             QStringLiteral("org.qindaqt"));
    settings.endGroup();
    settings.beginGroup(QStringLiteral("Windows"));
    QCOMPARE(settings.value(QStringLiteral("ElectricBorderTiling")).toBool(), false);
    QCOMPARE(settings.value(QStringLiteral("ElectricBorderMaximize")).toBool(), false);
    settings.endGroup();
    settings.beginGroup(QStringLiteral("TabBox"));
    QCOMPARE(settings.value(QStringLiteral("LayoutName")).toString(),
             QStringLiteral("qindaqt"));
}

void SessionDefaultsTest::preservesExplicitDesktopChoices()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("kwinrc"));
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("org.kde.kdecoration2"));
        settings.setValue(QStringLiteral("library"), QStringLiteral("org.example.choice"));
        settings.endGroup();
        settings.beginGroup(QStringLiteral("Windows"));
        settings.setValue(QStringLiteral("ElectricBorderTiling"), true);
        settings.setValue(QStringLiteral("ElectricBorderMaximize"), true);
        settings.endGroup();
        settings.beginGroup(QStringLiteral("TabBox"));
        settings.setValue(QStringLiteral("LayoutName"), QStringLiteral("compact"));
    }

    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    QCOMPARE(settings.value(QStringLiteral("library")).toString(),
             QStringLiteral("org.example.choice"));
    settings.endGroup();
    settings.beginGroup(QStringLiteral("Windows"));
    QCOMPARE(settings.value(QStringLiteral("ElectricBorderTiling")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("ElectricBorderMaximize")).toBool(), true);
    settings.endGroup();
    settings.beginGroup(QStringLiteral("TabBox"));
    QCOMPARE(settings.value(QStringLiteral("LayoutName")).toString(),
             QStringLiteral("compact"));
}

void SessionDefaultsTest::seedsEachMissingChoiceIndependently()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("kwinrc"));
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Windows"));
        settings.setValue(QStringLiteral("ElectricBorderTiling"), true);
    }

    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));

    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Windows"));
    QCOMPARE(settings.value(QStringLiteral("ElectricBorderTiling")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("ElectricBorderMaximize")).toBool(), false);
    settings.endGroup();
    settings.beginGroup(QStringLiteral("TabBox"));
    QCOMPARE(settings.value(QStringLiteral("LayoutName")).toString(),
             QStringLiteral("qindaqt"));
}

void SessionDefaultsTest::createsMissingConfigurationHome()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto config = QDir(temporary.path()).filePath(QStringLiteral("nested/config"));
    QString error;
    QVERIFY2(SessionDefaults::ensure(config, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(QDir(config).filePath(QStringLiteral("kwinrc"))));
}

void SessionDefaultsTest::rejectsEmptyConfigurationHome()
{
    QString error;
    QVERIFY(!SessionDefaults::ensure(QStringLiteral("   "), &error));
    QVERIFY(!error.isEmpty());
}

void SessionDefaultsTest::seedsDirectoryHandlerWhenMissing()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));

    QFile seeded(QDir(temporary.path()).filePath(QStringLiteral("mimeapps.list")));
    QVERIFY(seeded.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(seeded.readAll());
    QVERIFY(contents.contains(QLatin1String("[Default Applications]")));
    QVERIFY(contents.contains(
        QLatin1String("inode/directory=org.qindaqt.FileManager.desktop")));
}

void SessionDefaultsTest::preservesExistingDirectoryHandler()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("mimeapps.list"));
    {
        QFile existing(path);
        QVERIFY(existing.open(QIODevice::WriteOnly | QIODevice::Text));
        existing.write("[Default Applications]\n"
                       "inode/directory=org.example.manager.desktop\n");
    }
    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));

    QFile preserved(path);
    QVERIFY(preserved.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(preserved.readAll());
    QVERIFY(contents.contains(
        QLatin1String("inode/directory=org.example.manager.desktop")));
    QVERIFY(!contents.contains(QLatin1String("org.qindaqt.FileManager.desktop")));
}

void SessionDefaultsTest::addsDirectoryHandlerToExistingDefaults()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("mimeapps.list"));
    {
        QFile existing(path);
        QVERIFY(existing.open(QIODevice::WriteOnly | QIODevice::Text));
        existing.write("[Added Associations]\n"
                       "text/plain=org.example.editor.desktop\n"
                       "\n"
                       "[Default Applications]\n"
                       "text/plain=org.example.editor.desktop\n");
    }
    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));

    QFile updated(path);
    QVERIFY(updated.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(updated.readAll());
    QVERIFY(contents.contains(
        QLatin1String("inode/directory=org.qindaqt.FileManager.desktop")));
    QVERIFY(contents.contains(QLatin1String("text/plain=org.example.editor.desktop")));
    QVERIFY(contents.contains(QLatin1String("[Added Associations]")));
    // The default must land inside the defaults section, after its header.
    QVERIFY(contents.indexOf(QLatin1String("inode/directory="))
            > contents.indexOf(QLatin1String("[Default Applications]")));
}

void SessionDefaultsTest::ignoresDirectoryHandlerInOtherSections()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("mimeapps.list"));
    {
        QFile existing(path);
        QVERIFY(existing.open(QIODevice::WriteOnly | QIODevice::Text));
        existing.write("[Added Associations]\n"
                       "inode/directory=org.example.other.desktop\n");
    }
    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));

    QFile updated(path);
    QVERIFY(updated.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(updated.readAll());
    QVERIFY(contents.contains(QLatin1String("[Default Applications]")));
    QVERIFY(contents.contains(
        QLatin1String("inode/directory=org.qindaqt.FileManager.desktop")));
}

QTEST_GUILESS_MAIN(SessionDefaultsTest)
#include "tst_sessiondefaults.moc"
