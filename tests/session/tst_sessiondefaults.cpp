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
    void seedsQindaDecorationWhenMissing();
    void preservesExplicitDecorationChoice();
    void createsMissingConfigurationHome();
    void rejectsEmptyConfigurationHome();
};

void SessionDefaultsTest::seedsQindaDecorationWhenMissing()
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
}

void SessionDefaultsTest::preservesExplicitDecorationChoice()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("kwinrc"));
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("org.kde.kdecoration2"));
        settings.setValue(QStringLiteral("library"), QStringLiteral("org.example.choice"));
    }

    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    QCOMPARE(settings.value(QStringLiteral("library")).toString(),
             QStringLiteral("org.example.choice"));
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
