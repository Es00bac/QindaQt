// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessioncommandline.h"

#include "installpaths.h"

#include <QtTest>

using namespace QindaQt::Session;

class SessionCommandLineTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesVirtualOptions();
    void usesInstalledPluginRootByDefault();
    void acceptsExplicitPluginRoot();
    void rejectsConflictingBackends();
    void rejectsNonNumericDimensions();
};

void SessionCommandLineTest::parsesVirtualOptions()
{
    QString error;
    const auto options = SessionCommandLine::parse(
        {QStringLiteral("qindaqt-wm"),
         QStringLiteral("--virtual"),
         QStringLiteral("--width"),
         QStringLiteral("1920"),
         QStringLiteral("--height"),
         QStringLiteral("1200"),
         QStringLiteral("--output-count"),
         QStringLiteral("3"),
         QStringLiteral("--no-lockscreen")},
        &error);
    QVERIFY2(options.has_value(), qPrintable(error));
    QCOMPARE(options->backend, Backend::Virtual);
    QCOMPARE(options->outputSize, QSize(1920, 1200));
    QCOMPARE(options->outputCount, 3);
    QVERIFY(options->xwayland);
    QVERIFY(!options->lockscreen);
}

void SessionCommandLineTest::usesInstalledPluginRootByDefault()
{
    QString error;
    const auto options = SessionCommandLine::parse({QStringLiteral("qindaqt-wm")}, &error);
    QVERIFY2(options.has_value(), qPrintable(error));
    QCOMPARE(options->pluginRoot, InstallPaths::pluginRoot());
}

void SessionCommandLineTest::acceptsExplicitPluginRoot()
{
    QString error;
    const auto options = SessionCommandLine::parse(
        {QStringLiteral("qindaqt-wm"),
         QStringLiteral("--plugin-root"),
         QStringLiteral("/tmp/qindaqt-build/plugins")},
        &error);
    QVERIFY2(options.has_value(), qPrintable(error));
    QCOMPARE(options->pluginRoot, QStringLiteral("/tmp/qindaqt-build/plugins"));
}

void SessionCommandLineTest::rejectsConflictingBackends()
{
    QString error;
    const auto options = SessionCommandLine::parse(
        {QStringLiteral("qindaqt-wm"), QStringLiteral("--drm"), QStringLiteral("--virtual")},
        &error);
    QVERIFY(!options.has_value());
    QVERIFY(error.contains(QStringLiteral("only one")));
}

void SessionCommandLineTest::rejectsNonNumericDimensions()
{
    QString error;
    const auto options = SessionCommandLine::parse(
        {QStringLiteral("qindaqt-wm"),
         QStringLiteral("--virtual"),
         QStringLiteral("--width"),
         QStringLiteral("wide")},
        &error);
    QVERIFY(!options.has_value());
    QVERIFY(error.contains(QStringLiteral("numeric")));
}

QTEST_APPLESS_MAIN(SessionCommandLineTest)

#include "tst_sessioncommandline.moc"
