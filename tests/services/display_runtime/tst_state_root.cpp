// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_runtime/state_root.h>

#include <QtTest/QTest>

using namespace QindaQt::DisplayRuntime;

class StateRootTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void selectsOneDeterministicSource();
    void rejectsMissingAmbiguousAndHostileValues();
};

void StateRootTest::selectsOneDeterministicSource()
{
    const StateRootSelection explicitRoot = selectStateRoot(
        {.explicitPath = QStringLiteral("/explicit/qindaqt"),
         .systemdStateDirectory = QStringLiteral("/systemd/qindaqt"),
         .xdgStateHome = QStringLiteral("/xdg"),
         .home = QStringLiteral("/home/user")});
    QVERIFY(explicitRoot.accepted());
    QCOMPARE(explicitRoot.path, QStringLiteral("/explicit/qindaqt"));

    const StateRootSelection systemdRoot = selectStateRoot(
        {.explicitPath = {},
         .systemdStateDirectory = QStringLiteral("/systemd/qindaqt"),
         .xdgStateHome = QStringLiteral("/xdg"),
         .home = QStringLiteral("/home/user")});
    QVERIFY(systemdRoot.accepted());
    QCOMPARE(systemdRoot.path, QStringLiteral("/systemd/qindaqt"));

    const StateRootSelection xdgRoot = selectStateRoot(
        {.explicitPath = {},
         .systemdStateDirectory = {},
         .xdgStateHome = QStringLiteral("/state"),
         .home = QStringLiteral("/home/user")});
    QVERIFY(xdgRoot.accepted());
    QCOMPARE(xdgRoot.path, QStringLiteral("/state/qindaqt"));

    const StateRootSelection homeRoot =
        selectStateRoot({.explicitPath = {},
                         .systemdStateDirectory = {},
                         .xdgStateHome = {},
                         .home = QStringLiteral("/home/user")});
    QVERIFY(homeRoot.accepted());
    QCOMPARE(homeRoot.path, QStringLiteral("/home/user/.local/state/qindaqt"));
}

void StateRootTest::rejectsMissingAmbiguousAndHostileValues()
{
    QCOMPARE(selectStateRoot({}).error, StateRootError::Missing);
    QCOMPARE(selectStateRoot(
                 {.explicitPath = {},
                  .systemdStateDirectory = QStringLiteral("/one:/two"),
                  .xdgStateHome = {},
                  .home = {}})
                 .error,
             StateRootError::AmbiguousSystemdDirectory);

    for (const QString &path : {QStringLiteral("relative"), QStringLiteral("/"),
                                QStringLiteral("/state/../escape"),
                                QStringLiteral("/state//qindaqt"),
                                QStringLiteral("/state\nqindaqt")}) {
        const StateRootSelection result =
            selectStateRoot({.explicitPath = path,
                             .systemdStateDirectory = {},
                             .xdgStateHome = {},
                             .home = {}});
        QCOMPARE(result.error, StateRootError::InvalidPath);
        QVERIFY(result.path.isEmpty());
    }

    QCOMPARE(selectStateRoot({.explicitPath = {},
                              .systemdStateDirectory = {},
                              .xdgStateHome = QStringLiteral("relative"),
                              .home = {}})
                 .error,
             StateRootError::InvalidPath);
    QCOMPARE(selectStateRoot({.explicitPath = {},
                              .systemdStateDirectory = {},
                              .xdgStateHome = {},
                              .home = QStringLiteral("/")})
                 .error,
             StateRootError::InvalidPath);
}

QTEST_GUILESS_MAIN(StateRootTest)

#include "tst_state_root.moc"
