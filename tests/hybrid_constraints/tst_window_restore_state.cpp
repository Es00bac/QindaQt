// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_constraints/window_restore_state.h"

#include <QHash>
#include <QJsonDocument>
#include <QTest>

using namespace QindaQt::HybridConstraints;

class WindowRestoreStateTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTripsEveryFieldLosslessly();
    void hasIndependentValueSemantics();
    void preservesUnmaximizedUntiledState();
    void readsSchemaV1WithVisibleTaskDefaults();
    void rejectsIncompleteAndInvalidRepresentations();
};

void WindowRestoreStateTest::roundTripsEveryFieldLosslessly()
{
    WindowRestoreState original{
        .geometry = QRectF(17.25, -3.5, 1280.5, 719.75),
        .minimized = true,
        .maximizedAxes = MaximizeAxis::Horizontal | MaximizeAxis::Vertical,
        .quickTileEdges = QuickTileEdge::Left | QuickTileEdge::Top,
        .fullscreen = true,
        .outputId = QStringLiteral("output-uuid-2"),
        .desktopIds = {QStringLiteral("desktop-2"), QStringLiteral("desktop-4")},
        .activityIds = {QStringLiteral("activity-writing"),
                        QStringLiteral("activity-testing")},
        .keepAbove = true,
        .keepBelow = false,
        .focused = true,
        .skipTaskbar = true,
        .skipSwitcher = false,
    };

    QString error;
    QVERIFY2(original.isValid(&error), qPrintable(error));
    const QJsonDocument encoded(original.toJson());
    QJsonParseError parseError;
    const auto decoded = QJsonDocument::fromJson(
        encoded.toJson(QJsonDocument::Compact), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    const auto restored = WindowRestoreState::fromJson(decoded.object(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(*restored, original);
    QVERIFY(restored->isMaximized());
    QVERIFY(restored->isQuickTiled());
}

void WindowRestoreStateTest::hasIndependentValueSemantics()
{
    const WindowRestoreState original{
        .geometry = QRectF(40.0, 60.0, 900.0, 700.0),
        .minimized = false,
        .maximizedAxes = {},
        .quickTileEdges = {},
        .fullscreen = false,
        .outputId = QStringLiteral("output-main"),
        .desktopIds = {QStringLiteral("desktop-1")},
        .activityIds = {QStringLiteral("activity-main")},
        .keepAbove = false,
        .keepBelow = false,
        .focused = true,
    };
    auto copy = original;
    copy.geometry.moveLeft(400.0);
    copy.desktopIds.append(QStringLiteral("desktop-2"));
    copy.focused = false;

    QCOMPARE(original.geometry, QRectF(40.0, 60.0, 900.0, 700.0));
    QCOMPARE(original.desktopIds, QStringList{QStringLiteral("desktop-1")});
    QVERIFY(original.focused);
    QVERIFY(copy != original);

    QHash<QString, WindowRestoreState> states;
    states.insert(QStringLiteral("window-1"), original);
    QCOMPARE(states.value(QStringLiteral("window-1")), original);
}

void WindowRestoreStateTest::preservesUnmaximizedUntiledState()
{
    const WindowRestoreState ordinary{
        .geometry = QRectF(0.0, 0.0, 640.0, 480.0),
        .minimized = false,
        .maximizedAxes = {},
        .quickTileEdges = {},
        .fullscreen = false,
        .outputId = QStringLiteral("output-main"),
        .desktopIds = {},
        .activityIds = {},
        .keepAbove = false,
        .keepBelow = false,
        .focused = false,
    };

    const auto restored = WindowRestoreState::fromJson(ordinary.toJson());
    QVERIFY(restored.has_value());
    QCOMPARE(*restored, ordinary);
    QVERIFY(!restored->isMaximized());
    QVERIFY(!restored->isQuickTiled());
}

void WindowRestoreStateTest::readsSchemaV1WithVisibleTaskDefaults()
{
    WindowRestoreState current{
        .geometry = QRectF(0.0, 0.0, 640.0, 480.0),
        .minimized = false,
        .maximizedAxes = {},
        .quickTileEdges = {},
        .fullscreen = false,
        .outputId = QStringLiteral("output-main"),
        .desktopIds = {},
        .activityIds = {},
        .keepAbove = false,
        .keepBelow = false,
        .focused = false,
        .skipTaskbar = true,
        .skipSwitcher = true,
    };
    auto legacy = current.toJson();
    legacy.insert(QStringLiteral("schemaVersion"), 1);
    legacy.remove(QStringLiteral("skipTaskbar"));
    legacy.remove(QStringLiteral("skipSwitcher"));

    QString error;
    const auto restored = WindowRestoreState::fromJson(legacy, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QVERIFY(!restored->skipTaskbar);
    QVERIFY(!restored->skipSwitcher);
}

void WindowRestoreStateTest::rejectsIncompleteAndInvalidRepresentations()
{
    const WindowRestoreState valid{
        .geometry = QRectF(0.0, 0.0, 640.0, 480.0),
        .minimized = false,
        .maximizedAxes = {},
        .quickTileEdges = {},
        .fullscreen = false,
        .outputId = QStringLiteral("output-main"),
        .desktopIds = {QStringLiteral("desktop-1")},
        .activityIds = {},
        .keepAbove = false,
        .keepBelow = false,
        .focused = false,
    };

    auto incomplete = valid.toJson();
    incomplete.remove(QStringLiteral("focused"));
    QString error;
    QVERIFY(!WindowRestoreState::fromJson(incomplete, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("focused")));

    auto missingTaskFlag = valid.toJson();
    missingTaskFlag.remove(QStringLiteral("skipSwitcher"));
    QVERIFY(!WindowRestoreState::fromJson(missingTaskFlag, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("skipSwitcher")));

    auto unknownFlags = valid.toJson();
    unknownFlags.insert(QStringLiteral("quickTileEdges"), 32);
    QVERIFY(!WindowRestoreState::fromJson(unknownFlags, &error).has_value());

    auto fractionalSchema = valid.toJson();
    fractionalSchema.insert(QStringLiteral("schemaVersion"), 1.5);
    QVERIFY(!WindowRestoreState::fromJson(fractionalSchema, &error).has_value());

    auto duplicateDesktop = valid;
    duplicateDesktop.desktopIds.append(QStringLiteral("desktop-1"));
    QVERIFY(!duplicateDesktop.isValid(&error));

    auto contradictoryLayer = valid;
    contradictoryLayer.keepAbove = true;
    contradictoryLayer.keepBelow = true;
    QVERIFY(!contradictoryLayer.isValid(&error));
}

QTEST_GUILESS_MAIN(WindowRestoreStateTest)
#include "tst_window_restore_state.moc"
