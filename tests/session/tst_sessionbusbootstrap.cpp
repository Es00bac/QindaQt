// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionbusbootstrap.h"

#include <QTest>

using namespace QindaQt::Session;

class SessionBusBootstrapTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesProvidedBus();
    void wrapsOriginalLauncherArgumentsWhenBusIsMissing();
    void treatsWhitespaceAddressAsMissing();
    void refusesAnUnavailableRunner();
};

void SessionBusBootstrapTest::preservesProvidedBus()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                       QStringLiteral("unix:path=/run/user/1000/bus"));
    const auto result = SessionBusBootstrap::command(
        {QStringLiteral("/usr/bin/qindaqt-wm"), QStringLiteral("--drm")},
        environment, QStringLiteral("/usr/bin/dbus-run-session"));
    QVERIFY(!result.required());
}

void SessionBusBootstrapTest::wrapsOriginalLauncherArgumentsWhenBusIsMissing()
{
    const auto result = SessionBusBootstrap::command(
        {QStringLiteral("/usr/bin/qindaqt-wm"), QStringLiteral("--drm")}, {},
        QStringLiteral("/usr/bin/dbus-run-session"));
    QVERIFY(result.required());
    QCOMPARE(result.executable, QStringLiteral("/usr/bin/dbus-run-session"));
    QCOMPARE(result.arguments,
             QStringList({QStringLiteral("--"), QStringLiteral("/usr/bin/qindaqt-wm"),
                          QStringLiteral("--drm")}));
}

void SessionBusBootstrapTest::treatsWhitespaceAddressAsMissing()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                       QStringLiteral("  \t"));
    const auto result = SessionBusBootstrap::command(
        {QStringLiteral("qindaqt-wm")}, environment,
        QStringLiteral("/usr/bin/dbus-run-session"));
    QVERIFY(result.required());
}

void SessionBusBootstrapTest::refusesAnUnavailableRunner()
{
    const auto result = SessionBusBootstrap::command(
        {QStringLiteral("qindaqt-wm")}, {}, {});
    QVERIFY(!result.required());
}

QTEST_GUILESS_MAIN(SessionBusBootstrapTest)

#include "tst_sessionbusbootstrap.moc"
