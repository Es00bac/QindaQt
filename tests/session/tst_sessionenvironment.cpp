// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionenvironment.h"

#include <QDir>
#include <QtTest>

using namespace QindaQt::Session;

namespace {

class EnvironmentRestore final
{
public:
    explicit EnvironmentRestore(const char *name)
        : m_name(name)
        , m_wasSet(qEnvironmentVariableIsSet(name))
        , m_value(qgetenv(name))
    {
    }

    ~EnvironmentRestore()
    {
        if (m_wasSet) {
            qputenv(m_name, m_value);
        } else {
            qunsetenv(m_name);
        }
    }

private:
    const char *m_name;
    bool m_wasSet;
    QByteArray m_value;
};

} // namespace

class SessionEnvironmentTest final : public QObject
{
    Q_OBJECT

private slots:
    void enablesDevelopmentControlForExplicitScenario();
    void clearsInheritedDevelopmentControlForProductionSession();
    void prependsExplicitPluginRoot();
    void defaultsToSharedQtThemeWithoutOverwritingUserChoice();
};

void SessionEnvironmentTest::enablesDevelopmentControlForExplicitScenario()
{
    EnvironmentRestore scenarioRestore("QINDAQT_TEST_SCENARIO");
    EnvironmentRestore controlRestore("QINDAQT_DEVELOPMENT_CONTROL");
    EnvironmentRestore outputBackendRestore("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND");
    qputenv("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND", "virtual");
    SessionOptions options;
    options.testScenario = QStringLiteral("/fixtures/single-1080p.json");

    SessionEnvironment::apply(options);

    QCOMPARE(qgetenv("QINDAQT_TEST_SCENARIO"), QByteArray("/fixtures/single-1080p.json"));
    QCOMPARE(qgetenv("QINDAQT_DEVELOPMENT_CONTROL"), QByteArray("1"));
    QVERIFY(!qEnvironmentVariableIsSet("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND"));

    options.backend = Backend::Virtual;
    SessionEnvironment::apply(options);
    QCOMPARE(qgetenv("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND"), QByteArray("virtual"));
}

void SessionEnvironmentTest::clearsInheritedDevelopmentControlForProductionSession()
{
    EnvironmentRestore scenarioRestore("QINDAQT_TEST_SCENARIO");
    EnvironmentRestore controlRestore("QINDAQT_DEVELOPMENT_CONTROL");
    EnvironmentRestore outputBackendRestore("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND");
    qputenv("QINDAQT_TEST_SCENARIO", "/stale/scenario.json");
    qputenv("QINDAQT_DEVELOPMENT_CONTROL", "1");
    qputenv("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND", "virtual");

    SessionEnvironment::apply(SessionOptions{});

    QVERIFY(!qEnvironmentVariableIsSet("QINDAQT_TEST_SCENARIO"));
    QVERIFY(!qEnvironmentVariableIsSet("QINDAQT_DEVELOPMENT_CONTROL"));
    QVERIFY(!qEnvironmentVariableIsSet("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND"));
}

void SessionEnvironmentTest::prependsExplicitPluginRoot()
{
    EnvironmentRestore pluginPathRestore("QT_PLUGIN_PATH");
    EnvironmentRestore scenarioRestore("QINDAQT_TEST_SCENARIO");
    EnvironmentRestore controlRestore("QINDAQT_DEVELOPMENT_CONTROL");
    EnvironmentRestore outputBackendRestore("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND");
    qputenv("QT_PLUGIN_PATH", "/system/qt/plugins");
    SessionOptions options;
    options.pluginRoot = QStringLiteral("/opt/qindaqt/plugins");

    SessionEnvironment::apply(options);

    const QString expected = options.pluginRoot + QDir::listSeparator()
        + QStringLiteral("/system/qt/plugins");
    QCOMPARE(QString::fromUtf8(qgetenv("QT_PLUGIN_PATH")), expected);
}

void SessionEnvironmentTest::defaultsToSharedQtThemeWithoutOverwritingUserChoice()
{
    EnvironmentRestore platform("QT_QPA_PLATFORMTHEME");
    EnvironmentRestore quickStyle("QT_QUICK_CONTROLS_STYLE");
    EnvironmentRestore widgetStyle("QT_STYLE_OVERRIDE");
    qunsetenv("QT_QPA_PLATFORMTHEME");
    qunsetenv("QT_QUICK_CONTROLS_STYLE");
    qputenv("QT_STYLE_OVERRIDE", "Windows");
    SessionEnvironment::apply(SessionOptions{});
    QCOMPARE(qgetenv("QT_QPA_PLATFORMTHEME"), QByteArray("qindaqt"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_STYLE"), QByteArray("Fusion"));
    QCOMPARE(qgetenv("QT_STYLE_OVERRIDE"), QByteArray("Windows"));
    qputenv("QT_QPA_PLATFORMTHEME", "custom");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    SessionEnvironment::apply(SessionOptions{});
    QCOMPARE(qgetenv("QT_QPA_PLATFORMTHEME"), QByteArray("custom"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_STYLE"), QByteArray("Basic"));
}

QTEST_APPLESS_MAIN(SessionEnvironmentTest)

#include "tst_sessionenvironment.moc"
