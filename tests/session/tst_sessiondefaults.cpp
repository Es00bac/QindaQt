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

QTEST_GUILESS_MAIN(SessionDefaultsTest)
#include "tst_sessiondefaults.moc"
