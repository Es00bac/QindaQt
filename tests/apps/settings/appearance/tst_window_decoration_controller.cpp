// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/window_decoration_controller.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using QindaQt::Apps::SettingsAppearance::WindowDecorationController;

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

bool makeTheme(const QDir &root, const QString &id, const QString &name)
{
    if (!root.mkpath(id)) {
        return false;
    }
    const QDir theme(root.filePath(id));
    return writeFile(theme.filePath(QStringLiteral("decoration.svg")),
                     QByteArrayLiteral("<svg/>"))
        && writeFile(theme.filePath(QStringLiteral("metadata.desktop")),
                     QByteArrayLiteral("[Desktop Entry]\nName=")
                         + name.toUtf8() + QByteArrayLiteral("\n"));
}

} // namespace

class WindowDecorationControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void discoversAppliesAndPreservesKWinConfiguration();
    void failedReloadNeverClaimsTheSavedSelectionIsLive();
};

void WindowDecorationControllerTests::discoversAppliesAndPreservesKWinConfiguration()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());
    QVERIFY(makeTheme(root, QStringLiteral("Scratchy"), QStringLiteral("Scratchy")));
    QVERIFY(makeTheme(root, QStringLiteral("Platinum"), QStringLiteral("Platinum")));

    const QString configPath = root.filePath(QStringLiteral("kwinrc"));
    {
        QSettings settings(configPath, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("org.kde.kdecoration2"));
        settings.setValue(QStringLiteral("library"),
                          QStringLiteral("org.kde.kwin.aurorae.v2"));
        settings.setValue(QStringLiteral("theme"),
                          QStringLiteral("__aurorae__svg__Scratchy"));
        settings.setValue(QStringLiteral("BorderSize"), QStringLiteral("Normal"));
        settings.endGroup();
        settings.sync();
    }

    int reloadRequests = 0;
    WindowDecorationController controller(
        configPath, {temporary.path()},
        [&reloadRequests](QString *) {
            ++reloadRequests;
            return true;
        });
    QCOMPARE(controller.configuredId(), QStringLiteral("aurorae:Scratchy"));
    QVERIFY(controller.decorations().size() >= 3);
    QVERIFY(controller.selectDecoration(QStringLiteral("aurorae:Platinum")));
    QVERIFY(controller.applyAvailable());
    QVERIFY(controller.applySelection());
    QCOMPARE(reloadRequests, 1);
    QCOMPARE(controller.configuredId(), QStringLiteral("aurorae:Platinum"));

    QSettings applied(configPath, QSettings::IniFormat);
    applied.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    QCOMPARE(applied.value(QStringLiteral("library")).toString(),
             QStringLiteral("org.kde.kwin.aurorae.v2"));
    QCOMPARE(applied.value(QStringLiteral("theme")).toString(),
             QStringLiteral("__aurorae__svg__Platinum"));
    QCOMPARE(applied.value(QStringLiteral("BorderSize")).toString(),
             QStringLiteral("Normal"));
    applied.endGroup();

    QVERIFY(controller.selectDecoration(QStringLiteral("native:org.qindaqt")));
    QVERIFY(controller.selectedUsesQindaQt());
    QVERIFY(controller.applySelection());
    QCOMPARE(reloadRequests, 2);
    applied.sync();
    applied.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    QCOMPARE(applied.value(QStringLiteral("library")).toString(),
             QStringLiteral("org.qindaqt"));
    QVERIFY(!applied.contains(QStringLiteral("theme")));
    QCOMPARE(applied.value(QStringLiteral("BorderSize")).toString(),
             QStringLiteral("Normal"));
}

void WindowDecorationControllerTests::failedReloadNeverClaimsTheSavedSelectionIsLive()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());
    QVERIFY(makeTheme(root, QStringLiteral("Scratchy"), QStringLiteral("Scratchy")));
    const QString configPath = root.filePath(QStringLiteral("kwinrc"));
    {
        QSettings settings(configPath, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("org.kde.kdecoration2"));
        settings.setValue(QStringLiteral("library"),
                          QStringLiteral("org.kde.kwin.aurorae.v2"));
        settings.setValue(QStringLiteral("theme"),
                          QStringLiteral("__aurorae__svg__Scratchy"));
        settings.endGroup();
    }

    WindowDecorationController controller(
        configPath, {temporary.path()}, [](QString *error) {
            *error = QStringLiteral("test reload rejection");
            return false;
        });
    QVERIFY(controller.selectDecoration(QStringLiteral("native:org.qindaqt")));
    QVERIFY(!controller.applySelection());
    QCOMPARE(controller.configuredId(), QStringLiteral("aurorae:Scratchy"));
    QVERIFY(controller.applyAvailable());
    QCOMPARE(controller.errorText(), QStringLiteral("test reload rejection"));

    QSettings saved(configPath, QSettings::IniFormat);
    saved.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    QCOMPARE(saved.value(QStringLiteral("library")).toString(),
             QStringLiteral("org.qindaqt"));
    QVERIFY(!saved.contains(QStringLiteral("theme")));
}

QTEST_GUILESS_MAIN(WindowDecorationControllerTests)
#include "tst_window_decoration_controller.moc"
