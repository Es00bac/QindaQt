// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtimepanelappletcompatibility.h"

#include <QtTest>

using QindaQt::Profiles::AppletSpec;
using QindaQt::Shell::RuntimePanelAppletCompatibility;

class RuntimePanelAppletCompatibilityTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void legacyLauncherBecomesTheCompiledLauncher();
    void redundantLegacyTaskListIsRemoved();
    void soleLegacyTaskListBecomesTheCompiledTaskList();
};

void RuntimePanelAppletCompatibilityTests::legacyLauncherBecomesTheCompiledLauncher()
{
    const auto normalized = RuntimePanelAppletCompatibility::normalize({
        {QStringLiteral("apps"), QStringLiteral("application-launcher"),
         {{QStringLiteral("zone"), QStringLiteral("center")}}},
    });
    QCOMPARE(normalized.size(), 1);
    QCOMPARE(normalized[0].id, QStringLiteral("apps"));
    QCOMPARE(normalized[0].plugin, QStringLiteral("launcher"));
    QCOMPARE(normalized[0].settings.value(QStringLiteral("zone")),
             QVariant(QStringLiteral("center")));
}

void RuntimePanelAppletCompatibilityTests::redundantLegacyTaskListIsRemoved()
{
    const auto normalized = RuntimePanelAppletCompatibility::normalize({
        {QStringLiteral("hosted-task-list"), QStringLiteral("task-list"), {}},
        {QStringLiteral("tasks"), QStringLiteral("grouped-task-list"), {}},
    });
    QCOMPARE(normalized.size(), 1);
    QCOMPARE(normalized[0].id, QStringLiteral("hosted-task-list"));
}

void RuntimePanelAppletCompatibilityTests::soleLegacyTaskListBecomesTheCompiledTaskList()
{
    const auto normalized = RuntimePanelAppletCompatibility::normalize({
        {QStringLiteral("tasks"), QStringLiteral("grouped-task-list"), {}},
    });
    QCOMPARE(normalized.size(), 1);
    QCOMPARE(normalized[0].id, QStringLiteral("tasks"));
    QCOMPARE(normalized[0].plugin, QStringLiteral("task-list"));
}

QTEST_GUILESS_MAIN(RuntimePanelAppletCompatibilityTests)
#include "tst_runtimepanelappletcompatibility.moc"
