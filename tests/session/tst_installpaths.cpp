// SPDX-License-Identifier: GPL-3.0-or-later
#include "installpaths.h"

#include <QDir>
#include <QtTest>

using namespace QindaQt::Session;

class InstallPathsTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesConfiguredKdeLayoutFromInstalledExecutable();
};

void InstallPathsTest::resolvesConfiguredKdeLayoutFromInstalledExecutable()
{
    const QString prefix = QStringLiteral("/opt/qindaqt-layout-test");
    const QString binaryDirectory = QStringLiteral(QINDAQT_EXPECTED_INSTALL_BIN_DIR);
    const QString pluginDirectory = QStringLiteral(QINDAQT_EXPECTED_INSTALL_PLUGIN_DIR);
    QVERIFY2(!QDir::isAbsolutePath(binaryDirectory),
             "This focused relocation test requires a relative KDE_INSTALL_BINDIR");

    const QString executableDirectory = QDir(prefix).absoluteFilePath(binaryDirectory);
    const QString expected = QDir::isAbsolutePath(pluginDirectory)
        ? QDir::cleanPath(pluginDirectory)
        : QDir::cleanPath(QDir(prefix).absoluteFilePath(pluginDirectory));
    QCOMPARE(InstallPaths::pluginRootForExecutableDirectory(executableDirectory), expected);
}

QTEST_APPLESS_MAIN(InstallPathsTest)

#include "tst_installpaths.moc"
