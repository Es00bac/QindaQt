// SPDX-License-Identifier: GPL-3.0-or-later
#include "windowmanagementconfig.h"

#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;

class WindowManagementConfigTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsMatchTheShippedBehaviour();
    void everySettingsSpellingMapsToItsChord();
    void closePolicyAndSessionRestoreDecode();
    void unknownSpellingsFallBackPerEntry();
};

void WindowManagementConfigTest::defaultsMatchTheShippedBehaviour()
{
    const WindowManagementConfig config;
    QVERIFY(config.dockingModifiers == std::optional(Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(!config.closeDecision.has_value());
    QVERIFY(config.sessionRestore);
    QVERIFY(WindowManagementConfig::fromEntries(QStringLiteral("super"), QStringLiteral("ask"),
                                                QStringLiteral("true"))
            == config);
}

void WindowManagementConfigTest::everySettingsSpellingMapsToItsChord()
{
    QVERIFY(WindowManagementConfig::dockingModifiersFor(QStringLiteral("super"))
            == std::optional(Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(WindowManagementConfig::dockingModifiersFor(QStringLiteral("alt"))
            == std::optional(Qt::AltModifier | Qt::ShiftModifier));
    QVERIFY(WindowManagementConfig::dockingModifiersFor(QStringLiteral("control"))
            == std::optional(Qt::ControlModifier | Qt::ShiftModifier));
    QVERIFY(!WindowManagementConfig::dockingModifiersFor(QStringLiteral("disabled")).has_value());
    QVERIFY(!WindowManagementConfig::dockingModifiersFor(QStringLiteral(" Disabled ")).has_value());
}

void WindowManagementConfigTest::closePolicyAndSessionRestoreDecode()
{
    const auto closeAll = WindowManagementConfig::fromEntries(
        QStringLiteral("alt"), QStringLiteral("close-all"), QStringLiteral("false"));
    QVERIFY(closeAll.closeDecision == std::optional(ContainerCloseDecision::CloseAll));
    QVERIFY(!closeAll.sessionRestore);
    const auto ungroup = WindowManagementConfig::fromEntries(
        QStringLiteral("control"), QStringLiteral("ungroup"), QStringLiteral("true"));
    QVERIFY(ungroup.closeDecision == std::optional(ContainerCloseDecision::Ungroup));
    QVERIFY(ungroup.sessionRestore);
    QVERIFY(ungroup != closeAll);
}

void WindowManagementConfigTest::unknownSpellingsFallBackPerEntry()
{
    const auto config = WindowManagementConfig::fromEntries(
        QStringLiteral("hyper"), QStringLiteral("maybe"), QStringLiteral("sometimes"));
    QVERIFY(config.dockingModifiers == std::optional(Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(!config.closeDecision.has_value());
    QVERIFY(config.sessionRestore);
    // One bad entry never drags a good neighbour back to its default.
    const auto mixed = WindowManagementConfig::fromEntries(
        QStringLiteral("hyper"), QStringLiteral("ungroup"), QStringLiteral("0"));
    QVERIFY(mixed.closeDecision == std::optional(ContainerCloseDecision::Ungroup));
    QVERIFY(!mixed.sessionRestore);
}

QTEST_GUILESS_MAIN(WindowManagementConfigTest)
#include "tst_windowmanagementconfig.moc"
