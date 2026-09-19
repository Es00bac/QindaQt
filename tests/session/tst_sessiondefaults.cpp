// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include <QDir>
#include <QFile>
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
    void doesNotCreateUserMimeDefaults();
    void preservesExistingDirectoryHandler();


    void seedsOnScreenKeyboardFromTheNamedDesktopFile();
    void seedsNoInputMethodWithoutADesktopFile();
    void keepsAnExplicitInputMethod();
    void seedsTranslucencyEffectsWithoutOverridingChoices();
    void seedsTheMetaKeyOntoTheLauncherAction();
    void keepsAnExplicitModifierOnlyShortcut();
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
    settings.endGroup();
    // The live customization chord: Meta+right must reach layer-shell panels.
    settings.beginGroup(QStringLiteral("MouseBindings"));
    QCOMPARE(settings.value(QStringLiteral("CommandAll3")).toString(),
             QStringLiteral("Nothing"));
}

void SessionDefaultsTest::seedsTheMetaKeyOntoTheLauncherAction()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString error;
    QVERIFY2(QindaQt::Session::SessionDefaults::ensure(temporary.path(), &error),
             qPrintable(error));

    // AGENT-GUARD: KWin reads this entry as a list -- service, path,
    // interface, method, then arguments. A value written as one quoted string
    // would leave KWin with a single malformed element and a dead Meta key,
    // so the row asserts the parsed shape rather than the raw text.
    QSettings settings(QDir(temporary.path()).filePath(QStringLiteral("kwinrc")),
                       QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ModifierOnlyShortcuts"));
    const QStringList call = settings.value(QStringLiteral("Meta")).toStringList();
    QCOMPARE(call.size(), 5);
    QCOMPARE(call.at(0), QStringLiteral("org.kde.kglobalaccel"));
    QCOMPARE(call.at(1), QStringLiteral("/component/qindaqt_shell"));
    QCOMPARE(call.at(2), QStringLiteral("org.kde.kglobalaccel.Component"));
    QCOMPARE(call.at(3), QStringLiteral("invokeShortcut"));
    QCOMPARE(call.at(4), QStringLiteral("qindaqt_open_launcher"));
    settings.endGroup();

    // KWin reads kwinrc with KConfig, not QSettings, and KConfig splits this
    // entry on bare commas. A quoted value or a comma-space separator would
    // parse into one malformed element there while still reading back
    // correctly above, so the row also pins the written text.
    QFile file(QDir(temporary.path()).filePath(QStringLiteral("kwinrc")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY2(contents.contains(QStringLiteral(
                 "Meta=org.kde.kglobalaccel, /component/qindaqt_shell, "
                 "org.kde.kglobalaccel.Component, invokeShortcut, "
                 "qindaqt_open_launcher"))
                 || contents.contains(QStringLiteral(
                     "Meta=org.kde.kglobalaccel,/component/qindaqt_shell,"
                     "org.kde.kglobalaccel.Component,invokeShortcut,"
                     "qindaqt_open_launcher")),
             qPrintable(contents));
}

void SessionDefaultsTest::keepsAnExplicitModifierOnlyShortcut()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("kwinrc"));
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("ModifierOnlyShortcuts"));
        settings.setValue(QStringLiteral("Meta"), QStringLiteral("org.example,/x,org.example.X,y"));
        settings.endGroup();
    }

    QString error;
    QVERIFY2(QindaQt::Session::SessionDefaults::ensure(temporary.path(), &error),
             qPrintable(error));

    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ModifierOnlyShortcuts"));
    QVERIFY(settings.value(QStringLiteral("Meta")).toString().startsWith(
        QStringLiteral("org.example")));
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
        settings.endGroup();
        settings.beginGroup(QStringLiteral("MouseBindings"));
        settings.setValue(QStringLiteral("CommandAll3"), QStringLiteral("Resize"));
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
    settings.endGroup();
    settings.beginGroup(QStringLiteral("MouseBindings"));
    QCOMPARE(settings.value(QStringLiteral("CommandAll3")).toString(),
             QStringLiteral("Resize"));
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

void SessionDefaultsTest::doesNotCreateUserMimeDefaults()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(QDir(temporary.path()).filePath(QStringLiteral("mimeapps.list"))));
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

void SessionDefaultsTest::seedsOnScreenKeyboardFromTheNamedDesktopFile()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString desktopFile = QDir(temporary.path()).filePath(QStringLiteral("osk.desktop"));
    QFile entry(desktopFile);
    QVERIFY(entry.open(QIODevice::WriteOnly | QIODevice::Text));
    entry.write("[Desktop Entry]\nType=Application\nExec=/opt/qindaqt-osk\n");
    entry.close();
    qputenv("QINDAQT_OSK_DESKTOP_FILE", desktopFile.toUtf8());
    QString error;
    const bool ensured = SessionDefaults::ensure(temporary.path(), &error);
    qunsetenv("QINDAQT_OSK_DESKTOP_FILE");
    QVERIFY2(ensured, qPrintable(error));
    QSettings settings(QDir(temporary.path()).filePath(QStringLiteral("kwinrc")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Wayland"));
    QCOMPARE(settings.value(QStringLiteral("InputMethod")).toString(), desktopFile);
}

void SessionDefaultsTest::seedsNoInputMethodWithoutADesktopFile()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    // An explicitly named but missing entry means "no keyboard", never a
    // fallback to whatever happens to be installed on the build host.
    qputenv("QINDAQT_OSK_DESKTOP_FILE", QDir(temporary.path()).filePath(QStringLiteral("missing.desktop")).toUtf8());
    QString error;
    const bool ensured = SessionDefaults::ensure(temporary.path(), &error);
    qunsetenv("QINDAQT_OSK_DESKTOP_FILE");
    QVERIFY2(ensured, qPrintable(error));
    QSettings settings(QDir(temporary.path()).filePath(QStringLiteral("kwinrc")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Wayland"));
    QVERIFY(!settings.contains(QStringLiteral("InputMethod")));
}

void SessionDefaultsTest::keepsAnExplicitInputMethod()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString desktopFile = QDir(temporary.path()).filePath(QStringLiteral("osk.desktop"));
    QFile entry(desktopFile);
    QVERIFY(entry.open(QIODevice::WriteOnly | QIODevice::Text));
    entry.write("[Desktop Entry]\nType=Application\nExec=/opt/qindaqt-osk\n");
    entry.close();
    {
        QSettings existing(QDir(temporary.path()).filePath(QStringLiteral("kwinrc")), QSettings::IniFormat);
        existing.beginGroup(QStringLiteral("Wayland"));
        existing.setValue(QStringLiteral("InputMethod"), QStringLiteral("/usr/share/applications/com.github.maliit.keyboard.desktop"));
    }
    qputenv("QINDAQT_OSK_DESKTOP_FILE", desktopFile.toUtf8());
    QString error;
    const bool ensured = SessionDefaults::ensure(temporary.path(), &error);
    qunsetenv("QINDAQT_OSK_DESKTOP_FILE");
    QVERIFY2(ensured, qPrintable(error));
    QSettings settings(QDir(temporary.path()).filePath(QStringLiteral("kwinrc")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Wayland"));
    QCOMPARE(settings.value(QStringLiteral("InputMethod")).toString(),
             QStringLiteral("/usr/share/applications/com.github.maliit.keyboard.desktop"));
    settings.endGroup();
}

void SessionDefaultsTest::seedsTranslucencyEffectsWithoutOverridingChoices()
{
    // Theming v2 (ADR-0206): the blur and background-contrast effects are
    // seeded on first run; a user who switched blur off keeps that choice.
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto path = QDir(temporary.path()).filePath(QStringLiteral("kwinrc"));
    {
        QSettings existing(path, QSettings::IniFormat);
        existing.beginGroup(QStringLiteral("Plugins"));
        existing.setValue(QStringLiteral("blurEnabled"), false);
        existing.endGroup();
        existing.sync();
    }
    QString error;
    QVERIFY2(SessionDefaults::ensure(temporary.path(), &error), qPrintable(error));

    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Plugins"));
    QCOMPARE(settings.value(QStringLiteral("blurEnabled")).toBool(), false);
    QCOMPARE(settings.value(QStringLiteral("contrastEnabled")).toBool(), true);
    settings.endGroup();
    settings.beginGroup(QStringLiteral("Effect-blur"));
    QCOMPARE(settings.value(QStringLiteral("BlurStrength")).toInt(), 8);
    QCOMPARE(settings.value(QStringLiteral("NoiseStrength")).toInt(), 2);
    settings.endGroup();
}

QTEST_GUILESS_MAIN(SessionDefaultsTest)
#include "tst_sessiondefaults.moc"
