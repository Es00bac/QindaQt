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
};

void SessionEnvironmentTest::enablesDevelopmentControlForExplicitScenario()
{
    EnvironmentRestore scenarioRestore("QINDAQT_TEST_SCENARIO");
    EnvironmentRestore controlRestore("QINDAQT_DEVELOPMENT_CONTROL");
    SessionOptions options;
    options.testScenario = QStringLiteral("/fixtures/single-1080p.json");

    SessionEnvironment::apply(options);

    QCOMPARE(qgetenv("QINDAQT_TEST_SCENARIO"), QByteArray("/fixtures/single-1080p.json"));
    QCOMPARE(qgetenv("QINDAQT_DEVELOPMENT_CONTROL"), QByteArray("1"));
}

void SessionEnvironmentTest::clearsInheritedDevelopmentControlForProductionSession()
{
    EnvironmentRestore scenarioRestore("QINDAQT_TEST_SCENARIO");
    EnvironmentRestore controlRestore("QINDAQT_DEVELOPMENT_CONTROL");
    qputenv("QINDAQT_TEST_SCENARIO", "/stale/scenario.json");
    qputenv("QINDAQT_DEVELOPMENT_CONTROL", "1");

    SessionEnvironment::apply(SessionOptions{});

    QVERIFY(!qEnvironmentVariableIsSet("QINDAQT_TEST_SCENARIO"));
    QVERIFY(!qEnvironmentVariableIsSet("QINDAQT_DEVELOPMENT_CONTROL"));
}

void SessionEnvironmentTest::prependsExplicitPluginRoot()
{
    EnvironmentRestore pluginPathRestore("QT_PLUGIN_PATH");
    EnvironmentRestore scenarioRestore("QINDAQT_TEST_SCENARIO");
    EnvironmentRestore controlRestore("QINDAQT_DEVELOPMENT_CONTROL");
    qputenv("QT_PLUGIN_PATH", "/system/qt/plugins");
    SessionOptions options;
    options.pluginRoot = QStringLiteral("/opt/qindaqt/plugins");

    SessionEnvironment::apply(options);

    const QString expected = options.pluginRoot + QDir::listSeparator()
        + QStringLiteral("/system/qt/plugins");
    QCOMPARE(QString::fromUtf8(qgetenv("QT_PLUGIN_PATH")), expected);
}

QTEST_APPLESS_MAIN(SessionEnvironmentTest)

#include "tst_sessionenvironment.moc"
